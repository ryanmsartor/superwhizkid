/* Copyright (c) 2026 superwhizkid contributors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
/* Diagnostic-only synthetic and corpus replay of the $F018 graphics service. */
#include "gamate/gamate.h"
#include "gamatebiosinput.h"

#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { MAX_EVENTS = 300000 };

struct Event {
	uint8_t reg, value;
};

struct Capture {
	struct Event events[MAX_EVENTS];
	size_t count;
	uint16_t romReads[MAX_EVENTS];
	size_t romCount;
};

struct Model {
	struct Capture ports;
	uint8_t ram[0x1C], written[0x1C];
	uint8_t a, x, y, p;
	uint64_t clocks;
};

struct Result {
	bool supported, returned, ports, rom, registers, ram, peripherals, interrupted;
	long long clockDelta;
	unsigned stackDepth;
};

static void lcdWrite(void* context, const struct Gamate* g, uint8_t reg, uint8_t value) {
	struct Capture* capture = context;
	(void) g;
	if (capture->count < MAX_EVENTS) {
		capture->events[capture->count].reg = reg;
		capture->events[capture->count].value = value;
	}
	capture->count++;
}

static void romRead(void* context, const struct Gamate* g, uint16_t address, size_t offset) {
	struct Capture* capture = context;
	(void) g;
	(void) offset;
	if (capture->romCount < MAX_EVENTS) capture->romReads[capture->romCount] = address;
	capture->romCount++;
}

static bool sameEvents(const struct Capture* actual, const struct Capture* expected) {
	if (actual->count != expected->count || actual->count > MAX_EVENTS) return false;
	for (size_t i = 0; i < actual->count; i++) {
		if (actual->events[i].reg != expected->events[i].reg ||
				actual->events[i].value != expected->events[i].value) return false;
	}
	return true;
}

static uint8_t syntheticByte(size_t address) {
	return (uint8_t) (address * 37 + (address >> 8) * 73 + 11);
}

static uint8_t* readFile(const char* path, size_t* size) {
	FILE* file = fopen(path, "rb");
	if (!file) return NULL;
	if (fseek(file, 0, SEEK_END)) {
		fclose(file);
		return NULL;
	}
	long length = ftell(file);
	if (length < 0 || fseek(file, 0, SEEK_SET)) {
		fclose(file);
		return NULL;
	}
	*size = (size_t) length;
	uint8_t* bytes = malloc(*size ? *size : 1);
	if (!bytes || fread(bytes, 1, *size, file) != *size) {
		free(bytes);
		bytes = NULL;
	}
	fclose(file);
	return bytes;
}

static bool boot(struct Gamate* g) {
	bool entered = false;
	for (uint64_t i = 0; i < 20000000 && !entered; i++) {
		unsigned cycles = gamateCpuStep(g);
		gamateRunClocks(g, cycles * 2);
		entered = g->protectionPassed && g->cpu.pc == 0x6020;
	}
	return entered;
}

static bool seekEntry(struct Gamate* g) {
	const char* press = getenv("GAMATEBIOSCALLS_PRESS");
	for (unsigned frame = 1; frame <= 1000; frame++) {
		g->buttons = gamateBiosButtonsForFrame(frame, press);
		g->frameDone = false;
		while (!g->frameDone) {
			unsigned cycles = gamateCpuStep(g);
			gamateRunClocks(g, cycles * 2);
			if (g->cpu.pc == 0xF018) return true;
		}
	}
	return false;
}

static bool cartridgeAddress(unsigned address) {
	return address >= 0x6000 && address < 0xE000;
}

static size_t cartridgeOffset(const struct Gamate* g, uint16_t address) {
	size_t offset = address - 0x6000;
	if (g->mapper == GAMATE_MAPPER_4IN1 && offset < 0x4000) {
		offset = (size_t) g->firstBank * 0x4000 + offset;
	} else if (g->mapper != GAMATE_MAPPER_PLAIN && offset >= 0x4000) {
		offset = (size_t) g->bank * 0x4000 + (offset & 0x3FFF);
	}
	return offset % g->romSize;
}

