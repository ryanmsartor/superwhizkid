/* Copyright (c) 2026 Ry
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef SUPERWHIZKID_GAMATE_H
#define SUPERWHIZKID_GAMATE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum { GAMATE_WIDTH = 160, GAMATE_HEIGHT = 150 };
enum { GAMATE_BIOS_SIZE = 0x1000, GAMATE_MAX_ROM_SIZE = 0x80000 };

enum GamateMapper { GAMATE_MAPPER_PLAIN, GAMATE_MAPPER_BANKED, GAMATE_MAPPER_4IN1 };

struct GamateCpu {
	uint16_t pc;
	uint8_t a, x, y, s, p;
	uint64_t cycles;
	bool stopped, irqMask;
};

struct GamateDiagnostics {
	uint64_t instructions, unsupportedOpcodes, irqsTaken, nmisTaken;
	uint64_t ayWrites, lcdWrites, vramWrites, bankWrites;
	uint64_t ayRegisterWrites[16], ayActiveSamples[3];
	uint64_t controllerReads, pressedControllerReads;
	uint16_t lastUnsupportedPc;
	uint8_t lastUnsupportedOpcode;
};

struct GamateAy {
	uint32_t toneCounter[3], noiseCounter, envelopeCounter;
	uint32_t noiseLfsr;
	int8_t envelopeStep;
	uint8_t envelopeAttack;
	bool toneLevel[3], noiseLevel;
	bool envelopeHold, envelopeAlternate, envelopeHolding;
	int32_t previousInput[2], highPass[2];
};

struct Gamate {
	struct GamateCpu cpu;
	uint8_t ram[0x400];
	uint8_t vram[0x4000];
	uint8_t bios[GAMATE_BIOS_SIZE];
	uint8_t* rom;
	size_t romSize;

	uint16_t vramAddress;
	uint8_t scrollX, scrollY;
	uint8_t bank, firstBank;
	bool vramPlane, incrementRow, window, swapPlanes, displayBlank, displayStopped;
	enum GamateMapper mapper;
	bool protectionPassed, protectionFailed, protectionReply;
	uint8_t protectionPosition, protectionByte, protectionSequence;

	uint8_t buttons, pendingPresses, openBus;
	uint8_t uartTx, uartRx, uartTxBits, uartRxBits;
	bool uartTxLine;
	uint64_t masterClocks;
	uint32_t frameClocks, irqClocks;
	bool frameDone, irq, nmi;

	uint8_t ayAddress;
	uint8_t ayRegisters[16];
	struct GamateAy ay;
	int16_t* audioOut;
	size_t audioCapacity, audioCount;
	unsigned sampleRate;
	uint64_t samplePhase;

	uint16_t frameBuffer[GAMATE_WIDTH * GAMATE_HEIGHT];
	uint8_t lcdAccum[GAMATE_WIDTH * GAMATE_HEIGHT];
	uint16_t lcdClock, lcdRowEvent;
	// Observational counters for bring-up tools; deliberately excluded from save states.
	struct GamateDiagnostics diagnostics;
};

void gamateInit(struct Gamate* gamate);
bool gamateLoad(struct Gamate* gamate, const uint8_t* rom, size_t romSize,
	const uint8_t* bios, size_t biosSize);
void gamateSetMapper(struct Gamate* gamate, enum GamateMapper mapper);
void gamateFree(struct Gamate* gamate);
void gamateReset(struct Gamate* gamate);

uint8_t gamateRead(struct Gamate* gamate, uint16_t address);
void gamateWrite(struct Gamate* gamate, uint16_t address, uint8_t value);
void gamateRunClocks(struct Gamate* gamate, unsigned clocks);
void gamateSetButtons(struct Gamate* gamate, uint8_t buttons);
void gamateRunFrame(struct Gamate* gamate);
// Supply one rising receiver-clock edge and its data bit from a future link transport.
void gamateUartReceiveBit(struct Gamate* gamate, bool bit);
// Immediate diagnostic preview; normal execution builds the picture progressively in run clocks.
void gamateRenderSnapshot(struct Gamate* gamate);
void gamateSetAudio(struct Gamate* gamate, int16_t* output, size_t frameCapacity, unsigned sampleRate);
void gamateCpuReset(struct Gamate* gamate);
unsigned gamateCpuStep(struct Gamate* gamate);

size_t gamateStateSize(const struct Gamate* gamate);
bool gamateSaveState(const struct Gamate* gamate, void* data, size_t size);
bool gamateLoadState(struct Gamate* gamate, const void* data, size_t size);

#endif
