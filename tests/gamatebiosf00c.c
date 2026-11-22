/* Copyright (c) 2026 superwhizkid contributors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
/* Diagnostic-only model and replay of the BIOS $F00C LCD clear. */
#include "gamate/gamate.h"
#include "gamatebiosinput.h"

#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { ENTRY = 0xF00C, CLOCKS = 247326, LCD_WRITES = 13602 };

struct Trace {
	unsigned lcdCount, lcdMismatch, romCount, romMismatch, scratchReads, scratchWrites;
	unsigned scratchMismatch, scratchAt[2], timingMismatch;
	uint64_t start, scratchTicks[2];
	uint8_t control;
	uint16_t returnAddress;
	bool cartridgeReturn;
};

struct Result {
	bool returned, interrupted, lcd, eventTiming, rom, registers, ram, vram, peripherals;
	long long clockDelta;
	unsigned stackDepth, scratchAt[2];
};

static void lcdWrite(void* context, const struct Gamate* g, uint8_t reg, uint8_t value) {
	struct Trace* t = context;
	unsigned n = t->lcdCount++;
	uint8_t expectedReg, expectedValue;
	uint64_t expectedTick;
	if (n == 0) {
		expectedReg = 1;
		expectedValue = t->control | 0x80;
		expectedTick = 16;
	} else if (n == LCD_WRITES - 1) {
		expectedReg = 1;
		expectedValue = t->control | 0x40;
		expectedTick = 247306;
	} else if (n < LCD_WRITES - 1) {
		unsigned index = n - 1, row = index / 34, position = index % 34;
		expectedReg = position == 0 ? 4 : position == 1 ? 5 : 7;
		expectedValue = position == 0 ? (uint8_t) ((row / 200) << 7) :
			position == 1 ? (uint8_t) (row % 200) : 0;
		expectedTick = 56 + (row / 200) * 123636 + (row % 200) * 618;
		if (position == 1) expectedTick += 8;
		else if (position >= 2) expectedTick += 24 + 18 * (position - 2);
	} else {
		t->lcdMismatch++;
		return;
	}
	t->lcdMismatch += reg != expectedReg || value != expectedValue;
	t->timingMismatch += g->masterClocks - t->start != expectedTick;
}

static void romRead(void* context, const struct Gamate* g, uint16_t address, size_t offset) {
	struct Trace* t = context;
	(void) g;
	(void) offset;
	t->romCount++;
	t->romMismatch += !t->cartridgeReturn || address != t->returnAddress;
}

static void scratchAccess(void* context, const struct Gamate* g, bool write,
		uint16_t address, uint8_t previous, uint8_t value) {
	struct Trace* t = context;
	(void) address;
	(void) previous;
	if (!write) {
		unsigned n = t->scratchReads++;
		t->timingMismatch += n >= 400 || g->masterClocks - t->start !=
			50 + (n / 200) * 123636 + (n % 200) * 618;
	} else {
		unsigned n = t->scratchWrites++;
		if (n < 2) {
			t->scratchAt[n] = t->lcdCount;
			t->scratchTicks[n] = g->masterClocks - t->start;
		}
		t->scratchMismatch += n >= 2 || value != (n ? 0x80 : 0);
	}
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
	for (uint64_t i = 0; i < 20000000; i++) {
		unsigned cycles = gamateCpuStep(g);
		gamateRunClocks(g, cycles * 2);
		if (g->protectionPassed && g->cpu.pc == 0x6020) return true;
	}
	return false;
}

static bool seekEntry(struct Gamate* g) {
	const char* press = getenv("GAMATEBIOSCALLS_PRESS");
	for (unsigned frame = 1; frame <= 1000; frame++) {
		g->buttons = gamateBiosButtonsForFrame(frame, press);
		g->frameDone = false;
		while (!g->frameDone) {
			unsigned cycles = gamateCpuStep(g);
			gamateRunClocks(g, cycles * 2);
			if (g->cpu.pc == ENTRY) return true;
		}
	}
	return false;
}