static bool sourceByte(const struct Gamate* g, uint16_t address, uint8_t* value) {
	if (address < 0x2000) {
		unsigned offset = address & 0x3FF;
		if ((offset >= 4 && offset < 0x18) || (offset >= 0x1C && offset < 0x100) ||
				offset >= 0x200) {
			*value = g->ram[offset];
			return true;
		}
	}
	if (cartridgeAddress(address)) {
		*value = g->rom[cartridgeOffset(g, address)];
		return true;
	}
	if (address >= 0xE000) {
		*value = g->bios[address & 0xFFF];
		return true;
	}
	return false;
}

static void modelRead(struct Model* m, uint16_t base, unsigned index) {
	unsigned sum = (base & 0xFF) + index;
	uint16_t actual = (uint16_t) (base + index);
	if (sum > 0xFF) {
		uint16_t dummy = (uint16_t) ((base & 0xFF00) | (sum & 0xFF));
		if (cartridgeAddress(dummy)) romRead(&m->ports, NULL, dummy, 0);
	}
	if (cartridgeAddress(actual)) romRead(&m->ports, NULL, actual, 0);
}

static void modelWrite(struct Model* m, unsigned address, uint8_t value) {
	m->ram[address] = value;
	m->written[address] = 1;
}

static uint8_t addColumn(uint8_t column, uint8_t count, bool decimal, bool* overflow) {
	unsigned binary = column + count;
	unsigned low = (column & 15) + (count & 15);
	if (decimal && low > 9) low = ((low + 6) & 15) + 16;
	unsigned intermediate = (column & 0xF0) + (count & 0xF0) + low;
	*overflow = (~(column ^ count) & (column ^ intermediate) & 0x80) != 0;
	if (!decimal) return (uint8_t) binary;
	if (intermediate >= 0xA0) intermediate += 0x60;
	return (uint8_t) intermediate;
}

