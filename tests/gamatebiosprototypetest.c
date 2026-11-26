/* SPDX-License-Identifier: GPL-2.0-or-later */

/* Tests use a purpose-written synthetic cartridge, not commercial content. */
#include "gamate/gamate.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
	ENTRY = 0x6020,
	RETURN = 0x6100,
	UNSUPPORTED = 0xF036,
	BAD_PROTECTION = 0xF039,
	BAD_IDENTITY = 0xF03C,
	BAD_CHECKSUM = 0xF03F,
	BOOT_LIMIT = 6000000,
};

static void step(struct Gamate* g) {
	unsigned cycles = gamateCpuStep(g);
	gamateRunClocks(g, cycles * 2);
}

static void runUntil(struct Gamate* g, uint16_t address, unsigned limit) {
	for (unsigned i = 0; i < limit; i++) {
		if (g->cpu.pc == address) return;
		assert(!g->cpu.stopped && !g->diagnostics.unsupportedOpcodes);
		step(g);
	}
	fprintf(stderr, "did not reach $%04X; PC=$%04X 0B=%02X IRQ=%u IRQCLOCKS=%u MASTER=%llu\n",
		address, g->cpu.pc, g->ram[0x0B], g->irq, g->irqClocks,
		(unsigned long long) g->masterClocks);
	abort();
}

static void makeCartridge(uint8_t* rom, size_t size) {
	memset(rom, 0, size);
	memcpy(rom + 5, "COPYRIGHT BIT CORPORATION", 25);
	for (unsigned i = 0x1000; i < 0x2000; i++) rom[i] = (uint8_t) (i * 73 + i / 7);
	unsigned checksum = 0;
	for (unsigned i = 0x1000; i < 0x2000; i++) checksum += rom[i];
	rom[0] = (uint8_t) checksum;
	rom[1] = (uint8_t) (checksum >> 8);
	rom[0x20] = 0x4C;
	rom[0x21] = 0x20;
	rom[0x22] = 0x60;
	rom[0x23] = 0x60;
	rom[0x26] = 0x4C;
	rom[0x27] = 0x30;
	rom[0x28] = 0x60;
	const uint8_t callback[] = { 0xA2, 0x44, 0xA0, 0x77, 0xA9, 0xB3, 0x60 };
	memcpy(rom + 0x30, callback, sizeof(callback));
}

static void checkEntry(struct Gamate* g) {
	assert(g->protectionPassed && !g->protectionFailed);
	assert(g->cpu.pc == ENTRY && g->cpu.a == 0xFF && g->cpu.x == 0x28);
	assert(g->cpu.y == 0xFF && g->cpu.s == 0xFF && g->cpu.p == 0xA1);
	assert(!g->cpu.irqMask && !g->irq && !g->nmi);
	for (unsigned i = 0; i < 0x20; i++) {
		uint8_t expected = 0;
		if (i == 4) expected = 0x47;
		if (i >= 5 && i <= 0x0A) expected = 0xFF;
		if (i == 0x0C) expected = 1;
		if (i == 0x10) expected = 3;
		if (i == 0x11) expected = 0x94;
		if (i == 0x12) expected = 0x8A;
		if (i == 0x13) expected = 0x35;
		assert(g->ram[i] == expected);
	}
	assert(g->ram[0x20] == 0xFF && g->ram[0x21] == 0);
	assert(g->ram[0xE0] == g->rom[0] && g->ram[0xE5] == g->rom[0]);
	assert(g->ram[0xE1] == g->rom[1] && g->ram[0xE6] == g->rom[1]);
	for (size_t i = 0; i < sizeof(g->vram); i++) assert(g->vram[i] == 0);
	for (size_t i = 0; i < sizeof(g->ayRegisters); i++) assert(g->ayRegisters[i] == 0);
	assert(g->scrollX == 0 && g->scrollY == 0x5F);
	assert(g->vramAddress == 0x18CE && g->vramPlane && g->incrementRow);
	assert(!g->window && !g->swapPlanes && !g->displayBlank && !g->displayStopped);
}

static void testBoot(struct Gamate* g) {
	for (unsigned run = 0; run < 2; run++) {
		gamateReset(g);
		memset(g->ram, 0xA5, sizeof(g->ram));
		memset(g->vram, 0xA5, sizeof(g->vram));
		runUntil(g, ENTRY, BOOT_LIMIT);
		checkEntry(g);
	}

	gamateReset(g);
	g->protectionFailed = true;
	runUntil(g, BAD_PROTECTION, 100000);
	assert(!g->protectionPassed);

	g->rom[5] ^= 1;
	gamateReset(g);
	runUntil(g, BAD_IDENTITY, 100000);
	assert(g->protectionPassed);
	g->rom[5] ^= 1;

	g->rom[0x1000] ^= 1;
	gamateReset(g);
	runUntil(g, BAD_CHECKSUM, 100000);
	assert(g->protectionPassed);
	g->rom[0x1000] ^= 1;

	gamateReset(g);
	runUntil(g, ENTRY, BOOT_LIMIT);
}

static void prepareCallWithStack(struct Gamate* g, uint16_t slot, uint8_t a, uint8_t x,
		uint8_t y, uint8_t p, uint8_t returnS) {
	g->cpu.pc = slot;
	g->cpu.a = a;
	g->cpu.x = x;
	g->cpu.y = y;
	g->cpu.s = (uint8_t) (returnS - 2);
	g->cpu.p = p;
	g->cpu.irqMask = (p & 4) != 0;
	g->cpu.stopped = false;
	g->ram[0x100 | (uint8_t) (returnS - 1)] = (uint8_t) (RETURN - 1);
	g->ram[0x100 | returnS] = (uint8_t) ((RETURN - 1) >> 8);
	g->irq = g->nmi = false;
	g->irqClocks = 0;
}

static void prepareCall(struct Gamate* g, uint16_t slot, uint8_t a, uint8_t x,
		uint8_t y, uint8_t p) {
	prepareCallWithStack(g, slot, a, x, y, p, 0xFF);
}

static void call(struct Gamate* g, uint16_t slot, uint8_t a, uint8_t x,
		uint8_t y, uint8_t p, unsigned limit) {
	prepareCall(g, slot, a, x, y, p);
	runUntil(g, RETURN, limit);
	assert(g->cpu.s == 0xFF);
}

static void testIrq(struct Gamate* g) {
	for (unsigned enabled = 0; enabled <= 1; enabled++) {
		g->cpu.pc = RETURN;
		g->cpu.a = 0xC1;
		g->cpu.x = 0x47;
		g->cpu.y = 0x19;
		g->cpu.s = 0xFF;
		g->cpu.p = 0xA1;
		g->cpu.irqMask = false;
		g->ram[0x0B] = 23;
		g->ram[0x0C] = (uint8_t) enabled;
		g->ram[0x0E] = 0;
		g->ram[0x0F] = g->ram[0x10] = g->ram[0x11] = 0xFF;
		g->irq = true;
		g->nmi = false;
		step(g);
		runUntil(g, RETURN, 1000);
		assert(g->cpu.a == 0xC1 && g->cpu.y == 0x19 && g->cpu.p == 0xA1);
		assert(g->cpu.x == (enabled ? 0x44 : 0x47) && g->cpu.s == 0xFF);
		assert(g->ram[0x0B] == 24);
		assert(g->ram[0x0E] == 1 && g->ram[0x0F] == 0);
		assert(g->ram[0x10] == 0 && g->ram[0x11] == 0);
	}
}

