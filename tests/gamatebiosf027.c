/* Copyright (c) 2026 superwhizkid contributors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
/* Diagnostic-only $F027 behavioral model with synthetic tests and authentic-call replay. */
#include "gamate/gamate.h"
#include "gamatebiosinput.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

static uint8_t pattern(unsigned seed, unsigned index) {
	uint32_t n = seed * 0x9E3779B9u + index * 0x85EBCA6Bu;
	n ^= n >> 16;
	n *= 0x7FEB352Du;
	n ^= n >> 15;
	return (uint8_t) n;
}

enum { MAX_EVENTS = 8192 };

struct LcdEvents {
	uint8_t reg[MAX_EVENTS], value[MAX_EVENTS];
	unsigned count, observed, mismatches;
	uint64_t start, firstTick;
	uint8_t firstRegister;
};

struct RomEvents {
	uint16_t address[MAX_EVENTS];
	unsigned count, observed, mismatches;
};

static void romEvent(struct RomEvents* events, uint16_t address) {
	if (!events || address < 0x6000 || address >= 0xE000) return;
	if (events->count < MAX_EVENTS) events->address[events->count] = address;
	events->count++;
}

static void tileReads(struct RomEvents* events, unsigned base, unsigned selector) {
	for (unsigned row = 0; row < 8; row++) {
		unsigned address = base + selector * 8 + row;
		if ((base & 0xFF) + ((selector & 31) << 3) + row > 255)
			romEvent(events, (uint16_t) (address - 0x100));
		romEvent(events, (uint16_t) address);
	}
}

static void checkRomEvent(void* context, const struct Gamate* g, uint16_t address,
		size_t physicalOffset) {
	struct RomEvents* events = context;
	(void) g;
	(void) physicalOffset;
	unsigned i = events->observed++;
	if (i >= events->count || i >= MAX_EVENTS || events->address[i] != address)
		events->mismatches++;
}

static bool romMatches(const struct RomEvents* events) {
	return events->count <= MAX_EVENTS && events->observed == events->count &&
		events->mismatches == 0;
}

static void lcdEvent(struct LcdEvents* events, uint8_t reg, uint8_t value) {
	if (!events) return;
	if (events->count < MAX_EVENTS) {
		events->reg[events->count] = reg;
		events->value[events->count] = value;
	}
	events->count++;
}

static void checkLcdEvent(void* context, const struct Gamate* g, uint8_t reg, uint8_t value) {
	struct LcdEvents* events = context;
	unsigned i = events->observed++;
	if (!i) {
		events->firstTick = g->masterClocks - events->start;
		events->firstRegister = reg;
	}
	if (i >= events->count || i >= MAX_EVENTS || events->reg[i] != reg ||
			events->value[i] != value) events->mismatches++;
}

static bool lcdMatches(const struct LcdEvents* events) {
	return events->count <= MAX_EVENTS && events->observed == events->count &&
		events->mismatches == 0;
}

static bool lcdStateMatches(const struct Gamate* entry, const struct Gamate* result,
		const struct LcdEvents* events) {
	uint16_t address = entry->vramAddress;
	bool plane = entry->vramPlane, incrementRow = entry->incrementRow;
	bool blank = entry->displayBlank, window = entry->window;
	bool swap = entry->swapPlanes, stopped = entry->displayStopped;
	for (unsigned i = 0; i < events->count && i < MAX_EVENTS; i++) {
		uint8_t value = events->value[i];
		switch (events->reg[i]) {
		case 1:
			blank = value & 0x80;
			incrementRow = value & 0x40;
			window = value & 0x20;
			swap = value & 0x10;
			stopped = value & 1;
			break;
		case 4:
			plane = value & 0x80;
			address = (address & 0x1FE0) | (value & 31);
			break;
		case 5: address = (address & 31) | ((uint16_t) value << 5); break;
		case 7: address = (address + (incrementRow ? 32 : 1)) & 0x1FFF; break;
		}
	}
	return result->vramAddress == address && result->vramPlane == plane &&
		result->incrementRow == incrementRow && result->displayBlank == blank &&
		result->window == window && result->swapPlanes == swap &&
		result->displayStopped == stopped;
}

static bool invokeAtStack(struct Gamate* g, uint8_t p, uint8_t s,
		struct LcdEvents* events, struct RomEvents* rom) {
	uint8_t a = g->cpu.a, x = g->cpu.x, y = g->cpu.y;
	uint8_t lastRecord[8];
	memcpy(lastRecord, g->ram + 0x99, sizeof(lastRecord));
	g->ram[0x100 | (uint8_t) (s + 2)] = 0x60;
	g->ram[0x100 | (uint8_t) (s + 1)] = 0x1F;
	g->cpu.pc = 0xF027;
	g->cpu.s = s;
	g->cpu.p = p;
	g->cpu.irqMask = (p & 4) != 0;
	g->irq = g->nmi = false;
	gamateResetVramDiagnostics(g);
	if (events) {
		events->start = g->masterClocks;
		gamateSetLcdDiagnosticWatch(g, checkLcdEvent, events);
	}
	if (rom) {
		romEvent(rom, 0x601F);
		gamateResetRomDiagnostics(g);
		gamateSetRomDiagnosticWatch(g, checkRomEvent, rom);
	}
	uint64_t start = g->masterClocks;
	bool returned = false;
	while (g->masterClocks - start < 1000000) {
		unsigned cycles = gamateCpuStep(g);
		gamateRunClocks(g, cycles * 2);
		g->irq = false;
		if (g->cpu.pc == 0x6020 && g->cpu.s == (uint8_t) (s + 2)) {
			uint8_t expectedP = (uint8_t) ((p & 0x1C) | 0x21 |
				(a & 0x80) | (a == 0 ? 2 : 0));
			returned = g->cpu.a == a && g->cpu.x == x && g->cpu.y == y &&
				g->cpu.p == expectedP && !memcmp(g->ram + 0x300, lastRecord, sizeof(lastRecord));
			break;
		}
	}
	if (events) gamateSetLcdDiagnosticWatch(g, NULL, NULL);
	if (rom) gamateSetRomDiagnosticWatch(g, NULL, NULL);
	return returned;
}

static bool invoke(struct Gamate* g, uint8_t p, struct LcdEvents* events, struct RomEvents* rom) {
	return invokeAtStack(g, p, 0xFD, events, rom);
}

static bool spriteCase(struct Gamate* g, const struct Gamate* base, unsigned record,
		unsigned x, unsigned y, unsigned selector, unsigned seed) {
	*g = *base;
	memset(g->ram + 0x21, 0xFF, 0x80);
	g->ram[6] = 0x00;
	g->ram[7] = 0x02;
	g->ram[8] = 0x40;
	g->ram[9] = 0x02;
	g->ram[0x15] = 0;
	for (unsigned i = 0; i < 0x80; i++) g->ram[0x200 + i] = pattern(seed, i);
	for (unsigned i = 0; i < sizeof(g->vram); i++) g->vram[i] = pattern(seed + 11, i);
	unsigned entry = 0x21 + 8 * record;
	g->ram[entry] = (uint8_t) x;
	g->ram[entry + 1] = (uint8_t) y;
	g->ram[entry + 2] = (uint8_t) selector;
	g->ram[entry + 3] = pattern(seed + 31, 0);
	g->ram[entry + 4] = pattern(seed + 31, 1);
	g->ram[entry + 5] = pattern(seed + 31, 2);
	g->ram[entry + 6] = pattern(seed + 31, 3);
	g->ram[entry + 7] = 0xFF;
	uint8_t expected[sizeof(g->vram)];
	uint8_t initial[sizeof(g->vram)];
	memcpy(expected, g->vram, sizeof(expected));
	memcpy(initial, g->vram, sizeof(initial));
	unsigned side = record < 8 ? 16 : 8;
	unsigned columns = side / 8 + 1;
	for (unsigned row = 0; row < side; row++) {
		uint32_t source[2] = { 0, 0 };
		for (unsigned plane = 0; plane < 2; plane++) {
			for (unsigned col = 0; col < side / 8; col++) {
				unsigned tile = selector + (row / 8) * 2 + col;
				source[plane] = (source[plane] << 8) |
					g->ram[0x200 + plane * 0x40 + tile * 8 + row % 8];
			}
			source[plane] = (source[plane] << 8) >> (x & 7);
		}
		for (unsigned col = 0; col < columns; col++) {
			unsigned shift = 8 * (columns - 1 - col);
			uint8_t pixel[2] = { (uint8_t) (source[0] >> shift),
				(uint8_t) (source[1] >> shift) };
			uint8_t mask = (uint8_t) ~(pixel[0] | pixel[1]);
			unsigned address = (((y + row) & 0xFF) << 5) | (((x >> 3) + col) & 31);
			for (unsigned plane = 0; plane < 2; plane++) {
				unsigned offset = 2 * address + plane;
				expected[offset] = pixel[plane] | (expected[offset] & mask);
			}
		}
	}
	bool returned = invoke(g, 0x64, NULL, NULL);
	bool pixels = !memcmp(g->vram, expected, sizeof(expected));
	unsigned writes = side * columns * 2;
	bool accesses = g->diagnostics.vramTrackedWrites == writes &&
		g->diagnostics.vramReads == writes;
	bool scratch = true;
	for (unsigned plane = 0; plane < 2; plane++)
		for (unsigned col = 0; col < columns; col++)
			for (unsigned row = 0; row < side; row++) {
				unsigned address = (((y + row) & 0xFF) << 5) | (((x >> 3) + col) & 31);
				unsigned buffer = 48 * plane + side * col + row;
				if (g->ram[0x308 + buffer] != initial[2 * address + plane] ||
						g->ram[0x368 + buffer] != expected[2 * address + plane]) scratch = false;
			}
	if (!returned || !pixels || !accesses || !scratch) {
		fprintf(stderr, "record=%u x=%02X y=%02X selector=%02X seed=%u "
			"returned=%u pixels=%u accesses=%u scratch=%u reads=%llu writes=%llu\n",
			record, x, y, selector, seed, returned, pixels, accesses, scratch,
			(unsigned long long) g->diagnostics.vramReads,
			(unsigned long long) g->diagnostics.vramTrackedWrites);
		return false;
	}
	return true;
}

