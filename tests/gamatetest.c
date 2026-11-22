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
	uint8_t previous, value;
};

struct RomReadCapture {
	unsigned count;
	uint16_t address;
	size_t physicalOffset;
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
}

static void captureRomRead(void* context, const struct Gamate* g,
		uint16_t address, size_t physicalOffset) {
	struct RomReadCapture* capture = context;
	assert(g);
	capture->count++;
	capture->address = address;
	capture->physicalOffset = physicalOffset;
}

static void captureLinkBit(void* context, bool bit) {
	struct LinkCapture* capture = context;
	capture->bits = (uint8_t) (capture->bits << 1 | bit);
	capture->count++;
	gamateUartReceiveBit(capture->receiver, bit);
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
	gamateWrite(&g, 0x4400, 0);
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

int main(void) {
	testMemory();
	testUart();
	testVideo();
	testVideoWindows();
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
