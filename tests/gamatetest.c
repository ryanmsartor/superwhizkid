/* Copyright (c) 2026 Ry
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
#include "gamate/gamate.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

struct LinkCapture {
	struct Gamate* receiver;
	uint8_t bits, count;
};

struct RamAccessCapture {
	unsigned count;
	bool write;
	uint16_t address;
	uint8_t previous, value, openBusBefore;
};

struct RomReadCapture {
	unsigned count;
	uint16_t address;
	size_t physicalOffset;
};

struct WatchpointCapture {
	unsigned count;
	bool write, dummy;
	uint16_t address;
	uint8_t previous, value;
};

static void captureRamAccess(void* context, const struct Gamate* g, bool write,
		uint16_t address, uint8_t previous, uint8_t value) {
	struct RamAccessCapture* capture = context;
	assert(g);
	capture->count++;
	capture->write = write;
	capture->address = address;
	capture->previous = previous;
	capture->value = value;
	capture->openBusBefore = g->openBus;
}

static void captureRomRead(void* context, const struct Gamate* g,
		uint16_t address, size_t physicalOffset) {
	struct RomReadCapture* capture = context;
	assert(g);
	capture->count++;
	capture->address = address;
	capture->physicalOffset = physicalOffset;
}

static void captureWatchpoint(void* context, const struct Gamate* g,
		bool write, bool dummy, uint16_t address,
		uint8_t previous, uint8_t value) {
	struct WatchpointCapture* capture = context;
	assert(g);
	capture->count++;
	capture->write = write;
	capture->dummy = dummy;
	capture->address = address;
	capture->previous = previous;
	capture->value = value;
}

static const char* labelAt(void* context, uint16_t address) {
	(void) context;
	if (address == 0xE010) return "subroutine";
	if (address == 0xE003) return "return_site";
	return NULL;
}

static void captureLinkBit(void* context, bool bit) {
	struct LinkCapture* capture = context;
	capture->bits = (uint8_t) (capture->bits << 1 | bit);
	capture->count++;
	gamateUartReceiveBit(capture->receiver, bit);
}

static void testMapperDetection(void) {
	uint8_t bios[GAMATE_BIOS_SIZE] = { 0 };
	uint8_t* rom = calloc(1, 0x20000);
	assert(rom);

	struct Gamate g;
	gamateInit(&g);
	assert(gamateLoad(&g, rom, 0x20000, bios, sizeof(bios)));
	assert(g.mapper == GAMATE_MAPPER_BANKED);
	gamateFree(&g);

	for (size_t offset = 0; offset < 0x20000; offset += 0x8000) {
		memcpy(rom + offset + 5, "COPYRIGHT BIT CORPORATION", 25);
	}
	assert(gamateLoad(&g, rom, 0x20000, bios, sizeof(bios)));
	assert(g.mapper == GAMATE_MAPPER_4IN1);
	gamateFree(&g);

	rom[0x10000 + 0x1000] = 1;
	assert(gamateLoad(&g, rom, 0x20000, bios, sizeof(bios)));
	assert(g.mapper == GAMATE_MAPPER_BANKED);
	gamateFree(&g);
	rom[0x10000 + 0x1000] = 0;

	rom[0x10000 + 5] = 0;
	assert(gamateLoad(&g, rom, 0x20000, bios, sizeof(bios)));
	assert(g.mapper == GAMATE_MAPPER_BANKED);
	gamateFree(&g);
	free(rom);
}

static void testMemory(void) {
	struct Gamate g;
	gamateInit(&g);
	uint8_t bios[GAMATE_BIOS_SIZE], rom[0x10000];
	for (size_t i = 0; i < sizeof(bios); i++) bios[i] = (uint8_t) i;
	for (size_t i = 0; i < sizeof(rom); i++) rom[i] = (uint8_t) (i >> 14);
	assert(gamateLoad(&g, rom, sizeof(rom), bios, sizeof(bios)));
	gamateResetRamDiagnostics(&g);
	g.diagnostics.diagnosticInstructionPc = 0x6000;
	gamateRead(&g, 0x000A);
	gamateWrite(&g, 0x000B, 0x12);
	gamateRead(&g, 0x000B);
	g.diagnostics.diagnosticInstructionPc = 0xE000;
	gamateRead(&g, 0x000C);
	assert(g.diagnostics.ramTrackedWrites[0] == 1 && g.diagnostics.ramUniqueWritten == 1);
	assert(g.diagnostics.ramReadBeforeWrite[0] == 1 &&
		g.diagnostics.ramUniqueReadBeforeWrite[0] == 1);
	assert(g.diagnostics.ramReadBeforeWrite[1] == 1 &&
		g.diagnostics.ramUniqueReadBeforeWrite[1] == 1);
	gamateResetRamDiagnostics(&g);
	struct RamAccessCapture capture = { 0 };
	g.ram[0x123] = 0;
	gamateSetRamDiagnosticWatch(&g, 0x123, captureRamAccess, &capture);
	gamateWrite(&g, 0x0123, 0x42);
	assert(capture.count == 1 && capture.write && capture.address == 0x123);
	assert(capture.previous == 0 && capture.value == 0x42);
	assert(gamateRead(&g, 0x0523) == 0x42);
	assert(capture.count == 2 && !capture.write && capture.previous == 0x42 && capture.value == 0x42);
	gamateSetRamDiagnosticWatch(&g, 0, NULL, NULL);

	gamateWrite(&g, 0x0123, 0x42);
	assert(gamateRead(&g, 0x0523) == 0x42);
	assert(gamateRead(&g, 0xE123) == 0x23);
	assert(gamateRead(&g, 0xF123) == 0x23);
	assert(gamateRead(&g, 0x6000) == 0); // protected before the BIOS handshake
	static const char key[] = "BIT CORPORATION";
	for (size_t byte = 0; byte < sizeof(key) - 1; byte++) {
		for (int bit = 7; bit >= 0; bit--) {
			gamateWrite(&g, 0x6000, (key[byte] >> bit & 1) << 2);
		}
		gamateWrite(&g, 0x6000, 0); // latch the completed byte
	}
	uint8_t reply = 0;
	for (int bit = 0; bit < 8; bit++) reply = (reply << 1) | (gamateRead(&g, 0x6000) >> 1);
	assert(reply == 0x47 && g.protectionPassed);
	assert(gamateRead(&g, 0x6000) == 0);
	assert(gamateRead(&g, 0xA000) == 0);
	gamateWrite(&g, 0xC000, 3);
	assert(gamateRead(&g, 0xA000) == 3);
	assert(g.diagnostics.bankWrites == 1);
	gamateSetMapper(&g, GAMATE_MAPPER_4IN1);
	gamateWrite(&g, 0x8000, 2);
	assert(gamateRead(&g, 0x6000) == 2);
	assert(g.diagnostics.bankWrites == 2);
	gamateResetRomDiagnostics(&g);
	struct RomReadCapture romCapture = { 0 };
	gamateSetRomDiagnosticWatch(&g, captureRomRead, &romCapture);
	g.diagnostics.diagnosticInstructionPc = 0xE000;
	assert(gamateRead(&g, 0x6000) == 2);
	assert(gamateRead(&g, 0x6000) == 2);
	g.diagnostics.diagnosticInstructionPc = 0x6000;
	assert(gamateRead(&g, 0x6001) == 2);
	assert(g.diagnostics.romTrackedReads == 2 && g.diagnostics.romUniqueRead == 1);
	assert(g.diagnostics.romRead[0x8000 >> 3] & 1);
	assert(romCapture.count == 2 && romCapture.address == 0x6000 &&
		romCapture.physicalOffset == 0x8000);
	gamateSetRomDiagnosticWatch(&g, NULL, NULL);

	assert(gamateRead(&g, 0x4100) == 0x41); // write-only PSG: open bus
	assert(gamateRead(&g, 0x5800) == 0x58);
	assert(gamateRead(&g, 0x5A00) == 0x5B);
	g.rom[0x3FFF] = 0x73;
	g.bios[0] = 0xA6;
	assert(gamateRead(&g, 0x5FFF) == 0x5F && g.openBus == 0x5F);
	assert(gamateRead(&g, 0xDFFF) == 0x73 && g.openBus == 0x73);
	assert(gamateRead(&g, 0xE000) == 0xA6 && g.openBus == 0xA6);
	gamateFree(&g);
}

static void testUart(void) {
	struct Gamate g, peer;
	gamateInit(&g);
	gamateInit(&peer);
	uint8_t bios[GAMATE_BIOS_SIZE] = { 0 }, rom[0x4000] = { 0 };
	bios[0xFFA] = 0x00; bios[0xFFB] = 0xE2; // NMI vector $E200
	assert(gamateLoad(&g, rom, sizeof(rom), bios, sizeof(bios)));
	assert(gamateLoad(&peer, rom, sizeof(rom), bios, sizeof(bios)));
	struct LinkCapture capture = { .receiver = &peer };
	gamateSetLink(&g, captureLinkBit, &capture);
	assert(gamateRead(&g, 0x4800) == 0 && !g.uartTxLine);

	// TX data appears MSB-first immediately. Every read or write in $4C00-$4FFF clocks one bit;
	// after the eighth clock the measured hardware holds the output low.
	gamateWrite(&g, 0x4400, 0xAA);
	assert(g.uartTxLine && g.uartTxBits == 0);
	static const bool afterClock[8] = { false, true, false, true, false, true, false, false };
	for (unsigned bit = 0; bit < 8; bit++) {
		if (bit & 1) assert(gamateRead(&g, 0x4C00) == 0x4C);
		else gamateWrite(&g, 0x4FFF, 0);
		assert(g.uartTxBits == bit + 1 && g.uartTxLine == afterClock[bit]);
	}
	gamateRead(&g, 0x4C00);
	assert(g.uartTxBits == 8 && !g.uartTxLine);
	assert(capture.count == 9);
	assert(capture.bits == 0x54);
	assert(peer.uartRx == 0x54 && peer.uartRxBits == 1 && peer.nmi);
	assert(g.diagnostics.uartClocks == 9 && peer.diagnostics.uartReceivedBits == 9);
	peer.nmi = false;

	// Loading new data restarts transmission, and an in-flight byte survives state save/load.
	gamateWrite(&g, 0x47FF, 0xC0);
	gamateRead(&g, 0x4C00);
	assert(g.uartTxBits == 1 && g.uartTxLine);
	size_t stateSize = gamateStateSize(&g);
	void* state = malloc(stateSize);
	assert(state && gamateSaveState(&g, state, stateSize));
	assert(!gamateSaveState(&g, state, stateSize - 1));
	assert(!gamateSaveState(&g, state, stateSize + 1));
	assert(!gamateLoadState(&g, state, stateSize - 1));
	assert(!gamateLoadState(&g, state, stateSize + 1));
	gamateWrite(&g, 0x4400, 0);
	((uint8_t*) state)[0] ^= 1;
	assert(!gamateLoadState(&g, state, stateSize));
	assert(g.uartTx == 0 && g.uartTxBits == 0 && g.uartTxLine == false);
	((uint8_t*) state)[0] ^= 1;
	assert(gamateLoadState(&g, state, stateSize));
	assert(g.uartTx == 0x80 && g.uartTxBits == 1 && g.uartTxLine);
	free(state);

	// The receiver shifts MSB-first and raises NMI after the eighth external clock, even while I is
	// set. NMI has priority over a simultaneously pending IRQ and does not clear the IRQ timer.
	for (int bit = 7; bit >= 0; bit--) {
		gamateUartReceiveBit(&g, (0xA5 >> bit) & 1);
		assert(g.nmi == (bit == 0));
	}
	assert(gamateRead(&g, 0x4800) == 0xA5 && g.uartRxBits == 0);
	g.irq = true;
	g.irqClocks = 1234;
	g.cpu.p |= 0x04;
	g.cpu.irqMask = true;
	assert(gamateCpuStep(&g) == 7 && g.cpu.pc == 0xE200);
	assert(!g.nmi && g.irq && g.irqClocks == 1234);
	assert(g.diagnostics.diagnosticInterrupt == 2);
	assert(g.diagnostics.nmisTaken == 1 && g.diagnostics.irqsTaken == 0);
	assert((g.ram[0x1FB] & 0x30) == 0x20);

	// A partial receive byte is machine state, not diagnostics.
	gamateUartReceiveBit(&g, true);
	state = malloc(stateSize);
	assert(state && gamateSaveState(&g, state, stateSize));
	g.uartRx = g.uartRxBits = 0; g.nmi = false;
	assert(gamateLoadState(&g, state, stateSize));
	assert(g.uartRxBits == 1 && (g.uartRx & 1));
	// Completing that byte also preserves the pending NMI until the CPU accepts it.
	for (unsigned bit = 0; bit < 7; bit++) gamateUartReceiveBit(&g, false);
	assert(g.uartRxBits == 0 && g.nmi);
	assert(gamateSaveState(&g, state, stateSize));
	g.nmi = false;
	assert(gamateLoadState(&g, state, stateSize) && g.nmi);
	free(state);
	gamateReset(&g);
	assert(g.uartTx == 0 && g.uartRx == 0 && g.uartTxBits == 0 && g.uartRxBits == 0);
	assert(!g.uartTxLine && !g.nmi);
	gamateFree(&peer);
	gamateFree(&g);
}

static void testVideo(void) {
	struct Gamate g;
	gamateInit(&g);
	uint8_t bios[GAMATE_BIOS_SIZE] = { 0 }, rom[0x4000] = { 0 };
	assert(gamateLoad(&g, rom, sizeof(rom), bios, sizeof(bios)));
	g.protectionPassed = true;

	// Diagnostic-only VRAM coverage distinguishes CPU/display reads before a post-boundary write.
	gamateResetVramDiagnostics(&g);
	gamateWrite(&g, 0x5004, 0x00);
	gamateWrite(&g, 0x5005, 0x00);
	gamateRead(&g, 0x5006);
	assert(g.diagnostics.vramReads == 1 && g.diagnostics.vramReadBeforeWrite == 1 &&
		g.diagnostics.vramUniqueReadBeforeWrite == 1);
	gamateWrite(&g, 0x5004, 0x00);
	gamateWrite(&g, 0x5005, 0x00);
	gamateWrite(&g, 0x5007, 0x42);
	gamateWrite(&g, 0x5004, 0x00);
	gamateWrite(&g, 0x5005, 0x00);
	assert(gamateRead(&g, 0x5006) == 0x42);
	assert(g.diagnostics.vramTrackedWrites == 1 && g.diagnostics.vramUniqueWritten == 1 &&
		g.diagnostics.vramReads == 2 && g.diagnostics.vramReadBeforeWrite == 1);
	gamateResetVramDiagnostics(&g);
	g.scrollX = g.scrollY = 0;
	g.lcdClock = g.lcdRowEvent = 0;
	gamateRunClocks(&g, 162);
	assert(g.diagnostics.vramDisplayReads == 160 &&
		g.diagnostics.vramDisplayReadBeforeWrite == 160 &&
		g.diagnostics.vramUniqueDisplayReadBeforeWrite == 20);
	gamateResetVramDiagnostics(&g);
	g.displayBlank = true;
	g.lcdClock = g.lcdRowEvent = 0;
	gamateRunClocks(&g, 162);
	assert(g.diagnostics.vramDisplayReads == 0);
	g.displayBlank = false;
	g.lcdClock = g.lcdRowEvent = 0;
	g.masterClocks = g.frameClocks = g.irqClocks = 0;
	g.frameDone = g.irq = false;
	gamateResetVramDiagnostics(&g);
	gamateWrite(&g, 0x5003, 0); // no vertical scroll
	gamateWrite(&g, 0x5004, 0); // plane 0, address low
	gamateWrite(&g, 0x5005, 0); // address high
	gamateWrite(&g, 0x5007, 0x80);
	gamateWrite(&g, 0x5004, 0x80); // plane 1, address low
	gamateWrite(&g, 0x5005, 0);
	gamateWrite(&g, 0x5007, 0x80);
	gamateRenderSnapshot(&g);
	assert(g.frameBuffer[0] == 3);
	assert(g.frameBuffer[1] == 0);
	gamateWrite(&g, 0x5001, 0x80);
	gamateRenderSnapshot(&g);
	assert(g.frameBuffer[0] == 0);

	// The physical LCD samples each line in three passes. A plane-0 write after its pass must not
	// retroactively change the completed line, even though a frame-end renderer would see it.
	gamateReset(&g);
	gamateWrite(&g, 0x5003, 0);
	g.vram[0] = 0x80;
	g.vram[1] = 0;
	gamateRunClocks(&g, 162);
	assert(g.lcdRowEvent == 1 && g.lcdAccum[0] == 1);
	size_t lcdStateSize = gamateStateSize(&g);
	void* lcdState = malloc(lcdStateSize);
	assert(lcdState && gamateSaveState(&g, lcdState, lcdStateSize));
	g.vram[0] = 0;
	gamateRunClocks(&g, 324);
	assert(g.lcdRowEvent == 3 && g.frameBuffer[0] == 1);
	assert(gamateLoadState(&g, lcdState, lcdStateSize));
	g.vram[0] = 0;
	gamateRunClocks(&g, 324);
	assert(g.lcdRowEvent == 3 && g.frameBuffer[0] == 1);
	free(lcdState);

	// LCDcon E stops the physical LCD clocks, so scan position freezes rather than advancing without
	// pixels. Clearing E resumes the interrupted 162-clock row pass.
	gamateReset(&g);
	gamateRunClocks(&g, 100);
	assert(g.lcdClock == 100 && g.lcdRowEvent == 0);
	gamateWrite(&g, 0x5001, 0x01);
	gamateRunClocks(&g, 1000);
	assert(g.lcdClock == 100 && g.lcdRowEvent == 0);
	gamateWrite(&g, 0x5001, 0x00);
	gamateRunClocks(&g, 62);
	assert(g.lcdClock == 0 && g.lcdRowEvent == 1);

	// Blanking does not stop refresh. A plane-0 pass already emitted before N is set remains in the
	// physical three-pass accumulation, while the two later passes emit zero.
	gamateReset(&g);
	gamateWrite(&g, 0x5003, 0);
	g.vram[0] = g.vram[1] = 0x80;
	gamateRunClocks(&g, 162);
	gamateWrite(&g, 0x5001, 0x80);
	gamateRunClocks(&g, 324);
	assert(g.lcdRowEvent == 3 && g.frameBuffer[0] == 1);
	gamateFree(&g);
}

static void setVideoPixel(struct Gamate* g, unsigned x, unsigned y, uint8_t shade) {
	size_t byte = ((size_t) y * 0x20 + (x >> 3)) << 1;
	uint8_t mask = 0x80 >> (x & 7);
	if (shade & 1) g->vram[byte] |= mask;
	if (shade & 2) g->vram[byte | 1] |= mask;
}

static void testVideoWindows(void) {
	struct Gamate g;
	gamateInit(&g);
	uint8_t bios[GAMATE_BIOS_SIZE] = { 0 }, rom[0x4000] = { 0 };
	assert(gamateLoad(&g, rom, sizeof(rom), bios, sizeof(bios)));

	// Y=$C8/$D8/$E8/$F8 selects the ordinary row-zero origin, not row zero for every line.
	setVideoPixel(&g, 0, 0, 1);
	setVideoPixel(&g, 0, 1, 2);
	for (unsigned y = 0xC8; y <= 0xF8; y += 0x10) {
		g.scrollY = y;
		gamateRenderSnapshot(&g);
		assert(g.frameBuffer[0] == 1 && g.frameBuffer[GAMATE_WIDTH] == 2);
	}

	// F0 exposes all eight fixed rows F8-FF, then resumes at row zero. The fixed area ignores X.
	memset(g.vram, 0, sizeof(g.vram));
	setVideoPixel(&g, 0, 0xF8, 1);
	setVideoPixel(&g, 0, 0xFF, 2);
	setVideoPixel(&g, 8, 0x00, 3);
	g.scrollX = 8;
	g.scrollY = 0xF0;
	gamateRenderSnapshot(&g);
	assert(g.frameBuffer[0] == 1);
	assert(g.frameBuffer[7 * GAMATE_WIDTH] == 2);
	assert(g.frameBuffer[8 * GAMATE_WIDTH] == 3);

	// F1 exposes seven rows F9-FF, then resumes at row zero.
	memset(g.vram, 0, sizeof(g.vram));
	setVideoPixel(&g, 0, 0xF9, 1);
	setVideoPixel(&g, 0, 0xFF, 2);
	setVideoPixel(&g, 8, 0x00, 3);
	g.scrollY = 0xF1;
	gamateRenderSnapshot(&g);
	assert(g.frameBuffer[0] == 1);
	assert(g.frameBuffer[6 * GAMATE_WIDTH] == 2);
	assert(g.frameBuffer[7 * GAMATE_WIDTH] == 3);

	// LCDcon W independently replaces the top 16 rows with D0-DF and disables X scroll there,
	// including when Y uses one of the special values above.
	memset(g.vram, 0, sizeof(g.vram));
	setVideoPixel(&g, 0, 0xD0, 3);
	setVideoPixel(&g, 0, 0xF8, 1);
	g.window = true;
	g.scrollY = 0xF0;
	gamateRenderSnapshot(&g);
	assert(g.frameBuffer[0] == 3);
	g.window = false;
	gamateRenderSnapshot(&g);
	assert(g.frameBuffer[0] == 1);
	gamateFree(&g);
}

static void testProgressiveRows(void) {
	static const struct {
		unsigned line, scrollY;
		bool window, swap, blank, fixedX;
	} cases[] = {
		{ 0, 0, false, false, false, false },
		{ 149, 199, false, true, false, false },
		{ 15, 17, true, false, false, true },
		{ 16, 17, true, true, false, false },
		{ 0, 0xF0, false, true, false, true },
		{ 7, 0xF0, false, false, false, true },
		{ 8, 0xF0, false, false, false, false },
		{ 0, 0xF8, false, true, false, false },
		{ 0, 0, false, false, true, false },
	};
	struct Gamate g;
	gamateInit(&g);
	uint8_t bios[GAMATE_BIOS_SIZE] = { 0 }, rom[0x4000] = { 0 };
	assert(gamateLoad(&g, rom, sizeof(rom), bios, sizeof(bios)));
	for (size_t i = 0; i < sizeof(g.vram); i++) {
		g.vram[i] = (uint8_t) (i * 37 + (i >> 5) * 13);
	}
	for (size_t c = 0; c < sizeof(cases) / sizeof(*cases); c++) {
		for (unsigned x = 0; x < 256; x++) {
			g.scrollX = x;
			g.scrollY = cases[c].scrollY;
			g.window = cases[c].window;
			g.swapPlanes = cases[c].swap;
			g.displayBlank = cases[c].blank;
			gamateRenderSnapshot(&g);
			size_t row = cases[c].line * GAMATE_WIDTH;
			uint16_t expected[GAMATE_WIDTH];
			memcpy(expected, &g.frameBuffer[row], sizeof(expected));
			memset(&g.frameBuffer[row], 0xFF, sizeof(expected));
			memset(&g.lcdAccum[row], 0xFF, GAMATE_WIDTH);
			g.lcdRowEvent = cases[c].line * 3;
			g.lcdClock = 0;
			gamateResetVramDiagnostics(&g);
			gamateRunClocks(&g, 162);
			assert(g.frameBuffer[row] == 0xFFFF);
			gamateRunClocks(&g, 162);
			assert(g.frameBuffer[row] == 0xFFFF);
			gamateRunClocks(&g, 162);
			assert(!memcmp(expected, &g.frameBuffer[row], sizeof(expected)));
			if (cases[c].blank) {
				assert(g.diagnostics.vramDisplayReads == 0);
			} else {
				assert(g.diagnostics.vramDisplayReads == 3 * GAMATE_WIDTH);
				assert(g.diagnostics.vramDisplayReadBeforeWrite == 3 * GAMATE_WIDTH);
				assert(g.diagnostics.vramUniqueDisplayReadBeforeWrite ==
					(cases[c].fixedX || !(x & 7) ? 40 : 42));
			}
		}
	}
	g.scrollX = 3;
	g.scrollY = 0;
	g.window = g.swapPlanes = g.displayBlank = false;
	g.lcdRowEvent = g.lcdClock = 0;
	gamateResetVramDiagnostics(&g);
	gamateWrite(&g, 0x5004, 0);
	gamateWrite(&g, 0x5005, 0);
	gamateWrite(&g, 0x5007, 0xFF);
	gamateRunClocks(&g, 486);
	assert(g.diagnostics.vramDisplayReads == 480);
	assert(g.diagnostics.vramDisplayReadBeforeWrite == 475);
	assert(g.diagnostics.vramUniqueDisplayReadBeforeWrite == 41);
	gamateFree(&g);
}

static void testTimingAndState(void) {
	struct Gamate g;
	gamateInit(&g);
	uint8_t bios[GAMATE_BIOS_SIZE] = { 0 }, rom[0x4000] = { 0 };
	assert(gamateLoad(&g, rom, sizeof(rom), bios, sizeof(bios)));
	gamateRunClocks(&g, 32767);
	assert(!g.irq);
	gamateRunClocks(&g, 1);
	assert(g.irq);
	gamateReset(&g);
	gamateRunFrame(&g);
	assert(g.frameDone && g.masterClocks >= 72900);
	assert(g.frameClocks == g.masterClocks - 72900); // preserve an instruction crossing the frame edge
	g.ram[7] = 0x5A;
	size_t size = gamateStateSize(&g);
	void* state = malloc(size);
	assert(state && gamateSaveState(&g, state, size));
	g.ram[7] = 0;
	assert(gamateLoadState(&g, state, size));
	assert(g.ram[7] == 0x5A);
	free(state);
	gamateFree(&g);
}

static void testIrq(void) {
	struct Gamate g;
	gamateInit(&g);
	uint8_t bios[GAMATE_BIOS_SIZE] = { 0 }, rom[0x4000] = { 0 };
	bios[0x000] = 0xEA; // $E000: NOP
	bios[0x001] = 0x00; // $E001: BRK, used after the hardware-IRQ checks
	bios[0x100] = 0xEA; // $E100: IRQ handler
	bios[0xFFC] = 0x00; bios[0xFFD] = 0xE0;
	bios[0xFFE] = 0x00; bios[0xFFF] = 0xE1;
	assert(gamateLoad(&g, rom, sizeof(rom), bios, sizeof(bios)));

	// The measured timer asserts after exactly 32,768 master clocks. Reset starts with IRQ masked.
	gamateRunClocks(&g, 32767);
	assert(!g.irq && g.irqClocks == 32767);
	gamateRunClocks(&g, 1);
	assert(g.irq && g.irqClocks == 32768);
	unsigned cycles = gamateCpuStep(&g);
	assert(cycles == 2 && g.cpu.pc == 0xE001 && g.irq); // masked IRQ does not preempt the NOP
	gamateRunClocks(&g, cycles * 2);
	assert(g.irq && g.irqClocks == 32772); // pending level remains asserted while masked

	// Clearing I permits service at the next instruction boundary. The logical vector read clears and
	// restarts the Gamate timer; exact placement within these seven CPU cycles remains unmeasured.
	g.cpu.p &= (uint8_t) ~0x04;
	g.cpu.irqMask = false; // direct test setup bypasses execution of CLI
	cycles = gamateCpuStep(&g);
	assert(cycles == 7 && g.cpu.pc == 0xE100 && !g.irq && g.irqClocks == 0);
	assert(g.diagnostics.diagnosticInterrupt == 1);
	assert(g.diagnostics.irqsTaken == 1 && g.cpu.s == 0xFA);
	assert(g.ram[0x1FD] == 0xE0 && g.ram[0x1FC] == 0x01);
	assert((g.ram[0x1FB] & 0x30) == 0x20); // hardware IRQ stacks U set and B clear
	assert(g.cpu.p & 0x04);                // service sets interrupt disable
	gamateRunClocks(&g, cycles * 2);
	assert(g.irqClocks == 14 && !g.irq);
	gamateRunClocks(&g, 32768 - 14);
	assert(g.irq && g.irqClocks == 32768);

	// A pending IRQ and its timer phase must survive state serialization.
	size_t stateSize = gamateStateSize(&g);
	void* state = malloc(stateSize);
	assert(state && gamateSaveState(&g, state, stateSize));
	g.irq = false; g.irqClocks = 3;
	assert(gamateLoadState(&g, state, stateSize));
	assert(g.irq && g.irqClocks == 32768);
	free(state);

	// If the timer expires during an instruction, that instruction completes and IRQ service begins
	// at the following boundary.
	gamateReset(&g);
	g.cpu.p &= (uint8_t) ~0x04;
	g.irqClocks = 32766;
	cycles = gamateCpuStep(&g);
	assert(cycles == 2 && g.cpu.pc == 0xE001 && !g.irq);
	gamateRunClocks(&g, cycles * 2);
	assert(g.irq && g.cpu.pc == 0xE001);
	cycles = gamateCpuStep(&g);
	assert(cycles == 7 && g.cpu.pc == 0xE100 && g.ram[0x1FD] == 0xE0 && g.ram[0x1FC] == 0x01);

	// BRK uses the IRQ vector and is interpreted as a vector pull by the current hardware model, but
	// remains a software interrupt for diagnostics and stacks B set.
	gamateReset(&g);
	g.cpu.pc = 0xE001;
	g.irqClocks = 123;
	cycles = gamateCpuStep(&g);
	assert(cycles == 7 && g.cpu.pc == 0xE100 && g.irqClocks == 0);
	assert(g.diagnostics.irqsTaken == 0 && (g.ram[0x1FB] & 0x30) == 0x30);

	g.irq = true; g.irqClocks = 999;
	gamateReset(&g);
	assert(!g.irq && g.irqClocks == 0 && g.cpu.pc == 0xE000);
	gamateFree(&g);
}

static void testIrqMaskTiming(void) {
	struct Gamate g;
	gamateInit(&g);
	uint8_t bios[GAMATE_BIOS_SIZE] = { 0 }, rom[0x4000] = { 0 };
	bios[0x000] = 0x58; // $E000: CLI
	bios[0x001] = 0xEA; // $E001: NOP
	bios[0x002] = 0xEA; // $E002: NOP (must be replaced by IRQ service)
	bios[0x010] = 0x78; // $E010: SEI
	bios[0x020] = 0x28; // $E020: PLP
	bios[0x021] = 0xEA; // $E021: NOP
	bios[0x030] = 0x40; // $E030: RTI
	bios[0x040] = 0xEA; // $E040: RTI target (must be replaced by IRQ service)
	bios[0x100] = 0xEA; // $E100: IRQ handler
	bios[0xFFC] = 0x00; bios[0xFFD] = 0xE0;
	bios[0xFFE] = 0x00; bios[0xFFF] = 0xE1;
	assert(gamateLoad(&g, rom, sizeof(rom), bios, sizeof(bios)));

	// NMOS CLI polls with the old I=1: one following instruction executes before a pending IRQ.
	g.irq = true;
	assert(gamateCpuStep(&g) == 2 && g.cpu.pc == 0xE001 && !(g.cpu.p & 0x04));
	assert(gamateCpuStep(&g) == 2 && g.cpu.pc == 0xE002);
	assert(gamateCpuStep(&g) == 7 && g.cpu.pc == 0xE100);

	// NMOS SEI also polls with the old I=0: an IRQ arriving during SEI is still serviced next.
	gamateReset(&g);
	g.cpu.pc = 0xE010;
	g.cpu.p &= (uint8_t) ~0x04;
	g.cpu.irqMask = false;
	assert(gamateCpuStep(&g) == 2 && (g.cpu.p & 0x04));
	g.irq = true;
	assert(gamateCpuStep(&g) == 7 && g.cpu.pc == 0xE100);

	// PLP delays its restored I value in the same way as CLI/SEI.
	gamateReset(&g);
	g.cpu.pc = 0xE020;
	g.cpu.s = 0xFC;
	g.ram[0x1FD] = 0x20; // restore I clear
	g.irq = true;
	assert(gamateCpuStep(&g) == 4 && g.cpu.pc == 0xE021 && !(g.cpu.p & 0x04));
	assert(gamateCpuStep(&g) == 2 && g.cpu.pc == 0xE022);
	assert(gamateCpuStep(&g) == 7 && g.cpu.pc == 0xE100);

	// RTI restores P before polling, so its new I=0 value permits immediate service at its target.
	gamateReset(&g);
	g.cpu.pc = 0xE030;
	g.cpu.s = 0xFA;
	g.ram[0x1FB] = 0x20; // P with I clear
	g.ram[0x1FC] = 0x40; // return PC $E040
	g.ram[0x1FD] = 0xE0;
	g.irq = true;
	assert(gamateCpuStep(&g) == 6 && g.cpu.pc == 0xE040 && !(g.cpu.p & 0x04));
	assert(gamateCpuStep(&g) == 7 && g.cpu.pc == 0xE100);
	gamateFree(&g);
}

static void testCpu(void) {
	struct Gamate g;
	gamateInit(&g);
	uint8_t bios[GAMATE_BIOS_SIZE] = { 0 }, rom[0x4000] = { 0 };
	uint8_t program[] = {
		0xA9, 0x45,       // LDA #$45
		0xF8,             // SED
		0x18,             // CLC
		0x69, 0x55,       // ADC #$55 -> $00 with carry in BCD
		0xD8,             // CLD
		0xA2, 0x03,       // LDX #3
		0xCA,             // DEX
		0xD0, 0xFD,       // BNE $E009
		0x20, 0x10, 0xE0, // JSR $E010
		0xEA,             // next instruction after RTS
		0xE8,             // INX
		0x60,             // RTS
	};
	memcpy(bios, program, sizeof(program));
	bios[0xFFC] = 0x00;
	bios[0xFFD] = 0xE0;
	assert(gamateLoad(&g, rom, sizeof(rom), bios, sizeof(bios)));
	for (int i = 0; i < 15; i++) gamateCpuStep(&g);
	assert(g.cpu.a == 0 && (g.cpu.p & 1));
	assert(!(g.cpu.p & 2)); // NMOS decimal Z comes from the unadjusted binary result ($9A)
	assert(g.cpu.x == 1 && g.cpu.pc == 0xE00F);
	gamateFree(&g);
}

static void testCpuDecimal(void) {
	struct Gamate g;
	gamateInit(&g);
	uint8_t bios[GAMATE_BIOS_SIZE] = { 0 }, rom[0x4000] = { 0 };
	bios[0] = 0x69; // ADC #operand; changed to SBC after the first exhaustive pass
	bios[0xFFC] = 0x00; bios[0xFFD] = 0xE0;
	assert(gamateLoad(&g, rom, sizeof(rom), bios, sizeof(bios)));
	for (unsigned operation = 0; operation < 2; operation++) {
		g.bios[0] = operation ? 0xE9 : 0x69;
		for (unsigned a = 0; a < 0x100; a++) for (unsigned value = 0; value < 0x100; value++) {
			g.bios[1] = value;
			for (unsigned carry = 0; carry < 2; carry++) {
				g.cpu.pc = 0xE000;
				g.cpu.a = a;
				g.cpu.p = 0x28 | carry; // U and decimal set
				g.cpu.irqMask = false;
				assert(gamateCpuStep(&g) == 2 && g.cpu.pc == 0xE002);
				unsigned binary = operation ? a - value - !carry : a + value + carry;
				unsigned expected, expectedFlags = ((uint8_t) binary == 0) ? 2 : 0;
				if (operation) {
					int low = (a & 0x0F) - (value & 0x0F) - !carry;
					int high = (a >> 4) - (value >> 4);
					if (low < 0) { low -= 6; high--; }
					if (high < 0) high -= 6;
					expected = ((unsigned) (uint8_t) high << 4 | (low & 0x0F)) & 0xFF;
					expectedFlags |= binary < 0x100;
					expectedFlags |= binary & 0x80;
					expectedFlags |= ((a ^ value) & (a ^ binary) & 0x80) ? 0x40 : 0;
				} else {
					unsigned low = (a & 0x0F) + (value & 0x0F) + carry;
					unsigned high = (a >> 4) + (value >> 4) + (low > 9);
					expectedFlags |= high & 8 ? 0x80 : 0;
					expectedFlags |= (((high << 4) ^ a) & 0x80 && !((a ^ value) & 0x80)) ? 0x40 : 0;
					if (low > 9) low += 6;
					if (high > 9) high += 6;
					expectedFlags |= high > 0x0F;
					expected = ((high << 4) | (low & 0x0F)) & 0xFF;
				}
				assert(g.cpu.a == expected);
				assert((g.cpu.p & 0xC3) == expectedFlags);
			}
		}
	}
	gamateFree(&g);
}

static void testCpuBusAccesses(void) {
	struct Gamate g;
	gamateInit(&g);
	uint8_t bios[GAMATE_BIOS_SIZE] = { 0 }, rom[0x4000] = { 0 };
	bios[0] = 0x0E; bios[1] = 0x00; bios[2] = 0x4C; // ASL $4C00
	bios[0xFFC] = 0x00; bios[0xFFD] = 0xE0;
	assert(gamateLoad(&g, rom, sizeof(rom), bios, sizeof(bios)));
	g.cpu.pc = 0xE000;
	gamateWrite(&g, 0x4400, 0xA0);
	assert(g.uartTxBits == 0 && g.uartTxLine);
	// NMOS RMW is read, write old, write new: all three accesses clock this Gamate register.
	assert(gamateCpuStep(&g) == 6);
	assert(g.uartTxBits == 3 && !g.uartTxLine);

	// Page-crossing indexed reads first access the uncorrected page. Here LDA's dummy read clocks
	// UART at $4F00 before the corrected operand read reaches LCD register $5000.
	g.bios[0] = 0xBD; g.bios[1] = 0xFF; g.bios[2] = 0x4F; // LDA $4FFF,X
	g.cpu.pc = 0xE000; g.cpu.x = 1;
	g.uartTxBits = 0;
	assert(gamateCpuStep(&g) == 5 && g.uartTxBits == 1);

	// Indexed stores always make the preliminary read, whether or not software uses its value.
	g.bios[0] = 0x9D; // STA $4FFF,X
	g.cpu.pc = 0xE000;
	g.uartTxBits = 0;
	assert(gamateCpuStep(&g) == 5 && g.uartTxBits == 1);
	// LDY abs,X uses the same crossing sequence despite living outside the generic opcode group.
	g.bios[0] = 0xBC; // LDY $4FFF,X
	g.cpu.pc = 0xE000;
	g.uartTxBits = 0;
	assert(gamateCpuStep(&g) == 5 && g.uartTxBits == 1 && g.cpu.y == 0x50);
	// STY zp,X reads the unindexed zero-page address before writing the indexed destination.
	g.bios[0] = 0x94; g.bios[1] = 0x10;
	g.ram[0x10] = 0xCC; g.cpu.pc = 0xE000; g.cpu.x = 1; g.cpu.y = 0x73;
	assert(gamateCpuStep(&g) == 4 && g.ram[0x11] == 0x73 && g.openBus == 0xCC);

	// A taken branch reads the fall-through PC, then the target low byte on the old page if the
	// branch crosses a page. The final discarded read is visible through open bus.
	g.bios[0] = 0x22;
	g.bios[0x0FC] = 0xD0; g.bios[0x0FD] = 0x02; // BNE $E100 from $E0FC
	g.bios[0x0FE] = 0x11;
	g.cpu.pc = 0xE0FC; g.cpu.p &= (uint8_t) ~0x02;
	assert(gamateCpuStep(&g) == 4 && g.cpu.pc == 0xE100 && g.openBus == 0x22);
	// An untaken branch stops after fetching its offset and performs neither discarded read.
	g.cpu.pc = 0xE0FC; g.cpu.p |= 0x02;
	assert(gamateCpuStep(&g) == 2 && g.cpu.pc == 0xE0FE && g.openBus == 0x02);

	// Two-cycle implied and accumulator instructions discard a fetch of the following opcode.
	g.bios[0] = 0xEA; g.bios[1] = 0x5A; // NOP, followed by a distinctive bus value
	g.cpu.pc = 0xE000;
	assert(gamateCpuStep(&g) == 2 && g.cpu.pc == 0xE001 && g.openBus == 0x5A);
	g.bios[0] = 0x0A; g.bios[1] = 0xA5; // ASL A
	g.cpu.pc = 0xE000; g.cpu.a = 0x40;
	assert(gamateCpuStep(&g) == 2 && g.cpu.a == 0x80 && g.openBus == 0xA5);

	// Stack pushes discard-fetch the following opcode before writing. Writes do not replace the
	// read-driven open-bus value.
	g.bios[0] = 0x48; g.bios[1] = 0x3C; // PHA
	g.cpu.pc = 0xE000; g.cpu.s = 0xFD; g.cpu.a = 0xA7;
	assert(gamateCpuStep(&g) == 3 && g.cpu.s == 0xFC && g.ram[0x1FD] == 0xA7);
	assert(g.openBus == 0x3C);
	// RTS performs its last discarded read at the stacked return address before incrementing PC.
	g.bios[0] = 0x60; g.bios[0x123] = 0x6D;
	g.cpu.pc = 0xE000; g.cpu.s = 0xFB;
	g.ram[0x1FC] = 0x23; g.ram[0x1FD] = 0xE1;
	assert(gamateCpuStep(&g) == 6 && g.cpu.pc == 0xE124 && g.cpu.s == 0xFD);
	assert(g.openBus == 0x6D);

	// Both zero-page indirect forms read the low pointer byte before the wrapped high byte.
	g.bios[0] = 0x81; g.bios[1] = 0xFF; // STA ($FF,X)
	g.cpu.pc = 0xE000; g.cpu.x = 0; g.cpu.a = 0x5C;
	g.ram[0xFF] = 0x20; g.ram[0] = 0x01;
	assert(gamateCpuStep(&g) == 6 && g.ram[0x120] == 0x5C && g.openBus == 0x01);
	g.bios[0] = 0xB1; // LDA ($FF),Y
	g.cpu.pc = 0xE000; g.cpu.y = 0;
	struct RamAccessCapture pointerRead = { 0 };
	gamateSetRamDiagnosticWatch(&g, 0, captureRamAccess, &pointerRead);
	assert(gamateCpuStep(&g) == 5 && g.cpu.a == 0x5C);
	assert(pointerRead.count == 1 && !pointerRead.write && pointerRead.openBusBefore == 0x20);
	gamateSetRamDiagnosticWatch(&g, 0, NULL, NULL);

	// NMOS JMP indirect wraps the vector's high-byte read within the pointer's original page.
	g.bios[0] = 0x6C; g.bios[1] = 0xFF; g.bios[2] = 0x10; // JMP ($10FF)
	g.ram[0x0FF] = 0x34; g.ram[0] = 0xE2; g.ram[0x100] = 0x99;
	g.cpu.pc = 0xE000;
	assert(gamateCpuStep(&g) == 5 && g.cpu.pc == 0xE234 && g.openBus == 0xE2);

	// Power reset performs seven reads, including three non-writing stack cycles, and ends with the
	// high reset-vector byte on open bus.
	g.bios[0xFFC] = 0x34; g.bios[0xFFD] = 0xE2;
	gamateReset(&g);
	assert(g.cpu.pc == 0xE234 && g.cpu.s == 0xFD && g.cpu.cycles == 7);
	assert(g.ram[0] == 0xFF && g.ram[0x100] == 0xFF && g.ram[0x1FE] == 0xFF && g.ram[0x1FF] == 0xFF);
	assert(g.openBus == 0xE2);
	gamateFree(&g);
}

static void testAudio(void) {
	struct Gamate g;
	gamateInit(&g);
	uint8_t bios[GAMATE_BIOS_SIZE] = { 0 }, rom[0x4000] = { 0 };
	assert(gamateLoad(&g, rom, sizeof(rom), bios, sizeof(bios)));
	int16_t audio[5000 * 2] = { 0 };
	gamateSetAudio(&g, audio, 5000, 48000);
	gamateWrite(&g, 0x4000, 10); // channel A period: 1.10825 MHz / (16 * 10)
	gamateWrite(&g, 0x4001, 0);
	gamateWrite(&g, 0x4007, 0x3E); // tone A only; all noise and other tones disabled
	gamateWrite(&g, 0x4008, 15);
	gamateRunClocks(&g, 443300); // exactly one tenth of a second
	assert(g.audioCount == 4800);
	bool left = false, right = false;
	for (size_t i = 0; i < g.audioCount; i++) {
		left |= audio[i * 2] != 0;
		right |= audio[i * 2 + 1] != 0;
	}
	assert(left && !right);

	// Period 2 is ultrasonic. It must settle to silence instead of aliasing into an audible whistle.
	memset(audio, 0, sizeof(audio));
	gamateReset(&g);
	gamateWrite(&g, 0x4000, 2);
	gamateWrite(&g, 0x4007, 0x3E);
	gamateWrite(&g, 0x4008, 15);
	gamateRunClocks(&g, 443300);
	for (size_t i = g.audioCount - 100; i < g.audioCount; i++) {
		assert(audio[i * 2] >= -1 && audio[i * 2] <= 1);
		assert(audio[i * 2 + 1] == 0);
	}

	memset(audio, 0, sizeof(audio));
	gamateReset(&g);
	gamateWrite(&g, 0x4002, 10);
	gamateWrite(&g, 0x4007, 0x3D); // tone B only
	gamateWrite(&g, 0x4009, 15);
	gamateRunClocks(&g, 44330);
	left = right = false;
	for (size_t i = 0; i < g.audioCount; i++) {
		left |= audio[i * 2] != 0;
		right |= audio[i * 2 + 1] != 0;
	}
	assert(!left && right);

	memset(audio, 0, sizeof(audio));
	gamateReset(&g);
	gamateWrite(&g, 0x4004, 10);
	gamateWrite(&g, 0x4007, 0x3B); // tone C only
	gamateWrite(&g, 0x400A, 15);
	gamateRunClocks(&g, 44330);
	left = right = false;
	for (size_t i = 0; i < g.audioCount; i++) {
		left |= audio[i * 2] != 0;
		right |= audio[i * 2 + 1] != 0;
		assert(audio[i * 2] == audio[i * 2 + 1]);
	}
	assert(left && right);

	gamateReset(&g);
	gamateWrite(&g, 0x4006, 0); // zero periods behave as one
	gamateRunClocks(&g, 64);
	assert(g.ay.noiseLfsr == 0xFFFF);
	gamateWrite(&g, 0x400B, 1);
	gamateWrite(&g, 0x400C, 0);
	gamateWrite(&g, 0x400D, 0x0C); // repeating rising envelope
	assert((g.ay.envelopeStep ^ g.ay.envelopeAttack) == 0);
	gamateRunClocks(&g, 1024);
	assert((g.ay.envelopeStep ^ g.ay.envelopeAttack) == 1);
	gamateWrite(&g, 0x4001, 0xFF);
	assert(g.ayRegisters[1] == 0x0F); // unused register bits are not stored
	gamateFree(&g);
}

static void configureClockTest(struct Gamate* g, int16_t* audio) {
	gamateSetAudio(g, audio, 2000, 48000);
	gamateWrite(g, 0x4000, 7);
	gamateWrite(g, 0x4002, 13);
	gamateWrite(g, 0x4004, 29);
	gamateWrite(g, 0x4006, 5);
	gamateWrite(g, 0x4007, 0);
	gamateWrite(g, 0x4008, 0x10);
	gamateWrite(g, 0x4009, 12);
	gamateWrite(g, 0x400A, 9);
	gamateWrite(g, 0x400B, 3);
	gamateWrite(g, 0x400D, 0x0E);
	for (size_t i = 0; i < sizeof(g->vram); i++) g->vram[i] = (uint8_t) (i * 37);
}

static void testClockBatching(void) {
	struct Gamate batch, single;
	gamateInit(&batch);
	gamateInit(&single);
	uint8_t bios[GAMATE_BIOS_SIZE] = { 0 }, rom[0x4000] = { 0 };
	assert(gamateLoad(&batch, rom, sizeof(rom), bios, sizeof(bios)));
	assert(gamateLoad(&single, rom, sizeof(rom), bios, sizeof(bios)));
	int16_t batchAudio[2000 * 2] = { 0 }, singleAudio[2000 * 2] = { 0 };
	configureClockTest(&batch, batchAudio);
	configureClockTest(&single, singleAudio);

	gamateRunClocks(&batch, 100000);
	for (unsigned clock = 0; clock < 100000; clock++) gamateRunClocks(&single, 1);
	assert(batch.audioCount == single.audioCount && batch.frameDone == single.frameDone);
	assert(!memcmp(batchAudio, singleAudio, batch.audioCount * 2 * sizeof(*batchAudio)));
	assert(!memcmp(&batch.diagnostics, &single.diagnostics, sizeof(batch.diagnostics)));
	size_t stateSize = gamateStateSize(&batch);
	void* batchState = malloc(stateSize);
	void* singleState = malloc(stateSize);
	assert(batchState && singleState);
	assert(gamateSaveState(&batch, batchState, stateSize));
	assert(gamateSaveState(&single, singleState, stateSize));
	assert(!memcmp(batchState, singleState, stateSize));
	free(batchState);
	free(singleState);
	gamateFree(&batch);
	gamateFree(&single);
}

static void testSampleEventBatching(void) {
	static const unsigned rates[] = { 1, 44100, 48000, 192000, 4433000, 8866000, UINT32_MAX };
	uint8_t bios[GAMATE_BIOS_SIZE] = { 0 }, rom[0x4000] = { 0 };
	for (size_t r = 0; r < sizeof(rates) / sizeof(*rates); r++) {
		for (unsigned phase = 0; phase < 3; phase++) {
			struct Gamate batch, single;
			gamateInit(&batch);
			gamateInit(&single);
			assert(gamateLoad(&batch, rom, sizeof(rom), bios, sizeof(bios)));
			assert(gamateLoad(&single, rom, sizeof(rom), bios, sizeof(bios)));
			int16_t batchAudio[2000 * 2] = { 0 }, singleAudio[2000 * 2] = { 0 };
			configureClockTest(&batch, batchAudio);
			configureClockTest(&single, singleAudio);
			gamateSetAudio(&batch, batchAudio, 2000, rates[r]);
			gamateSetAudio(&single, singleAudio, 2000, rates[r]);
			batch.samplePhase = single.samplePhase = phase == 0 ? 0 :
				phase == 1 ? 4432999 : 2 * UINT64_C(4433000) + 1;
			batch.ay.toneCounter[0] = single.ay.toneCounter[0] = batch.ayTonePeriod[0] - 1;
			batch.ay.noiseCounter = single.ay.noiseCounter = batch.ayNoisePeriod - 1;
			batch.lcdClock = single.lcdClock = 161;
			batch.irqClocks = single.irqClocks = 32767;
			batch.frameClocks = single.frameClocks = GAMATE_CLOCKS_PER_FRAME - 1;
			gamateWrite(&batch, 0x4400, 0xA5);
			gamateWrite(&single, 0x4400, 0xA5);
			gamateUartReceiveBit(&batch, true);
			gamateUartReceiveBit(&single, true);
			batch.nmi = single.nmi = true;
			gamateRunClocks(&batch, 1024);
			for (unsigned clock = 0; clock < 1024; clock++) gamateRunClocks(&single, 1);
			assert(batch.audioCount == single.audioCount && batch.audioCount <= 2000);
			assert(batch.frameDone == single.frameDone);
			assert(!memcmp(batchAudio, singleAudio, batch.audioCount * 2 * sizeof(*batchAudio)));
			assert(!memcmp(&batch.diagnostics, &single.diagnostics, sizeof(batch.diagnostics)));
			size_t stateSize = gamateStateSize(&batch);
			void* batchState = malloc(stateSize);
			void* singleState = malloc(stateSize);
			assert(batchState && singleState);
			assert(gamateSaveState(&batch, batchState, stateSize));
			assert(gamateSaveState(&single, singleState, stateSize));
			assert(!memcmp(batchState, singleState, stateSize));
			free(batchState);
			free(singleState);
			gamateFree(&batch);
			gamateFree(&single);
		}
	}
}

static void testWatchpoints(void) {
	struct Gamate g;
	gamateInit(&g);
	uint8_t bios[GAMATE_BIOS_SIZE] = { 0 }, rom[0x4000] = { 0 };
	bios[0x000] = 0xEA; // NOP
	bios[0x001] = 0xEA;
	bios[0xFFC] = 0x00;
	bios[0xFFD] = 0xE0;
	assert(gamateLoad(&g, rom, sizeof(rom), bios, sizeof(bios)));

	struct WatchpointCapture capture = { 0 };
	g.ram[0x123] = 0x5A;
	gamateSetWatchpoint(&g, 0x0523, GAMATE_WATCHPOINT_READ,
		captureWatchpoint, &capture);
	assert(gamateWatchpointEnabled(&g, 0x0523, GAMATE_WATCHPOINT_READ));
	assert(!gamateWatchpointEnabled(&g, 0x0523, GAMATE_WATCHPOINT_WRITE));
	assert(gamateRead(&g, 0x0523) == 0x5A);
	assert(capture.count == 1 && !capture.write && !capture.dummy &&
		capture.address == 0x0523 && capture.previous == 0x5A && capture.value == 0x5A);

	gamateClearWatchpoint(&g, 0x0523, GAMATE_WATCHPOINT_READ);
	assert(!gamateWatchpointEnabled(&g, 0x0523, GAMATE_WATCHPOINT_READ));
	capture.count = 0;
	assert(gamateRead(&g, 0x0523) == 0x5A);
	assert(capture.count == 0);

	gamateSetWatchpoint(&g, 0x0523, GAMATE_WATCHPOINT_WRITE,
		captureWatchpoint, &capture);
	gamateWrite(&g, 0x0523, 0xA5);
	assert(g.ram[0x123] == 0xA5);
	assert(capture.count == 1 && capture.write && !capture.dummy &&
		capture.address == 0x0523 && capture.previous == 0x5A && capture.value == 0xA5);

	gamateClearWatchpoints(&g);
	gamateSetWatchpoint(&g, 0x400B, GAMATE_WATCHPOINT_ACCESS,
		captureWatchpoint, &capture);
	assert(gamateWatchpointEnabled(&g, 0x400B, GAMATE_WATCHPOINT_ACCESS));
	capture.count = 0;
	gamateWrite(&g, 0x400B, 3);
	assert(capture.count == 1 && capture.write &&
		capture.address == 0x400B && capture.value == 3);
	gamateRead(&g, 0x400B);
	assert(capture.count == 2 && !capture.write &&
		capture.address == 0x400B);

	gamateClearWatchpoints(&g);
	gamateSetWatchpoint(&g, 0xE001, GAMATE_WATCHPOINT_READ,
		captureWatchpoint, &capture);
	gamateReset(&g);
	assert(gamateWatchpointEnabled(&g, 0xE001, GAMATE_WATCHPOINT_READ));
	capture.count = 0;
	gamateCpuStep(&g);
	assert(capture.count >= 1);
	assert(capture.address == 0xE001 && capture.dummy);

	gamateClearWatchpoints(&g);
	gamateFree(&g);
}

static void testTraceAndDisassembly(void) {
	struct Gamate g;
	gamateInit(&g);
	uint8_t bios[GAMATE_BIOS_SIZE] = { 0 }, rom[0x4000] = { 0 };
	bios[0x000] = 0xA9; bios[0x001] = 0x45; // LDA #$45
	bios[0x002] = 0x20; bios[0x003] = 0x10; bios[0x004] = 0xE0; // JSR subroutine
	bios[0x010] = 0x60; // RTS
	bios[0x100] = 0x02;
	bios[0xFFC] = 0x00;
	bios[0xFFD] = 0xE0;
	assert(gamateLoad(&g, rom, sizeof(rom), bios, sizeof(bios)));
	gamateReset(&g);
	gamateTraceEnable(&g, true);

	gamateStep(&g);
	gamateStep(&g);
	gamateStep(&g);
	assert(gamateTraceCount(&g) == 3);
	struct GamateTraceEntry oldest, newest;
	assert(gamateTraceGet(&g, 0, &oldest));
	assert(gamateTraceGet(&g, 2, &newest));
	assert(oldest.pc == 0xE000 && oldest.opcodeSize == 2 &&
		oldest.opcode[0] == 0xA9 && oldest.opcode[1] == 0x45);
	assert(oldest.a == 0x45);
	assert(newest.pc == 0xE010 && newest.opcodeSize == 1 &&
		newest.opcode[0] == 0x60);

	char text[64];
	assert(gamateDisassemble(&g, 0xE000, text, sizeof(text), NULL, NULL) == 2);
	assert(!strcmp(text, "LDA #$45"));
	assert(gamateDisassemble(&g, 0xE002, text, sizeof(text),
		labelAt, NULL) == 3);
	assert(!strcmp(text, "JSR subroutine"));
	assert(gamateDisassemble(&g, 0xE010, text, sizeof(text),
		labelAt, NULL) == 1);
	assert(!strcmp(text, "RTS"));
	assert(gamateDisassemble(&g, 0xE100, text, sizeof(text),
		NULL, NULL) == 1);
	assert(!strcmp(text, ".byte $02"));

	gamateTraceClear(&g);
	assert(gamateTraceCount(&g) == 0);
	for (unsigned i = 0; i < GAMATE_TRACE_CAPACITY + 17; i++) {
		struct GamateTraceEntry entry = { .pc = (uint16_t) i,
			.opcodeSize = 1, .opcode = { 0xEA }, .kind = GAMATE_TRACE_NORMAL };
		gamateTraceRecord(&g, &entry);
	}
	assert(gamateTraceCount(&g) == GAMATE_TRACE_CAPACITY);
	assert(gamateTraceGet(&g, 0, &oldest));
	assert(gamateTraceGet(&g, GAMATE_TRACE_CAPACITY - 1, &newest));
	assert(oldest.pc == 17 && newest.pc == GAMATE_TRACE_CAPACITY + 16);

	gamateTraceEnable(&g, false);
	assert(!gamateTraceIsEnabled(&g));
	gamateTraceClear(&g);
	gamateTraceEnable(&g, true);
	assert(gamateTraceIsEnabled(&g) && gamateTraceCount(&g) == 0);
	gamateFree(&g);
}

static void testStateViewsAndEditing(void) {
	struct Gamate g;
	gamateInit(&g);
	uint8_t bios[GAMATE_BIOS_SIZE] = { 0 }, rom[0x4000] = { 0 };
	bios[0xFFC] = 0x00;
	bios[0xFFD] = 0xE0;
	assert(gamateLoad(&g, rom, sizeof(rom), bios, sizeof(bios)));

	gamateSetCpuRegisters(&g, 0xE123, 0x11, 0x22, 0x33, 0x44, 0x04);
	assert(g.cpu.pc == 0xE123 && g.cpu.a == 0x11 && g.cpu.x == 0x22 &&
		g.cpu.y == 0x33 && g.cpu.s == 0x44);
	assert(g.cpu.p == 0x24 && g.cpu.irqMask);
	gamateSetCpuP(&g, 0);
	assert(g.cpu.p == 0x20 && !g.cpu.irqMask);
	gamateSetCpuP(&g, 0xFF);
	assert(g.cpu.p == 0xEF && g.cpu.irqMask);
	gamateSetCpuStopped(&g, true);
	assert(g.cpu.stopped);

	g.vramAddress = 0x123;
	g.scrollX = 0x12;
	g.scrollY = 0x34;
	g.vramPlane = true;
	g.incrementRow = true;
	g.window = true;
	g.swapPlanes = true;
	g.displayBlank = true;
	g.displayStopped = true;
	g.lcdClock = 99;
	g.lcdRowEvent = 5;
	g.protectionPassed = true;
	g.protectionFailed = true;
	g.protectionReply = true;
	g.protectionPosition = 3;
	g.protectionByte = 0xA5;
	g.protectionSequence = 7;
	g.uartTx = 0x81;
	g.uartRx = 0x42;
	g.uartTxBits = 3;
	g.uartRxBits = 5;
	g.uartTxLine = true;
	g.ayAddress = 6;
	g.ayRegisters[6] = 0x0F;
	g.ayTonePeriod[0] = 1234;
	g.ayNoisePeriod = 5678;
	g.ayEnvelopePeriod = 9012;
	g.mapper = GAMATE_MAPPER_BANKED;
	g.bank = 4;
	g.firstBank = 5;
	g.irq = true;
	g.nmi = true;
	g.irqClocks = 12345;
	g.masterClocks = 67890;
	g.frameClocks = 54321;
	g.frameDone = true;

	struct GamatePeripheralState state;
	gamateGetPeripheralState(&g, &state);
	assert(state.lcd.vramAddress == 0x123 && state.lcd.scrollX == 0x12 &&
		state.lcd.scrollY == 0x34 && state.lcd.vramPlane &&
		state.lcd.incrementRow && state.lcd.window && state.lcd.swapPlanes &&
		state.lcd.displayBlank && state.lcd.displayStopped &&
		state.lcd.lcdClock == 99 && state.lcd.lcdRowEvent == 5);
	assert(state.protection.passed && state.protection.failed &&
		state.protection.reply && state.protection.position == 3 &&
		state.protection.byte == 0xA5 && state.protection.sequence == 7);
	assert(state.uart.tx == 0x81 && state.uart.rx == 0x42 &&
		state.uart.txBits == 3 && state.uart.rxBits == 5 &&
		state.uart.txLine);
	assert(state.ay.address == 6 && state.ay.registers[6] == 0x0F &&
		state.ay.tonePeriod[0] == 1234 &&
		state.ay.noisePeriod == 5678 &&
		state.ay.envelopePeriod == 9012);
	assert(state.cartridge.mapper == GAMATE_MAPPER_BANKED &&
		state.cartridge.bank == 4 && state.cartridge.firstBank == 5);
	assert(state.interrupt.irq && state.interrupt.nmi &&
		state.interrupt.irqClocks == 12345);
	assert(state.clocks.masterClocks == 67890 &&
		state.clocks.frameClocks == 54321 && state.clocks.frameDone);

	gamateSetVramByte(&g, 0, 0x81);
	assert(gamateGetVramByte(&g, 0) == 0x81);
	assert(!gamateSetVramByte(&g, sizeof(g.vram), 0));
	uint8_t tile[64];
	assert(gamateGetVramTile(&g, 0, 0, tile));
	assert(tile[0] == 1 && tile[1] == 0 && tile[7] == 1);
	assert(!gamateGetVramTile(&g, 32 * 32, 0, tile));
	assert(!gamateGetVramTile(&g, 0, 2, tile));
	gamateFree(&g);
}

static void assertPeekMatchesRead(const struct Gamate* g, uint16_t address) {
	struct Gamate copy = *g; // reads on the copy leave the machine under test untouched
	assert(gamatePeek(g, address) == gamateRead(&copy, address));
}

static void testDebugExecutionControl(void) {
	struct Gamate g, stepped;
	gamateInit(&g);
	gamateInit(&stepped);
	uint8_t bios[GAMATE_BIOS_SIZE] = { 0 }, rom[0x4000] = { 0 };
	bios[0x000] = 0x20; bios[0x001] = 0x10; bios[0x002] = 0xE0; // JSR $E010
	bios[0x003] = 0xEA; // return site
	bios[0x010] = 0xEA;
	bios[0x011] = 0x60; // RTS
	bios[0x020] = 0x02; // unsupported
	bios[0x030] = 0x00; // BRK
	bios[0x040] = 0xEA;
	bios[0x036] = 0xEA; // BIOS trap $F036
	bios[0x100] = 0xEA;
	bios[0xFFC] = 0x00; bios[0xFFD] = 0xE0;
	bios[0xFFE] = 0x00; bios[0xFFF] = 0xE1;
	assert(gamateLoad(&g, rom, sizeof(rom), bios, sizeof(bios)));
	assert(gamateLoad(&stepped, rom, sizeof(rom), bios, sizeof(bios)));

	// One instruction plus device clocks is the primitive behind frame and debugger execution.
	gamateReset(&g);
	unsigned cycles = gamateStep(&g);
	assert(cycles == 6 && g.cpu.pc == 0xE010 &&
		g.masterClocks == 12 && g.cpu.s == 0xFB);

	gamateReset(&g);
	assert(gamateDebugStepOver(&g) == GAMATE_DEBUG_STOP_STEP_OVER);
	assert(g.cpu.pc == 0xE003);
	gamateReset(&g);
	g.cpu.pc = 0xE010;
	g.cpu.s = 0xFB;
	g.ram[0x1FC] = 0x02;
	g.ram[0x1FD] = 0xE0;
	assert(gamateDebugStepOut(&g) == GAMATE_DEBUG_STOP_STEP_OUT);
	assert(g.cpu.pc == 0xE003);

	gamateReset(&g);
	gamateDebugSetBreakpoint(&g, 0xE003, true);
	assert(gamateDebugRun(&g) == GAMATE_DEBUG_STOP_BREAKPOINT);
	assert(g.cpu.pc == 0xE003);
	assert(gamateDebugBreakpointEnabled(&g, 0xE003));
	gamateDebugClearBreakpoint(&g, 0xE003);
	assert(!gamateDebugBreakpointEnabled(&g, 0xE003));

	gamateReset(&g);
	g.cpu.pc = 0xE003;
	assert(gamateDebugRunTo(&g, 0xE004) == GAMATE_DEBUG_STOP_RUN_TO);
	assert(g.cpu.pc == 0xE004);

	gamateReset(&g);
	g.cpu.pc = 0xE020;
	gamateDebugSetBreakOnUnsupportedOpcode(&g, true);
	assert(gamateDebugRun(&g) == GAMATE_DEBUG_STOP_UNSUPPORTED_OPCODE);
	assert(g.diagnostics.lastUnsupportedPc == 0xE020 &&
		g.diagnostics.lastUnsupportedOpcode == 0x02);

	gamateReset(&g);
	g.cpu.pc = 0xE030;
	gamateSetBreakOnBrk(&g, true);
	assert(gamateDebugRun(&g) == GAMATE_DEBUG_STOP_BRK);

	gamateReset(&g);
	g.cpu.pc = 0xF036;
	gamateSetBreakOnBiosTrap(&g, true);
	assert(gamateDebugRun(&g) == GAMATE_DEBUG_STOP_BIOS_TRAP);
	assert(g.cpu.pc == 0xF037);

	gamateReset(&g);
	g.cpu.pc = 0xE040;
	g.cpu.p &= (uint8_t) ~0x04;
	g.cpu.irqMask = false;
	g.irq = true;
	gamateSetBreakOnIrq(&g, true);
	assert(gamateDebugRun(&g) == GAMATE_DEBUG_STOP_IRQ);
	assert(g.diagnostics.irqsTaken == 1);

	gamateReset(&g);
	g.cpu.pc = 0xE040;
	g.nmi = true;
	gamateSetBreakOnNmi(&g, true);
	assert(gamateDebugRun(&g) == GAMATE_DEBUG_STOP_NMI);
	assert(g.diagnostics.nmisTaken == 1);

	// Repeated instruction stepping produces the same machine state as gamateRunFrame.
	gamateReset(&g);
	gamateReset(&stepped);
	gamateRunFrame(&g);
	g.frameDone = false;
	gamateReset(&stepped);
	g.frameDone = false;
	stepped.frameDone = false;
	while (!stepped.frameDone) gamateStep(&stepped);
	size_t stateSize = gamateStateSize(&g);
	void* frameState = malloc(stateSize);
	void* steppedState = malloc(stateSize);
	assert(frameState && steppedState);
	assert(gamateSaveState(&g, frameState, stateSize));
	assert(gamateSaveState(&stepped, steppedState, stateSize));
	assert(!memcmp(frameState, steppedState, stateSize));
	free(frameState);
	free(steppedState);

	gamateFree(&g);
	gamateFree(&stepped);
}

static void testDebugAccess(void) {
	struct Gamate g;
	gamateInit(&g);
	uint8_t bios[GAMATE_BIOS_SIZE], rom[0x10000];
	for (size_t i = 0; i < sizeof(bios); i++) bios[i] = (uint8_t) (i * 3);
	for (size_t i = 0; i < sizeof(rom); i++) rom[i] = (uint8_t) (i >> 8);
	assert(gamateLoad(&g, rom, sizeof(rom), bios, sizeof(bios)));
	g.ram[0x123] = 0x5A;
	g.buttons = 0xFE;
	g.vramAddress = 0x42;
	g.vramPlane = true;
	g.vram[(0x42 << 1) | 1] = 0x77;
	assert(gamatePeek(&g, 0x6000) == 0); // no protection reply before the handshake

	static const char key[] = "BIT CORPORATION";
	for (size_t byte = 0; byte < sizeof(key) - 1; byte++) {
		for (int bit = 7; bit >= 0; bit--) {
			gamateWrite(&g, 0x6000, (key[byte] >> bit & 1) << 2);
		}
		gamateWrite(&g, 0x6000, 0);
	}

	uint16_t addresses[] = {
		0x0000, 0x0523, 0x1FFF, 0x2000, 0x4000, 0x4100, 0x4400, 0x4800, 0x4C00, 0x5001,
		0x5006, 0x5400, 0x5800, 0x5A00, 0x5FFF, 0x6000, 0x6001, 0x8000, 0xC000, 0xDFFF,
		0xE000, 0xFFFF,
	};
	size_t addressCount = sizeof(addresses) / sizeof(addresses[0]);

	size_t stateSize = gamateStateSize(&g);
	uint8_t* stateBefore = malloc(stateSize);
	uint8_t* stateAfter = malloc(stateSize);
	assert(stateBefore && stateAfter);
	assert(gamateSaveState(&g, stateBefore, stateSize));
	struct GamateDiagnostics diagnosticsBefore = g.diagnostics;
	uint8_t openBusBefore = g.openBus;
	for (uint32_t address = 0; address <= UINT16_MAX; address++) {
		(void) gamatePeek(&g, (uint16_t) address);
	}
	assert(gamateSaveState(&g, stateAfter, stateSize));
	assert(memcmp(stateBefore, stateAfter, stateSize) == 0);
	assert(memcmp(&diagnosticsBefore, &g.diagnostics, sizeof(diagnosticsBefore)) == 0);
	assert(g.openBus == openBusBefore);
	free(stateBefore);
	free(stateAfter);

	for (size_t i = 0; i < addressCount; i++) {
		assertPeekMatchesRead(&g, addresses[i]);
	}
	// The protection reply is consumed by reads, not by peeks: each peek shows the next bit.
	for (int bit = 0; bit < 8; bit++) {
		uint8_t peeked = gamatePeek(&g, 0x6000);
		assert(peeked == gamateRead(&g, 0x6000));
	}
	assert(g.protectionPassed);
	assert(gamatePeek(&g, 0x6100) == 1 && gamateRead(&g, 0x6100) == 1);

	gamateResetRamDiagnostics(&g);
	struct RamAccessCapture capture = { 0 };
	uint64_t ramWritesBefore = g.diagnostics.ramTrackedWrites[0];
	gamateSetRamDiagnosticWatch(&g, 0x123, captureRamAccess, &capture);
	assert(gamatePoke(&g, 0x0523, 0x11));
	assert(g.ram[0x123] == 0x11 && gamatePeek(&g, 0x0123) == 0x11);
	assert(capture.count == 0 && g.diagnostics.ramTrackedWrites[0] == ramWritesBefore);
	gamateSetRamDiagnosticWatch(&g, 0, NULL, NULL);

	assert(gamatePoke(&g, 0xF123, 0x99));
	assert(g.bios[0x123] == 0x99 && gamateRead(&g, 0xE123) == 0x99);
	assert(gamatePoke(&g, 0x8000, 0xAB));
	assert(g.rom[0x2000] == 0xAB && gamateRead(&g, 0x8000) == 0xAB);
	uint8_t* state = malloc(stateSize);
	assert(state && gamateSaveState(&g, state, stateSize));
	assert(gamatePoke(&g, 0xF123, 0x55) && gamatePoke(&g, 0x8000, 0x66));
	assert(gamateLoadState(&g, state, stateSize));
	assert(g.bios[0x123] == 0x55 && g.rom[0x2000] == 0x66);
	free(state);

	bool displayBlankBefore = g.displayBlank;
	uint8_t ayRegisterBefore = g.ayRegisters[0];
	assert(!gamatePoke(&g, 0x4000, 0x01));
	assert(!gamatePoke(&g, 0x5001, 0x80));
	assert(!gamatePoke(&g, 0x5006, 0x12));
	assert(g.displayBlank == displayBlankBefore && g.ayRegisters[0] == ayRegisterBefore);
	assert(g.vramAddress == 0x42);

	assert(gamateRomOffset(&g, 0x6000) == 0 && gamateRomOffset(&g, 0x8000) == 0x2000);
	gamateWrite(&g, 0xC000, 3);
	assert(gamateRomOffset(&g, 0xA123) == 0xC123);
	gamateSetMapper(&g, GAMATE_MAPPER_4IN1);
	gamateWrite(&g, 0x8000, 2);
	assert(gamateRomOffset(&g, 0x6123) == 0x8123);
	assert(gamateRomOffset(&g, 0x5FFF) == SIZE_MAX && gamateRomOffset(&g, 0xE000) == SIZE_MAX);
	gamateFree(&g);
}

int main(void) {
	testMapperDetection();
	testMemory();
	testDebugAccess();
	testDebugExecutionControl();
	testWatchpoints();
	testTraceAndDisassembly();
	testStateViewsAndEditing();
	testUart();
	testVideo();
	testVideoWindows();
	testProgressiveRows();
	testTimingAndState();
	testIrq();
	testIrqMaskTiming();
	testCpu();
	testCpuDecimal();
	testCpuBusAccesses();
	testAudio();
	testClockBatching();
	testSampleEventBatching();
	return 0;
}