static bool backgroundCase(struct Gamate* g, const struct Gamate* base, unsigned record,
		unsigned x, unsigned y, unsigned seed) {
	*g = *base;
	memset(g->ram + 0x21, 0xFF, 0x80);
	g->ram[6] = 0x00;
	g->ram[7] = 0x02;
	g->ram[8] = 0x40;
	g->ram[9] = 0x02;
	g->ram[0x15] = 0;
	for (unsigned i = 0; i < 0x80; i++) g->ram[0x200 + i] = pattern(seed, i);
	for (unsigned i = 0; i < sizeof(g->vram); i++) g->vram[i] = pattern(seed + 11, i);
	unsigned entry = 0x21 + 8 * record;
	g->ram[entry] = pattern(seed + 31, 0);
	g->ram[entry + 1] = pattern(seed + 31, 1);
	g->ram[entry + 3] = pattern(seed + 31, 2);
	g->ram[entry + 4] = pattern(seed + 31, 3);
	g->ram[entry + 5] = (uint8_t) x;
	g->ram[entry + 6] = (uint8_t) y;
	g->ram[entry + 7] = (uint8_t) seed;
	unsigned tiles = record < 8 ? 3 : 2;
	uint8_t selectors[3][3] = { { 0 } };
	for (unsigned row = 0; row < tiles; row++) {
		for (unsigned col = 0; col < tiles; col++) {
			unsigned mapRow = (0xE6 + (y >> 3) + row) & 0xFF;
			unsigned mapCol = ((x >> 3) + col) & 31;
			unsigned offset = 2 * ((mapRow << 5) | mapCol);
			selectors[row][col] = (uint8_t) ((row * tiles + col + seed) & 3);
			g->vram[offset] = selectors[row][col];
		}
	}
	uint8_t expected[sizeof(g->vram)];
	memcpy(expected, g->vram, sizeof(expected));
	for (unsigned row = 0; row < tiles; row++) {
		for (unsigned col = 0; col < tiles; col++) {
			for (unsigned pixelRow = 0; pixelRow < 8; pixelRow++) {
				unsigned screenRow = ((y & 0xF8) + row * 8 + pixelRow) & 0xFF;
				unsigned screenCol = ((x >> 3) + col) & 31;
				unsigned address = (screenRow << 5) | screenCol;
				for (unsigned plane = 0; plane < 2; plane++) {
					expected[2 * address + plane] = g->ram[0x200 + plane * 0x40 +
						selectors[row][col] * 8 + pixelRow];
				}
			}
		}
	}
	bool returned = invoke(g, 0x64, NULL, NULL);
	bool pixels = !memcmp(g->vram, expected, sizeof(expected));
	bool accesses = g->diagnostics.vramTrackedWrites == tiles * tiles * 16 &&
		g->diagnostics.vramReads == tiles * tiles;
	bool scratch = true;
	for (unsigned col = 0; col < tiles; col++)
		for (unsigned row = 0; row < tiles; row++)
			if (g->ram[0x308 + col * tiles + row] != selectors[row][col]) scratch = false;
	if (!returned || !pixels || !accesses || !scratch) {
		fprintf(stderr, "background record=%u x=%02X y=%02X seed=%u returned=%u "
			"pixels=%u accesses=%u scratch=%u reads=%llu writes=%llu\n", record, x, y, seed,
			returned, pixels, accesses, scratch, (unsigned long long) g->diagnostics.vramReads,
			(unsigned long long) g->diagnostics.vramTrackedWrites);
		return false;
	}
	return true;
}

static bool firstEventCase(struct Gamate* g, const struct Gamate* base, unsigned record,
		bool background) {
	*g = *base;
	memset(g->ram + 0x21, 0xFF, 0x80);
	g->ram[6] = 0x00;
	g->ram[7] = 0x02;
	g->ram[8] = 0x40;
	g->ram[9] = 0x02;
	g->ram[0x15] = 0;
	for (unsigned i = 0; i < 0x80; i++) g->ram[0x200 + i] = pattern(51, i);
	for (unsigned i = 0; i < sizeof(g->vram); i++) g->vram[i] = pattern(62, i);
	unsigned entry = 0x21 + 8 * record;
	g->ram[entry] = g->ram[entry + 5] = 0;
	g->ram[entry + 1] = g->ram[entry + 6] = 0x20;
	g->ram[entry + (background ? 7 : 2)] = 0;
	struct LcdEvents events = { 0 };
	bool returned = invoke(g, 0x24, &events, NULL);
	unsigned firstTick = background ? 166 + 44 * record + (record >= 8 ? 6 : 0) :
		1132 + 340 * record + (record >= 8 ? 44 : 0);
	unsigned count = background ? (record < 8 ? 175 : 81) : (record < 8 ? 303 : 106);
	if (!returned || events.firstRegister != 1 || events.firstTick != firstTick ||
			events.observed != count) {
		fprintf(stderr, "first LCD event record=%u background=%u returned=%u "
			"register=%u tick=%llu/%u count=%u/%u\n", record, background, returned,
			events.firstRegister, (unsigned long long) events.firstTick, firstTick,
			events.observed, count);
		return false;
	}
	return true;
}

static bool emptyScratchCase(struct Gamate* g, const struct Gamate* base, unsigned seed) {
	*g = *base;
	memset(g->ram + 0x21, 0xFF, 0x80);
	for (unsigned address = 0; address < 0x400; address++)
		if (address <= 3 || (address >= 0x18 && address <= 0x1F) ||
				(address >= 0x300 && address <= 0x3C7))
			g->ram[address] = pattern(seed, address);
	gamateResetRamDiagnostics(g);
	bool returned = invoke(g, (uint8_t) ((seed & 0xD3) | 0x24), NULL, NULL);
	for (unsigned address = 0x300; address <= 0x307; address++)
		if (!(g->diagnostics.ramWritten[address >> 3] & (1u << (address & 7))))
			returned = false;
	for (unsigned address = 0; address < 0x400; address++) {
		if (address <= 3 || (address >= 0x18 && address <= 0x1F) ||
				(address >= 0x308 && address <= 0x3C7)) {
			unsigned bit = 1u << (address & 7);
			if ((g->diagnostics.ramWritten[address >> 3] & bit) ||
					g->ram[address] != pattern(seed, address)) returned = false;
		}
	}
	if (!returned) fprintf(stderr, "empty scratch case seed=%u failed\n", seed);
	return returned;
}

struct LiveMutation {
	uint16_t addresses[3];
	uint8_t replacements[3], reg;
	uint8_t values[3];
	unsigned changes, count;
};

static void captureLiveMutation(void* context, const struct Gamate* g, uint8_t reg, uint8_t value) {
	struct LiveMutation* trace = context;
	(void) g;
	if (reg == trace->reg) {
		if (trace->count < sizeof(trace->values)) trace->values[trace->count] = value;
		trace->count++;
	}
}

static bool invokeWithMutation(struct Gamate* g, struct LiveMutation* trace) {
	g->ram[0x1FF] = 0x60;
	g->ram[0x1FE] = 0x1F;
	g->cpu.pc = 0xF027;
	g->cpu.s = 0xFD;
	g->cpu.p = 0x24;
	g->cpu.irqMask = true;
	g->irq = g->nmi = false;
	gamateSetLcdDiagnosticWatch(g, captureLiveMutation, trace);
	uint64_t start = g->masterClocks;
	bool mutated = false, returned = false;
	while (g->masterClocks - start < 1000000) {
		unsigned cycles = gamateCpuStep(g);
		gamateRunClocks(g, cycles * 2);
		g->irq = false;
		if (!mutated && trace->count == 1) {
			for (unsigned i = 0; i < trace->changes; i++)
				g->ram[trace->addresses[i]] = trace->replacements[i];
			mutated = true;
		}
		if (g->cpu.pc == 0x6020 && g->cpu.s == 0xFF) {
			returned = true;
			break;
		}
	}
	gamateSetLcdDiagnosticWatch(g, NULL, NULL);
	return returned && mutated;
}

static bool liveControlCase(struct Gamate* g, const struct Gamate* base, bool small) {
	*g = *base;
	memset(g->ram + 0x21, 0xFF, 0x80);
	g->ram[6] = 0;
	g->ram[7] = 2;
	g->ram[8] = 0x40;
	g->ram[9] = 2;
	g->ram[0x15] = 0;
	for (unsigned i = 0; i < 0x80; i++) g->ram[0x200 + i] = pattern(51, i);
	for (unsigned i = 0; i < sizeof(g->vram); i++) g->vram[i] = pattern(62, i);
	unsigned entry = 0x21 + (small ? 64 : 0);
	g->ram[entry] = 0;
	g->ram[entry + 1] = 0x20;
	g->ram[entry + 2] = 0;
	struct LiveMutation trace = { .addresses = { 0x15 }, .replacements = { 0x10 },
		.reg = 1, .changes = 1 };
	bool returned = invokeWithMutation(g, &trace);
	unsigned count = small ? 2 : 3;
	bool matched = returned && trace.count == count && trace.values[0] == 0x40;
	for (unsigned i = 1; i < count; i++) matched &= trace.values[i] == 0x50;
	if (!matched) fprintf(stderr, "live LCD control small=%u returned=%u count=%u/%u\n",
		small, returned, trace.count, count);
	return matched;
}

static bool livePointerCase(struct Gamate* g, const struct Gamate* base) {
	*g = *base;
	memset(g->ram + 0x21, 0xFF, 0x80);
	memset(g->vram, 0, sizeof(g->vram));
	memset(g->ram + 0x200, 0, 0xC0);
	memset(g->ram + 0x200, 0xA5, 8);
	memset(g->ram + 0x240, 0x3C, 8);
	memset(g->ram + 0x280, 0x66, 8);
	g->ram[6] = 0;
	g->ram[7] = 2;
	g->ram[8] = 0x80;
	g->ram[9] = 2;
	g->ram[0x15] = 0;
	g->ram[0x66] = 0;
	g->ram[0x67] = 0x20;
	g->ram[0x68] = 0;
	struct LiveMutation trace = { .addresses = { 6 }, .replacements = { 0x40 },
		.reg = 7, .changes = 1 };
	bool returned = invokeWithMutation(g, &trace);
	bool matched = returned && trace.count == 64;
	for (unsigned row = 0; row < 16; row++) {
		unsigned offset = 2 * ((0x20 + row) << 5);
		if (g->vram[offset] != (row < 8 ? 0xA5 : 0x3C)) matched = false;
	}
	if (!matched) fprintf(stderr, "live graphics pointer returned=%u writes=%u\n",
		returned, trace.count);
	return matched;
}

static bool liveRecordCase(struct Gamate* g, const struct Gamate* base) {
	*g = *base;
	memset(g->ram + 0x21, 0xFF, 0x80);
	memset(g->vram, 0, sizeof(g->vram));
	memset(g->ram + 0x200, 0xFF, 0x20);
	memset(g->ram + 0x240, 0, 0x20);
	g->ram[6] = 0;
	g->ram[7] = 2;
	g->ram[8] = 0x40;
	g->ram[9] = 2;
	g->ram[0x15] = 0;
	g->ram[0x21] = 0;
	g->ram[0x22] = 0x20;
	g->ram[0x23] = 0;
	struct LiveMutation trace = { .addresses = { 0x29, 0x2A, 0x2B },
		.replacements = { 0x40, 0x60, 0 }, .reg = 7, .changes = 3 };
	bool returned = invokeWithMutation(g, &trace);
	bool matched = returned && trace.count == 192;
	for (unsigned row = 0; row < 16; row++)
		for (unsigned col = 0; col < 2; col++) {
			unsigned offset = 2 * (((0x60 + row) << 5) | (8 + col));
			if (g->vram[offset] != 0xFF || g->vram[offset + 1] != 0) matched = false;
		}
	if (!matched) fprintf(stderr, "live sprite record returned=%u writes=%u\n", returned, trace.count);
	return matched;
}

