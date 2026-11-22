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
};

struct RomEvents {
	uint16_t address[MAX_EVENTS];
	unsigned count, observed, mismatches;
};

static void romEvent(struct RomEvents* events, uint16_t address) {
	if (!events) return;
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
	(void) g;
	unsigned i = events->observed++;
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

static bool invoke(struct Gamate* g, uint8_t p, struct LcdEvents* events, struct RomEvents* rom) {
	uint8_t a = g->cpu.a, x = g->cpu.x, y = g->cpu.y;
	uint8_t lastRecord[8];
	memcpy(lastRecord, g->ram + 0x99, sizeof(lastRecord));
	g->ram[0x1FF] = 0x60;
	g->ram[0x1FE] = 0x1F;
	g->cpu.pc = 0xF027;
	g->cpu.s = 0xFD;
	g->cpu.p = p;
	g->cpu.irqMask = (p & 4) != 0;
	g->irq = g->nmi = false;
	gamateResetVramDiagnostics(g);
	if (events) gamateSetLcdDiagnosticWatch(g, checkLcdEvent, events);
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
		if (g->cpu.pc == 0x6020 && g->cpu.s == 0xFF) {
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
	memcpy(expected, g->vram, sizeof(expected));
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
	if (!returned || !pixels || !accesses) {
		fprintf(stderr, "record=%u x=%02X y=%02X selector=%02X seed=%u "
			"returned=%u pixels=%u accesses=%u reads=%llu writes=%llu\n", record, x, y,
			selector, seed, returned, pixels, accesses,
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
	if (!returned || !pixels || !accesses) {
		fprintf(stderr, "background record=%u x=%02X y=%02X seed=%u returned=%u "
			"pixels=%u accesses=%u reads=%llu writes=%llu\n", record, x, y, seed,
			returned, pixels, accesses, (unsigned long long) g->diagnostics.vramReads,
			(unsigned long long) g->diagnostics.vramTrackedWrites);
		return false;
	}
	return true;
}

static unsigned tileClocks(unsigned selector, const unsigned low[2]) {
	unsigned clocks = 64 * (selector >> 5);
	for (unsigned plane = 0; plane < 2; plane++)
		for (unsigned row = 0; row < 8; row++)
			clocks += 2 * (low[plane] + ((selector & 31) << 3) + row > 255);
	return clocks;
}

static unsigned modelBackground(uint8_t* vram, const uint8_t* const graphics[2],
		const unsigned base[2], bool* used, struct LcdEvents* events, struct RomEvents* rom,
		uint8_t control,
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
		uint8_t control,
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
				vram[offset] = pixel[plane] | (vram[offset] & mask);
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
	unsigned reads = 0, writes = 0, clocks = 6326;
	for (unsigned record = 0; record < 16; record++) {
		unsigned entry = 0x21 + 8 * record;
		if (g->ram[entry + 7] != 0xFF) {
			unsigned tiles = record < 8 ? 3 : 2;
			reads += tiles * tiles;
			writes += tiles * tiles * 16;
			clocks += modelBackground(expected, sources, bases, NULL, &events, &rom,
				g->ram[0x15], record,
				g->ram[entry + 5], g->ram[entry + 6]);
		}
	}
	for (unsigned record = 0; record < 16; record++) {
		unsigned entry = 0x21 + 8 * record;
		if (g->ram[entry + 2] != 0xFF) {
			unsigned side = record < 8 ? 16 : 8;
			unsigned count = side * (side / 8 + 1) * 2;
			reads += count;
			writes += count;
			clocks += modelSprite(expected, sources, bases, NULL, &events, &rom,
				g->ram[0x15], record,
				g->ram[entry], g->ram[entry + 1],
				g->ram[entry + 2]);
		}
	}
	uint64_t start = g->masterClocks;
	bool returned = invoke(g, 0x64, &events, &rom);
	bool pixels = !memcmp(g->vram, expected, sizeof(expected));
	bool accesses = g->diagnostics.vramTrackedWrites == writes && g->diagnostics.vramReads == reads;
	bool timing = g->masterClocks - start == clocks;
	if (!returned || !pixels || !accesses || !timing || !lcdMatches(&events) || !romMatches(&rom)) {
		fprintf(stderr, "combined seed=%u returned=%u pixels=%u accesses=%u timing=%lld "
			"lcd=%u/%u/%u rom=%u/%u/%u reads=%llu/%u writes=%llu/%u\n",
			seed, returned, pixels, accesses,
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
	unsigned reads = 0, writes = 0, clocks = 6326;
	if (background && !disabled) {
		unsigned tiles = record < 8 ? 3 : 2;
		reads = tiles * tiles;
		writes = reads * 16;
		clocks += modelBackground(expected, sources, bases, NULL, &events, &rom, g->ram[0x15],
			record, x, y);
	} else if (!background && selector != 0xFF) {
		unsigned side = record < 8 ? 16 : 8;
		reads = writes = side * (side / 8 + 1) * 2;
		clocks += modelSprite(expected, sources, bases, NULL, &events, &rom, g->ram[0x15],
			record, x, y, selector);
	}
	uint64_t start = g->masterClocks;
	bool returned = invoke(g, (uint8_t) ((selector & 0xD7) | 0x20), &events, &rom);
	bool pixels = !memcmp(g->vram, expected, sizeof(expected));
	bool accesses = g->diagnostics.vramTrackedWrites == writes && g->diagnostics.vramReads == reads;
	bool timing = g->masterClocks - start == clocks;
	if (!returned || !pixels || !accesses || !timing || !lcdMatches(&events) || !romMatches(&rom)) {
		fprintf(stderr, "selector record=%u selector=%02X background=%u disabled=%u "
			"returned=%u pixels=%u accesses=%u p=%02X timing=%lld lcd=%u/%u/%u rom=%u/%u/%u\n",
			record, selector, background,
			disabled, returned, pixels, accesses, g->cpu.p,
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

static int replay(struct Gamate* g, const struct Gamate* entry) {
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
				if (address + 7 >= 0xE000) continue;
				valid[plane][selector] = true;
				for (unsigned row = 0; row < 8; row++)
					tiles[plane][selector * 8 + row] =
						g->rom[cartridgeOffset(g, (uint16_t) (address + row))];
			}
		}
	}
	uint8_t expected[sizeof(g->vram)];
	memcpy(expected, g->vram, sizeof(expected));
	struct LcdEvents events = { 0 };
	struct RomEvents rom = { 0 };
	unsigned reads = 0, writes = 0, clocks = 6326;
	unsigned bases[2] = { g->ram[6] | (unsigned) g->ram[7] << 8,
		g->ram[8] | (unsigned) g->ram[9] << 8 };
	for (unsigned record = 0; record < 16; record++) {
		unsigned address = 0x21 + 8 * record;
		if (g->ram[address + 7] != 0xFF) {
			unsigned count = record < 8 ? 9 : 4;
			reads += count;
			writes += count * 16;
			clocks += modelBackground(expected, graphics, bases, used, &events, &rom,
				g->ram[0x15], record,
				g->ram[address + 5], g->ram[address + 6]);
		}
	}
	for (unsigned record = 0; record < 16; record++) {
		unsigned address = 0x21 + 8 * record;
		if (g->ram[address + 2] != 0xFF) {
			unsigned side = record < 8 ? 16 : 8;
			unsigned count = side * (side / 8 + 1) * 2;
			reads += count;
			writes += count;
			clocks += modelSprite(expected, graphics, bases, used, &events, &rom,
				g->ram[0x15], record,
				g->ram[address], g->ram[address + 1],
				g->ram[address + 2]);
		}
	}
	for (unsigned selector = 0; selector < 256; selector++) {
		if (used[selector] && (!valid[0][selector] || !valid[1][selector])) {
			unsigned plane = valid[0][selector] ? 1 : 0;
			fprintf(stderr, "source skip: tile crosses cartridge boundary at $%04X\n",
				(uint16_t) (bases[plane] + selector * 8 + 7));
			return 4;
		}
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
	bool ram = !memcmp(g->ram + 0x300, entry->ram + 0x99, 8);
	for (unsigned i = 0; i < sizeof(g->ram); i++) {
		bool scratch = i <= 3 || (i >= 0x18 && i <= 0x1F) ||
			(i >= 0x100 && i < 0x200) || (i >= 0x300 && i <= 0x3C7);
		if (!scratch && ((g->diagnostics.ramWritten[i >> 3] & (1u << (i & 7))) ||
				g->ram[i] != entry->ram[i])) ram = false;
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
		return 1;
	}
	return 0;
}

static int corpus(struct Gamate* g, struct Gamate* copy) {
	unsigned calls = 0, checked = 0, decimal = 0, nmi = 0, source = 0, failures = 0;
	const char* press = getenv("GAMATEBIOSCALLS_PRESS");
	for (unsigned frame = 1; frame <= 1000; frame++) {
		g->buttons = gamateBiosButtonsForFrame(frame, press);
		g->frameDone = false;
		while (!g->frameDone) {
			unsigned cycles = gamateCpuStep(g);
			gamateRunClocks(g, cycles * 2);
			if (g->cpu.pc != 0xF027) continue;
			calls++;
			int result = replay(copy, g);
			if (result == 2) decimal++;
			else if (result == 3) nmi++;
			else if (result == 4) source++;
			else {
				checked++;
				failures += result != 0;
			}
		}
	}
	printf("calls=%u checked=%u decimal_skips=%u nmi_skips=%u source_skips=%u failures=%u\n",
		calls, checked, decimal, nmi, source, failures);
	return failures != 0;
}

int main(int argc, char** argv) {
	if (argc < 3 || argc > 4 || (argc == 4 && strcmp(argv[3], "corpus"))) {
		fprintf(stderr, "usage: %s bios cartridge [corpus]\n", argv[0]);
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
		int result = corpus(base, g);
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
	printf("combined_cases=256 failures=%u\n", combinedFailures);
	printf("selector_cases=%u failures=%u\n", selectorCases, selectorFailures);
	gamateFree(base);
	free(base); free(g);
	return failures != 0 || backgroundFailures != 0 || combinedFailures != 0 || selectorFailures != 0;
}