static void testNmi(struct Gamate* g) {
	static const uint8_t disabled[] = { 0x60, 0xEA, 0xEA };
	static const uint8_t direct[] = { 0xE6, 0x08, 0xA9, 0x78, 0xA2, 0x34, 0xA0, 0x56, 0x60 };
	static const uint8_t absStub[] = { 0x4C, 0x40, 0x60 };
	static const uint8_t absHandler[] = { 0xE6, 0x08, 0xA9, 0x78, 0xA2, 0x34, 0xA0, 0x56, 0x60 };
	for (unsigned form = 0; form < 3; form++) {
		if (form == 0) memcpy(g->rom + 0x23, disabled, sizeof(disabled));
		else if (form == 1) memcpy(g->rom + 0x23, direct, sizeof(direct));
		else memcpy(g->rom + 0x23, absStub, sizeof(absStub));
		memcpy(g->rom + 0x40, absHandler, sizeof(absHandler));
		for (unsigned byteIndex = 0; byteIndex < 2; byteIndex++) {
			uint8_t byte = byteIndex ? 0x3C : 0xA5;
			g->cpu.pc = RETURN;
			g->cpu.a = 0xC1;
			g->cpu.x = 0x47;
			g->cpu.y = 0x19;
			g->cpu.s = 0xFF;
			g->cpu.p = 0xA1;
			g->cpu.irqMask = false;
			g->cpu.stopped = false;
			g->ram[0x0A] = 0;
			g->ram[0x0C] = (uint8_t) (form != 0);
			g->ram[0x08] = 0;
			g->irq = false;
			g->nmi = false;
			g->uartRx = 0;
			g->uartRxBits = 0;
			uint64_t startClocks = g->masterClocks;
			for (int bit = 7; bit >= 0; bit--) {
				gamateUartReceiveBit(g, (byte >> bit) & 1);
			}
			assert(g->nmi);
			step(g);
			bool returned = false;
			for (unsigned instruction = 0; instruction < 1000; instruction++) {
				if (g->cpu.pc == RETURN && g->cpu.s == 0xFF) {
					returned = true;
					break;
				}
				step(g);
			}
			assert(returned);
			if (form == 0)
				assert(g->masterClocks - startClocks == 66);
			if (form == 1)
				assert(g->masterClocks - startClocks == 110);
			if (form == 2)
				assert(g->masterClocks - startClocks == 116);

			assert(g->cpu.pc == RETURN && g->cpu.s == 0xFF);
			assert(g->cpu.a == 0xC1);
			assert(g->cpu.x == (form ? 0x34 : 0x47));
			assert(g->cpu.y == (form ? 0x56 : 0x19));
			assert(g->cpu.p == 0xA1);
			assert(g->ram[0x0A] == byte);
			assert(g->ram[0x08] == (form == 0 ? 0 : 1));
			assert(!g->nmi);
			assert(!g->diagnostics.unsupportedOpcodes);
		}
	}

	for (unsigned p = 0; p < 64; p++) {
		memcpy(g->rom + 0x23, disabled, sizeof(disabled));
		uint8_t entryP = 0x20;
		if (p & 1) entryP |= 1;
		if (p & 2) entryP |= 2;
		if (p & 4) entryP |= 4;
		if (p & 8) entryP |= 8;
		if (p & 16) entryP |= 64;
		if (p & 32) entryP |= 128;
		g->cpu.pc = RETURN;
		g->cpu.a = 0xC1;
		g->cpu.x = 0x47;
		g->cpu.y = 0x19;
		g->cpu.s = 0xFF;
		g->cpu.p = entryP;
		g->cpu.stopped = false;
		g->ram[0x0C] = 0;
		g->irq = g->nmi = false;
		for (int bit = 7; bit >= 0; bit--)
			gamateUartReceiveBit(g, (0xA5 >> bit) & 1);
		step(g);
		for (unsigned i = 0; i < 1000 && g->cpu.pc != RETURN; i++) step(g);
		assert(g->cpu.pc == RETURN && g->cpu.s == 0xFF);
		assert(g->cpu.a == 0xC1 && g->cpu.x == 0x47 && g->cpu.y == 0x19);
		assert(g->cpu.p == (uint8_t) (entryP | 0x20));
		assert(g->ram[0x0A] == 0xA5);
	}

	memcpy(g->rom + 0x23, disabled, sizeof(disabled));
	for (unsigned s = 0; s < 256; s++) {
		g->cpu.pc = RETURN;
		g->cpu.a = 0xC1;
		g->cpu.x = 0x47;
		g->cpu.y = 0x19;
		g->cpu.s = (uint8_t) s;
		g->cpu.p = 0x24;
		g->cpu.stopped = false;
		g->ram[0x0C] = 0;
		g->irq = g->nmi = false;
		for (int bit = 7; bit >= 0; bit--)
			gamateUartReceiveBit(g, (0xA5 >> bit) & 1);
		step(g);
		for (unsigned i = 0; i < 1000 && g->cpu.pc != RETURN; i++) step(g);
		assert(g->cpu.pc == RETURN && g->cpu.s == (uint8_t) s);
		assert(g->cpu.a == 0xC1 && g->cpu.x == 0x47 && g->cpu.y == 0x19);
		assert(g->cpu.p == 0x24);
		assert(g->ram[0x0A] == 0xA5);
	}

	memcpy(g->rom + 0x23, disabled, sizeof(disabled));
	g->cpu.pc = RETURN;
	g->cpu.a = 0xC1;
	g->cpu.x = 0x47;
	g->cpu.y = 0x19;
	g->cpu.s = 0xFF;
	g->cpu.p = 0x24;
	g->ram[0x0C] = 0;
	g->irq = g->nmi = false;
	for (int bit = 7; bit >= 5; bit--)
		gamateUartReceiveBit(g, (0xA5 >> bit) & 1);
	size_t stateSize = gamateStateSize(g);
	uint8_t* state = malloc(stateSize);
	assert(state);
	assert(gamateSaveState(g, state, stateSize));
	g->uartRx = g->uartRxBits = 0;
	g->nmi = false;
	assert(gamateLoadState(g, state, stateSize));
	assert(g->uartRxBits == 3);
	for (int bit = 4; bit >= 0; bit--)
		gamateUartReceiveBit(g, (0xA5 >> bit) & 1);
	step(g);
	for (unsigned i = 0; i < 1000 && g->cpu.pc != RETURN; i++) step(g);
	assert(g->cpu.pc == RETURN && g->cpu.s == 0xFF);
	assert(g->ram[0x0A] == 0xA5);
	assert(!g->nmi);
	free(state);

	memcpy(g->rom + 0x23, direct, sizeof(direct));
	g->cpu.pc = RETURN;
	g->cpu.a = 0xC1;
	g->cpu.x = 0x47;
	g->cpu.y = 0x19;
	g->cpu.s = 0xFF;
	g->cpu.p = 0x24;
	g->ram[0x0C] = 1;
	g->ram[0x08] = 0;
	g->irq = g->nmi = false;
	for (int bit = 7; bit >= 0; bit--)
		gamateUartReceiveBit(g, (0xA5 >> bit) & 1);
	assert(g->nmi);
	step(g);
	stateSize = gamateStateSize(g);
	state = malloc(stateSize);
	assert(state);
	bool savedInCallback = false;
	for (unsigned i = 0; i < 100 && g->cpu.pc != RETURN; i++) {
		if (g->cpu.pc >= 0x6023 && g->cpu.pc < 0x602C) {
			assert(gamateSaveState(g, state, stateSize));
			savedInCallback = true;
			break;
		}
		step(g);
	}
	assert(savedInCallback);
	g->cpu.a = 0;
	g->cpu.x = 0;
	g->cpu.y = 0;
	g->ram[0x08] = 0xFF;
	assert(gamateLoadState(g, state, stateSize));
	for (unsigned i = 0; i < 1000 && g->cpu.pc != RETURN; i++) step(g);
	assert(g->cpu.pc == RETURN && g->cpu.s == 0xFF);
	assert(g->cpu.a == 0xC1 && g->cpu.x == 0x34 && g->cpu.y == 0x56);
	assert(g->cpu.p == 0x24 && g->ram[0x08] == 1);
	assert(g->ram[0x0A] == 0xA5 && !g->nmi);
	free(state);

	g->rom[0x23] = 0x60;
	g->rom[0x24] = 0;
	g->rom[0x25] = 0;
	g->rom[0x26] = 0x4C;
	g->rom[0x27] = 0x30;
	g->rom[0x28] = 0x60;
}