static struct Result check(struct Gamate* g, bool suppressIrq) {
	struct Result r = { 0 };
	uint8_t entryS = g->cpu.s, inP = g->cpu.p, control = g->ram[0x15];
	uint8_t returnS = (uint8_t) (entryS + 2);
	uint16_t stacked = (uint16_t) (g->ram[0x100 | (uint8_t) (entryS + 1)] |
		g->ram[0x100 | (uint8_t) (entryS + 2)] << 8);
	uint16_t ret = (uint16_t) (stacked + 1);
	struct Trace trace = { .control = control, .returnAddress = stacked, .start = g->masterClocks,
		.cartridgeReturn = stacked >= 0x6000 && stacked < 0xE000 };
	uint64_t start = g->masterClocks, irqs = g->diagnostics.irqsTaken;
	uint64_t nmis = g->diagnostics.nmisTaken, ay = g->diagnostics.ayWrites;
	uint64_t banks = g->diagnostics.bankWrites, uart = g->diagnostics.uartClocks;
	uint8_t tx = g->uartTx, txBits = g->uartTxBits, scrollX = g->scrollX, scrollY = g->scrollY;
	bool txLine = g->uartTxLine;
	uint8_t initialVram[sizeof(g->vram)];
	memcpy(initialVram, g->vram, sizeof(initialVram));
	gamateResetRamDiagnostics(g);
	gamateResetRomDiagnostics(g);
	gamateResetVramDiagnostics(g);
	gamateSetLcdDiagnosticWatch(g, lcdWrite, &trace);
	gamateSetRomDiagnosticWatch(g, romRead, &trace);
	gamateSetRamDiagnosticWatch(g, 0x1E, scratchAccess, &trace);
	while (!r.returned && g->masterClocks - start < CLOCKS + 1000000) {
		unsigned depth = (uint8_t) (entryS - g->cpu.s);
		if (depth > r.stackDepth && depth < 128) r.stackDepth = depth;
		uint16_t pc = g->cpu.pc;
		bool rts = pc >= 0xE000 && g->bios[pc & 0xFFF] == 0x60;
		unsigned cycles = gamateCpuStep(g);
		gamateRunClocks(g, cycles * 2);
		if (suppressIrq) g->irq = false;
		r.returned = rts && g->cpu.pc == ret && g->cpu.s == returnS;
	}
	gamateSetLcdDiagnosticWatch(g, NULL, NULL);
	gamateSetRomDiagnosticWatch(g, NULL, NULL);
	gamateSetRamDiagnosticWatch(g, 0x1E, NULL, NULL);
	r.interrupted = g->diagnostics.irqsTaken != irqs || g->diagnostics.nmisTaken != nmis;
	r.lcd = trace.lcdCount == LCD_WRITES && !trace.lcdMismatch;
	r.eventTiming = !trace.timingMismatch && trace.scratchTicks[0] == 28 &&
		trace.scratchTicks[1] == 123664;
	r.rom = trace.romCount == (unsigned) trace.cartridgeReturn && !trace.romMismatch;
	uint8_t a = control | 0x40;
	r.registers = r.returned && g->cpu.a == a && g->cpu.x == 0 && g->cpu.y == 0xC8 &&
		g->cpu.p == (uint8_t) ((inP & 0x5C) | 0x21 | (a & 0x80));
	r.ram = g->ram[0x1E] == 0x80 && trace.scratchReads == 400 && trace.scratchWrites == 2 &&
		!trace.scratchMismatch && trace.scratchAt[0] == 1 && trace.scratchAt[1] == 6801;
	for (unsigned i = 0; i < 0x400; i++) {
		if (i == 0x1E || (i >= 0x100 && i < 0x200)) continue;
		if (g->diagnostics.ramWritten[i >> 3] & (1u << (i & 7))) r.ram = false;
		bool read = (g->diagnostics.ramReadBeforeWriteMap[1][i >> 3] & (1u << (i & 7))) != 0;
		if (read != (i == 0x15)) r.ram = false;
	}
	r.vram = g->diagnostics.vramReads == 0;
	for (unsigned address = 0; address < 0x2000; address++) {
		bool cleared = control & 0x40 ? !(address & 31) && address <= 0x1CC0 :
			address < 0x1900;
		for (unsigned plane = 0; plane < 2; plane++) {
			unsigned index = 2 * address + plane;
			if (g->vram[index] != (cleared ? 0 : initialVram[index])) r.vram = false;
		}
	}
	r.peripherals = g->diagnostics.ayWrites == ay && g->diagnostics.bankWrites == banks &&
		g->diagnostics.uartClocks == uart && g->uartTx == tx && g->uartTxBits == txBits &&
		g->uartTxLine == txLine && g->scrollX == scrollX && g->scrollY == scrollY &&
		g->vramPlane && g->incrementRow && g->displayBlank == ((control & 0x80) != 0) &&
		g->window == ((control & 0x20) != 0) && g->swapPlanes == ((control & 0x10) != 0) &&
		g->displayStopped == ((control & 1) != 0) &&
		g->vramAddress == ((control & 0x40) ? 0x1CE0 : 0x1900);
	r.clockDelta = (long long) (g->masterClocks - start) - CLOCKS;
	r.scratchAt[0] = trace.scratchAt[0];
	r.scratchAt[1] = trace.scratchAt[1];
	return r;
}