/* Model the observed two-pass contract without retaining any cartridge data. */
static bool model(const struct Gamate* g, struct Model* m) {
	memset(m, 0, sizeof(*m));
	memcpy(m->ram, g->ram, sizeof(m->ram));
	uint16_t pointer = (uint16_t) (g->ram[0] | g->ram[1] << 8);
	bool overflow = (g->cpu.p & 0x40) != 0;
	bool decimal = (g->cpu.p & 0x08) != 0;
	bool opaque = false;
	m->clocks = 144;
	lcdWrite(&m->ports, g, 1, g->ram[0x15] | 0x40);
	modelWrite(m, 0, g->ram[0]);
	modelWrite(m, 1, g->ram[1]);
	for (unsigned plane = 0; plane < 2; plane++) {
		unsigned index = 0;
		uint16_t base = (uint16_t) (g->ram[6 + plane * 2] | g->ram[7 + plane * 2] << 8);
		for (;;) {
			uint8_t row;
			if (index >= 0x10000 || !sourceByte(g, (uint16_t) (pointer + index), &row))
				return false;
			modelRead(m, (uint16_t) (pointer + (index & ~255u)), index & 255);
			if ((pointer & 255) + (index & 255) > 255) m->clocks += 2;
			if (row == 0xFE) break;
			index++;
			m->clocks += 130;
			uint8_t column = (uint8_t) (plane << 7);
			modelWrite(m, 0x18, column);
			modelWrite(m, 0x19, row);
			for (;;) {
				uint8_t tile, count;
				if (index >= 0xFFFF ||
						!sourceByte(g, (uint16_t) (pointer + index), &tile)) return false;
				modelRead(m, (uint16_t) (pointer + (index & ~255u)), index & 255);
				if ((pointer & 255) + (index & 255) > 255) m->clocks += 2;
				index++;
				if (tile == 0xFF) break;
				if (!sourceByte(g, (uint16_t) (pointer + index), &count)) return false;
				modelRead(m, (uint16_t) (pointer + (index & ~255u)), index & 255);
				if ((pointer & 255) + (index & 255) > 255) m->clocks += 2;
				index++;
				m->clocks += 236;
				m->clocks += 32 * (tile >> 5);
				if (tile >= 32) overflow = false;
				uint8_t low = (uint8_t) base;
				uint8_t high = (uint8_t) ((base >> 8) + (tile >> 5));
				uint8_t offset = (uint8_t) (tile << 3);
				modelWrite(m, 2, low);
				modelWrite(m, 3, high);
				modelWrite(m, 0x1A, offset);
				modelWrite(m, 0x1B, count);
				if (!tile) {
					column = addColumn(column, count, decimal, &overflow);
					modelWrite(m, 0x18, column);
					continue;
				}
				opaque = true;
				unsigned length = count ? count : 256;
				for (unsigned col = 0; col < length; col++) {
					lcdWrite(&m->ports, g, 4, column);
					lcdWrite(&m->ports, g, 5, (uint8_t) (row << 3));
					m->clocks += 330;
					for (unsigned j = 0; j < 8; j++) {
						uint8_t value;
						unsigned indexed = offset + j;
						uint16_t address = (uint16_t) ((high << 8 | low) + indexed);
						if (!sourceByte(g, address, &value)) return false;
						modelRead(m, (uint16_t) (high << 8 | low), indexed);
						if (low + indexed > 255) m->clocks += 2;
						lcdWrite(&m->ports, g, 7, value);
					}
					if (m->ports.count > MAX_EVENTS) return false;
					column++;
				}
				modelWrite(m, 0x18, column);
				modelWrite(m, 0x1B, 0);
			}
		}
		m->clocks += 8 * (index >> 8);
		m->y = (uint8_t) index;
		modelWrite(m, 1, (uint8_t) (g->ram[1] + (index >> 8)));
	}
	m->a = 0xFE;
	m->x = opaque ? 0xFF : g->cpu.x;
	m->p = (uint8_t) ((g->cpu.p & 0x1C) | 0x23 | (overflow ? 0x40 : 0));
	uint16_t ret = (uint16_t) (g->ram[0x100 | (uint8_t) (g->cpu.s + 1)] |
		g->ram[0x100 | (uint8_t) (g->cpu.s + 2)] << 8);
	if (cartridgeAddress(ret)) romRead(&m->ports, NULL, ret, 0);
	if (m->ports.romCount > MAX_EVENTS) return false;
	return true;
}