static void testIrqWaits(struct Gamate* g) {
	static const struct {
		uint16_t slot;
		uint8_t threshold;
	} services[] = {
		{ 0xF003, 8 },
		{ 0xF006, 5 },
		{ 0xF009, 24 },
	};
	static const uint8_t waitPs[] = { 0x20, 0x28, 0x60 };
	static const uint8_t readyPs[] = { 0x24, 0x28, 0x60, 0x64 };

	for (size_t service = 0; service < sizeof(services) / sizeof(services[0]); service++) {
		for (unsigned counter = 0; counter <= 0xFF; counter++) {
			const uint8_t* ps = counter < services[service].threshold ? waitPs : readyPs;
			unsigned count = counter < services[service].threshold ?
				sizeof(waitPs) : sizeof(readyPs);
			for (unsigned i = 0; i < count; i++) {
				g->ram[0x0B] = (uint8_t) counter;
				g->ram[0x0C] = 0;
				uint64_t irqs = g->diagnostics.irqsTaken;
				uint64_t clocks = g->masterClocks;
				uint64_t vramWrites = g->diagnostics.vramWrites;
				uint64_t lcdWrites = g->diagnostics.lcdWrites;
				uint8_t p = ps[i];
				call(g, services[service].slot, 0xA5, 0x35, 0x68, p, 1000000);
				unsigned expected = counter < services[service].threshold ?
					services[service].threshold - counter : 0;
				assert(g->diagnostics.irqsTaken - irqs == expected);
				assert(g->ram[0x0B] == 0);
				assert(g->cpu.a == 0 && g->cpu.x == 0x35 && g->cpu.y == 0x68);
				assert(g->cpu.p == (uint8_t) ((p & 0x7C) | 0x03));
				assert(g->cpu.s == 0xFF);
				assert(g->diagnostics.vramWrites == vramWrites);
				assert(g->diagnostics.lcdWrites == lcdWrites);
				if (!expected) assert(g->masterClocks - clocks == 42);
			}
		}

		g->ram[0x0B] = (uint8_t) (services[service].threshold - 1);
		g->ram[0x0C] = 1;
		uint64_t irqs = g->diagnostics.irqsTaken;
		call(g, services[service].slot, 0xA5, 0x35, 0x68, 0x20, 100000);
		assert(g->diagnostics.irqsTaken - irqs == 1);
		assert(g->ram[0x0B] == 0);
		assert(g->cpu.a == 0 && g->cpu.x == 0x44 && g->cpu.y == 0x68);
		assert(g->cpu.p == 0x23 && g->cpu.s == 0xFF);
	}
}

static void testRecords(struct Gamate* g) {
	uint8_t expected[0x100];
	for (unsigned i = 0; i < sizeof(expected); i++) expected[i] = (uint8_t) (i ^ 0x5A);
	memcpy(g->ram, expected, sizeof(expected));
	for (unsigned i = 0; i < 16; i++) expected[0x23 + 8 * i] = expected[0x28 + 8 * i] = 0xFF;
	uint64_t clocks = g->masterClocks;
	call(g, 0xF024, 0x13, 0x49, 0x72, 0x24, 10000);
	assert(g->masterClocks - clocks == 824);
	assert(!memcmp(g->ram, expected, sizeof(expected)));
	assert(g->cpu.a == 0x80 && g->cpu.x == 0 && g->cpu.y == 0x80);
	assert(g->cpu.p == 0x66);
}

static void testClear(struct Gamate* g) {
	for (unsigned control = 0; control <= 0x40; control += 0x40) {
		memset(g->vram, 0xA5, sizeof(g->vram));
		g->ram[0x15] = (uint8_t) control;
		uint64_t writes = g->diagnostics.vramWrites, lcd = g->diagnostics.lcdWrites;
		uint64_t clocks = g->masterClocks;
		call(g, 0xF00C, 0x27, 0x81, 0x76, 0x64, 200000);
		for (unsigned row = 0; row < 256; row++) {
			for (unsigned column = 0; column < 32; column++) {
				bool cleared = control ? row <= 230 && column == 0 : row < 200;
				for (unsigned plane = 0; plane < 2; plane++) {
					size_t offset = ((row * 32 + column) << 1) | plane;
					assert(g->vram[offset] == (cleared ? 0 : 0xA5));
				}
			}
		}
		assert(g->diagnostics.vramWrites - writes == 12800);
		assert(g->diagnostics.lcdWrites - lcd == 13602);
		assert(g->masterClocks - clocks == 247326);
		assert(g->cpu.a == 0x40 && g->cpu.x == 0 && g->cpu.y == 0xC8);
		assert(g->cpu.p == 0x65 && g->ram[0x1E] == 0x80);
	}
}