static bool passed(struct Result r) {
	return r.returned && !r.interrupted && r.lcd && r.eventTiming && r.rom && r.registers && r.ram &&
		r.vram && r.peripherals && !r.clockDelta;
}

static int synthetic(struct Gamate* g, const struct Gamate* base) {
	unsigned failures = 0;
	for (unsigned i = 0; i < 768; i++) {
		*g = *base;
		uint8_t control = i < 256 ? (uint8_t) i : (i < 512 ? 0 : 0x80);
		uint8_t p = i < 256 ? 0x24 : (uint8_t) i;
		uint8_t returnS = (uint8_t) (i * 73);
		g->ram[0x100 | returnS] = 0x60;
		g->ram[0x100 | (uint8_t) (returnS - 1)] = 0x1F;
		g->cpu.s = (uint8_t) (returnS - 2);
		g->cpu.pc = ENTRY;
		g->cpu.p = p;
		g->cpu.irqMask = (p & 4) != 0;
		g->cpu.a = (uint8_t) i;
		g->cpu.x = (uint8_t) (i * 7);
		g->cpu.y = (uint8_t) (i * 13);
		g->irq = g->nmi = false;
		g->ram[0x15] = control;
		g->ram[0x1E] = (uint8_t) (i ^ 0x5A);
		g->scrollX = (uint8_t) i;
		g->scrollY = (uint8_t) ~i;
		memset(g->vram, (uint8_t) (i * 17), sizeof(g->vram));
		struct Result r = check(g, true);
		if (!passed(r)) {
			fprintf(stderr, "case=%u return=%u interrupted=%u lcd=%u event_timing=%u rom=%u registers=%u "
				"ram=%u vram=%u peripherals=%u clock_delta=%lld\n", i, r.returned,
				r.interrupted, r.lcd, r.eventTiming, r.rom, r.registers, r.ram, r.vram, r.peripherals,
				r.clockDelta);
			failures++;
		}
		if (i == 0) printf("scratch_lcd_positions=%u,%u stack_depth=%u\n",
			r.scratchAt[0], r.scratchAt[1], r.stackDepth);
	}
	printf("synthetic_cases=768 failures=%u\n", failures);
	return failures != 0;
}

static int corpus(struct Gamate* g, struct Gamate* copy, unsigned frames) {
	unsigned calls = 0, failures = 0, spanned = 0, openFailures = 0, decimal = 0, maxStack = 0;
	const char* press = getenv("GAMATEBIOSCALLS_PRESS");
	for (unsigned frame = 1; frame <= frames; frame++) {
		g->buttons = gamateBiosButtonsForFrame(frame, press);
		g->frameDone = false;
		while (!g->frameDone) {
			unsigned cycles = gamateCpuStep(g);
			gamateRunClocks(g, cycles * 2);
			if (g->cpu.pc != ENTRY) continue;
			calls++;
			decimal += (g->cpu.p & 8) != 0;
			*copy = *g;
			copy->cpu.p |= 4;
			copy->cpu.irqMask = true;
			struct Result r = check(copy, false);
			failures += !passed(r);
			if (r.stackDepth > maxStack) maxStack = r.stackDepth;
			*copy = *g;
			r = check(copy, false);
			spanned += r.interrupted;
			openFailures += !r.returned || !r.lcd || !r.vram || !r.registers || r.clockDelta < 0 ||
				(!r.interrupted && (r.clockDelta != 0 || !r.eventTiming));
		}
	}
	printf("calls=%u masked_failures=%u interrupt_spanning=%u open_failures=%u decimal=%u "
		"max_stack=%u\n", calls, failures, spanned, openFailures, decimal, maxStack);
	return failures || openFailures;
}

