/* Copyright (c) 2026 Ry
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
/* Diagnostic-only checker for the observed BIOS $F01B row-run and $F021 row-literal stream
 * contracts. It compares BIOS execution with an independent behavioral model at LCD-port
 * granularity. The two services share their LCD preamble, row commands, escape, and terminator;
 * $F01B decodes value/count runs, while $F021 writes each data byte once.
 *
 * corpus mode runs the standard post-entry window; at every service entry it replays a copy of the
 * machine with I set and checks the call, then replays a second copy with the authentic interrupt
 * state to confirm that interrupts add only handler time. The original machine continues
 * unchanged.
 * stdin mode snapshots the first service entry and replays purpose-written synthetic streams, one
 * per line: [@LCD] [!PPAAXXYY] [%POINTER] HEX... Pointers in cartridge space place the stream in
 * the private in-memory ROM copy, which is restored after the case.
 *
 * Only derived counts are printed for authentic calls; no ROM, BIOS, or VRAM payload is printed. */
#include "gamate/gamate.h"
#include "gamatebiosinput.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { MAX_PORTS = 1 << 18, BASE_ROW = 0xE6, CLEAR_ROWS = 25, TERMINATOR = 0xFE, ESCAPE = 0xFF };

/* Clock terms fitted from synthetic streams and checked against every authentic call. */
enum {
	BASE_CLOCKS = 15362, ROW_CLOCKS = 140, PAIR_CLOCKS = 134, REPEAT_CLOCKS = 18,
	LITERAL_CLOCKS = 66, CROSSING_CLOCKS = 2, WRAP_CLOCKS = 8,
};

/* Selected service: false for $F01B value/count runs, true for $F021 literal bytes. */
static bool literal;
static uint16_t entry;

struct Ports {
	uint8_t regs[MAX_PORTS], values[MAX_PORTS];
	size_t count;
};

struct Model {
	struct Ports ports;
	uint8_t y, page, lastValue;
	unsigned pairs, runBytes, rows, wraps, crossings;
	bool overflow, terminated;
	uint64_t clocks;
};

struct Result {
	bool returned, interrupted, ports, registers, status, ram, peripherals, clocks;
	long long clockDelta;
	unsigned stackDepth;
};

struct Totals {
	unsigned long calls, returned, interrupted, ports, registers, status, ram, peripherals, clocks;
	unsigned long romStreams, ramStreams, maxPairs, maxRunBytes, maxRows, maxWraps, maxCrossings;
	unsigned long overflowCalls, decimalEntry, maxStackDepth;
	unsigned long openEntry, openReturned, openSpanned, openPorts, openRegisters, openExtraMax;
};

static struct Ports captured;

static uint8_t* readFile(const char* path, size_t* size) {
	FILE* file = fopen(path, "rb");
	if (!file) return NULL;
	if (fseek(file, 0, SEEK_END) || (*size = (size_t) ftell(file), fseek(file, 0, SEEK_SET))) {
		fclose(file);
		return NULL;
	}
	uint8_t* data = malloc(*size ? *size : 1);
	if (!data || fread(data, 1, *size, file) != *size) {
		free(data);
		data = NULL;
	}
	fclose(file);
	return data;
}

static bool isCartridge(uint16_t address) {
	return address >= 0x6000 && address < 0xE000;
}

static size_t cartridgeOffset(const struct Gamate* g, uint16_t address) {
	size_t offset = address - 0x6000;
	if (g->mapper == GAMATE_MAPPER_4IN1 && offset < 0x4000) {
		offset = (size_t) g->firstBank * 0x4000 + offset;
	} else if (g->mapper != GAMATE_MAPPER_PLAIN && offset >= 0x4000) {
		offset = (size_t) g->bank * 0x4000 + (offset & 0x3FFF);
	}
	return offset < g->romSize ? offset : offset % g->romSize;
}

static uint8_t peek(const struct Gamate* g, uint16_t address) {
	if (address < 0x2000) return g->ram[address & 0x3FF];
	if (isCartridge(address)) return g->rom[cartridgeOffset(g, address)];
	if (address >= 0xE000) return g->bios[address & 0x0FFF];
	return 0;
}

static void emit(struct Ports* p, uint8_t reg, uint8_t value) {
	if (p->count < MAX_PORTS) {
		p->regs[p->count] = reg;
		p->values[p->count] = value;
	}
	p->count++;
}

static void captureLcdWrite(void* context, const struct Gamate* g, uint8_t reg, uint8_t value) {
	(void) g;
	emit(context, reg, value);
}