static void testPlaceholder(struct Gamate* g) {
	memset(g->vram, 0xA5, sizeof(g->vram));
	g->ram[0x15] = 0;
	uint64_t writes = g->diagnostics.vramWrites, lcd = g->diagnostics.lcdWrites;
	uint64_t clocks = g->masterClocks;
	call(g, 0xF000, 0x12, 0x34, 0x56, 0x64, 10000);
	for (unsigned row = 0; row < 256; row++) {
		for (unsigned column = 0; column < 32; column++) {
			bool cleared = row >= 0x8C && row < 0x94 && column >= 8 && column < 12;
			for (unsigned plane = 0; plane < 2; plane++) {
				size_t offset = ((row * 32 + column) << 1) | plane;
				assert(g->vram[offset] == (cleared ? 0 : 0xA5));
			}
		}
	}
	assert(g->diagnostics.vramWrites - writes == 64 && g->diagnostics.lcdWrites - lcd == 81);
	assert(g->masterClocks - clocks == 3278);
	assert(g->cpu.a == 0x38 && g->cpu.x == 0x40 && g->cpu.y == 0x20);
	assert(g->cpu.p == 0x26 && g->vramAddress == 0x128B && g->vramPlane);
}

static unsigned glyphIndex(uint8_t selector) {
	if (selector <= 0x20) return 1;
	if (selector <= 0x40) return ((selector + 32u - 0x2F) & 31) + 1;
	return ((selector - 1u) & 31) + 1;
}

static uint16_t glyphAddress(uint8_t selector) {
	unsigned base = selector <= 0x40 ? 0x6021 : selector <= 0x60 ? 0x6079 : 0x6149;
	return (uint16_t) (base + 8 * glyphIndex(selector));
}

static void putExpected(uint8_t* vram, unsigned column, unsigned row, uint8_t value) {
	unsigned offset = (((row & 0xFF) * 32 + (column & 31)) << 1) | ((column >> 7) & 1);
	vram[offset] = value;
}

static void testGlyphs(struct Gamate* g) {
	for (unsigned i = 0; i < g->romSize; i++) g->rom[i] = (uint8_t) (i * 73 + i / 7 + 0x5D);
	for (unsigned selector = 0; selector < 256; selector++) {
		for (unsigned initialV = 0; initialV <= 0x40; initialV += 0x40) {
			memset(g->vram, 0xA5, sizeof(g->vram));
			g->ram[0] = 0;
			g->ram[1] = 3;
			g->ram[0x15] = 0;
			g->ram[0x1A] = 0xA5;
			g->ram[0x300] = 1;
			g->ram[0x301] = (uint8_t) selector;
			uint64_t writes = g->diagnostics.vramWrites, lcd = g->diagnostics.lcdWrites;
			call(g, 0xF012, 0x93, 0xF9, 0xFC, (uint8_t) (0x24 | initialV), 10000);
			for (unsigned row = 0; row < 8; row++) {
				unsigned address = (((0xFC + row) & 0xFF) * 32 + 31) * 2 + 1;
				assert(g->vram[address] == g->rom[glyphAddress(selector) - 0x6000 + row]);
			}
			bool carry = (selector >= 0x20 && selector <= 0x2E) ||
				(selector >= 0x81 && selector <= 0xA0) ||
				(selector >= 0xC1 && selector <= 0xE0);
			bool overflow = selector <= 0x20 ? initialV != 0 :
				selector >= 0x80 && selector <= 0xE0;
			assert(g->cpu.a == 1 && g->cpu.y == 1);
			assert(g->cpu.x == (uint8_t) (8 * glyphIndex(selector)));
			assert(g->cpu.p == (uint8_t) (0x26 | carry | (overflow ? 0x40 : 0)));
			assert(g->diagnostics.vramWrites - writes == 8);
			assert(g->diagnostics.lcdWrites - lcd == 11);
		}
	}

	uint8_t expected[0x4000];
	memset(expected, 0xA5, sizeof(expected));
	memset(g->vram, 0xA5, sizeof(g->vram));
	g->ram[0] = 0;
	g->ram[1] = 2;
	g->ram[0x15] = 0;
	g->ram[0x200] = 0;
	for (unsigned i = 1; i < 256; i++) g->ram[0x200 + i] = (uint8_t) (i * 17);
	for (unsigned tile = 0; tile < 256; tile++) {
		uint8_t selector = g->ram[0x200 + ((tile + 1) & 0xFF)];
		for (unsigned row = 0; row < 8; row++) {
			putExpected(expected, (0x80 + 31 + tile) & 0xFF, 0xFC + row,
				g->rom[glyphAddress(selector) - 0x6000 + row]);
		}
	}
	call(g, 0xF012, 0x93, 0xF9, 0xFC, 0x24, 1000000);
	assert(!memcmp(g->vram, expected, sizeof(expected)));
	assert(g->cpu.a == 0 && g->cpu.y == 0 && g->cpu.x == 8);

	g->ram[0] = 0;
	g->ram[1] = 3;
	g->ram[0x300] = 2;
	g->ram[0x301] = 0x80;
	g->ram[0x302] = 0x20;
	call(g, 0xF012, 0, 0, 0, 0x24, 10000);
	assert(g->cpu.a == 2 && g->cpu.y == 2 && g->cpu.p == 0x67);
	g->ram[0x301] = 0x41;
	call(g, 0xF012, 0, 0, 0, 0x64, 10000);
	assert(g->cpu.p == 0x27);

	g->ram[0x300] = 1;
	g->ram[0x301] = 0x41;
	memset(g->rom + glyphAddress(0x41) - 0x6000, 0, 8);
	memset(g->vram, 0x5A, sizeof(g->vram));
	uint64_t writes = g->diagnostics.vramWrites;
	call(g, 0xF012, 0, 0, 0, 0x24, 10000);
	assert(g->diagnostics.vramWrites - writes == 8);
	for (unsigned row = 0; row < 8; row++) assert(g->vram[(row * 32 << 1) | 1] == 0);

	prepareCallWithStack(g, 0xF012, 0, 0, 0, 0x24, 3);
	runUntil(g, RETURN, 10000);
	assert(g->cpu.s == 3 && g->cpu.a == 1 && g->cpu.x == 8 && g->cpu.y == 1);
}