static bool stackWritesMatch(const struct Gamate* g, uint8_t s, unsigned expected) {
	for (unsigned i = 0; i < 256; i++) {
		unsigned address = 0x100 | (uint8_t) (s - i);
		bool written = (g->diagnostics.ramWritten[address >> 3] & (1u << (address & 7))) != 0;
		if (written != (i < expected)) return false;
	}
	return true;
}

static bool stackCase(struct Gamate* g, const struct Gamate* base, unsigned record,
		bool active, bool background, unsigned x, unsigned selector, uint8_t s) {
	*g = *base;
	memset(g->ram + 0x21, 0xFF, 0x80);
	g->ram[6] = 0;
	g->ram[7] = 0x80;
	g->ram[8] = 0;
	g->ram[9] = 0x90;
	memset(g->vram, (uint8_t) selector, sizeof(g->vram));
	if (active) {
		unsigned entry = 0x21 + 8 * record;
		g->ram[entry] = g->ram[entry + 5] = (uint8_t) x;
		g->ram[entry + 1] = g->ram[entry + 6] = 0x20;
		g->ram[entry + (background ? 7 : 2)] = (uint8_t) selector;
	}
	gamateResetRamDiagnostics(g);
	bool returned = invokeAtStack(g, 0x64, s, NULL, NULL);
	unsigned expected = !active ? 9 : background ? 11 : record < 8 ? 13 : 18;
	bool writes = stackWritesMatch(g, s, expected);
	if (!returned || !writes)
		fprintf(stderr, "stack record=%u active=%u background=%u x=%02X selector=%02X "
			"entry_s=%02X returned=%u writes=%u\n", record, active, background, x,
			selector, s, returned, writes);
	return returned && writes;
}

static unsigned tileClocks(unsigned selector, const unsigned low[2]) {
	unsigned clocks = 64 * (selector >> 5);
	for (unsigned plane = 0; plane < 2; plane++)
		for (unsigned row = 0; row < 8; row++)
			clocks += 2 * (low[plane] + ((selector & 31) << 3) + row > 255);
	return clocks;
}

static const uint8_t zeroPageScratchAddresses[] = {
	0x00, 0x01, 0x02, 0x03, 0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F
};

struct ZeroPageScratch {
	uint8_t value[sizeof(zeroPageScratchAddresses)];
	bool written[sizeof(zeroPageScratchAddresses)];
};

static void initZeroPageScratch(struct ZeroPageScratch* scratch, const struct Gamate* g) {
	memset(scratch, 0, sizeof(*scratch));
	for (unsigned i = 0; i < sizeof(zeroPageScratchAddresses); i++)
		scratch->value[i] = g->ram[zeroPageScratchAddresses[i]];
}

static void modelZeroPageScratch(struct ZeroPageScratch* scratch, bool background,
		unsigned record, unsigned x, unsigned y, unsigned selector, unsigned plane1Base) {
	unsigned finalSelector = background || record >= 8 ? selector : (selector + 3) & 255;
	uint8_t values[] = {
		(uint8_t) (record < 8 ? 0xB0 : 0xA0), 3,
		(uint8_t) plane1Base, (uint8_t) ((plane1Base >> 8) + (finalSelector >> 5)),
		(uint8_t) (0x80 | (x >> 3)), (uint8_t) (background ? y & 0xF8 : y),
		(uint8_t) finalSelector, (uint8_t) !background, 0, 0, (uint8_t) (x >> 3),
		(uint8_t) (0xE6 + (y >> 3))
	};
	for (unsigned i = background ? 2 : 0; i < (background ? 12 : 9); i++) {
		scratch->value[i] = values[i];
		scratch->written[i] = true;
	}
}

static bool zeroPageScratchMatches(const struct ZeroPageScratch* expected,
		const struct Gamate* g, bool verbose) {
	bool matched = true;
	for (unsigned i = 0; i < sizeof(zeroPageScratchAddresses); i++) {
		unsigned address = zeroPageScratchAddresses[i];
		bool written = (g->diagnostics.ramWritten[address >> 3] & (1u << (address & 7))) != 0;
		if (g->ram[address] != expected->value[i] || written != expected->written[i]) {
			if (verbose) fprintf(stderr, "scratch address=%03X written=%u/%u\n",
				address, written, expected->written[i]);
			matched = false;
		}
	}
	return matched;
}

static void seedScratch(struct Gamate* g, unsigned seed) {
	for (unsigned i = 0; i < sizeof(zeroPageScratchAddresses); i++)
		g->ram[zeroPageScratchAddresses[i]] = pattern(seed, zeroPageScratchAddresses[i]);
	for (unsigned i = 0; i < 0xC0; i++)
		g->ram[0x308 + i] = pattern(seed, 0x308 + i);
}

static unsigned finalBackgroundSelector(const uint8_t* vram, unsigned record, unsigned x,
		unsigned y) {
	unsigned last = record < 8 ? 2 : 1;
	unsigned row = (0xE6 + (y >> 3) + last) & 255;
	unsigned col = ((x >> 3) + last) & 31;
	return vram[2 * ((row << 5) | col)];
}

static unsigned modelBackground(uint8_t* vram, const uint8_t* const graphics[2],
		const unsigned base[2], bool* used, struct LcdEvents* events, struct RomEvents* rom,
		uint8_t* scratch, uint8_t* scratchValid, uint8_t control,
		unsigned record, unsigned x, unsigned y) {
	unsigned tiles = record < 8 ? 3 : 2;
	unsigned clocks = record < 8 ? 9084 : 4270;
	unsigned low[2] = { base[0] & 255, base[1] & 255 };
	uint8_t selectors[3][3];
	lcdEvent(events, 1, control | 0x40);
	for (unsigned col = 0; col < tiles; col++) {
		for (unsigned row = 0; row < tiles; row++) {
			unsigned mapRow = (0xE6 + (y >> 3) + row) & 0xFF;
			unsigned mapCol = ((x >> 3) + col) & 31;
			lcdEvent(events, 5, (uint8_t) mapRow);
			lcdEvent(events, 4, (uint8_t) ((x >> 3) + col));
			selectors[row][col] = vram[2 * ((mapRow << 5) | mapCol)];
			if (scratch) {
				unsigned index = col * tiles + row;
				scratch[index] = selectors[row][col];
				scratchValid[index] = 1;
			}
			if (used) used[selectors[row][col]] = true;
			clocks += tileClocks(selectors[row][col], low);
		}
	}
	for (unsigned plane = 0; plane < 2; plane++) {
		for (unsigned col = 0; col < tiles; col++) {
			lcdEvent(events, 4, (uint8_t) (((x >> 3) + col) | (plane << 7)));
			lcdEvent(events, 5, (uint8_t) (y & 0xF8));
			for (unsigned row = 0; row < tiles; row++) {
				tileReads(rom, base[plane], selectors[row][col]);
				for (unsigned pixelRow = 0; pixelRow < 8; pixelRow++) {
					unsigned screenRow = ((y & 0xF8) + row * 8 + pixelRow) & 0xFF;
					unsigned screenCol = ((x >> 3) + col) & 31;
					unsigned address = (screenRow << 5) | screenCol;
					vram[2 * address + plane] =
						graphics[plane][selectors[row][col] * 8 + pixelRow];
					lcdEvent(events, 7, vram[2 * address + plane]);
				}
			}
		}
	}
	return clocks;
}

static unsigned modelSprite(uint8_t* vram, const uint8_t* const graphics[2],
		const unsigned base[2], bool* used, struct LcdEvents* events, struct RomEvents* rom,
		uint8_t* scratch, uint8_t* scratchValid, uint8_t control,
		unsigned record, unsigned x, unsigned y, unsigned selector) {
	unsigned side = record < 8 ? 16 : 8, columns = side / 8 + 1;
	unsigned low[2] = { base[0] & 255, base[1] & 255 };
	unsigned clocks = (record < 8 ? 23326 : 7556) +
		(x & 7) * (record < 8 ? 1516 : 540);
	for (unsigned row = 0; row < side / 8; row++) {
		for (unsigned col = 0; col < side / 8; col++) {
			unsigned tile = (selector + row * 2 + col) & 0xFF;
			if (used) used[tile] = true;
			clocks += tileClocks(tile, low);
		}
	}
	lcdEvent(events, 1, control | 0x40);
	for (unsigned plane = 0; plane < 2; plane++)
		for (unsigned col = 0; col < columns; col++)
			for (unsigned row = 0; row < side; row++) {
				lcdEvent(events, 5, (uint8_t) (y + row));
				lcdEvent(events, 4, (uint8_t) (((x >> 3) + col) | (plane << 7)));
			}
	for (unsigned col = 0; col < side / 8; col++) lcdEvent(events, 1, control | 0x40);
	for (unsigned plane = 0; plane < 2; plane++)
		for (unsigned col = 0; col < side / 8; col++)
			for (unsigned row = 0; row < side / 8; row++)
				tileReads(rom, base[plane], (selector + row * 2 + col) & 0xFF);
	uint8_t output[2][3][16];
	for (unsigned row = 0; row < side; row++) {
		uint32_t source[2] = { 0, 0 };
		for (unsigned plane = 0; plane < 2; plane++) {
			for (unsigned col = 0; col < side / 8; col++) {
				unsigned tile = (selector + (row / 8) * 2 + col) & 0xFF;
				source[plane] = (source[plane] << 8) |
					graphics[plane][tile * 8 + row % 8];
			}
			source[plane] = (source[plane] << 8) >> (x & 7);
		}
		for (unsigned col = 0; col < columns; col++) {
			unsigned shift = 8 * (columns - 1 - col);
			uint8_t pixel[2] = { (uint8_t) (source[0] >> shift),
				(uint8_t) (source[1] >> shift) };
			uint8_t mask = (uint8_t) ~(pixel[0] | pixel[1]);
			unsigned address = (((y + row) & 0xFF) << 5) | (((x >> 3) + col) & 31);
			for (unsigned plane = 0; plane < 2; plane++) {
				unsigned offset = 2 * address + plane;
				unsigned buffer = 48 * plane + side * col + row;
				if (scratch) {
					scratch[buffer] = vram[offset];
					scratchValid[buffer] = scratchValid[0x60 + buffer] = 1;
				}
				vram[offset] = pixel[plane] | (vram[offset] & mask);
				if (scratch) scratch[0x60 + buffer] = vram[offset];
				output[plane][col][row] = vram[offset];
			}
		}
	}
	for (unsigned plane = 0; plane < 2; plane++) {
		for (unsigned col = 0; col < columns; col++) {
			lcdEvent(events, 5, (uint8_t) y);
			lcdEvent(events, 4, (uint8_t) (((x >> 3) + col) | (plane << 7)));
			for (unsigned row = 0; row < side; row++) lcdEvent(events, 7, output[plane][col][row]);
		}
	}
	return clocks;
}