/* Adds a row offset to BASE_ROW with carry clear under the caller's D flag. Decimal mode follows
 * the published NMOS sequence, with V taken after the low-nibble adjustment. */
static uint8_t rowAdd(uint8_t offset, bool decimal, bool* overflow) {
	if (!decimal) {
		int sum = (int8_t) BASE_ROW + (int8_t) offset;
		*overflow = sum < -128 || sum > 127;
		return (uint8_t) (BASE_ROW + offset);
	}
	unsigned low = (BASE_ROW & 0x0F) + (offset & 0x0F);
	if (low > 9) low = ((low + 6) & 0x0F) + 0x10;
	int high = (int8_t) (BASE_ROW & 0xF0) + (int8_t) (offset & 0xF0) + (int) low;
	*overflow = high < -128 || high > 127;
	unsigned sum = (BASE_ROW & 0xF0) + (offset & 0xF0) + low;
	if (sum >= 0xA0) sum += 0x60;
	return (uint8_t) sum;
}

/* The stream at RAM $000-$001 is read sequentially through an indexed pointer. The first byte
 * and every byte following an escape are row offsets from BASE_ROW; other bytes form value/count
 * pairs, with count zero meaning 256. */
static void model(const struct Gamate* g, struct Model* m) {
	memset(m, 0, sizeof(*m));
	uint16_t pointer = (uint16_t) (g->ram[0] | g->ram[1] << 8);
	emit(&m->ports, 1, g->ram[0x15]);
	for (unsigned row = BASE_ROW; row < BASE_ROW + CLEAR_ROWS; row++) {
		emit(&m->ports, 5, (uint8_t) row);
		emit(&m->ports, 4, 0);
		for (unsigned i = 0; i < 32; i++) emit(&m->ports, 7, 0);
	}
	unsigned index = 0;
	uint8_t offset = peek(g, pointer);
	while (offset != TERMINATOR && index < 0x10000 && m->ports.count < MAX_PORTS) {
		m->rows++;
		emit(&m->ports, 5, rowAdd(offset, g->cpu.p & 0x08, &m->overflow));
		emit(&m->ports, 4, 0);
		for (;;) {
			uint8_t value = peek(g, (uint16_t) (pointer + ++index));
			if (value == ESCAPE) {
				offset = peek(g, (uint16_t) (pointer + ++index));
				break;
			}
			unsigned length = 1;
			if (!literal) {
				uint8_t count = peek(g, (uint16_t) (pointer + ++index));
				length = count ? count : 256u;
				m->pairs++;
			}
			m->runBytes += length;
			m->lastValue = value;
			for (unsigned i = 0; i < length; i++) emit(&m->ports, 7, value);
			if (index >= 0x10000 || m->ports.count >= MAX_PORTS) break;
		}
	}
	m->terminated = offset == TERMINATOR && index < 0x10000 && m->ports.count < MAX_PORTS;
	m->y = (uint8_t) index;
	m->page = (uint8_t) (g->ram[1] + (index >> 8));
	m->wraps = index >> 8;
	for (unsigned i = 0; i <= index; i++) m->crossings += (pointer & 0xFF) + (i & 0xFF) > 0xFF;
	m->clocks = BASE_CLOCKS + (uint64_t) ROW_CLOCKS * m->rows + (uint64_t) PAIR_CLOCKS * m->pairs +
		(uint64_t) REPEAT_CLOCKS * (m->runBytes - m->pairs) * !literal +
		(uint64_t) LITERAL_CLOCKS * m->runBytes * literal +
		(uint64_t) CROSSING_CLOCKS * m->crossings + (uint64_t) WRAP_CLOCKS * m->wraps;
}

static bool ramWritten(const struct Gamate* g, unsigned offset) {
	return (g->diagnostics.ramWritten[offset >> 3] >> (offset & 7)) & 1;
}