static void testTileStream(struct Gamate* g) {
	static const uint8_t stream[] = {
		3, 1, 0, 0x1F, 0x20, 0xFF, 0xFF, 0xFE, 0xFF, 0xFE,
	};
	uint8_t expected[0x4000];
	memset(expected, 0xA5, sizeof(expected));
	memset(g->vram, 0xA5, sizeof(g->vram));
	g->ram[0] = 0;
	g->ram[1] = 3;
	g->ram[6] = 0xF9;
	g->ram[7] = 0x62;
	g->ram[8] = 0x7F;
	g->ram[9] = 0x6B;
	g->ram[0x15] = 0;
	memcpy(g->ram + 0x300, stream, sizeof(stream));
	for (unsigned pass = 0; pass < 2; pass++) {
		unsigned base = pass ? 0x6B7F : 0x62F9;
		const uint8_t selectors[] = { 1, 0x1F, 0x20, 0xFE };
		const uint8_t columns[] = { 0, 2, 3, 0 };
		const uint8_t rows[] = { 3, 3, 3, 0xFF };
		for (unsigned tile = 0; tile < 4; tile++) {
			for (unsigned row = 0; row < 8; row++) {
				putExpected(expected, (pass ? 0x80 : 0) + columns[tile],
					8 * rows[tile] + row, g->rom[base + 8 * selectors[tile] + row - 0x6000]);
			}
		}
	}
	uint64_t writes = g->diagnostics.vramWrites;
	call(g, 0xF01E, 0x42, 0x55, 0x71, 0x64, 100000);
	assert(!memcmp(g->vram, expected, sizeof(expected)));
	assert(g->diagnostics.vramWrites - writes == 64);
	assert(g->cpu.a == 0xFE && g->cpu.x == 0xFF && g->cpu.y == 9 && g->cpu.p == 0x27);
	assert(g->ram[0] == 0 && g->ram[1] == 3 && g->ram[2] == 0x7F && g->ram[3] == 0x72);
	assert(g->ram[0x18] == 0x81 && g->ram[0x19] == 0xFF);
	assert(g->ram[0x1A] == 0xF0 && g->ram[0x1B] == 0);

	g->ram[0x300] = 0xFE;
	memset(g->ram + 0x18, 0xA5, 4);
	g->ram[2] = 0xA5;
	g->ram[3] = 0xA5;
	writes = g->diagnostics.vramWrites;
	call(g, 0xF01E, 0x42, 0x55, 0x71, 0x64, 10000);
	assert(g->cpu.a == 0xFE && g->cpu.x == 0x55 && g->cpu.y == 0 && g->cpu.p == 0x67);
	assert(g->diagnostics.vramWrites == writes && g->ram[2] == 0xA5 && g->ram[3] == 0xA5);
	for (unsigned i = 0x18; i <= 0x1B; i++) assert(g->ram[i] == 0xA5);

	g->ram[0] = 0;
	g->ram[1] = 2;
	memset(g->ram + 0x200, 0, 0x100);
	g->ram[0x2FF] = 0xFF;
	g->ram[0x300] = 0xFE;
	call(g, 0xF01E, 0, 0x55, 0, 0x64, 200000);
	assert(g->ram[0] == 0 && g->ram[1] == 3 && g->cpu.y == 0 && g->cpu.x == 0x55);
	assert(g->cpu.a == 0xFE && g->cpu.p == 0x27 && g->ram[0x18] == 0x7E);

	g->ram[0] = 0;
	g->ram[1] = 3;
	memcpy(g->ram + 0x300, stream, sizeof(stream));
	prepareCallWithStack(g, 0xF01E, 0, 0x55, 0, 0x24, 5);
	runUntil(g, RETURN, 100000);
	assert(g->cpu.s == 5 && g->cpu.a == 0xFE && g->cpu.x == 0xFF && g->cpu.y == 9);
}

static void testRectangle(struct Gamate* g) {
	for (unsigned plane = 0; plane < 2; plane++) {
		for (unsigned shape = 0; shape < 4; shape++) {
			uint8_t expected[0x4000];
			memset(expected, 0xA5, sizeof(expected));
			memset(g->vram, 0xA5, sizeof(g->vram));
			g->ram[0x15] = 0;
			g->ram[0x1C] = shape == 0 ? 16 : shape == 1 ? 0 : shape == 2 ? 8 : 8;
			g->ram[0x1D] = shape == 0 ? 3 : shape == 1 ? 1 : shape == 2 ? 0 : 1;
			unsigned columns = shape == 0 ? 2 : shape == 1 ? 256 : 1;
			unsigned rows = shape == 0 ? 3 : shape == 1 ? 1 : shape == 2 ? 256 : 1;
			for (unsigned column = 0; column < columns; column++) {
				for (unsigned row = 0; row < rows; row++)
					putExpected(expected, (0x80 * plane + 31 + column) & 0xFF, 0xFE + row, 0x5A);
			}
			uint64_t writes = g->diagnostics.vramWrites, lcd = g->diagnostics.lcdWrites;
			uint64_t clocks = g->masterClocks;
			call(g, plane ? 0xF02D : 0xF02A, 0x5A, 0xF8, 0xFE, 0x64, 100000);
			/* SWK plane 1 remains two master clocks short for ordinary nonzero dimensions. */
			unsigned baseline = plane ? 118u : 126u;
			assert(g->masterClocks - clocks ==
				baseline + columns * 58u + columns * rows * 18u);
			assert(!memcmp(g->vram, expected, sizeof(expected)));
			assert(g->diagnostics.vramWrites - writes == columns * rows);
			assert(g->diagnostics.lcdWrites - lcd == 1 + columns * (2 + rows));
			assert(g->cpu.a == 0x5A && g->cpu.x == 0 && g->cpu.y == 0 && g->cpu.p == 0x66);
			assert(g->ram[0xA8] == 0x80 * plane && g->ram[0x19] == 0xFE && g->ram[0x1A] == 0x5A);
		}
	}
}