static struct Result check(struct Gamate* g, struct Capture* capture, struct Model* expected) {
	struct Result r = { 0 };
	capture->count = 0;
	capture->romCount = 0;
	r.supported = model(g, expected);
	if (!r.supported) return r;
	uint8_t entryS = g->cpu.s;
	uint8_t returnS = (uint8_t) (entryS + 2);
	uint16_t ret = (uint16_t) ((g->ram[0x100 | (uint8_t) (entryS + 1)] |
		g->ram[0x100 | (uint8_t) (entryS + 2)] << 8) + 1);
	uint64_t start = g->masterClocks, irqs = g->diagnostics.irqsTaken;
	uint64_t nmis = g->diagnostics.nmisTaken, ay = g->diagnostics.ayWrites;
	uint64_t banks = g->diagnostics.bankWrites, uart = g->diagnostics.uartClocks;
	uint8_t tx = g->uartTx, txBits = g->uartTxBits;
	bool txLine = g->uartTxLine;
	capture->count = 0;
	gamateResetRamDiagnostics(g);
	gamateResetRomDiagnostics(g);
	gamateSetLcdDiagnosticWatch(g, lcdWrite, capture);
	gamateSetRomDiagnosticWatch(g, romRead, capture);
	while (!r.returned && g->masterClocks - start < expected->clocks + 1000000) {
		unsigned depth = (uint8_t) (entryS - g->cpu.s);
		if (depth > r.stackDepth && depth < 128) r.stackDepth = depth;
		uint16_t pc = g->cpu.pc;
		bool rts = pc >= 0xE000 && g->bios[pc & 0xFFF] == 0x60;
		unsigned cycles = gamateCpuStep(g);
		gamateRunClocks(g, cycles * 2);
		r.returned = rts && g->cpu.pc == ret && g->cpu.s == returnS;
	}
	gamateSetLcdDiagnosticWatch(g, NULL, NULL);
	gamateSetRomDiagnosticWatch(g, NULL, NULL);
	r.interrupted = g->diagnostics.irqsTaken != irqs || g->diagnostics.nmisTaken != nmis;
	r.ports = sameEvents(capture, &expected->ports);
	r.rom = capture->romCount == expected->ports.romCount && capture->romCount <= MAX_EVENTS &&
		!memcmp(capture->romReads, expected->ports.romReads,
			capture->romCount * sizeof(capture->romReads[0]));
	r.registers = r.returned && g->cpu.a == expected->a && g->cpu.x == expected->x &&
		g->cpu.y == expected->y && g->cpu.p == expected->p;
	r.ram = true;
	for (unsigned i = 0; i < 0x400; i++) {
		if (i >= 0x100 && i < 0x200) continue;
		bool written = (g->diagnostics.ramWritten[i >> 3] & (1u << (i & 7))) != 0;
		if (i < sizeof(expected->ram)) {
			if (written != (expected->written[i] != 0) || g->ram[i] != expected->ram[i])
				r.ram = false;
		} else if (written) {
			r.ram = false;
		}
	}
	r.peripherals = g->diagnostics.ayWrites == ay && g->diagnostics.bankWrites == banks &&
		g->diagnostics.uartClocks == uart && g->uartTx == tx && g->uartTxBits == txBits &&
		g->uartTxLine == txLine;
	r.clockDelta = (long long) (g->masterClocks - start) - (long long) expected->clocks;
	return r;
}

static int corpus(struct Gamate* g, struct Gamate* copy, struct Capture* capture,
		struct Model* expected, unsigned frames) {
	unsigned calls = 0, unsupported = 0, ports = 0, rom = 0, registers = 0, ram = 0;
	unsigned clocks = 0, peripherals = 0, returned = 0, spanned = 0, openMismatch = 0;
	unsigned decimal = 0, maxStack = 0;
	const char* press = getenv("GAMATEBIOSCALLS_PRESS");
	for (unsigned frame = 1; frame <= frames; frame++) {
		g->buttons = gamateBiosButtonsForFrame(frame, press);
		g->frameDone = false;
		while (!g->frameDone) {
			unsigned cycles = gamateCpuStep(g);
			gamateRunClocks(g, cycles * 2);
			if (g->cpu.pc != 0xF018) continue;
			calls++;
			decimal += (g->cpu.p & 8) != 0;
			*copy = *g;
			copy->cpu.p |= 4;
			copy->cpu.irqMask = true;
			struct Result r = check(copy, capture, expected);
			unsupported += !r.supported;
			returned += r.returned;
			ports += !r.ports;
			rom += !r.rom;
			registers += !r.registers;
			ram += !r.ram;
			clocks += !r.supported || r.clockDelta != 0;
			peripherals += !r.peripherals;
			if (r.stackDepth > maxStack) maxStack = r.stackDepth;
			*copy = *g;
			r = check(copy, capture, expected);
			spanned += r.interrupted;
			openMismatch += !r.returned || !r.ports || !r.registers ||
				r.clockDelta < 0 || (!r.interrupted && r.clockDelta != 0);
		}
	}
	printf("calls=%u returned=%u unsupported=%u port_mismatch=%u rom_mismatch=%u register_mismatch=%u "
		"ram_mismatch=%u clock_mismatch=%u peripheral_mismatch=%u decimal=%u max_stack=%u "
		"interrupt_spanning=%u open_mismatch=%u\n", calls, returned, unsupported, ports, rom,
		registers, ram, clocks, peripherals, decimal, maxStack, spanned, openMismatch);
	return unsupported || returned != calls || ports || rom || registers || ram || clocks ||
		peripherals || openMismatch;
}