static int pair(struct Gamate* first, const char* biosPath) {
	size_t biosSize;
	uint8_t* bios = readFile(biosPath, &biosSize);
	struct Gamate* second = malloc(sizeof(*second));
	if (!bios || !second) {
		fprintf(stderr, "could not read comparison BIOS or allocate machine\n");
		free(bios); free(second);
		return 2;
	}
	gamateInit(second);
	bool loaded = gamateLoad(second, first->rom, first->romSize, bios, biosSize);
	free(bios);
	if (!loaded || !boot(second) || !seekEntry(first) || !seekEntry(second)) {
		fprintf(stderr, "could not reach paired service entries\n");
		gamateFree(second); free(second);
		return 1;
	}
	uint8_t beforeFirst[sizeof(first->ram)], beforeSecond[sizeof(second->ram)];
	memcpy(beforeFirst, first->ram, sizeof(beforeFirst));
	memcpy(beforeSecond, second->ram, sizeof(beforeSecond));
	first->cpu.p |= 4;
	first->cpu.irqMask = true;
	second->cpu.p |= 4;
	second->cpu.irqMask = true;
	struct Result a = check(first, false), b = check(second, false);
	unsigned initial[2] = { 0 }, final[2] = { 0 }, added[2] = { 0 }, changed[2] = { 0 };
	for (size_t i = 0; i < sizeof(beforeFirst); i++) {
		unsigned stack = i >= 0x100 && i < 0x200;
		bool before = beforeFirst[i] != beforeSecond[i];
		bool after = first->ram[i] != second->ram[i];
		initial[stack] += before;
		final[stack] += after;
		added[stack] += !before && after;
		changed[stack] += before &&
			(beforeFirst[i] != first->ram[i] || beforeSecond[i] != second->ram[i]);
	}
	printf("initial_nonstack=%u initial_stack=%u final_nonstack=%u final_stack=%u "
		"added_nonstack=%u added_stack=%u changed_initial_nonstack=%u changed_initial_stack=%u\n",
		initial[0], initial[1], final[0], final[1], added[0], added[1], changed[0], changed[1]);
	gamateFree(second);
	free(second);
	if (!passed(a) || !passed(b)) {
		fprintf(stderr, "a paired call did not match the model\n");
		return 1;
	}
	return 0;
}

struct ControlProbe {
	uint64_t start, readsAt[2];
	uint8_t reads[2], lcd[2];
	unsigned readCount, writeCount, lcdCount;
};

static void controlAccess(void* context, const struct Gamate* g, bool write,
		uint16_t address, uint8_t previous, uint8_t value) {
	struct ControlProbe* p = context;
	(void) address;
	(void) previous;
	if (write) {
		p->writeCount++;
	} else {
		unsigned n = p->readCount++;
		if (n < 2) {
			p->readsAt[n] = g->masterClocks - p->start;
			p->reads[n] = value;
		}
	}
}

static void controlLcd(void* context, const struct Gamate* g, uint8_t reg, uint8_t value) {
	struct ControlProbe* p = context;
	(void) g;
	if (reg == 1) {
		unsigned n = p->lcdCount++;
		if (n < 2) p->lcd[n] = value;
	}
}