/* Runs one call from an entry state already at $F01B and compares it with the model. */
static struct Result check(struct Gamate* g, struct Model* m, bool events) {
	model(g, m);
	struct Result r = { 0 };
	uint8_t inP = g->cpu.p, returnS = (uint8_t) (g->cpu.s + 2);
	uint16_t expectedReturn = (uint16_t) ((g->ram[0x100 | (uint8_t) (g->cpu.s + 1)] |
		g->ram[0x100 | (uint8_t) (g->cpu.s + 2)] << 8) + 1);
	uint64_t start = g->masterClocks, irqs = g->diagnostics.irqsTaken;
	uint64_t nmis = g->diagnostics.nmisTaken;
	uint64_t ayWrites = g->diagnostics.ayWrites, bankWrites = g->diagnostics.bankWrites;
	uint64_t uartClocks = g->diagnostics.uartClocks;
	uint8_t uartTx = g->uartTx, uartTxBits = g->uartTxBits;
	bool uartTxLine = g->uartTxLine;
	uint64_t limit = m->clocks + 1000000;
	captured.count = 0;
	gamateResetRamDiagnostics(g);
	gamateSetLcdDiagnosticWatch(g, captureLcdWrite, &captured);
	uint8_t entryS = g->cpu.s;
	while (!r.returned && g->masterClocks - start < limit) {
		if ((uint8_t) (entryS - g->cpu.s) > r.stackDepth && g->cpu.s < entryS) {
			r.stackDepth = (uint8_t) (entryS - g->cpu.s);
		}
		uint16_t source = g->cpu.pc;
		uint8_t opcode = peek(g, source);
		unsigned cycles = gamateCpuStep(g);
		r.returned = source >= 0xE000 && opcode == 0x60 && g->cpu.pc == expectedReturn &&
			g->cpu.s == returnS;
		gamateRunClocks(g, cycles * 2);
	}
	gamateSetLcdDiagnosticWatch(g, NULL, NULL);
	r.interrupted = g->diagnostics.irqsTaken != irqs || g->diagnostics.nmisTaken != nmis;
	r.ports = m->terminated && captured.count == m->ports.count &&
		!memcmp(captured.regs, m->ports.regs, m->ports.count) &&
		!memcmp(captured.values, m->ports.values, m->ports.count);
	r.registers = r.returned && g->cpu.a == TERMINATOR && g->cpu.x == 0 && g->cpu.y == m->y;
	r.status = g->cpu.p == (uint8_t) ((inP & 0x1C) | 0x23 | (m->overflow ? 0x40 : 0));
	unsigned other = 0;
	for (unsigned offset = 0; offset < 0x400; offset++) {
		if (offset != 0x01 && offset != 0x1A && (offset < 0x100 || offset > 0x1FF)) {
			other += ramWritten(g, offset);
		}
	}
	r.ram = !other && g->ram[1] == m->page && ramWritten(g, 0x01) == (m->wraps != 0) &&
		ramWritten(g, 0x1A) == (m->pairs != 0) && (!m->pairs || g->ram[0x1A] == m->lastValue);
	r.peripherals = g->diagnostics.ayWrites == ayWrites && g->diagnostics.bankWrites == bankWrites &&
		g->diagnostics.uartClocks == uartClocks && g->uartTx == uartTx &&
		g->uartTxBits == uartTxBits && g->uartTxLine == uartTxLine;
	r.clockDelta = (long long) (g->masterClocks - start) - (long long) m->clocks;
	r.clocks = r.returned && r.clockDelta == 0;
	if (events) {
		for (size_t i = 0; i < captured.count && i < MAX_PORTS; i++) {
			printf(" %u:%02X", captured.regs[i], captured.values[i]);
		}
		putchar('\n');
	}
	return r;
}

static void tally(struct Totals* t, const struct Result* r, const struct Model* m,
		uint16_t pointer, uint8_t inP) {
	t->calls++;
	t->returned += r->returned;
	t->interrupted += r->interrupted;
	t->ports += !r->ports;
	t->registers += !r->registers;
	t->status += !r->status;
	t->ram += !r->ram;
	t->peripherals += !r->peripherals;
	t->clocks += !r->clocks;
	t->romStreams += isCartridge(pointer);
	t->ramStreams += pointer < 0x2000;
	t->overflowCalls += m->overflow;
	t->decimalEntry += (inP & 0x08) != 0;
	if (r->stackDepth > t->maxStackDepth) t->maxStackDepth = r->stackDepth;
	if (m->pairs > t->maxPairs) t->maxPairs = m->pairs;
	if (m->runBytes > t->maxRunBytes) t->maxRunBytes = m->runBytes;
	if (m->rows > t->maxRows) t->maxRows = m->rows;
	if (m->wraps > t->maxWraps) t->maxWraps = m->wraps;
	if (m->crossings > t->maxCrossings) t->maxCrossings = m->crossings;
}