static bool syntheticAddress(unsigned address) {
	return (address >= 0x20 && address < 0x100) ||
		(address >= 0x200 && address < 0x400) || cartridgeAddress(address);
}

/* Input: pointer, plane-0 base, plane-1 base, LCD byte, P, A, X, Y, followed by stream bytes.
 * Stream and tile bytes are synthetic; cartridge bytes are changed only in the private ROM copy. */
static int replay(struct Gamate* g, const struct Gamate* base, struct Capture* capture,
		struct Model* expected, FILE* input, bool preserveInterrupt) {
	char line[8192];
	uint8_t* savedRom = malloc(g->romSize);
	if (!savedRom) {
		fprintf(stderr, "could not allocate private ROM backup\n");
		return 2;
	}
	memcpy(savedRom, g->rom, g->romSize);
	unsigned cases = 0, failures = 0;
	while (fgets(line, sizeof(line), input)) {
		if (!strchr(line, '\n') && !feof(input)) {
			fprintf(stderr, "synthetic case %u exceeds the input limit\n", cases + 1);
			free(savedRom);
			return 2;
		}
		unsigned pointer, b0, b1, lcd, p, a, x, y;
		int consumed = 0;
		if (sscanf(line, "%x %x %x %x %x %x %x %x %n",
				&pointer, &b0, &b1, &lcd, &p, &a, &x, &y, &consumed) != 8 ||
				!syntheticAddress(pointer) || !syntheticAddress(b0) || !syntheticAddress(b1) ||
				lcd > 0xFF || p > 0xFF || a > 0xFF || x > 0xFF || y > 0xFF) {
			fprintf(stderr, "invalid synthetic case %u\n", cases + 1);
			free(savedRom);
			return 2;
		}
		*g = *base;
		for (unsigned i = 0x20; i < 0x100; i++) g->ram[i] = syntheticByte(i);
		for (unsigned i = 0x200; i < 0x400; i++) g->ram[i] = syntheticByte(i);
		for (size_t i = 0; i < g->romSize; i++) g->rom[i] = syntheticByte(i);
		char* cursor = line + consumed;
		unsigned length = 0;
		while (*cursor) {
			char* end;
			unsigned long value = strtoul(cursor, &end, 16);
			unsigned address = pointer + length;
			if (end == cursor || value > 0xFF || !syntheticAddress(address) ||
					(cartridgeAddress(pointer) != cartridgeAddress(address))) {
				fprintf(stderr, "invalid stream in synthetic case %u\n", cases + 1);
				free(savedRom);
				return 2;
			}
			if (cartridgeAddress(address)) {
				g->rom[cartridgeOffset(g, (uint16_t) address)] = (uint8_t) value;
			} else {
				g->ram[address] = (uint8_t) value;
			}
			length++;
			cursor = end;
			while (*cursor == ' ' || *cursor == '\t') cursor++;
			if (*cursor == '\n' || *cursor == '\r') break;
		}
		g->ram[0] = (uint8_t) pointer;
		g->ram[1] = (uint8_t) (pointer >> 8);
		g->ram[6] = (uint8_t) b0;
		g->ram[7] = (uint8_t) (b0 >> 8);
		g->ram[8] = (uint8_t) b1;
		g->ram[9] = (uint8_t) (b1 >> 8);
		g->ram[0x15] = (uint8_t) lcd;
		g->cpu.p = (uint8_t) (preserveInterrupt ? p : p | 4);
		g->cpu.irqMask = (g->cpu.p & 4) != 0;
		if (preserveInterrupt) {
			g->irqClocks = 0;
			g->irq = false;
			g->nmi = false;
		}
		g->cpu.a = (uint8_t) a;
		g->cpu.x = (uint8_t) x;
		g->cpu.y = (uint8_t) y;
		struct Result r = check(g, capture, expected);
		printf("case=%u return=%u model=%u ports=%u rom=%u registers=%u ram_match=%u "
			"peripherals=%u clock_delta=%lld\n",
			++cases, r.returned, r.supported, r.ports, r.rom, r.registers, r.ram, r.peripherals,
			r.clockDelta);
		if (!r.supported || !r.returned || r.interrupted || !r.ports || !r.rom ||
				!r.registers || !r.ram || !r.peripherals || r.clockDelta != 0) failures++;
	}
	memcpy(g->rom, savedRom, g->romSize);
	free(savedRom);
	if (ferror(input) || !cases) {
		fprintf(stderr, "no complete synthetic cases were read\n");
		return 2;
	}
	fprintf(stderr, "synthetic cases=%u failures=%u\n", cases, failures);
	return failures ? 1 : 0;
}