static bool combinedCase(struct Gamate* g, const struct Gamate* base,
		const uint8_t* const graphics[2], unsigned seed) {
	*g = *base;
	static const unsigned lows[] = { 0, 1, 0x7F, 0xF8, 0xFF };
	unsigned low[2] = { lows[seed % 5], lows[(seed / 5) % 5] };
	unsigned bases[2] = { 0x8000 + low[0], 0x9000 + low[1] };
	const uint8_t* sources[2] = { graphics[0] + low[0], graphics[1] + low[1] };
	g->ram[6] = (uint8_t) low[0];
	g->ram[7] = 0x80;
	g->ram[8] = (uint8_t) low[1];
	g->ram[9] = 0x90;
	g->ram[0x15] = (uint8_t) seed;
	seedScratch(g, seed + 71);
	for (unsigned i = 0; i < sizeof(g->vram); i++) g->vram[i] = pattern(seed + 11, i);
	for (unsigned record = 0; record < 16; record++) {
		unsigned entry = 0x21 + 8 * record;
		for (unsigned field = 0; field < 8; field++)
			g->ram[entry + field] = pattern(seed + 31, 8 * record + field);
		bool enabled = (record + seed) % 4 == 0;
		g->ram[entry + 2] = enabled ? (uint8_t) ((seed * 7 + record * 31) % 255) : 0xFF;
		g->ram[entry + 7] = enabled ? (uint8_t) ((seed + record) % 255) : 0xFF;
		if (seed & 1) {
			g->ram[entry] = g->ram[entry + 5] = 0xF8 + (seed & 7);
			g->ram[entry + 1] = g->ram[entry + 6] = 0xF8;
		}
	}
	uint8_t expected[sizeof(g->vram)];
	memcpy(expected, g->vram, sizeof(expected));
	struct LcdEvents events = { 0 };
	struct RomEvents rom = { 0 };
	uint8_t scratch[0xC0], scratchValid[0xC0] = { 0 };
	memcpy(scratch, g->ram + 0x308, sizeof(scratch));
	struct ZeroPageScratch zeroPage;
	initZeroPageScratch(&zeroPage, g);
	unsigned reads = 0, writes = 0, clocks = 6326, stackWrites = 9;
	for (unsigned record = 0; record < 16; record++) {
		unsigned entry = 0x21 + 8 * record;
		if (g->ram[entry + 7] != 0xFF) {
			if (stackWrites < 11) stackWrites = 11;
			unsigned tiles = record < 8 ? 3 : 2;
			unsigned selector = finalBackgroundSelector(expected, record,
				g->ram[entry + 5], g->ram[entry + 6]);
			modelZeroPageScratch(&zeroPage, true, record, g->ram[entry + 5],
				g->ram[entry + 6], selector, bases[1]);
			reads += tiles * tiles;
			writes += tiles * tiles * 16;
			clocks += modelBackground(expected, sources, bases, NULL, &events, &rom,
				scratch, scratchValid,
				g->ram[0x15], record,
				g->ram[entry + 5], g->ram[entry + 6]);
		}
	}
	for (unsigned record = 0; record < 16; record++) {
		unsigned entry = 0x21 + 8 * record;
		if (g->ram[entry + 2] != 0xFF) {
			unsigned depth = record < 8 ? 13 : 18;
			if (stackWrites < depth) stackWrites = depth;
			unsigned side = record < 8 ? 16 : 8;
			unsigned count = side * (side / 8 + 1) * 2;
			reads += count;
			writes += count;
			modelZeroPageScratch(&zeroPage, false, record, g->ram[entry], g->ram[entry + 1],
				g->ram[entry + 2], bases[1]);
			clocks += modelSprite(expected, sources, bases, NULL, &events, &rom,
				scratch, scratchValid,
				g->ram[0x15], record,
				g->ram[entry], g->ram[entry + 1],
				g->ram[entry + 2]);
		}
	}
	uint64_t start = g->masterClocks;
	gamateResetRamDiagnostics(g);
	bool returned = invoke(g, 0x64, &events, &rom);
	bool pixels = !memcmp(g->vram, expected, sizeof(expected));
	bool accesses = g->diagnostics.vramTrackedWrites == writes && g->diagnostics.vramReads == reads;
	bool timing = g->masterClocks - start == clocks;
	bool scratchMatches = stackWritesMatch(g, 0xFD, stackWrites);
	scratchMatches &= zeroPageScratchMatches(&zeroPage, g, true);
	for (unsigned i = 0; i < sizeof(scratch); i++) {
		unsigned address = 0x308 + i;
		bool written = (g->diagnostics.ramWritten[address >> 3] & (1u << (address & 7))) != 0;
		if (g->ram[address] != scratch[i] || written != (scratchValid[i] != 0))
			scratchMatches = false;
	}
	if (!returned || !pixels || !accesses || !timing || !scratchMatches ||
			!lcdMatches(&events) || !romMatches(&rom)) {
		fprintf(stderr, "combined seed=%u returned=%u pixels=%u accesses=%u scratch=%u timing=%lld "
			"lcd=%u/%u/%u rom=%u/%u/%u reads=%llu/%u writes=%llu/%u\n",
			seed, returned, pixels, accesses, scratchMatches,
			(long long) (g->masterClocks - start) - clocks,
			events.observed, events.count, events.mismatches,
			rom.observed, rom.count, rom.mismatches,
			(unsigned long long) g->diagnostics.vramReads, reads,
			(unsigned long long) g->diagnostics.vramTrackedWrites, writes);
		return false;
	}
	return true;
}