static int controlProbe(struct Gamate* g, const struct Gamate* base) {
	unsigned failures = 0;
	for (unsigned i = 0; i < 2; i++) {
		*g = *base;
		uint8_t initial = i ? 0x40 : 0, replacement = i ? 0x20 : 0x60;
		uint8_t returnS = g->cpu.s;
		g->ram[0x100 | returnS] = 0x60;
		g->ram[0x100 | (uint8_t) (returnS - 1)] = 0x1F;
		g->cpu.s -= 2;
		g->cpu.pc = ENTRY;
		g->cpu.p = 0x24;
		g->cpu.irqMask = true;
		g->irq = g->nmi = false;
		g->ram[0x15] = initial;
		memset(g->vram, 0xFF, sizeof(g->vram));
		struct ControlProbe p = { .start = g->masterClocks };
		gamateSetRamDiagnosticWatch(g, 0x15, controlAccess, &p);
		gamateSetLcdDiagnosticWatch(g, controlLcd, &p);
		bool returned = false, changed = false;
		while (!returned && g->masterClocks - p.start < CLOCKS + 1000000) {
			unsigned cycles = gamateCpuStep(g);
			gamateRunClocks(g, cycles * 2);
			if (!changed && g->masterClocks - p.start >= 128000) {
				g->ram[0x15] = replacement;
				changed = true;
			}
			returned = g->cpu.pc == 0x6020 && g->cpu.s == returnS;
		}
		gamateSetRamDiagnosticWatch(g, 0x15, NULL, NULL);
		gamateSetLcdDiagnosticWatch(g, NULL, NULL);
		bool vram = true;
		for (unsigned address = 0; address < 0x2000; address++) {
			bool cleared = initial & 0x40 ? !(address & 31) && address <= 0x1CC0 :
				address < 0x1900;
			for (unsigned plane = 0; plane < 2; plane++)
				if (g->vram[2 * address + plane] != (cleared ? 0 : 0xFF)) vram = false;
		}
		bool ok = returned && changed && p.readCount == 2 && !p.writeCount && p.lcdCount == 2 &&
			p.reads[0] == initial && p.reads[1] == replacement &&
			p.lcd[0] == (initial | 0x80) && p.lcd[1] == (replacement | 0x40) &&
			p.readsAt[0] == 10 && p.readsAt[1] == 247300 && g->cpu.a == (replacement | 0x40) &&
			g->masterClocks - p.start == CLOCKS && vram;
		printf("control_case=%u control_reads=%u read_ticks=%llu,%llu matched=%u\n", i,
			p.readCount, (unsigned long long) p.readsAt[0],
			(unsigned long long) p.readsAt[1], ok);
		if (!ok) {
			fprintf(stderr, "control mutation case %u did not match the observed contract\n", i);
			failures++;
		}
	}
	return failures != 0;
}

struct ScratchProbe {
	uint64_t start, readsAt[4];
	uint8_t lastRead;
	unsigned reads, writes, columns, mismatch, changedReads, timingMismatch;
	bool changed;
};

static void scratchProbeAccess(void* context, const struct Gamate* g, bool write,
		uint16_t address, uint8_t previous, uint8_t value) {
	struct ScratchProbe* p = context;
	(void) address;
	(void) previous;
	if (write) {
		p->writes++;
	} else {
		unsigned n = p->reads++;
		if (n == 0) p->readsAt[0] = g->masterClocks - p->start;
		if (n == 199) p->readsAt[1] = g->masterClocks - p->start;
		if (n == 200) p->readsAt[2] = g->masterClocks - p->start;
		if (n == 399) p->readsAt[3] = g->masterClocks - p->start;
		p->timingMismatch += n >= 400 || g->masterClocks - p->start !=
			50 + (n / 200) * 123636 + (n % 200) * 618;
		p->lastRead = value;
		p->changedReads += p->changed && value == 0x23;
	}
}

static void scratchProbeLcd(void* context, const struct Gamate* g, uint8_t reg, uint8_t value) {
	struct ScratchProbe* p = context;
	(void) g;
	if (reg == 4) {
		p->columns++;
		p->mismatch += p->columns != p->reads || value != p->lastRead;
	}
}