static FILE* boundaryCases(void) {
	FILE* input = tmpfile();
	if (!input) return NULL;
	for (unsigned i = 0; i < 255; i++)
		fprintf(input, "6200 6800 70FF 00 60 71 37 A5 00 %02X 01 FF FE\n", i);
	for (unsigned i = 0; i < 256; i++)
		fprintf(input, "62FE 68FF 70F9 %02X %02X 71 37 A5 %02X 00 %02X 01 02 FF FE\n",
			i, i, i == 254 ? 255 : i, i);
	for (unsigned i = 0; i < 256; i++)
		fprintf(input, "6200 %04X %04X %02X 20 71 37 A5 00 11 01 FF FE\n",
			0x6800 + i, 0x70FF - i, i);
	for (unsigned i = 0; i < 256; i++)
		fprintf(input, "6200 6800 7000 %02X %02X 71 37 A5 00 00 %02X FF FE\n",
			i, i & 1 ? 0x28 : 0x20, i);
	for (unsigned i = 0; i < 256; i++)
		fprintf(input, "6200 6800 7080 00 20 71 37 A5 00 01 %02X FF FE\n", i);
	for (unsigned i = 0; i < 16; i++) {
		fprintf(input, "0200 0300 0380 00 20 71 37 A5 00 %02X 02 FF FE\n", i);
		fprintf(input, "02FC 0300 0380 FF 28 71 37 A5 00 %02X 02 FF FE\n", i);
	}
	fputs("0020 0040 0080 00 20 71 37 A5 00 01 02 FF FE\n", input);
	fputs("00FA 0040 0080 FF 28 71 37 A5 00 02 01 FF FE\n", input);
	static const unsigned lengths[] = { 127, 128, 129, 255, 256, 257, 300 };
	static const unsigned lows[] = { 0, 1, 127, 254, 255 };
	for (size_t i = 0; i < sizeof(lengths) / sizeof(lengths[0]); i++) {
		for (size_t k = 0; k < sizeof(lows) / sizeof(lows[0]); k++) {
			fprintf(input, "%04X 6800 7000 00 20 71 37 A5 ", 0x6200 + lows[k]);
			for (unsigned j = 0; j < lengths[i]; j++) fprintf(input, "%02X FF ", j % 254);
			fputs("FE\n", input);
		}
	}
	static const unsigned runs[] = { 120, 127, 128, 129, 190 };
	for (size_t i = 0; i < sizeof(runs) / sizeof(runs[0]); i++) {
		for (size_t k = 0; k < sizeof(lows) / sizeof(lows[0]); k++) {
			fprintf(input, "%04X 6800 7000 00 28 71 37 A5 00 ", 0x6200 + lows[k]);
			for (unsigned j = 0; j < runs[i]; j++) fprintf(input, "00 %02X ", (j * 7) & 255);
			fputs("FF FE\n", input);
		}
	}
	if (ferror(input) || fseek(input, 0, SEEK_SET)) {
		fclose(input);
		return NULL;
	}
	return input;
}