static void testBitmapCopy(struct Gamate* g) {
	static const struct {
		uint8_t horizontal;
		uint8_t rows;
		uint16_t plane0;
		uint16_t plane1;
	} shapes[] = {
		{ 8, 8, 0x6000, 0x6100 },
		{ 8, 1, 0x6000, 0x6100 },
		{ 16, 8, 0x6000, 0x6100 },
		{ 24, 8, 0x6000, 0x6100 },
		{ 32, 8, 0x6000, 0x6100 },
		{ 24, 8, 0x6000, 0x60FE },
	};
	for (size_t s = 0; s < sizeof(shapes) / sizeof(shapes[0]); s++) {
		for (unsigned i = 0; i < 0x4000; i++)
			g->rom[i] = (uint8_t) (i * 73 + i / 7);
		uint8_t expected[0x4000];
		memset(expected, 0xA5, sizeof(expected));
		memset(g->vram, 0xA5, sizeof(g->vram));
		g->ram[0x00] = (uint8_t) shapes[s].plane0;
		g->ram[0x01] = (uint8_t) (shapes[s].plane0 >> 8);
		g->ram[0x02] = (uint8_t) shapes[s].plane1;
		g->ram[0x03] = (uint8_t) (shapes[s].plane1 >> 8);
		g->ram[0x15] = 0x20;
		g->ram[0x1C] = shapes[s].horizontal;
		g->ram[0x1D] = shapes[s].rows;
		unsigned columns = shapes[s].horizontal >> 3;
		unsigned rows = shapes[s].rows;
		unsigned plane0Offset = shapes[s].plane0 - 0x6000;
		unsigned plane1Offset = shapes[s].plane1 - 0x6000;
		unsigned crossingReads = 0, pointerCarries = 0;
		uint8_t finalByte = 0;
		for (unsigned column = 0; column < columns; column++) {
			for (unsigned row = 0; row < rows; row++) {
				crossingReads += ((shapes[s].plane0 + column * rows) & 0xFF) + row > 0xFF;
				crossingReads += ((shapes[s].plane1 + column * rows) & 0xFF) + row > 0xFF;
				uint8_t source0 = g->rom[(plane0Offset + column * rows + row) & 0x3FFF];
				uint8_t source1 = g->rom[(plane1Offset + column * rows + row) & 0x3FFF];
				putExpected(expected, column + 2, row + 0xF8, source0);
				putExpected(expected, 0x80 + column + 2, row + 0xF8, source1);
				if (column == columns - 1 && row == rows - 1)
					finalByte = source1;
			}
			pointerCarries += ((shapes[s].plane0 + column * rows) & 0xFF) + rows > 0xFF;
			pointerCarries += ((shapes[s].plane1 + column * rows) & 0xFF) + rows > 0xFF;
		}
		uint64_t writes = g->diagnostics.vramWrites;
		uint64_t lcd = g->diagnostics.lcdWrites;
		uint64_t clocks = g->masterClocks;
		call(g, 0xF015, 0xA5, 0x13, 0xF8, 0x64, 100000);
		assert(g->masterClocks - clocks == 362u + columns * (200u + 76u * rows) +
			2u * crossingReads + 8u * pointerCarries);
		assert(!memcmp(g->vram, expected, sizeof(g->vram)));
		assert(g->diagnostics.vramWrites - writes == 2 * columns * rows);
		assert(g->diagnostics.lcdWrites - lcd == 1 + 2 * columns * (2 + rows));
		assert(g->cpu.a == finalByte);
		assert(g->cpu.x == 0x13);
		assert(g->cpu.y == (columns * rows & 0xFF));
		assert(g->cpu.p == 0x66);
		assert(g->ram[0x000] == (uint8_t) shapes[s].plane1);
		assert(g->ram[0x001] == (uint8_t) (shapes[s].plane1 >> 8));
		assert(g->ram[0x002] == (uint8_t) shapes[s].plane1);
		assert(g->ram[0x003] == (uint8_t) (shapes[s].plane1 >> 8));
	}
}

static void testFixedRectangle(struct Gamate* g) {
	static const uint8_t controls[] = { 0, 0x40, 0x80, 0xFF };
	static const uint8_t ps[] = { 0x24, 0x28, 0x64, 0x68 };
	for (unsigned c = 0; c < sizeof(controls); c++) {
		for (unsigned p = 0; p < sizeof(ps); p++) {
			uint8_t expected[0x4000];
			memset(expected, 0xA5, sizeof(expected));
			memset(g->vram, 0xA5, sizeof(g->vram));
			g->ram[0x15] = controls[c];
			g->ram[0x18] = 0x5A;
			g->ram[0x19] = 0x5A;
			g->ram[0x1A] = 0x5A;
			g->ram[0x1C] = 0x5A;
			g->ram[0x1D] = 0x5A;
			g->ram[0xA8] = 0x5A;
			uint64_t writes = g->diagnostics.vramWrites;
			uint64_t lcd = g->diagnostics.lcdWrites;
			uint64_t clocks = g->masterClocks;
			prepareCall(g, 0xF00F, 0xA5, 0x35, 0x68, ps[p]);
			for (unsigned instruction = 0; instruction < 20000 && g->cpu.pc != RETURN; instruction++)
				step(g);
			assert(g->cpu.pc == RETURN);
			for (unsigned row = 0xE6; row <= 0xFE; row++) {
				for (unsigned column = 0; column < 31; column++)
					putExpected(expected, column, row, 0);
			}
			assert(!memcmp(g->vram, expected, sizeof(g->vram)));
			assert(g->diagnostics.vramWrites - writes == 775);
			assert(g->diagnostics.lcdWrites - lcd == 829);
			assert(g->masterClocks - clocks == 15930);
			assert(g->cpu.a == 0 && g->cpu.x == 0 && g->cpu.y == 0);
			assert(g->cpu.p == (uint8_t) ((ps[p] & 0x7C) | 0x03));
			assert(g->cpu.s == 0xFF);
			assert(g->ram[0x18] == 0x1F && g->ram[0x19] == 0xE6);
			assert(g->ram[0x1A] == 0 && g->ram[0x1C] == 0xFF);
			assert(g->ram[0x1D] == 0x19 && g->ram[0xA8] == 0);
			assert(g->vramAddress == 0x1FFE);
			assert(!g->vramPlane && g->incrementRow);
		}
	}
}

static void testRandom(struct Gamate* g) {
	for (uint32_t input = 0; input <= UINT16_MAX; input++) {
		for (unsigned p = 0x24; p <= 0x6C; p += 0x48) {
			g->ram[0x12] = (uint8_t) input;
			g->ram[0x13] = (uint8_t) (input >> 8);
			g->ram[0x14] = 0x5A;
			uint64_t clocks = g->masterClocks;
			call(g, 0xF030, 0x11, 0x22, 0x33, (uint8_t) p, 1000);
			uint16_t state = input ? (uint16_t) input : 0xFFFF;
			uint16_t next = (uint16_t) ((state << 1) | (((state >> 4) ^ (state >> 10)) & 1));
			assert(g->ram[0x12] == (uint8_t) next && g->ram[0x13] == next >> 8);
			assert(g->ram[0x14] == ((state >> 3) & 0x80));
			assert(g->cpu.a == (uint8_t) next && g->cpu.x == 0x22 && g->cpu.y == 0x33);
			assert(g->cpu.p == (uint8_t) (0x20 | (p & 0x4C) | (state >> 15) |
				(next & 0x80) | ((next & 0xFF) ? 0 : 2)));
			assert(g->masterClocks - clocks == (input ? 174 : 188));
		}
	}
}

static void testDigits(struct Gamate* g) {
	static const uint32_t inputs[] = { 0x123456, 0x000001, 0x000000, 0x0012FF, 0x0000FF, 0x090807 };
	for (size_t i = 0; i < sizeof(inputs) / sizeof(inputs[0]); i++) {
		uint32_t input = inputs[i];
		memset(g->ram + 0xA1, 0xA5, 8);
		call(g, 0xF033, (uint8_t) (input >> 16), (uint8_t) (input >> 8), (uint8_t) input,
			0x6C, 1000);
		bool blankSuffix = (uint8_t) input == 0xFF;
		unsigned first = 0, limit = blankSuffix ? 4 : 5;
		while (first < limit && !((input >> (20 - 4 * first)) & 15)) first++;
		for (unsigned j = 0; j < 6; j++) {
			uint8_t digit = (uint8_t) (0x30 | ((input >> (20 - 4 * j)) & 15));
			assert(g->ram[0xA2 + j] == (j < first || (blankSuffix && j >= 4) ? 0x20 : digit));
		}
		assert(g->ram[0xA1] == 6 && g->ram[0xA8] == 0xA5);
		assert(g->cpu.a == 6 && g->cpu.x == (uint8_t) (input >> 8) && g->cpu.y == first);
		assert(g->cpu.p == (0x6C | (blankSuffix && first == 4 ? 0 : 1)));
	}
}