static void printTotals(const char* bios, const char* title, const struct Totals* t) {
	printf("%s\t%s\tcalls=%lu\treturned=%lu\tinterrupted=%lu\tport_mismatch=%lu\t"
		"register_mismatch=%lu\tstatus_mismatch=%lu\tram_mismatch=%lu\tperipheral_mismatch=%lu\t"
		"clock_mismatch=%lu\trom_streams=%lu\tram_streams=%lu\tmax_pairs=%lu\tmax_run_bytes=%lu\t"
		"max_rows=%lu\tmax_wraps=%lu\tmax_crossings=%lu\toverflow_calls=%lu\tdecimal_entry=%lu\t"
		"max_stack_depth=%lu\n",
		bios, title, t->calls, t->returned, t->interrupted, t->ports, t->registers, t->status,
		t->ram, t->peripherals, t->clocks, t->romStreams, t->ramStreams, t->maxPairs,
		t->maxRunBytes, t->maxRows, t->maxWraps, t->maxCrossings, t->overflowCalls, t->decimalEntry,
		t->maxStackDepth);
}

static void maskInterrupts(struct Gamate* g) {
	g->cpu.p |= 0x04;
	g->cpu.irqMask = true;
}

static int corpus(struct Gamate* g, struct Gamate* replay, struct Model* m,
		const char* bios, const char* title, unsigned frames) {
	struct Totals t = { 0 };
	const char* press = getenv("GAMATEBIOSCALLS_PRESS");
	for (unsigned frame = 1; frame <= frames; frame++) {
		g->buttons = gamateBiosButtonsForFrame(frame, press);
		g->frameDone = false;
		while (!g->frameDone) {
			unsigned cycles = gamateCpuStep(g);
			gamateRunClocks(g, cycles * 2);
			if (g->cpu.pc != entry) continue;
			*replay = *g;
			maskInterrupts(replay);
			uint16_t pointer = (uint16_t) (replay->ram[0] | replay->ram[1] << 8);
			uint8_t inP = replay->cpu.p;
			struct Result r = check(replay, m, false);
			tally(&t, &r, m, pointer, inP);

			/* Authentic interrupt state: the body must be unchanged, and an interrupt may only
			 * add handler time. RAM is not compared because handlers own their own writes. */
			*replay = *g;
			t.openEntry += !(replay->cpu.p & 0x04);
			r = check(replay, m, false);
			t.openReturned += r.returned;
			t.openSpanned += r.interrupted;
			t.openPorts += !r.ports;
			t.openRegisters += !r.registers || !r.status || (!r.interrupted && r.clockDelta) ||
				r.clockDelta < 0;
			if (r.clockDelta > 0 && (unsigned long) r.clockDelta > t.openExtraMax) {
				t.openExtraMax = (unsigned long) r.clockDelta;
			}
		}
	}
	printTotals(bios, title, &t);
	printf("%s\t%s\topen_entry=%lu\topen_returned=%lu\topen_spanned=%lu\topen_port_mismatch=%lu\t"
		"open_register_status_clock_mismatch=%lu\topen_max_interrupt_clocks=%lu\n", bios, title,
		t.openEntry, t.openReturned, t.openSpanned, t.openPorts, t.openRegisters, t.openExtraMax);
	if (t.openPorts || t.openRegisters || t.openReturned != t.calls) return 1;
	return t.ports || t.registers || t.status || t.ram || t.peripherals || t.clocks ||
		t.returned != t.calls;
}

static size_t parseHex(const char* text, uint8_t* out, size_t capacity) {
	size_t n = 0;
	int high = -1;
	for (const char* p = text; *p && n < capacity; p++) {
		int v;
		if (*p >= '0' && *p <= '9') v = *p - '0';
		else if (*p >= 'a' && *p <= 'f') v = *p - 'a' + 10;
		else if (*p >= 'A' && *p <= 'F') v = *p - 'A' + 10;
		else continue;
		if (high < 0) {
			high = v;
		} else {
			out[n++] = (uint8_t) (high << 4 | v);
			high = -1;
		}
	}
	return n;
}

static uint8_t* streamByte(struct Gamate* g, uint16_t address) {
	return isCartridge(address) ? &g->rom[cartridgeOffset(g, address)] : &g->ram[address & 0x3FF];
}