static FILE* interruptCases(void) {
	FILE* input = tmpfile();
	if (!input) return NULL;
	for (unsigned p = 0; p < 256; p++) {
		static const char* streams[] = {
			"FE", "00 FF FE", "00 00 8A FF FE", "00 01 01 FF FE",
			"00 01 01 00 80 20 01 FF FE"
		};
		for (size_t i = 0; i < sizeof(streams) / sizeof(streams[0]); i++)
			fprintf(input, "6200 6800 7080 00 %02X 71 37 A5 %s\n", p, streams[i]);
	}
	if (ferror(input) || fseek(input, 0, SEEK_SET)) {
		fclose(input);
		return NULL;
	}
	return input;
}

static FILE* flagCases(void) {
	FILE* input = tmpfile();
	if (!input) return NULL;
	for (unsigned decimal = 0; decimal < 2; decimal++) {
		for (unsigned first = 0; first < 256; first++) {
			for (unsigned second = 0; second < 256; second++) {
				fprintf(input, "6200 6800 7000 00 %02X 71 37 A5 00 00 %02X 00 %02X FF FE\n",
					decimal ? 0x28 : 0x20, first, second);
			}
		}
	}
	if (ferror(input) || fseek(input, 0, SEEK_SET)) {
		fclose(input);
		return NULL;
	}
	return input;
}

int main(int argc, char** argv) {
	if (argc < 4 || (strcmp(argv[3], "stdin") && strcmp(argv[3], "selftest") &&
			strcmp(argv[3], "selftest-open") && strcmp(argv[3], "selftest-flags") &&
			strcmp(argv[3], "corpus")) ||
			(argc != 4 && (argc != 5 || strcmp(argv[3], "corpus")))) {
		fprintf(stderr, "usage: %s bios cartridge "
			"stdin|selftest|selftest-open|selftest-flags|corpus [frames]\n", argv[0]);
		return 2;
	}
	unsigned frames = 1000;
	if (argc == 5) {
		char* end;
		errno = 0;
		unsigned long parsed = strtoul(argv[4], &end, 0);
		if (errno || end == argv[4] || *end || !parsed || parsed > UINT_MAX) {
			fprintf(stderr, "frames must be a positive integer\n");
			return 2;
		}
		frames = (unsigned) parsed;
	}
	size_t biosSize, romSize;
	uint8_t* bios = readFile(argv[1], &biosSize);
	uint8_t* rom = readFile(argv[2], &romSize);
	struct Gamate* g = malloc(sizeof(*g));
	struct Gamate* base = malloc(sizeof(*base));
	struct Capture* capture = malloc(sizeof(*capture));
	struct Model* expected = malloc(sizeof(*expected));
	if (!bios || !rom || !g || !base || !capture || !expected) {
		fprintf(stderr, "could not read inputs or allocate replay\n");
		return 2;
	}
	gamateInit(g);
	if (!gamateLoad(g, rom, romSize, bios, biosSize) || !boot(g)) {
		fprintf(stderr, "could not reach cartridge entry\n");
		return 1;
	}
	free(bios);
	free(rom);
	int result;
	if (!strcmp(argv[3], "corpus")) {
		result = corpus(g, base, capture, expected, frames);
	} else if (seekEntry(g)) {
		*base = *g;
		bool open = !strcmp(argv[3], "selftest-open");
		FILE* input = stdin;
		if (open) input = interruptCases();
		else if (!strcmp(argv[3], "selftest")) input = boundaryCases();
		else if (!strcmp(argv[3], "selftest-flags")) input = flagCases();
		if (!input) {
			fprintf(stderr, "could not create synthetic test cases\n");
			result = 2;
		} else {
			result = replay(g, base, capture, expected, input, open);
			if (input != stdin) fclose(input);
		}
	} else {
		fprintf(stderr, "could not reach an authentic $F018 entry\n");
		result = 1;
	}
	gamateFree(g);
	free(g);
	free(base);
	free(capture);
	free(expected);
	return result;
}