static void testPairStream(struct Gamate* g) {
	static const uint8_t stream[] = {
		2, 0, 3, 0x21, 2, 0xFF, 5, 1, 1, 0xFF, 0xFE,
	};
	uint8_t expected[0x4000];
	memset(expected, 0xA5, sizeof(expected));
	memset(g->vram, 0xA5, sizeof(g->vram));
	g->ram[0] = 0;
	g->ram[1] = 3;
	g->ram[6] = 0xF9;
	g->ram[7] = 0x62;
	g->ram[8] = 0x7F;
	g->ram[9] = 0x6B;
	g->ram[0x15] = 0;
	memcpy(g->ram + 0x300, stream, sizeof(stream));
	for (unsigned plane = 0; plane < 2; plane++) {
		unsigned base = plane ? 0x6B7F : 0x62F9;
		const uint8_t tiles[] = { 0x21, 0x21, 1 };
		const uint8_t columns[] = { 3, 4, 0 };
		const uint8_t rows[] = { 2, 2, 5 };
		for (unsigned i = 0; i < 3; i++) {
			for (unsigned row = 0; row < 8; row++) {
				putExpected(expected, 0x80 * plane + columns[i], 8 * rows[i] + row,
					g->rom[base + 8 * tiles[i] + row - 0x6000]);
			}
		}
	}
	uint64_t writes = g->diagnostics.vramWrites;
	call(g, 0xF018, 0x42, 0x55, 0x71, 0x74, 100000);
	assert(!memcmp(g->vram, expected, sizeof(expected)));
	assert(g->diagnostics.vramWrites - writes == 48);
	assert(g->cpu.a == 0xFE && g->cpu.x == 0xFF && g->cpu.y == 10 && g->cpu.p == 0x37);
	assert(g->ram[0] == 0 && g->ram[1] == 3 && g->ram[2] == 0x7F && g->ram[3] == 0x6B);
	assert(g->ram[0x18] == 0x81 && g->ram[0x19] == 5);
	assert(g->ram[0x1A] == 8 && g->ram[0x1B] == 0);

	static const uint8_t transparent[] = { 1, 0, 0x80, 0xFF, 0xFE };
	memcpy(g->ram + 0x300, transparent, sizeof(transparent));
	writes = g->diagnostics.vramWrites;
	call(g, 0xF018, 0x42, 0x55, 0x71, 0x24, 100000);
	assert(g->diagnostics.vramWrites == writes);
	assert(g->cpu.a == 0xFE && g->cpu.x == 0x55 && g->cpu.y == 4 && g->cpu.p == 0x67);
	assert(g->ram[0x18] == 0 && g->ram[0x19] == 1 && g->ram[0x1A] == 0 && g->ram[0x1B] == 0x80);
}

static void putLinear(uint8_t* vram, unsigned address, uint8_t value) {
	vram[(address & 0x1FFF) << 1] = value;
}

static void testRowRuns(struct Gamate* g) {
	static const uint8_t stream[] = {
		2, 0x11, 3, 0x22, 0, 0xFF, 0x80, 0x33, 1, 0xFF, 0xFE,
	};
	uint8_t expected[0x4000];
	for (size_t i = 0; i < sizeof(expected); i++) expected[i] = (uint8_t) (i * 37 + i / 5);
	memcpy(g->vram, expected, sizeof(expected));
	for (unsigned address = 0xE6 * 32; address < 0xFF * 32; address++)
		putLinear(expected, address, 0);
	unsigned address = 0xE8 * 32;
	for (unsigned i = 0; i < 3; i++) putLinear(expected, address++, 0x11);
	for (unsigned i = 0; i < 256; i++) putLinear(expected, address++, 0x22);
	putLinear(expected, 0x66 * 32, 0x33);
	g->ram[0] = 0;
	g->ram[1] = 3;
	g->ram[0x15] = 0;
	g->ram[0x1A] = 0xA5;
	memcpy(g->ram + 0x300, stream, sizeof(stream));
	uint64_t writes = g->diagnostics.vramWrites;
	call(g, 0xF01B, 0x42, 0x55, 0x71, 0x34, 100000);
	assert(!memcmp(g->vram, expected, sizeof(expected)));
	assert(g->diagnostics.vramWrites - writes == 25 * 32 + 260);
	assert(g->cpu.a == 0xFE && g->cpu.x == 0 && g->cpu.y == 10 && g->cpu.p == 0x77);
	assert(g->ram[0] == 0 && g->ram[1] == 3 && g->ram[0x1A] == 0x33);
}

static void testRowLiterals(struct Gamate* g) {
	static const uint8_t stream[] = {
		2, 0x11, 0x22, 0xFF, 5, 0x33, 0xFF, 0xFE,
	};
	uint8_t expected[0x4000];
	for (unsigned i = 0; i < sizeof(expected); i++) expected[i] = (uint8_t) (i * 37 + i / 5);
	memcpy(g->vram, expected, sizeof(expected));
	for (unsigned address = 0xE6 * 32; address < 0xFF * 32; address++)
		putLinear(expected, address, 0);
	putLinear(expected, 0xE8 * 32, 0x11);
	putLinear(expected, 0xE8 * 32 + 1, 0x22);
	putLinear(expected, 0xEB * 32, 0x33);
	g->ram[0] = 0;
	g->ram[1] = 3;
	g->ram[0x15] = 0;
	g->ram[0x1A] = 0xA5;
	memcpy(g->ram + 0x300, stream, sizeof(stream));
	uint64_t writes = g->diagnostics.vramWrites;
	uint64_t lcd = g->diagnostics.lcdWrites;
	call(g, 0xF021, 0x42, 0x55, 0x71, 0x24, 100000);
	assert(!memcmp(g->vram, expected, sizeof(g->vram)));
	assert(g->diagnostics.vramWrites - writes == 25 * 32 + 3);
	assert(g->diagnostics.lcdWrites - lcd == 858);
	assert(g->cpu.a == 0xFE && g->cpu.x == 0 && g->cpu.y == 7);
	assert(g->cpu.p == 0x27);
	assert(g->ram[0] == 0 && g->ram[1] == 3);
	assert(g->ram[0x1A] == 0xA5);
}

static uint8_t vramAt(const uint8_t* vram, unsigned column, unsigned row) {
	return vram[(((row & 0xFF) * 32 + (column & 31)) << 1) | ((column >> 7) & 1)];
}