static bool selectorCase(struct Gamate* g, const struct Gamate* base,
		const uint8_t* const graphics[2], unsigned record, unsigned selector, bool background,
		bool disabled) {
	*g = *base;
	memset(g->ram + 0x21, 0xFF, 0x80);
	static const unsigned lows[] = { 0, 1, 0x7F, 0xF8, 0xFF };
	unsigned low[2] = { lows[selector % 5], lows[(selector / 5) % 5] };
	unsigned bases[2] = { 0x8000 + low[0], 0x9000 + low[1] };
	const uint8_t* sources[2] = { graphics[0] + low[0], graphics[1] + low[1] };
	g->ram[6] = (uint8_t) low[0];
	g->ram[7] = 0x80;
	g->ram[8] = (uint8_t) low[1];
	g->ram[9] = 0x90;
	g->ram[0x15] = (uint8_t) selector;
	seedScratch(g, selector + record * 256 + (background ? 4096 : 0));
	g->cpu.a = (uint8_t) selector;
	g->cpu.x = (uint8_t) (selector * 3);
	g->cpu.y = (uint8_t) (selector * 5);
	for (unsigned i = 0; i < sizeof(g->vram); i++)
		g->vram[i] = background ? (uint8_t) selector : pattern(selector + 11, i);
	unsigned entry = 0x21 + 8 * record;
	unsigned x = (selector & 7) | (selector & 8 ? 0xF8 : 0);
	unsigned y = selector & 16 ? 0xF8 : 0x20;
	g->ram[entry] = g->ram[entry + 5] = (uint8_t) x;
	g->ram[entry + 1] = g->ram[entry + 6] = (uint8_t) y;
	if (background) g->ram[entry + 7] = disabled ? 0xFF : (uint8_t) (selector % 255);
	else g->ram[entry + 2] = (uint8_t) selector;
	uint8_t expected[sizeof(g->vram)];
	memcpy(expected, g->vram, sizeof(expected));
	struct LcdEvents events = { 0 };
	struct RomEvents rom = { 0 };
	uint8_t scratchBytes[0xC0], scratchValid[0xC0] = { 0 };
	memcpy(scratchBytes, g->ram + 0x308, sizeof(scratchBytes));
	struct ZeroPageScratch zeroPage;
	initZeroPageScratch(&zeroPage, g);
	unsigned reads = 0, writes = 0, clocks = 6326;
	if (background && !disabled) {
		unsigned tiles = record < 8 ? 3 : 2;
		reads = tiles * tiles;
		writes = reads * 16;
		clocks += modelBackground(expected, sources, bases, NULL, &events, &rom,
			scratchBytes, scratchValid, g->ram[0x15],
			record, x, y);
		modelZeroPageScratch(&zeroPage, true, record, x, y, selector, bases[1]);
	} else if (!background && selector != 0xFF) {
		unsigned side = record < 8 ? 16 : 8;
		reads = writes = side * (side / 8 + 1) * 2;
		clocks += modelSprite(expected, sources, bases, NULL, &events, &rom,
			scratchBytes, scratchValid, g->ram[0x15],
			record, x, y, selector);
		modelZeroPageScratch(&zeroPage, false, record, x, y, selector, bases[1]);
	}
	uint64_t start = g->masterClocks;
	gamateResetRamDiagnostics(g);
	bool returned = invoke(g, (uint8_t) ((selector & 0xD7) | 0x20), &events, &rom);
	bool pixels = !memcmp(g->vram, expected, sizeof(expected));
	bool accesses = g->diagnostics.vramTrackedWrites == writes && g->diagnostics.vramReads == reads;
	bool timing = g->masterClocks - start == clocks;
	bool scratch = zeroPageScratchMatches(&zeroPage, g, true);
	for (unsigned i = 0; i < sizeof(scratchBytes); i++) {
		unsigned address = 0x308 + i;
		bool written = (g->diagnostics.ramWritten[address >> 3] & (1u << (address & 7))) != 0;
		if (g->ram[address] != scratchBytes[i] || written != (scratchValid[i] != 0))
			scratch = false;
	}
	if (!returned || !pixels || !accesses || !timing || !scratch ||
			!lcdMatches(&events) || !romMatches(&rom)) {
		fprintf(stderr, "selector record=%u selector=%02X background=%u disabled=%u "
			"returned=%u pixels=%u accesses=%u scratch=%u p=%02X timing=%lld "
			"lcd=%u/%u/%u rom=%u/%u/%u\n",
			record, selector, background,
			disabled, returned, pixels, accesses, scratch, g->cpu.p,
			(long long) (g->masterClocks - start) - clocks,
			events.observed, events.count, events.mismatches, rom.observed, rom.count, rom.mismatches);
		return false;
	}
	return true;
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

static bool sourcesAvailable(bool valid[2][256], const bool used[256], const unsigned bases[2]) {
	for (unsigned selector = 0; selector < 256; selector++) {
		if (used[selector] && (!valid[0][selector] || !valid[1][selector])) {
			unsigned plane = valid[0][selector] ? 1 : 0;
			fprintf(stderr, "source skip: tile crosses cartridge boundary at $%04X\n",
				(uint16_t) (bases[plane] + selector * 8 + 7));
			return false;
		}
	}
	return true;
}

static int replay(struct Gamate* g, const struct Gamate* entry, bool biosSources,
		bool* usedBios, bool* outputDependsOnBios) {
	*g = *entry;
	if (g->cpu.p & 8) return 2;
	if (g->nmi) return 3;
	uint8_t tiles[2][0x800] = { { 0 } };
	bool valid[2][256] = { { false } }, used[256] = { false };
	const uint8_t* graphics[2] = { tiles[0], tiles[1] };
	bool active = false;
	for (unsigned record = 0; record < 16; record++) {
		unsigned address = 0x21 + 8 * record;
		active |= g->ram[address + 2] != 0xFF || g->ram[address + 7] != 0xFF;
	}
	if (active) {
		for (unsigned plane = 0; plane < 2; plane++) {
			unsigned address = 6 + 2 * plane;
			unsigned base = g->ram[address] | (unsigned) g->ram[address + 1] << 8;
			if (base < 0x6000 || base >= 0xE000) {
				fprintf(stderr, "source skip: base outside cartridge window ($%04X)\n", base);
				return 4;
			}
			for (unsigned selector = 0; selector < 256; selector++) {
				unsigned address = base + selector * 8;
				if (address + 7 >= (biosSources ? 0x10000u : 0xE000u)) continue;
				valid[plane][selector] = true;
				for (unsigned row = 0; row < 8; row++) {
					unsigned source = address + row;
					tiles[plane][selector * 8 + row] = source >= 0xE000 ?
						g->bios[source & 0xFFF] :
						g->rom[cartridgeOffset(g, (uint16_t) source)];
				}
			}
		}
	}
	uint8_t expected[sizeof(g->vram)];
	memcpy(expected, g->vram, sizeof(expected));
	struct LcdEvents events = { 0 };
	struct RomEvents rom = { 0 };
	uint8_t scratch[0xC0], scratchValid[0xC0] = { 0 };
	memcpy(scratch, g->ram + 0x308, sizeof(scratch));
	struct ZeroPageScratch zeroPage;
	initZeroPageScratch(&zeroPage, g);
	unsigned reads = 0, writes = 0, clocks = 6326, stackWrites = 9;
	unsigned largeRedraws = 0, smallRedraws = 0, largeObjects = 0, smallObjects = 0;
	unsigned bases[2] = { g->ram[6] | (unsigned) g->ram[7] << 8,
		g->ram[8] | (unsigned) g->ram[9] << 8 };
	for (unsigned record = 0; record < 16; record++) {
		unsigned address = 0x21 + 8 * record;
		if (g->ram[address + 7] != 0xFF) {
			if (record < 8) largeRedraws++;
			else smallRedraws++;
			if (stackWrites < 11) stackWrites = 11;
			unsigned count = record < 8 ? 9 : 4;
			unsigned selector = finalBackgroundSelector(expected, record,
				g->ram[address + 5], g->ram[address + 6]);
			modelZeroPageScratch(&zeroPage, true, record, g->ram[address + 5],
				g->ram[address + 6], selector, bases[1]);
			reads += count;
			writes += count * 16;
			clocks += modelBackground(expected, graphics, bases, used, &events, &rom,
				scratch, scratchValid,
				g->ram[0x15], record,
				g->ram[address + 5], g->ram[address + 6]);
			if (!sourcesAvailable(valid, used, bases)) return 4;
		}
	}
	for (unsigned record = 0; record < 16; record++) {
		unsigned address = 0x21 + 8 * record;
		if (g->ram[address + 2] != 0xFF) {
			if (record < 8) largeObjects++;
			else smallObjects++;
			unsigned depth = record < 8 ? 13 : 18;
			if (stackWrites < depth) stackWrites = depth;
			unsigned side = record < 8 ? 16 : 8;
			unsigned count = side * (side / 8 + 1) * 2;
			reads += count;
			writes += count;
			modelZeroPageScratch(&zeroPage, false, record, g->ram[address],
				g->ram[address + 1], g->ram[address + 2], bases[1]);
			clocks += modelSprite(expected, graphics, bases, used, &events, &rom,
				scratch, scratchValid,
				g->ram[0x15], record,
				g->ram[address], g->ram[address + 1],
				g->ram[address + 2]);
			if (!sourcesAvailable(valid, used, bases)) return 4;
		}
	}
	for (unsigned selector = 0; selector < 256; selector++)
		for (unsigned plane = 0; plane < 2; plane++)
			if (used[selector] && bases[plane] + 8 * selector + 7 >= 0xE000)
				*usedBios = true;
	if (*usedBios) {
		uint8_t alternateTiles[2][0x800];
		memcpy(alternateTiles, tiles, sizeof(alternateTiles));
		for (unsigned plane = 0; plane < 2; plane++)
			for (unsigned offset = 0; offset < 0x800; offset++)
				if (bases[plane] + offset >= 0xE000) alternateTiles[plane][offset] = 0;
		const uint8_t* alternate[2] = { alternateTiles[0], alternateTiles[1] };
		uint8_t alternateVram[sizeof(g->vram)];
		memcpy(alternateVram, g->vram, sizeof(alternateVram));
		for (unsigned record = 0; record < 16; record++) {
			unsigned address = 0x21 + 8 * record;
			if (g->ram[address + 7] != 0xFF)
				modelBackground(alternateVram, alternate, bases, NULL, NULL, NULL, NULL, NULL,
					g->ram[0x15], record, g->ram[address + 5], g->ram[address + 6]);
		}
		for (unsigned record = 0; record < 16; record++) {
			unsigned address = 0x21 + 8 * record;
			if (g->ram[address + 2] != 0xFF)
				modelSprite(alternateVram, alternate, bases, NULL, NULL, NULL, NULL, NULL,
					g->ram[0x15], record, g->ram[address], g->ram[address + 1],
					g->ram[address + 2]);
		}
		*outputDependsOnBios = memcmp(alternateVram, expected, sizeof(expected)) != 0;
	}
	uint8_t a = g->cpu.a, x = g->cpu.x, y = g->cpu.y, p = g->cpu.p;
	uint8_t s = g->cpu.s;
	uint16_t stacked = (uint16_t) (g->ram[0x100 | (uint8_t) (s + 1)] |
		g->ram[0x100 | (uint8_t) (s + 2)] << 8);
	if (stacked >= 0x6000 && stacked < 0xE000) romEvent(&rom, stacked);
	g->cpu.p |= 4;
	g->cpu.irqMask = true;
	g->irq = false;
	gamateResetVramDiagnostics(g);
	gamateResetRamDiagnostics(g);
	gamateResetRomDiagnostics(g);
	gamateSetLcdDiagnosticWatch(g, checkLcdEvent, &events);
	gamateSetRomDiagnosticWatch(g, checkRomEvent, &rom);
	uint64_t start = g->masterClocks, nmis = g->diagnostics.nmisTaken;
	bool returned = false;
	while (g->masterClocks - start < 3000000) {
		unsigned cycles = gamateCpuStep(g);
		gamateRunClocks(g, cycles * 2);
		g->irq = false;
		if (g->cpu.pc == (uint16_t) (stacked + 1) && g->cpu.s == (uint8_t) (s + 2)) {
			returned = true;
			break;
		}
	}
	gamateSetLcdDiagnosticWatch(g, NULL, NULL);
	gamateSetRomDiagnosticWatch(g, NULL, NULL);
	if (g->diagnostics.nmisTaken != nmis) return 3;
	bool pixels = !memcmp(g->vram, expected, sizeof(expected));
	bool accesses = g->diagnostics.vramTrackedWrites == writes && g->diagnostics.vramReads == reads;
	uint8_t expectedP = (uint8_t) ((p & 0x18) | 0x25 | (a & 0x80) | (a == 0 ? 2 : 0));
	bool registers = g->cpu.a == a && g->cpu.x == x && g->cpu.y == y && g->cpu.p == expectedP;
	bool timing = g->masterClocks - start == clocks;
	bool stackMatches = stackWritesMatch(g, s, stackWrites);
	bool recordMatches = !memcmp(g->ram + 0x300, entry->ram + 0x99, 8);
	bool zeroPageMatches = zeroPageScratchMatches(&zeroPage, g, true);
	bool scratchMatches = true, unrelatedMatches = true;
	bool ram = stackMatches && recordMatches && zeroPageMatches;
	for (unsigned i = 0; i < sizeof(scratch); i++) {
		unsigned address = 0x308 + i;
		bool written = (g->diagnostics.ramWritten[address >> 3] & (1u << (address & 7))) != 0;
		if (g->ram[address] != scratch[i] || written != (scratchValid[i] != 0))
			ram = scratchMatches = false;
	}
	for (unsigned i = 0; i < sizeof(g->ram); i++) {
		bool scratch = i <= 3 || (i >= 0x18 && i <= 0x1F) ||
			(i >= 0x100 && i < 0x200) || (i >= 0x300 && i <= 0x3C7);
		if (!scratch && ((g->diagnostics.ramWritten[i >> 3] & (1u << (i & 7))) ||
				g->ram[i] != entry->ram[i])) ram = unrelatedMatches = false;
	}
	bool peripherals = g->scrollX == entry->scrollX && g->scrollY == entry->scrollY &&
		g->bank == entry->bank && g->firstBank == entry->firstBank &&
		g->ayAddress == entry->ayAddress &&
		!memcmp(g->ayRegisters, entry->ayRegisters, sizeof(g->ayRegisters)) &&
		g->diagnostics.ayWrites == entry->diagnostics.ayWrites &&
		g->diagnostics.bankWrites == entry->diagnostics.bankWrites &&
		g->diagnostics.uartClocks == entry->diagnostics.uartClocks &&
		lcdStateMatches(entry, g, &events);
	if (!returned || !pixels || !accesses || !registers || !timing || !lcdMatches(&events) ||
			!romMatches(&rom) ||
			!ram || !peripherals) {
		fprintf(stderr, "replay returned=%u pixels=%u accesses=%u registers=%u "
			"ram=%u peripherals=%u reads=%llu/%u writes=%llu/%u p=%02X/%02X "
			"timing=%lld lcd=%u/%u/%u rom=%u/%u/%u\n",
			returned, pixels, accesses, registers, ram, peripherals,
			(unsigned long long) g->diagnostics.vramReads, reads,
			(unsigned long long) g->diagnostics.vramTrackedWrites, writes, g->cpu.p, expectedP,
			(long long) (g->masterClocks - start) - clocks,
			events.observed, events.count, events.mismatches, rom.observed, rom.count, rom.mismatches);
		if (getenv("GAMATEBIOSF027_VERBOSE"))
			fprintf(stderr, "shape redraw=%u/%u object=%u/%u ram stack=%u record=%u "
				"zero-page=%u scratch=%u unrelated=%u\n",
				largeRedraws, smallRedraws, largeObjects, smallObjects,
				stackMatches, recordMatches, zeroPageMatches, scratchMatches, unrelatedMatches);
		return 1;
	}
	return 0;
}

static int corpus(struct Gamate* g, struct Gamate* copy, bool biosSources) {
	unsigned calls = 0, checked = 0, decimal = 0, nmi = 0, source = 0, failures = 0;
	unsigned biosReads = 0, outputChanges = 0;
	const char* press = getenv("GAMATEBIOSCALLS_PRESS");
	for (unsigned frame = 1; frame <= 1000; frame++) {
		g->buttons = gamateBiosButtonsForFrame(frame, press);
		g->frameDone = false;
		while (!g->frameDone) {
			unsigned cycles = gamateCpuStep(g);
			gamateRunClocks(g, cycles * 2);
			if (g->cpu.pc != 0xF027) continue;
			calls++;
			bool usedBios = false, outputDependsOnBios = false;
			int result = replay(copy, g, biosSources, &usedBios, &outputDependsOnBios);
			if (result == 2) decimal++;
			else if (result == 3) nmi++;
			else if (result == 4) source++;
			else {
				checked++;
				failures += result != 0;
				if (!result) {
					biosReads += usedBios;
					outputChanges += outputDependsOnBios;
				}
			}
		}
	}
	printf("calls=%u checked=%u decimal_skips=%u nmi_skips=%u source_skips=%u failures=%u\n",
		calls, checked, decimal, nmi, source, failures);
	if (biosSources) printf("bios_source_calls=%u output_changes_against_zero=%u\n",
		biosReads, outputChanges);
	return failures != 0;
}

static bool runOriginalCall(struct Gamate* g, bool maskIrq, uint64_t* elapsed,
		unsigned* irqs, unsigned* nmis) {
	uint8_t s = g->cpu.s;
	uint16_t ret = (uint16_t) (1 + g->ram[0x100 | (uint8_t) (s + 1)] +
		((unsigned) g->ram[0x100 | (uint8_t) (s + 2)] << 8));
	uint64_t start = g->masterClocks, startIrqs = g->diagnostics.irqsTaken;
	uint64_t startNmis = g->diagnostics.nmisTaken;
	if (maskIrq) {
		g->cpu.p |= 4;
		g->cpu.irqMask = true;
		g->irq = false;
	}
	while (g->masterClocks - start < 3000000) {
		unsigned cycles = gamateCpuStep(g);
		gamateRunClocks(g, cycles * 2);
		if (maskIrq) g->irq = false;
		if (g->cpu.pc == ret && g->cpu.s == (uint8_t) (s + 2)) {
			*elapsed = g->masterClocks - start;
			*irqs = (unsigned) (g->diagnostics.irqsTaken - startIrqs);
			*nmis = (unsigned) (g->diagnostics.nmisTaken - startNmis);
			return true;
		}
	}
	return false;
}

static bool lcdRegistersDiffer(const struct Gamate* a, const struct Gamate* b) {
	return a->vramAddress != b->vramAddress || a->vramPlane != b->vramPlane ||
		a->incrementRow != b->incrementRow || a->displayBlank != b->displayBlank ||
		a->window != b->window || a->swapPlanes != b->swapPlanes ||
		a->displayStopped != b->displayStopped;
}

enum CallDifference {
	DIFF_VRAM = 1u << 0, DIFF_SCRATCH = 1u << 1, DIFF_INPUTS = 1u << 2,
	DIFF_REGISTERS = 1u << 3, DIFF_LCD = 1u << 4, DIFF_MAPPER = 1u << 5,
	DIFF_OTHER_RAM = 1u << 6
};

static unsigned callDifferences(const struct Gamate* a, const struct Gamate* b) {
	unsigned differences = 0;
	if (memcmp(a->vram, b->vram, sizeof(a->vram))) differences |= DIFF_VRAM;
	for (unsigned address = 0; address < sizeof(a->ram); address++) {
		if ((address >= 0x100 && address < 0x200) || a->ram[address] == b->ram[address])
			continue;
		if (address <= 3 || (address >= 0x18 && address <= 0x1F) ||
				(address >= 0x300 && address <= 0x3C7)) differences |= DIFF_SCRATCH;
		else if ((address >= 6 && address <= 9) || address == 0x15 ||
				(address >= 0x21 && address <= 0xA0)) differences |= DIFF_INPUTS;
		else differences |= DIFF_OTHER_RAM;
	}
	if (a->cpu.a != b->cpu.a || a->cpu.x != b->cpu.x || a->cpu.y != b->cpu.y ||
			a->cpu.s != b->cpu.s || ((a->cpu.p ^ b->cpu.p) & (uint8_t) ~4))
		differences |= DIFF_REGISTERS;
	if (lcdRegistersDiffer(a, b)) differences |= DIFF_LCD;
	if (a->bank != b->bank || a->firstBank != b->firstBank) differences |= DIFF_MAPPER;
	return differences;
}

static bool callDifferencesSelfTest(const struct Gamate* base, struct Gamate* a, struct Gamate* b) {
	*a = *base;
	*b = *base;
	b->cpu.p ^= 4;
	b->ram[0x100] ^= 1;
	if (callDifferences(a, b)) return false;
	b->vram[0] ^= 1;
	if (callDifferences(a, b) != DIFF_VRAM) return false;
	b->vram[0] ^= 1;
	b->ram[0x308] ^= 1;
	if (callDifferences(a, b) != DIFF_SCRATCH) return false;
	b->ram[0x308] ^= 1;
	b->ram[6] ^= 1;
	if (callDifferences(a, b) != DIFF_INPUTS) return false;
	b->ram[6] ^= 1;
	b->ram[4] ^= 1;
	if (callDifferences(a, b) != DIFF_OTHER_RAM) return false;
	b->ram[4] ^= 1;
	b->cpu.a ^= 1;
	if (callDifferences(a, b) != DIFF_REGISTERS) return false;
	b->cpu.a ^= 1;
	b->vramAddress ^= 1;
	if (callDifferences(a, b) != DIFF_LCD) return false;
	b->vramAddress ^= 1;
	b->bank ^= 1;
	return callDifferences(a, b) == DIFF_MAPPER;
}

static int interruptCompare(struct Gamate* g, struct Gamate* masked, struct Gamate* original) {
	unsigned calls = 0, compared = 0, pendingNmi = 0, decimal = 0, timeouts = 0;
	unsigned interrupted = 0, irqOnly = 0, nmi = 0, quietMismatch = 0;
	unsigned vram = 0, scratch = 0, inputs = 0, registers = 0, lcd = 0;
	unsigned cartridgeState = 0, otherRam = 0;
	const char* press = getenv("GAMATEBIOSCALLS_PRESS");
	for (unsigned frame = 1; frame <= 1000; frame++) {
		g->buttons = gamateBiosButtonsForFrame(frame, press);
		g->frameDone = false;
		while (!g->frameDone) {
			unsigned cycles = gamateCpuStep(g);
			gamateRunClocks(g, cycles * 2);
			if (g->cpu.pc != 0xF027) continue;
			calls++;
			if (g->cpu.p & 8) { decimal++; continue; }
			if (g->nmi) { pendingNmi++; continue; }
			*masked = *g;
			*original = *g;
			uint64_t maskedClocks, originalClocks;
			unsigned maskedIrqs, maskedNmis, originalIrqs, originalNmis;
			bool returnedMasked = runOriginalCall(masked, true, &maskedClocks,
				&maskedIrqs, &maskedNmis);
			bool returnedOriginal = runOriginalCall(original, false, &originalClocks,
				&originalIrqs, &originalNmis);
			if (!returnedMasked || !returnedOriginal) {
				fprintf(stderr, "interrupt comparison timed out: masked=%u original=%u\n",
					returnedMasked, returnedOriginal);
				timeouts++;
				continue;
			}
			compared++;
			bool hadInterrupt = originalIrqs || originalNmis || maskedIrqs || maskedNmis;
			unsigned differences = callDifferences(masked, original);
			if (!hadInterrupt) {
				if (differences || maskedClocks != originalClocks)
					quietMismatch++;
				continue;
			}
			interrupted++;
			irqOnly += originalIrqs && !originalNmis && !maskedNmis;
			nmi += originalNmis || maskedNmis;
			vram += (differences & DIFF_VRAM) != 0;
			scratch += (differences & DIFF_SCRATCH) != 0;
			inputs += (differences & DIFF_INPUTS) != 0;
			registers += (differences & DIFF_REGISTERS) != 0;
			lcd += (differences & DIFF_LCD) != 0;
			cartridgeState += (differences & DIFF_MAPPER) != 0;
			otherRam += (differences & DIFF_OTHER_RAM) != 0;
		}
	}
	printf("calls=%u compared=%u decimal_skips=%u pending_nmi_skips=%u timeouts=%u "
		"interrupted=%u irq_only=%u nmi=%u quiet_mismatch=%u\n",
		calls, compared, decimal, pendingNmi, timeouts, interrupted, irqOnly, nmi, quietMismatch);
	printf("interrupted_differences: vram=%u scratch=%u inputs=%u registers=%u lcd=%u "
		"mapper=%u other_ram=%u\n", vram, scratch, inputs, registers, lcd, cartridgeState, otherRam);
	return timeouts != 0 || quietMismatch != 0;
}

static bool serviceScratch(unsigned address) {
	return address <= 3 || (address >= 0x18 && address <= 0x1F) ||
		(address >= 0x300 && address <= 0x3C7);
}

static void recordScratchUse(const struct Gamate* g, const uint8_t* written,
		unsigned* reads) {
	for (unsigned address = 0; address < 0x400; address++) {
		unsigned mask = 1u << (address & 7), index = address >> 3;
		if (serviceScratch(address) && (written[index] & mask) &&
				(g->diagnostics.ramReadBeforeWriteMap[0][index] & mask))
			reads[address]++;
	}
}

static void classifyScratchReads(const struct Gamate* g, uint8_t* seen,
		uint8_t* separate, uint8_t* sameInstruction) {
	for (unsigned i = 0; i < sizeof(g->diagnostics.ramWritten); i++) {
		uint8_t newReads = g->diagnostics.ramReadBeforeWriteMap[0][i] & (uint8_t) ~seen[i];
		separate[i] |= newReads & (uint8_t) ~g->diagnostics.ramWritten[i];
		sameInstruction[i] |= newReads & g->diagnostics.ramWritten[i];
		seen[i] |= newReads;
	}
}

static void countScratchSameInstruction(const struct Gamate* g, const uint8_t* written,
		const uint8_t* seen, const uint8_t* lastBios, const uint8_t* lastCartridge,
		const uint8_t* lastInterrupt, const uint8_t* matchesOutput, uint16_t pc,
		unsigned* stores, unsigned* rmwReads, unsigned* rmwOutputMatches,
		unsigned* rmwBios, unsigned* rmwCartridge, unsigned* rmwInterrupt, unsigned* other) {
	if (pc < 0x6000 || pc >= 0xE000) return;
	uint8_t opcode = g->rom[cartridgeOffset(g, pc)];
	bool indexedStore = opcode == 0x91 || opcode == 0x99 || opcode == 0x9D;
	unsigned operation = opcode >> 5, mode = (opcode >> 2) & 7;
	bool rmw = (opcode & 3) == 2 && operation != 4 && operation != 5 &&
		(mode == 3 || mode == 7);
	for (unsigned i = 0; i < sizeof(g->diagnostics.ramWritten); i++) {
		uint8_t candidates = written[i] & (uint8_t) ~seen[i] &
			g->diagnostics.ramReadBeforeWriteMap[0][i] & g->diagnostics.ramWritten[i];
		for (unsigned bit = 0; candidates && bit < 8; bit++) {
			unsigned address = 8 * i + bit;
			if ((candidates & (1u << bit)) && serviceScratch(address)) {
				if (indexedStore) (*stores)++;
				else if (rmw) {
					rmwReads[address]++;
					if (lastBios[i] & (1u << bit)) rmwBios[address]++;
					if (lastCartridge[i] & (1u << bit)) rmwCartridge[address]++;
					if (lastInterrupt[i] & (1u << bit)) rmwInterrupt[address]++;
					if (matchesOutput[i] & (1u << bit)) rmwOutputMatches[address]++;
				}
				else (*other)++;
			}
		}
	}
}

static void printScratchReads(const char* label, const unsigned* reads) {
	printf("%s:", label);
	for (unsigned address = 0; address < 0x400; address++)
		if (reads[address]) printf(" %03X:%u", address, reads[address]);
	putchar('\n');
}

static void recordClassifiedScratchUse(const uint8_t* written, const uint8_t* separate,
		const uint8_t* sameInstruction, unsigned* separateReads, unsigned* sameInstructionReads) {
	for (unsigned address = 0; address < 0x400; address++) {
		unsigned bit = 1u << (address & 7), index = address >> 3;
		if (serviceScratch(address) && (written[index] & bit)) {
			separateReads[address] += (separate[index] & bit) != 0;
			sameInstructionReads[address] += (sameInstruction[index] & bit) != 0;
		}
	}
}

static void recordCallWrites(struct Gamate* g, uint16_t pc, uint8_t* written,
		uint8_t* lastBios, uint8_t* lastCartridge, uint8_t* lastInterrupt) {
	unsigned owner = pc >= 0x6000 && pc < 0xE000 ? 0 : pc >= 0xE000 ? 1 : 2;
	for (unsigned i = 0; i < sizeof(g->diagnostics.ramWritten); i++) {
		uint8_t bits = g->diagnostics.ramWritten[i];
		written[i] |= bits;
		lastBios[i] = (lastBios[i] & (uint8_t) ~bits) | (owner == 1 ? bits : 0);
		lastCartridge[i] = (lastCartridge[i] & (uint8_t) ~bits) | (owner == 0 ? bits : 0);
		lastInterrupt[i] = (lastInterrupt[i] & (uint8_t) ~bits) |
			(g->diagnostics.diagnosticInterrupt ? bits : 0);
		g->diagnostics.ramWritten[i] = 0;
	}
}

static bool scratchUseSelfTest(struct Gamate* g, const struct Gamate* base) {
	*g = *base;
	gamateResetRamDiagnostics(g);
	uint8_t written[sizeof(g->diagnostics.ramWritten)] = { 0 };
	written[0x18 >> 3] |= (1u << (0x18 & 7)) | (1u << (0x19 & 7)) | (1u << (0x1A & 7));
	g->diagnostics.diagnosticInstructionPc = 0x6020;
	(void) gamateRead(g, 0x18);
	uint8_t seen[sizeof(written)] = { 0 }, separate[sizeof(written)] = { 0 };
	uint8_t sameInstruction[sizeof(written)] = { 0 };
	classifyScratchReads(g, seen, separate, sameInstruction);
	gamateWrite(g, 0x19, 0);
	(void) gamateRead(g, 0x19);
	g->diagnostics.diagnosticInstructionPc = 0xF027;
	(void) gamateRead(g, 0x1A);
	g->diagnostics.diagnosticInstructionPc = 0x6020;
	(void) gamateRead(g, 0x1B);
	gamateWrite(g, 0x1B, 0);
	classifyScratchReads(g, seen, separate, sameInstruction);
	unsigned reads[0x400] = { 0 };
	recordScratchUse(g, written, reads);
	for (unsigned address = 0; address < 0x400; address++)
		if (reads[address] != (unsigned) (address == 0x18)) return false;
	if (separate[0x18 >> 3] != (1u << (0x18 & 7)) ||
			sameInstruction[0x1B >> 3] != (1u << (0x1B & 7))) return false;

	uint8_t program[] = { 0x9D, 0x18, 0x03, 0xFE, 0x1A, 0x03 };
	*g = *base;
	g->rom = program;
	g->romSize = sizeof(program);
	g->mapper = GAMATE_MAPPER_PLAIN;
	g->protectionPassed = true;
	g->irq = g->nmi = false;
	g->cpu.pc = 0x6000;
	g->cpu.x = 0;
	gamateResetRamDiagnostics(g);
	memset(written, 0, sizeof(written));
	memset(seen, 0, sizeof(seen));
	memset(separate, 0, sizeof(separate));
	memset(sameInstruction, 0, sizeof(sameInstruction));
	written[0x318 >> 3] = (1u << (0x318 & 7)) | (1u << (0x31A & 7));
	uint8_t none[sizeof(written)] = { 0 };
	unsigned stores = 0, other = 0, rmw[0x400] = { 0 }, matches[0x400] = { 0 };
	unsigned bios[0x400] = { 0 }, cartridge[0x400] = { 0 }, interrupt[0x400] = { 0 };
	for (unsigned i = 0; i < 2; i++) {
		uint16_t pc = g->cpu.pc;
		(void) gamateCpuStep(g);
		countScratchSameInstruction(g, written, seen, none, none, none, none, pc, &stores,
			rmw, matches, bios, cartridge, interrupt, &other);
		classifyScratchReads(g, seen, separate, sameInstruction);
	}
	if (stores != 1 || other != 0 || rmw[0x318] != 0 || rmw[0x31A] != 1 ||
			separate[0x318 >> 3] != 0 ||
			sameInstruction[0x318 >> 3] != written[0x318 >> 3]) return false;

	gamateResetRamDiagnostics(g);
	memset(written, 0, sizeof(written));
	uint8_t lastBios[sizeof(written)] = { 0 }, lastCartridge[sizeof(written)] = { 0 };
	uint8_t lastInterrupt[sizeof(written)] = { 0 };
	g->diagnostics.diagnosticInstructionPc = 0xF027;
	gamateWrite(g, 0x318, 0);
	recordCallWrites(g, 0xF027, written, lastBios, lastCartridge, lastInterrupt);
	g->diagnostics.diagnosticInstructionPc = 0x6020;
	g->diagnostics.diagnosticInterrupt = 1;
	gamateWrite(g, 0x318, 0);
	gamateWrite(g, 0x31A, 0);
	recordCallWrites(g, 0x6020, written, lastBios, lastCartridge, lastInterrupt);
	g->diagnostics.diagnosticInstructionPc = 0xF027;
	g->diagnostics.diagnosticInterrupt = 0;
	gamateWrite(g, 0x318, 0);
	recordCallWrites(g, 0xF027, written, lastBios, lastCartridge, lastInterrupt);
	return written[0x318 >> 3] == ((1u << (0x318 & 7)) | (1u << (0x31A & 7))) &&
		lastBios[0x318 >> 3] == (1u << (0x318 & 7)) &&
		lastCartridge[0x318 >> 3] == (1u << (0x31A & 7)) &&
		lastInterrupt[0x318 >> 3] == (1u << (0x31A & 7));
}

static int scratchUse(struct Gamate* g) {
	uint8_t written[sizeof(g->diagnostics.ramWritten)] = { 0 };
	uint8_t seen[sizeof(written)] = { 0 }, separate[sizeof(written)] = { 0 };
	uint8_t sameInstruction[sizeof(written)] = { 0 };
	uint8_t lastBios[sizeof(written)] = { 0 }, lastCartridge[sizeof(written)] = { 0 };
	uint8_t lastInterrupt[sizeof(written)] = { 0 };
	uint8_t matchesOutput[sizeof(written)] = { 0 };
	unsigned reads[0x400] = { 0 };
	unsigned separateReads[0x400] = { 0 }, sameInstructionReads[0x400] = { 0 };
	unsigned rmwReads[0x400] = { 0 };
	unsigned rmwBios[0x400] = { 0 }, rmwCartridge[0x400] = { 0 }, rmwInterrupt[0x400] = { 0 };
	unsigned rmwOutputMatches[0x400] = { 0 };
	unsigned indexedStores = 0, otherSameInstruction = 0;
	unsigned entries = 0, returns = 0, interrupted = 0, windows = 0;
	uint8_t entryS = 0;
	unsigned lastRecord = 16, lastX = 0, lastY = 0;
	uint16_t returnAddress = 0;
	uint64_t irqs = 0, nmis = 0;
	bool inCall = false, watching = false;
	const char* press = getenv("GAMATEBIOSCALLS_PRESS");
	for (unsigned frame = 1; frame <= 1000; frame++) {
		g->buttons = gamateBiosButtonsForFrame(frame, press);
		g->frameDone = false;
		while (!g->frameDone) {
			uint64_t priorReads = g->diagnostics.ramReadBeforeWrite[0];
			uint64_t priorWrites = g->diagnostics.ramTrackedWrites[0] +
				g->diagnostics.ramTrackedWrites[1] + g->diagnostics.ramTrackedWrites[2];
			uint16_t instructionPc = g->cpu.pc;
			unsigned cycles = gamateCpuStep(g);
			gamateRunClocks(g, cycles * 2);
			uint64_t currentWrites = g->diagnostics.ramTrackedWrites[0] +
				g->diagnostics.ramTrackedWrites[1] + g->diagnostics.ramTrackedWrites[2];
			if (inCall && currentWrites != priorWrites)
				recordCallWrites(g, instructionPc, written, lastBios, lastCartridge, lastInterrupt);
			if (watching && g->diagnostics.ramReadBeforeWrite[0] != priorReads) {
				countScratchSameInstruction(g, written, seen, lastBios, lastCartridge,
					lastInterrupt, matchesOutput, instructionPc, &indexedStores, rmwReads,
					rmwOutputMatches, rmwBios, rmwCartridge, rmwInterrupt, &otherSameInstruction);
				classifyScratchReads(g, seen, separate, sameInstruction);
			}
			if (inCall && g->cpu.pc == returnAddress && g->cpu.s == (uint8_t) (entryS + 2)) {
				returns++;
				if (g->diagnostics.irqsTaken != irqs || g->diagnostics.nmisTaken != nmis)
					interrupted++;
				memset(matchesOutput, 0, sizeof(matchesOutput));
				if (lastRecord < 8) {
					for (unsigned row = 0; row < 16; row++) {
						unsigned scratch = 0x398 + row;
						unsigned vram = ((((lastY + row) & 255) << 5) |
							((lastX >> 3) & 31)) * 2 + 1;
						if (g->ram[scratch] == g->vram[vram])
							matchesOutput[scratch >> 3] |= 1u << (scratch & 7);
					}
				}
				gamateResetRamDiagnostics(g);
				memset(seen, 0, sizeof(seen));
				memset(separate, 0, sizeof(separate));
				memset(sameInstruction, 0, sizeof(sameInstruction));
				inCall = false;
				watching = true;
			} else if (!inCall && g->cpu.pc == 0xF027) {
				if (watching) {
					recordScratchUse(g, written, reads);
					recordClassifiedScratchUse(written, separate, sameInstruction,
						separateReads, sameInstructionReads);
					windows++;
				}
				entries++;
				memset(written, 0, sizeof(written));
				memset(lastBios, 0, sizeof(lastBios));
				memset(lastCartridge, 0, sizeof(lastCartridge));
				memset(lastInterrupt, 0, sizeof(lastInterrupt));
				lastRecord = 16;
				for (unsigned record = 0; record < 16; record++) {
					unsigned address = 0x21 + 8 * record;
					if (g->ram[address + 2] != 0xFF) {
						lastRecord = record;
						lastX = g->ram[address];
						lastY = g->ram[address + 1];
					}
				}
				entryS = g->cpu.s;
				uint16_t stacked = (uint16_t) (g->ram[0x100 | (uint8_t) (entryS + 1)] |
					g->ram[0x100 | (uint8_t) (entryS + 2)] << 8);
				returnAddress = (uint16_t) (stacked + 1);
				irqs = g->diagnostics.irqsTaken;
				nmis = g->diagnostics.nmisTaken;
				gamateResetRamDiagnostics(g);
				inCall = true;
				watching = false;
			}
		}
	}
	if (watching) {
		recordScratchUse(g, written, reads);
		recordClassifiedScratchUse(written, separate, sameInstruction,
			separateReads, sameInstructionReads);
		windows++;
	}
	printf("entries=%u returns=%u interrupted=%u windows=%u in_flight=%u\n",
		entries, returns, interrupted, windows, inCall);
	printScratchReads("caller_read_before_write", reads);
	printScratchReads("separate_instruction_reads", separateReads);
	printScratchReads("same_instruction_writes", sameInstructionReads);
	printScratchReads("same_instruction_rmw", rmwReads);
	printScratchReads("rmw_last_written_by_bios", rmwBios);
	printScratchReads("rmw_last_written_by_cartridge", rmwCartridge);
	printScratchReads("rmw_last_written_in_interrupt", rmwInterrupt);
	printScratchReads("rmw_matches_final_large_object", rmwOutputMatches);
	printf("same_instruction_classes: indexed_store=%u other=%u\n",
		indexedStores, otherSameInstruction);
	return 0;
}

int main(int argc, char** argv) {
	if (argc < 3 || argc > 4 ||
			(argc == 4 && strcmp(argv[3], "corpus") && strcmp(argv[3], "corpus-bios-source") &&
			strcmp(argv[3], "scratch-use") && strcmp(argv[3], "interrupt-compare"))) {
		fprintf(stderr, "usage: %s bios cartridge "
			"[corpus|corpus-bios-source|scratch-use|interrupt-compare]\n", argv[0]);
		return 2;
	}
	size_t biosSize, romSize;
	uint8_t* bios = readFile(argv[1], &biosSize);
	uint8_t* rom = readFile(argv[2], &romSize);
	struct Gamate* base = malloc(sizeof(*base));
	struct Gamate* g = malloc(sizeof(*g));
	if (!bios || !rom || !base || !g) {
		fprintf(stderr, "could not read inputs or allocate machines\n");
		free(bios); free(rom); free(base); free(g);
		return 2;
	}
	gamateInit(base);
	bool loaded = gamateLoad(base, rom, romSize, bios, biosSize);
	free(bios);
	free(rom);
	if (!loaded || !boot(base)) {
		fprintf(stderr, "could not reach cartridge entry\n");
		gamateFree(base);
		free(base); free(g);
		return 1;
	}
	if (argc == 4) {
		if (!strcmp(argv[3], "interrupt-compare")) {
			struct Gamate* original = malloc(sizeof(*original));
			if (!original) {
				fprintf(stderr, "could not allocate interrupt comparison machine\n");
				gamateFree(base);
				free(base); free(g);
				return 2;
			}
			if (!callDifferencesSelfTest(base, g, original)) {
				fprintf(stderr, "interrupt comparison self-test failed\n");
				free(original);
				gamateFree(base);
				free(base); free(g);
				return 1;
			}
			int result = interruptCompare(base, g, original);
			free(original);
			gamateFree(base);
			free(base); free(g);
			return result;
		}
		if (!strcmp(argv[3], "scratch-use") && !scratchUseSelfTest(g, base)) {
			fprintf(stderr, "scratch-use diagnostic self-test failed\n");
			gamateFree(base);
			free(base); free(g);
			return 1;
		}
		int result = !strcmp(argv[3], "scratch-use") ? scratchUse(base) :
			corpus(base, g, !strcmp(argv[3], "corpus-bios-source"));
		gamateFree(base);
		free(base); free(g);
		return result;
	}
	unsigned cases = 0, failures = 0, backgroundFailures = 0;
	for (unsigned record = 0; record < 16; record++) {
		for (unsigned x = 0; x < 16; x++) {
			for (unsigned seed = 0; seed < 4; seed++) {
				unsigned position = (x & 7) | (x & 8 ? 0xF8 : 0);
				unsigned y = seed & 1 ? 0xF8 : 0x20;
				failures += !spriteCase(g, base, record, position, y, seed, seed);
				backgroundFailures += !backgroundCase(g, base, record, position, y, seed);
				cases++;
			}
		}
	}
	unsigned firstEventFailures = 0;
	unsigned emptyScratchFailures = 0;
	unsigned liveControlFailures = 0;
	liveControlFailures += !liveControlCase(g, base, false);
	liveControlFailures += !liveControlCase(g, base, true);
	unsigned livePointerFailures = !livePointerCase(g, base);
	unsigned liveRecordFailures = !liveRecordCase(g, base);
	for (unsigned record = 0; record < 16; record++) {
		firstEventFailures += !firstEventCase(g, base, record, false);
		firstEventFailures += !firstEventCase(g, base, record, true);
		emptyScratchFailures += !emptyScratchCase(g, base, record);
	}
	size_t fixedOffset = base->mapper == GAMATE_MAPPER_4IN1 ?
		(size_t) base->firstBank * 0x4000 : 0;
	if (fixedOffset + 0x3900 > base->romSize) {
		fprintf(stderr, "cartridge is too small for synthetic graphics sources\n");
		gamateFree(base);
		free(base); free(g);
		return 1;
	}
	uint8_t* writableGraphics = base->rom + fixedOffset + 0x2000;
	for (unsigned plane = 0; plane < 2; plane++)
		for (unsigned i = 0; i < 0x900; i++)
			writableGraphics[plane * 0x1000 + i] = pattern(51 + plane, i);
	const uint8_t* graphics[2] = { writableGraphics, writableGraphics + 0x1000 };
	static const unsigned stackRecords[] = { 0, 7, 8, 15 };
	static const unsigned stackPositions[] = { 0, 1, 7, 255 };
	static const unsigned stackSelectors[] = { 0, 31, 32, 254 };
	static const uint8_t stackEntries[] = { 0xFD, 1, 0, 0xFF };
	unsigned stackCases = 0, stackFailures = 0;
	for (unsigned i = 0; i < sizeof(stackEntries); i++) {
		stackFailures += !stackCase(g, base, 0, false, false, 0, 0, stackEntries[i]);
		stackCases++;
		for (unsigned r = 0; r < sizeof(stackRecords) / sizeof(stackRecords[0]); r++)
			for (unsigned x = 0; x < sizeof(stackPositions) / sizeof(stackPositions[0]); x++)
				for (unsigned selector = 0;
						selector < sizeof(stackSelectors) / sizeof(stackSelectors[0]); selector++)
					for (unsigned background = 0; background < 2; background++) {
						stackFailures += !stackCase(g, base, stackRecords[r], true,
							background != 0, stackPositions[x], stackSelectors[selector],
							stackEntries[i]);
						stackCases++;
					}
	}
	unsigned combinedFailures = 0;
	for (unsigned seed = 0; seed < 256; seed++)
		combinedFailures += !combinedCase(g, base, graphics, seed);
	static const unsigned records[] = { 0, 7, 8, 15 };
	unsigned selectorCases = 0, selectorFailures = 0;
	for (unsigned i = 0; i < sizeof(records) / sizeof(records[0]); i++) {
		for (unsigned selector = 0; selector < 256; selector++) {
			selectorFailures += !selectorCase(g, base, graphics, records[i], selector, false, false);
			selectorFailures += !selectorCase(g, base, graphics, records[i], selector, true, false);
			selectorCases += 2;
		}
		selectorFailures += !selectorCase(g, base, graphics, records[i], 255, true, true);
		selectorCases++;
	}
	printf("sprite_cases=%u failures=%u background_cases=%u failures=%u\n",
		cases, failures, cases, backgroundFailures);
	printf("first_lcd_event_cases=32 failures=%u\n", firstEventFailures);
	printf("empty_scratch_cases=16 failures=%u\n", emptyScratchFailures);
	printf("live_control_cases=2 failures=%u\n", liveControlFailures);
	printf("live_pointer_cases=1 failures=%u\n", livePointerFailures);
	printf("live_record_cases=1 failures=%u\n", liveRecordFailures);
	printf("stack_cases=%u failures=%u\n", stackCases, stackFailures);
	printf("combined_cases=256 failures=%u\n", combinedFailures);
	printf("selector_cases=%u failures=%u\n", selectorCases, selectorFailures);
	gamateFree(base);
	free(base); free(g);
	return failures != 0 || backgroundFailures != 0 || firstEventFailures != 0 ||
		emptyScratchFailures != 0 || liveControlFailures != 0 || livePointerFailures != 0 ||
		liveRecordFailures != 0 || stackFailures != 0 ||
		combinedFailures != 0 || selectorFailures != 0;
}