static int synthetic(struct Gamate* g, const struct Gamate* base, struct Model* m) {
	static char line[1 << 18];
	static uint8_t stream[1 << 16], saved[1 << 16];
	bool events = getenv("GAMATEBIOSROWSTREAM_EVENTS") != NULL;
	struct Totals t = { 0 };
	while (fgets(line, sizeof(line), stdin)) {
		*g = *base;
		char* p = line;
		while (*p == ' ') p++;
		if (*p == '@') g->ram[0x15] = (uint8_t) strtoul(p + 1, &p, 16);
		while (*p == ' ') p++;
		if (*p == '!') {
			unsigned long regs = strtoul(p + 1, &p, 16);
			g->cpu.p = (uint8_t) (regs >> 24);
			g->cpu.a = (uint8_t) (regs >> 16);
			g->cpu.x = (uint8_t) (regs >> 8);
			g->cpu.y = (uint8_t) regs;
		}
		maskInterrupts(g);
		while (*p == ' ') p++;
		uint16_t pointer = 0x0300;
		if (*p == '%') pointer = (uint16_t) strtoul(p + 1, &p, 16);
		size_t n = parseHex(p, stream, sizeof(stream));
		for (size_t i = 0; i < n; i++) {
			uint8_t* byte = streamByte(g, (uint16_t) (pointer + i));
			saved[i] = *byte;
			*byte = stream[i];
		}
		g->ram[0] = (uint8_t) pointer;
		g->ram[1] = (uint8_t) (pointer >> 8);
		uint8_t inP = g->cpu.p;
		struct Result r = check(g, m, events);
		tally(&t, &r, m, pointer, inP);
		for (size_t i = n; i-- > 0;) *streamByte(g, (uint16_t) (pointer + i)) = saved[i];
		printf("case returned=%d ports=%d registers=%d status=%d ram=%d peripherals=%d "
			"clock_delta=%lld pairs=%u run_bytes=%u rows=%u wraps=%u crossings=%u overflow=%d\n",
			r.returned, r.ports, r.registers, r.status, r.ram, r.peripherals, r.clockDelta,
			m->pairs, m->runBytes, m->rows, m->wraps, m->crossings, m->overflow);
		fflush(stdout);
	}
	printTotals("synthetic", "-", &t);
	return 0;
}

int main(int argc, char** argv) {
	if (argc < 5 || (strcmp(argv[1], "F01B") && strcmp(argv[1], "F021")) ||
			(strcmp(argv[4], "corpus") && strcmp(argv[4], "stdin"))) {
		fprintf(stderr, "usage: %s F01B|F021 gamate_bios.bin cartridge.bin corpus [frames]|stdin\n",
			argv[0]);
		return 2;
	}
	literal = !strcmp(argv[1], "F021");
	entry = literal ? 0xF021 : 0xF01B;
	argc--;
	argv++;
	size_t biosSize, romSize;
	uint8_t* bios = readFile(argv[1], &biosSize);
	uint8_t* rom = readFile(argv[2], &romSize);
	struct Gamate* g = malloc(sizeof(*g));
	struct Gamate* base = malloc(sizeof(*base));
	struct Model* m = malloc(sizeof(*m));
	if (!bios || !rom || !g || !base || !m) {
		fprintf(stderr, "could not read BIOS or cartridge\n");
		return 2;
	}
	gamateInit(g);
	if (!gamateLoad(g, rom, romSize, bios, biosSize)) {
		fprintf(stderr, "invalid BIOS or cartridge size\n");
		return 2;
	}
	free(bios);
	free(rom);
	bool entered = false;
	for (uint64_t i = 0; i < 20000000 && !entered; i++) {
		unsigned cycles = gamateCpuStep(g);
		gamateRunClocks(g, cycles * 2);
		entered = g->protectionPassed && g->cpu.pc == 0x6020;
	}
	if (!entered) {
		fprintf(stderr, "BIOS did not reach cartridge entry\n");
		return 1;
	}
	const char* title = strrchr(argv[2], '/') ? strrchr(argv[2], '/') + 1 : argv[2];
	const char* biosName = strrchr(argv[1], '/') ? strrchr(argv[1], '/') + 1 : argv[1];
	int result;
	if (!strcmp(argv[3], "corpus")) {
		unsigned frames = argc >= 5 ? (unsigned) strtoul(argv[4], NULL, 0) : 1000;
		result = corpus(g, base, m, biosName, title, frames);
	} else {
		const char* press = getenv("GAMATEBIOSCALLS_PRESS");
		bool found = false;
		for (unsigned frame = 1; frame <= 1000 && !found; frame++) {
			g->buttons = gamateBiosButtonsForFrame(frame, press);
			g->frameDone = false;
			while (!g->frameDone && !found) {
				unsigned cycles = gamateCpuStep(g);
				gamateRunClocks(g, cycles * 2);
				found = g->cpu.pc == entry;
			}
		}
		if (!found) {
			fprintf(stderr, "no $%04X call in the standard window\n", entry);
			return 1;
		}
		*base = *g;
		result = synthetic(g, base, m);
	}
	gamateFree(g);
	free(g);
	free(base);
	free(m);
	return result;
}