static uint8_t sourceRow(const struct Gamate* g, unsigned plane, uint8_t selector, unsigned row) {
	unsigned base = g->ram[6 + 2 * plane] | g->ram[7 + 2 * plane] << 8;
	return g->rom[((base + 8 * selector + row) & 0xFFFF) - 0x6000];
}

static void modelRedraw(const struct Gamate* g, uint8_t* vram, const uint8_t* record, unsigned q) {
	uint8_t selectors[9];
	for (unsigned c = 0; c < q; c++) {
		for (unsigned r = 0; r < q; r++)
			selectors[c * q + r] = vramAt(vram, (record[5] >> 3) + c, 0xE6 + (record[6] >> 3) + r);
	}
	for (unsigned plane = 0; plane < 2; plane++) {
		for (unsigned c = 0; c < q; c++) {
			for (unsigned r = 0; r < q; r++) {
				for (unsigned k = 0; k < 8; k++) {
					putExpected(vram, 0x80 * plane + (((record[5] >> 3) + c) & 31),
						(record[6] & 0xF8) + 8 * r + k,
						sourceRow(g, plane, selectors[c * q + r], k));
				}
			}
		}
	}
}

static void modelObject(const struct Gamate* g, uint8_t* vram, const uint8_t* record, bool large) {
	unsigned height = large ? 16 : 8, columns = large ? 3 : 2, shift = record[0] & 7;
	for (unsigned row = 0; row < height; row++) {
		uint8_t source[2][3];
		for (unsigned plane = 0; plane < 2; plane++) {
			uint32_t bits;
			if (large) {
				uint8_t s = (uint8_t) (record[2] + 2 * (row >> 3));
				bits = (uint32_t) sourceRow(g, plane, s, row & 7) << 16 |
					(uint32_t) sourceRow(g, plane, (uint8_t) (s + 1), row & 7) << 8;
			} else {
				bits = (uint32_t) sourceRow(g, plane, record[2], row) << 16;
			}
			bits >>= shift;
			for (unsigned c = 0; c < columns; c++) source[plane][c] = (uint8_t) (bits >> (16 - 8 * c));
		}
		for (unsigned c = 0; c < columns; c++) {
			unsigned column = (record[0] >> 3) + c, y = record[1] + row;
			uint8_t mask = (uint8_t) ~(source[0][c] | source[1][c]);
			for (unsigned plane = 0; plane < 2; plane++) {
				uint8_t old = vramAt(vram, 0x80 * plane + column, y);
				putExpected(vram, 0x80 * plane + (column & 31), y,
					(uint8_t) (source[plane][c] | (old & mask)));
			}
		}
	}
}

static void testCompositor(struct Gamate* g) {
	g->ram[6] = 0xF9;
	g->ram[7] = 0x62;
	g->ram[8] = 0x7F;
	g->ram[9] = 0x6B;
	g->ram[0x15] = 0;
	memset(g->ram + 0x21, 0xFF, 0x80);
	memset(g->ram + 0x300, 0xA5, 0xC8);
	uint64_t writes = g->diagnostics.vramWrites, lcd = g->diagnostics.lcdWrites;
	call(g, 0xF027, 0x8C, 0x47, 0x19, 0x64, 100000);
	assert(g->diagnostics.vramWrites == writes && g->diagnostics.lcdWrites == lcd);
	assert(g->cpu.a == 0x8C && g->cpu.x == 0x47 && g->cpu.y == 0x19 && g->cpu.p == 0xA5);
	for (unsigned i = 0; i < 8; i++) assert(g->ram[0x300 + i] == 0xFF);
	assert(g->ram[0x308] == 0xA5);

	static const uint8_t largeObject[8] = { 0x46, 0x70, 0x1E, 0, 0, 0, 0, 0xFF };
	static const uint8_t largeRedraw[8] = { 0, 0, 0xFF, 0, 0, 0x50, 0x58, 0 };
	static const uint8_t smallBoth[8] = { 0x2B, 0x31, 0x05, 0, 0, 0x13, 0x0A, 0 };
	memcpy(g->ram + 0x21, largeObject, 8);
	memcpy(g->ram + 0x29, largeRedraw, 8);
	memcpy(g->ram + 0x21 + 8 * 8, smallBoth, 8);
	uint8_t expected[0x4000];
	for (size_t i = 0; i < sizeof(expected); i++) expected[i] = (uint8_t) (i * 37 + i / 5);
	memcpy(g->vram, expected, sizeof(expected));
	modelRedraw(g, expected, largeRedraw, 3);
	modelRedraw(g, expected, smallBoth, 2);
	modelObject(g, expected, largeObject, true);
	modelObject(g, expected, smallBoth, false);
	writes = g->diagnostics.vramWrites;
	call(g, 0xF027, 0x0C, 0x47, 0x19, 0x65, 1000000);
	assert(!memcmp(g->vram, expected, sizeof(expected)));
	assert(g->diagnostics.vramWrites - writes == 144 + 64 + 96 + 32);
	assert(g->cpu.a == 0x0C && g->cpu.x == 0x47 && g->cpu.y == 0x19 && g->cpu.p == 0x25);
	for (unsigned i = 0; i < 8; i++) assert(g->ram[0x300 + i] == 0xFF);
	assert(g->ram[0] == 0xA0 && g->ram[1] == 3 && g->ram[2] == 0x7F && g->ram[3] == 0x6B);
	assert(g->ram[0x18] == 0x85 && g->ram[0x19] == 0x31 && g->ram[0x1A] == 5);
	assert(g->ram[0x1B] == 1 && g->ram[0x1C] == 0 && g->ram[0x1D] == 0);
	assert(g->ram[0x1E] == 2 && g->ram[0x1F] == 0xE7);
}

int main(int argc, char** argv) {
	assert(argc == 2);
	FILE* image = fopen(argv[1], "rb");
	if (!image) {
		fprintf(stderr, "could not open experimental firmware\n");
		return 1;
	}
	uint8_t bios[GAMATE_BIOS_SIZE], rom[0x4000];
	assert(fread(bios, 1, sizeof(bios), image) == sizeof(bios));
	assert(fgetc(image) == EOF && !ferror(image));
	assert(!fclose(image));
	makeCartridge(rom, sizeof(rom));
	struct Gamate g;
	gamateInit(&g);
	assert(gamateLoad(&g, rom, sizeof(rom), bios, sizeof(bios)));
	testBoot(&g);
	testIrq(&g);
	testNmi(&g);
	testIrqWaits(&g);
	testRecords(&g);
	testClear(&g);
	testPlaceholder(&g);
	testGlyphs(&g);
	testTileStream(&g);
	testRectangle(&g);
	testBitmapCopy(&g);
	testFixedRectangle(&g);
	testRandom(&g);
	testDigits(&g);
	testPairStream(&g);
	testRowRuns(&g);
	testRowLiterals(&g);
	testCompositor(&g);
	gamateFree(&g);
	puts("experimental firmware synthetic tests passed");
	return 0;
}