static int scratchProbe(struct Gamate* g) {
	uint8_t returnS = g->cpu.s;
	g->ram[0x100 | returnS] = 0x60;
	g->ram[0x100 | (uint8_t) (returnS - 1)] = 0x1F;
	g->cpu.s -= 2;
	g->cpu.pc = ENTRY;
	g->cpu.p = 0x24;
	g->cpu.irqMask = true;
	g->irq = g->nmi = false;
	g->ram[0x15] = 0;
	struct ScratchProbe p = { .start = g->masterClocks };
	gamateSetRamDiagnosticWatch(g, 0x1E, scratchProbeAccess, &p);
	gamateSetLcdDiagnosticWatch(g, scratchProbeLcd, &p);
	bool returned = false;
	while (!returned && g->masterClocks - p.start < CLOCKS + 1000000) {
		unsigned cycles = gamateCpuStep(g);
		gamateRunClocks(g, cycles * 2);
		if (!p.changed && g->masterClocks - p.start >= 128000) {
			g->ram[0x1E] = 0x23;
			p.changed = true;
		}
		returned = g->cpu.pc == 0x6020 && g->cpu.s == returnS;
	}
	gamateSetRamDiagnosticWatch(g, 0x1E, NULL, NULL);
	gamateSetLcdDiagnosticWatch(g, NULL, NULL);
	bool ok = returned && p.changed && p.reads == 400 && p.writes == 2 && p.columns == 400 &&
		!p.mismatch && !p.timingMismatch && p.changedReads && g->ram[0x1E] == 0x23 &&
		g->cpu.a == 0x40 &&
		g->masterClocks - p.start == CLOCKS;
	printf("scratch_reads=%u changed_reads=%u read_ticks=%llu,%llu,%llu,%llu matched=%u\n",
		p.reads, p.changedReads, (unsigned long long) p.readsAt[0],
		(unsigned long long) p.readsAt[1], (unsigned long long) p.readsAt[2],
		(unsigned long long) p.readsAt[3], ok);
	if (!ok) fprintf(stderr, "scratch mutation did not match the observed contract\n");
	return !ok;
}

int main(int argc, char** argv) {
	if (argc < 4 || argc > 5 || (strcmp(argv[3], "synthetic") && strcmp(argv[3], "corpus") &&
			strcmp(argv[3], "pair") && strcmp(argv[3], "control") && strcmp(argv[3], "scratch")) ||
			(argc == 5 && (!strcmp(argv[3], "synthetic") || !strcmp(argv[3], "control") ||
			!strcmp(argv[3], "scratch"))) ||
			(argc == 4 && !strcmp(argv[3], "pair"))) {
		fprintf(stderr, "usage: %s bios cartridge "
			"synthetic|control|scratch|corpus [frames]|pair other_bios\n", argv[0]);
		return 2;
	}
	unsigned frames = 1000;
	if (argc == 5 && !strcmp(argv[3], "corpus")) {
		char* end;
		errno = 0;
		unsigned long n = strtoul(argv[4], &end, 0);
		if (errno || end == argv[4] || *end || !n || n > UINT_MAX) {
			fprintf(stderr, "frames must be a positive integer\n");
			return 2;
		}
		frames = (unsigned) n;
	}
	size_t biosSize, romSize;
	uint8_t* bios = readFile(argv[1], &biosSize);
	uint8_t* rom = readFile(argv[2], &romSize);
	struct Gamate* g = malloc(sizeof(*g));
	struct Gamate* copy = malloc(sizeof(*copy));
	if (!bios || !rom || !g || !copy) {
		fprintf(stderr, "could not read inputs or allocate machines\n");
		free(bios); free(rom); free(g); free(copy);
		return 2;
	}
	gamateInit(g);
	bool loaded = gamateLoad(g, rom, romSize, bios, biosSize);
	free(bios);
	free(rom);
	if (!loaded || !boot(g)) {
		fprintf(stderr, "could not reach cartridge entry\n");
		gamateFree(g); free(g); free(copy);
		return 1;
	}
	*copy = *g;
	int result;
	if (!strcmp(argv[3], "corpus")) result = corpus(g, copy, frames);
	else if (!strcmp(argv[3], "pair")) result = pair(g, argv[4]);
	else if (!strcmp(argv[3], "control")) result = controlProbe(g, copy);
	else if (!strcmp(argv[3], "scratch")) result = scratchProbe(g);
	else result = synthetic(g, copy);
	gamateFree(g);
	free(g);
	free(copy);
	return result;
}
