/* Copyright (c) 2026 Ry
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
#include "gamate.h"

#include <stdlib.h>
#include <string.h>

// The LCD receives 72,900 clocks per frame from the 4.433 MHz master oscillator.
enum { IRQ_PERIOD = 32768 };
enum { STATE_MAGIC = 0x54414D47, STATE_VERSION = 4 };

static uint32_t crc32(const uint8_t* data, size_t size) {
	uint32_t crc = 0xFFFFFFFF;
	while (size--) {
		crc ^= *data++;
		for (int bit = 0; bit < 8; bit++) crc = crc >> 1 ^ (0xEDB88320U & -(crc & 1));
	}
	return ~crc;
}

struct GamateState {
	uint32_t magic, version, size, romSize;
	uint8_t ram[0x400];
	uint8_t vram[0x4000];
	uint16_t vramAddress;
	uint8_t scrollX, scrollY, bank, firstBank, mapper;
	uint8_t flags;
	uint8_t protectionFlags, protectionPosition, protectionByte, protectionSequence;
	uint8_t buttons, openBus;
	uint8_t uartTx, uartRx, uartTxBits, uartRxBits, uartTxLine;
	uint64_t masterClocks;
	uint32_t frameClocks, irqClocks;
	uint8_t ayAddress, ayRegisters[16];
	struct GamateAy ay;
	uint64_t samplePhase;
	struct GamateCpu cpu;
	uint8_t lcdAccum[GAMATE_WIDTH * GAMATE_HEIGHT];
	uint16_t frameBuffer[GAMATE_WIDTH * GAMATE_HEIGHT];
	uint16_t lcdClock, lcdRowEvent;
};

static uint32_t tonePeriodFromRegisters(const struct Gamate* g, unsigned channel) {
	uint32_t period = g->ayRegisters[channel * 2] |
		(uint32_t) g->ayRegisters[channel * 2 + 1] << 8;
	return (period ? period : 1) * 32;
}

static void updateAyPeriods(struct Gamate* g) {
	for (unsigned channel = 0; channel < 3; channel++) {
		g->ayTonePeriod[channel] = tonePeriodFromRegisters(g, channel);
	}
	g->ayNoisePeriod = (g->ayRegisters[6] ? g->ayRegisters[6] : 1) * 64;
	uint32_t envelope = g->ayRegisters[11] | (uint32_t) g->ayRegisters[12] << 8;
	g->ayEnvelopePeriod = (envelope ? envelope : 1) * 1024;
}

void gamateInit(struct Gamate* g) {
	memset(g, 0, sizeof(*g));
	g->sampleRate = 48000;
}

bool gamateLoad(struct Gamate* g, const uint8_t* rom, size_t romSize,
		const uint8_t* bios, size_t biosSize) {
	if (!rom || !romSize || romSize > GAMATE_MAX_ROM_SIZE || !bios || biosSize != GAMATE_BIOS_SIZE) {
		return false;
	}
	uint8_t* copy = malloc(romSize);
	if (!copy) {
		return false;
	}
	memcpy(copy, rom, romSize);
	free(g->rom);
	g->rom = copy;
	g->romSize = romSize;
	memcpy(g->bios, bios, GAMATE_BIOS_SIZE);
	// The only known 4-in-1 has the same size as ordinary banked games, so size cannot identify it.
	// Use its full-image fingerprint; unknown large cartridges retain the common banked mapper.
	g->mapper = romSize == 0x20000 && crc32(rom, romSize) == 0x10E81571 ? GAMATE_MAPPER_4IN1 :
		romSize >= 0x10000 ? GAMATE_MAPPER_BANKED : GAMATE_MAPPER_PLAIN;
	gamateReset(g);
	return true;
}

void gamateSetMapper(struct Gamate* g, enum GamateMapper mapper) {
	if (mapper >= GAMATE_MAPPER_PLAIN && mapper <= GAMATE_MAPPER_4IN1) {
		g->mapper = mapper;
		g->bank = g->firstBank = 0;
	}
}

void gamateFree(struct Gamate* g) {
	free(g->rom);
	g->rom = NULL;
	g->romSize = 0;
}

void gamateReset(struct Gamate* g) {
	memset(g->ram, 0xFF, sizeof(g->ram));
	memset(g->vram, 0, sizeof(g->vram));
	g->vramAddress = 0;
	g->scrollX = 0;
	g->scrollY = 10;
	g->bank = 0;
	g->firstBank = 0;
	g->vramPlane = false;
	g->incrementRow = false;
	g->window = false;
	g->swapPlanes = false;
	g->displayBlank = false;
	g->displayStopped = false;
	g->protectionPassed = false;
	g->protectionFailed = false;
	g->protectionReply = false;
	g->protectionPosition = 0;
	g->protectionByte = 0;
	g->protectionSequence = 0;
	g->masterClocks = 0;
	g->frameClocks = 0;
	g->irqClocks = 0;
	g->frameDone = false;
	g->irq = g->nmi = false;
	g->buttons = 0xFF; // all controls are active low
	g->openBus = 0xFF;
	g->uartTx = g->uartRx = g->uartTxBits = g->uartRxBits = 0;
	g->uartTxLine = false;
	g->ayAddress = 0;
	memset(g->ayRegisters, 0, sizeof(g->ayRegisters));
	updateAyPeriods(g);
	memset(&g->ay, 0, sizeof(g->ay));
	g->ay.noiseLfsr = 0x1FFFF;
	g->ay.noiseLevel = true;
	g->ay.envelopeStep = 15;
	g->samplePhase = 0;
	g->audioCount = 0;
	g->lcdClock = g->lcdRowEvent = 0;
	memset(g->lcdAccum, 0, sizeof(g->lcdAccum));
	memset(g->frameBuffer, 0, sizeof(g->frameBuffer));
	memset(&g->diagnostics, 0, sizeof(g->diagnostics));
	gamateCpuReset(g);
}

static size_t romOffset(const struct Gamate* g, uint16_t address) {
	size_t offset = address - 0x6000;
	if (g->mapper == GAMATE_MAPPER_4IN1 && offset < 0x4000) {
		offset = (size_t) g->firstBank * 0x4000 + offset;
	} else if (g->mapper != GAMATE_MAPPER_PLAIN && offset >= 0x4000) {
		offset = (size_t) g->bank * 0x4000 + (offset & 0x3FFF);
	}
	if (!g->romSize) return 0;
	return offset < g->romSize ? offset : offset % g->romSize;
}

static void incrementVram(struct Gamate* g) {
	g->vramAddress = (g->vramAddress + (g->incrementRow ? 0x20 : 1)) & 0x1FFF;
}

#ifdef GAMATE_ACCESS_DIAGNOSTICS
static bool setDiagnosticBit(uint8_t* bits, size_t offset) {
	uint8_t mask = (uint8_t) 1 << (offset & 7);
	uint8_t* byte = &bits[offset >> 3];
	bool newlySet = !(*byte & mask);
	*byte |= mask;
	return newlySet;
}

void gamateResetVramDiagnostics(struct Gamate* g) {
	g->diagnostics.vramCoverageEnabled = true;
	g->diagnostics.vramTrackedWrites = 0;
	g->diagnostics.vramReads = 0;
	g->diagnostics.vramReadBeforeWrite = 0;
	g->diagnostics.vramDisplayReads = 0;
	g->diagnostics.vramDisplayReadBeforeWrite = 0;
	g->diagnostics.vramUniqueWritten = 0;
	g->diagnostics.vramUniqueReadBeforeWrite = 0;
	g->diagnostics.vramUniqueDisplayReadBeforeWrite = 0;
	memset(g->diagnostics.vramWritten, 0, sizeof(g->diagnostics.vramWritten));
	memset(g->diagnostics.vramReadBeforeWriteMap, 0,
		sizeof(g->diagnostics.vramReadBeforeWriteMap));
	memset(g->diagnostics.vramDisplayReadBeforeWriteMap, 0,
		sizeof(g->diagnostics.vramDisplayReadBeforeWriteMap));
}

static void markVramRead(struct Gamate* g, uint16_t offset, bool display) {
	if (!g->diagnostics.vramCoverageEnabled) return;
	if (display) g->diagnostics.vramDisplayReads++;
	else g->diagnostics.vramReads++;
	if (g->diagnostics.vramWritten[offset >> 3] & ((uint8_t) 1 << (offset & 7))) return;
	if (display) {
		g->diagnostics.vramDisplayReadBeforeWrite++;
		g->diagnostics.vramUniqueDisplayReadBeforeWrite += setDiagnosticBit(
			g->diagnostics.vramDisplayReadBeforeWriteMap, offset);
	} else {
		g->diagnostics.vramReadBeforeWrite++;
		g->diagnostics.vramUniqueReadBeforeWrite += setDiagnosticBit(
			g->diagnostics.vramReadBeforeWriteMap, offset);
	}
}

static void markVramWrite(struct Gamate* g, uint16_t offset) {
	if (!g->diagnostics.vramCoverageEnabled) return;
	g->diagnostics.vramTrackedWrites++;
	g->diagnostics.vramUniqueWritten += setDiagnosticBit(g->diagnostics.vramWritten, offset);
}

void gamateResetRamDiagnostics(struct Gamate* g) {
	g->diagnostics.ramCoverageEnabled = true;
	g->diagnostics.ramUniqueWritten = 0;
	memset(g->diagnostics.ramTrackedWrites, 0, sizeof(g->diagnostics.ramTrackedWrites));
	memset(g->diagnostics.ramReads, 0, sizeof(g->diagnostics.ramReads));
	memset(g->diagnostics.ramReadBeforeWrite, 0, sizeof(g->diagnostics.ramReadBeforeWrite));
	memset(g->diagnostics.ramUniqueReadBeforeWrite, 0,
		sizeof(g->diagnostics.ramUniqueReadBeforeWrite));
	memset(g->diagnostics.ramWritten, 0, sizeof(g->diagnostics.ramWritten));
	memset(g->diagnostics.ramReadBeforeWriteMap, 0,
		sizeof(g->diagnostics.ramReadBeforeWriteMap));
}

void gamateResetRomDiagnostics(struct Gamate* g) {
	g->diagnostics.romCoverageEnabled = true;
	g->diagnostics.romTrackedReads = 0;
	g->diagnostics.romUniqueRead = 0;
	memset(g->diagnostics.romRead, 0, sizeof(g->diagnostics.romRead));
}

static void markRomRead(struct Gamate* g, uint16_t address, size_t offset) {
	if (!g->diagnostics.romCoverageEnabled || g->diagnostics.diagnosticInstructionPc < 0xE000) return;
	g->diagnostics.romTrackedReads++;
	g->diagnostics.romUniqueRead += setDiagnosticBit(g->diagnostics.romRead, offset);
	if (g->diagnostics.romReadCallback) {
		g->diagnostics.romReadCallback(g->diagnostics.romReadContext, g, address, offset);
	}
}

void gamateSetRomDiagnosticWatch(struct Gamate* g,
		GamateRomReadCallback callback, void* context) {
	g->diagnostics.romReadCallback = callback;
	g->diagnostics.romReadContext = context;
}

void gamateSetRamDiagnosticWatch(struct Gamate* g, uint16_t address,
		GamateRamAccessCallback callback, void* context) {
	g->diagnostics.ramWatchAddress = address & 0x3FF;
	g->diagnostics.ramWatchEnabled = callback != NULL;
	g->diagnostics.ramAccessCallback = callback;
	g->diagnostics.ramAccessContext = context;
}

static unsigned ramDiagnosticOwner(const struct Gamate* g) {
	uint16_t pc = g->diagnostics.diagnosticInstructionPc;
	return pc >= 0x6000 && pc < 0xE000 ? 0 : pc >= 0xE000 ? 1 : 2;
}

static void markRamRead(struct Gamate* g, uint16_t offset) {
	if (!g->diagnostics.ramCoverageEnabled) return;
	unsigned owner = ramDiagnosticOwner(g);
	g->diagnostics.ramReads[owner]++;
	if (g->diagnostics.ramWritten[offset >> 3] & ((uint8_t) 1 << (offset & 7))) return;
	g->diagnostics.ramReadBeforeWrite[owner]++;
	g->diagnostics.ramUniqueReadBeforeWrite[owner] += setDiagnosticBit(
		g->diagnostics.ramReadBeforeWriteMap[owner], offset);
}

static void markRamWrite(struct Gamate* g, uint16_t offset) {
	if (!g->diagnostics.ramCoverageEnabled) return;
	unsigned owner = ramDiagnosticOwner(g);
	g->diagnostics.ramTrackedWrites[owner]++;
	g->diagnostics.ramUniqueWritten += setDiagnosticBit(g->diagnostics.ramWritten, offset);
}
#endif

static void protectionWrite(struct Gamate* g, bool bit) {
	static const uint8_t key[] = "BIT CORPORATION";
	if (g->protectionPosition < 8) {
		g->protectionByte |= bit << (7 - g->protectionPosition++);
		return;
	}
	if (!g->protectionFailed && g->protectionSequence < sizeof(key) - 1 &&
			g->protectionByte != key[g->protectionSequence]) {
		g->protectionFailed = true;
	}
	g->protectionPosition = 0;
	g->protectionByte = 0;
	if (++g->protectionSequence == sizeof(key) - 1 && !g->protectionFailed) {
		g->protectionByte = 0x47;
		g->protectionReply = true;
	}
}

static uint8_t protectionRead(struct Gamate* g) {
	if (!g->protectionReply) {
		return 0;
	}
	uint8_t bit = (g->protectionByte >> (7 - g->protectionPosition++)) & 1;
	if (g->protectionPosition == 8) {
		g->protectionPassed = true;
		g->protectionPosition = 0;
		g->protectionByte = 0;
	}
	return bit;
}

static void uartClock(struct Gamate* g) {
	g->diagnostics.uartClocks++;
	if (g->linkBitCallback) g->linkBitCallback(g->linkBitContext, g->uartTxLine);
	if (g->uartTxBits < 8) {
		g->uartTx <<= 1;
		g->uartTxBits++;
	}
	g->uartTxLine = g->uartTxBits < 8 && (g->uartTx & 0x80);
}

void gamateSetLink(struct Gamate* g, GamateLinkBitCallback callback, void* context) {
	g->linkBitCallback = callback;
	g->linkBitContext = context;
}

void gamateUartReceiveBit(struct Gamate* g, bool bit) {
	g->diagnostics.uartReceivedBits++;
	g->uartRx = (uint8_t) (g->uartRx << 1 | bit);
	if (++g->uartRxBits == 8) {
		g->uartRxBits = 0;
		g->nmi = true;
	}
}

uint8_t gamateRead(struct Gamate* g, uint16_t address) {
	uint8_t value;
	if (address < 0x2000) {
		uint16_t offset = address & 0x3FF;
#ifdef GAMATE_ACCESS_DIAGNOSTICS
		markRamRead(g, offset);
		if (g->diagnostics.ramWatchEnabled && offset == g->diagnostics.ramWatchAddress) {
			uint8_t value = g->ram[offset];
			g->diagnostics.ramAccessCallback(g->diagnostics.ramAccessContext,
				g, false, offset, value, value);
		}
#endif
		value = g->ram[offset];
		goto done;
	}
	if ((address & 0xFC00) == 0x4000) {
		// The PSG is write-only. Undriven reads expose the high address byte.
		value = address >> 8;
		goto done;
	}
	if ((address & 0xFC00) == 0x4400) {
		g->diagnostics.controllerReads++;
		value = g->buttons;
		g->diagnostics.pressedControllerReads += value != 0xFF;
		goto done;
	}
	if ((address & 0xFC00) == 0x4800) {
		value = g->uartRx; // remains zero until a link peer is connected
		goto done;
	}
	if ((address & 0xFC00) == 0x4C00) {
		uartClock(g);
		value = address >> 8; // clock register does not drive read data
		goto done;
	}
	if ((address & 0xFC00) == 0x5000 && (address & 7) == 6) {
		uint16_t offset = (g->vramAddress << 1) | g->vramPlane;
#ifdef GAMATE_ACCESS_DIAGNOSTICS
		markVramRead(g, offset, false);
#endif
		value = g->vram[offset];
		incrementVram(g);
		goto done;
	}
	if ((address & 0xFF00) == 0x5A00) {
		value = (address >> 8) | 3;
		goto done;
	}
	if (address >= 0x6000 && address < 0xE000) {
		if (g->protectionPassed && g->rom) {
			size_t offset = romOffset(g, address);
#ifdef GAMATE_ACCESS_DIAGNOSTICS
			markRomRead(g, address, offset);
#endif
			value = g->rom[offset];
		} else value = protectionRead(g) << 1;
		goto done;
	}
	if (address >= 0xE000) {
		value = g->bios[address & 0x0FFF];
		goto done;
	}
	value = address >= 0x5400 && address < 0x5800 ? 0xFF : address >> 8;
done:
	g->openBus = value;
	return value;
}

void gamateWrite(struct Gamate* g, uint16_t address, uint8_t value) {
	if (address < 0x2000) {
		uint16_t offset = address & 0x3FF;
#ifdef GAMATE_ACCESS_DIAGNOSTICS
		uint8_t previous = g->ram[offset];
#endif
		g->ram[offset] = value;
#ifdef GAMATE_ACCESS_DIAGNOSTICS
		markRamWrite(g, offset);
		if (g->diagnostics.ramWatchEnabled && offset == g->diagnostics.ramWatchAddress) {
			g->diagnostics.ramAccessCallback(g->diagnostics.ramAccessContext,
				g, true, offset, previous, value);
		}
#endif
		return;
	}
	if ((address & 0xFC00) == 0x4000) {
		static const uint8_t masks[16] = {
			0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0x1F, 0x3F,
			0x1F, 0x1F, 0x1F, 0xFF, 0xFF, 0x0F, 0xFF, 0xFF,
		};
		g->ayAddress = address & 0x0F;
		g->diagnostics.ayWrites++;
		g->diagnostics.ayRegisterWrites[g->ayAddress]++;
		g->ayRegisters[g->ayAddress] = value & masks[g->ayAddress];
		if (g->ayAddress <= 5) {
			g->ayTonePeriod[g->ayAddress >> 1] = tonePeriodFromRegisters(g, g->ayAddress >> 1);
		} else if (g->ayAddress == 6) {
			g->ayNoisePeriod = (g->ayRegisters[6] ? g->ayRegisters[6] : 1) * 64;
		} else if (g->ayAddress == 11 || g->ayAddress == 12) {
			uint32_t envelope = g->ayRegisters[11] | (uint32_t) g->ayRegisters[12] << 8;
			g->ayEnvelopePeriod = (envelope ? envelope : 1) * 1024;
		}
		if (g->ayAddress == 13) {
			uint8_t shape = value & 0x0F;
			g->ay.envelopeAttack = shape & 4 ? 15 : 0;
			if (!(shape & 8)) {
				g->ay.envelopeHold = true;
				g->ay.envelopeAlternate = g->ay.envelopeAttack != 0;
			} else {
				g->ay.envelopeHold = shape & 1;
				g->ay.envelopeAlternate = shape & 2;
			}
			g->ay.envelopeHolding = false;
			g->ay.envelopeStep = 15;
			g->ay.envelopeCounter = 0;
		}
		return;
	}
	if ((address & 0xFC00) == 0x4400) {
		g->uartTx = value;
		g->uartTxBits = 0;
		g->uartTxLine = value & 0x80;
		return;
	}
	if ((address & 0xFC00) == 0x4C00) {
		uartClock(g);
		return;
	}
	if ((address & 0xFC00) == 0x5000) {
		g->diagnostics.lcdWrites++;
		switch (address & 7) {
		case 1:
			g->displayBlank = value & 0x80;
			g->incrementRow = value & 0x40;
			g->window = value & 0x20;
			g->swapPlanes = value & 0x10;
			g->displayStopped = value & 0x01;
			break;
		case 2: g->scrollX = value; break;
		case 3: g->scrollY = value; break;
		case 4:
			g->vramPlane = value & 0x80;
			g->vramAddress = (g->vramAddress & 0x3FE0) | (value & 0x1F);
			break;
		case 5: g->vramAddress = (g->vramAddress & 0x001F) | ((uint16_t) value << 5); break;
		case 7: {
			uint16_t offset = (g->vramAddress << 1) | g->vramPlane;
			g->vram[offset] = value;
#ifdef GAMATE_ACCESS_DIAGNOSTICS
			markVramWrite(g, offset);
#endif
			g->diagnostics.vramWrites++;
			incrementVram(g);
			break;
		}
		default: break;
		}
		return;
	}
	if (address >= 0x6000 && address < 0xE000 && !g->protectionPassed) {
		protectionWrite(g, (value & 4) != 0);
		return;
	}
	if (g->mapper == GAMATE_MAPPER_4IN1 && address == 0x8000) {
		if (g->firstBank != value) g->diagnostics.bankWrites++;
		g->firstBank = value;
	} else if (g->mapper != GAMATE_MAPPER_PLAIN && address == 0xC000) {
		if (g->bank != value) g->diagnostics.bankWrites++;
		g->bank = value;
	}
}

static int channelLevel(const struct Gamate* g, int channel) {
	static const uint8_t levels[16] = { 0, 1, 2, 3, 5, 7, 11, 15, 22, 31, 45, 63, 90, 127, 180, 255 };
	bool toneDisabled = g->ayRegisters[7] & (1 << channel);
	bool noise = (g->ayRegisters[7] & (8 << channel)) || g->ay.noiseLevel;
	if (!noise) return 0;
	uint8_t volume = g->ayRegisters[8 + channel];
	uint8_t index = volume & 0x10 ? (g->ay.envelopeStep ^ g->ay.envelopeAttack) : volume & 0x0F;
	int level = levels[index];
	if (toneDisabled) return level;

	// A point-sampled square wave above Nyquist aliases into a loud, false whistle. The physical
	// speaker cannot reproduce these ultrasonic periods; retain their average (50% duty) level so
	// noise mixing remains correct, then let the output high-pass remove the resulting DC.
	uint32_t period = g->ayTonePeriod[channel];
	if ((uint64_t) period * g->sampleRate <= 4433000) return level / 2;
	return g->ay.toneLevel[channel] ? level : 0;
}

static void audioSample(struct Gamate* g) {
	int a = channelLevel(g, 0), b = channelLevel(g, 1), c = channelLevel(g, 2);
	g->diagnostics.ayActiveSamples[0] += a != 0;
	g->diagnostics.ayActiveSamples[1] += b != 0;
	g->diagnostics.ayActiveSamples[2] += c != 0;
	int input[2] = { a * 80 + c * 40, b * 80 + c * 40 };
	int16_t sample[2];
	for (int side = 0; side < 2; side++) {
		// First-order high-pass filter removes the AY's unipolar DAC offset.
		int32_t out = input[side] - g->ay.previousInput[side] +
			(int32_t) ((int64_t) g->ay.highPass[side] * 32700 / 32768);
		g->ay.previousInput[side] = input[side];
		g->ay.highPass[side] = out;
		if (out > 32767) out = 32767;
		if (out < -32768) out = -32768;
		sample[side] = out;
	}
	if (g->audioOut && g->audioCount < g->audioCapacity) {
		g->audioOut[g->audioCount * 2] = sample[0];
		g->audioOut[g->audioCount * 2 + 1] = sample[1];
	}
	g->audioCount++;
}

void gamateSetAudio(struct Gamate* g, int16_t* output, size_t frameCapacity, unsigned sampleRate) {
	g->audioOut = output;
	g->audioCapacity = frameCapacity;
	g->sampleRate = sampleRate ? sampleRate : 48000;
}

static void displayPosition(const struct Gamate* g, int line, int* x, int* y) {
	if (g->window && line < 0x10) {
		*x = 0;
		*y = 0xD0 + line;
		return;
	}
	if (g->scrollY < 0xC8) {
		*y = line + g->scrollY;
		if (*y >= 0xC8) {
			*y -= 0xC8;
		}
		*x = g->scrollX;
		return;
	}
	*x = g->scrollX;
	if (g->scrollY & 8) {
		*y = line;
		return;
	}
	int first = g->scrollY & 7;
	int fixedLines = 8 - first;
	if (line < fixedLines) {
		*x = 0;
		*y = 0xF8 + first + line;
	} else {
		*y = line - fixedLines;
	}
}

static void lcdRefreshRow(struct Gamate* g) {
	if (g->displayStopped) return;
	int line = g->lcdRowEvent / 3;
	int pass = g->lcdRowEvent % 3;
	int sx, sy;
	displayPosition(g, line, &sx, &sy);
	int plane = pass != 0;
	if (g->swapPlanes) plane ^= 1;
	for (int x = 0; x < GAMATE_WIDTH; x++) {
		int px = (x + sx) & 0xFF;
		int offset = (((sy & 0xFF) * 0x20) + (px >> 3)) << 1 | plane;
#ifdef GAMATE_ACCESS_DIAGNOSTICS
		if (!g->displayBlank) markVramRead(g, (uint16_t) offset, true);
#endif
		uint8_t bit = g->displayBlank ? 0 : (g->vram[offset] >> (7 - (px & 7))) & 1;
		size_t pixel = (size_t) line * GAMATE_WIDTH + x;
		if (pass == 0) g->lcdAccum[pixel] = bit;
		else g->lcdAccum[pixel] += bit;
		if (pass == 2) g->frameBuffer[pixel] = g->lcdAccum[pixel];
	}
}

void gamateRenderSnapshot(struct Gamate* g) {
	if (g->displayStopped) {
		return;
	}
	for (int y = 0; y < GAMATE_HEIGHT; y++) {
		int sx, sy;
		displayPosition(g, y, &sx, &sy);
		for (int x = 0; x < GAMATE_WIDTH; x++) {
			int px = (x + sx) & 0xFF;
			int offset = (((sy & 0xFF) * 0x20) + (px >> 3)) << 1;
			int shift = 7 - (px & 7);
			int a = (g->vram[offset] >> shift) & 1;
			int b = (g->vram[offset + 1] >> shift) & 1;
			int pixel = g->swapPlanes ? b | (a << 1) : a | (b << 1);
			g->frameBuffer[y * GAMATE_WIDTH + x] = g->displayBlank ? 0 : (uint16_t) pixel;
		}
	}
}

static unsigned earlierEvent(unsigned current, uint32_t counter, uint32_t period) {
	unsigned until = counter < period ? period - counter : 1;
	return until < current ? until : current;
}

void gamateRunClocks(struct Gamate* g, unsigned clocks) {
	// Most CPU instructions end before the next observable device edge. Advance those counters in
	// one step; the event loop below retains the original ordering whenever any boundary is crossed.
	uint64_t sampleAfter = g->samplePhase + (uint64_t) clocks * g->sampleRate;
	bool tonesQuiet = true;
	for (int channel = 0; channel < 3; channel++) {
		tonesQuiet &= g->ay.toneCounter[channel] + clocks < g->ayTonePeriod[channel];
	}
	if (tonesQuiet && g->ay.noiseCounter + clocks < g->ayNoisePeriod &&
			g->ay.envelopeCounter + clocks < g->ayEnvelopePeriod && sampleAfter < 4433000 &&
			(g->irqClocks >= IRQ_PERIOD || g->irqClocks + clocks < IRQ_PERIOD) &&
			(g->displayStopped || g->lcdClock >= 162 || g->lcdClock + clocks < 162) &&
			(g->frameClocks >= GAMATE_CLOCKS_PER_FRAME ||
				g->frameClocks + clocks < GAMATE_CLOCKS_PER_FRAME)) {
		g->masterClocks += clocks;
		for (int channel = 0; channel < 3; channel++) g->ay.toneCounter[channel] += clocks;
		g->ay.noiseCounter += clocks;
		g->ay.envelopeCounter += clocks;
		g->samplePhase = sampleAfter;
		g->irqClocks += clocks;
		if (!g->displayStopped) g->lcdClock += clocks;
		g->frameClocks += clocks;
		return;
	}
	while (clocks) {
		unsigned step = clocks;
		for (int channel = 0; channel < 3; channel++) {
			step = earlierEvent(step, g->ay.toneCounter[channel], g->ayTonePeriod[channel]);
		}
		step = earlierEvent(step, g->ay.noiseCounter, g->ayNoisePeriod);
		step = earlierEvent(step, g->ay.envelopeCounter, g->ayEnvelopePeriod);
		if (g->irqClocks < IRQ_PERIOD) {
			step = earlierEvent(step, g->irqClocks, IRQ_PERIOD);
		}
		if (!g->displayStopped && g->lcdClock < 162) {
			step = earlierEvent(step, g->lcdClock, 162);
		}
		if (g->frameClocks < GAMATE_CLOCKS_PER_FRAME) {
			step = earlierEvent(step, g->frameClocks, GAMATE_CLOCKS_PER_FRAME);
		}
		// Avoid dividing unless the next device interval actually reaches a sample boundary.
		if (g->sampleRate && (g->samplePhase >= 4433000 ||
				g->samplePhase + (uint64_t) step * g->sampleRate >= 4433000)) {
			uint64_t remaining = g->samplePhase < 4433000 ? 4433000 - g->samplePhase : 0;
			uint64_t untilSample = remaining ?
				(remaining + g->sampleRate - 1) / g->sampleRate : 1;
			if (untilSample < step) step = (unsigned) untilSample;
		}

		g->masterClocks += step;
		for (int channel = 0; channel < 3; channel++) {
			g->ay.toneCounter[channel] += step;
			if (g->ay.toneCounter[channel] >= g->ayTonePeriod[channel]) {
				g->ay.toneCounter[channel] = 0;
				g->ay.toneLevel[channel] = !g->ay.toneLevel[channel];
			}
		}
		g->ay.noiseCounter += step;
		if (g->ay.noiseCounter >= g->ayNoisePeriod) {
			g->ay.noiseCounter = 0;
			uint32_t feedback = (g->ay.noiseLfsr ^ (g->ay.noiseLfsr >> 3)) & 1;
			g->ay.noiseLfsr = g->ay.noiseLfsr >> 1 | feedback << 16;
			g->ay.noiseLevel = g->ay.noiseLfsr & 1;
		}
		g->ay.envelopeCounter += step;
		if (g->ay.envelopeCounter >= g->ayEnvelopePeriod) {
			g->ay.envelopeCounter = 0;
			if (!g->ay.envelopeHolding && --g->ay.envelopeStep < 0) {
				if (g->ay.envelopeHold) {
					if (g->ay.envelopeAlternate) g->ay.envelopeAttack ^= 15;
					g->ay.envelopeHolding = true;
					g->ay.envelopeStep = 0;
				} else {
					if (g->ay.envelopeAlternate) g->ay.envelopeAttack ^= 15;
					g->ay.envelopeStep = 15;
				}
			}
		}
		g->samplePhase += (uint64_t) step * g->sampleRate;
		if (g->samplePhase >= 4433000) {
			g->samplePhase -= 4433000;
			audioSample(g);
		}
		g->irqClocks += step;
		if (g->irqClocks == IRQ_PERIOD) g->irq = true;
		if (!g->displayStopped) {
			g->lcdClock += step;
			if (g->lcdClock == 162) {
				g->lcdClock = 0;
				lcdRefreshRow(g);
				if (++g->lcdRowEvent == GAMATE_HEIGHT * 3) g->lcdRowEvent = 0;
			}
		}
		g->frameClocks += step;
		if (g->frameClocks == GAMATE_CLOCKS_PER_FRAME) {
			g->frameClocks = 0;
			g->frameDone = true;
		}
		clocks -= step;
	}
}

void gamateRunFrame(struct Gamate* g) {
	g->frameDone = false;
	g->audioCount = 0;
	while (!g->frameDone) {
		gamateRunClocks(g, gamateCpuStep(g) * 2);
	}
}

size_t gamateStateSize(const struct Gamate* g) {
	(void) g;
	return sizeof(struct GamateState);
}

bool gamateSaveState(const struct Gamate* g, void* data, size_t size) {
	if (!data || size < sizeof(struct GamateState)) {
		return false;
	}
	struct GamateState state = {
		.magic = STATE_MAGIC, .version = STATE_VERSION, .size = sizeof(state), .romSize = (uint32_t) g->romSize,
		.vramAddress = g->vramAddress, .scrollX = g->scrollX, .scrollY = g->scrollY,
		.bank = g->bank, .firstBank = g->firstBank, .mapper = g->mapper,
		.flags = g->vramPlane | g->incrementRow << 1 | g->window << 2 | g->swapPlanes << 3 |
			g->displayBlank << 4 | g->displayStopped << 5 | g->irq << 6 | g->nmi << 7,
		.protectionFlags = g->protectionPassed | g->protectionFailed << 1 | g->protectionReply << 2,
		.protectionPosition = g->protectionPosition, .protectionByte = g->protectionByte,
		.protectionSequence = g->protectionSequence,
		.buttons = g->buttons, .openBus = g->openBus,
		.uartTx = g->uartTx, .uartRx = g->uartRx, .uartTxBits = g->uartTxBits,
		.uartRxBits = g->uartRxBits,
		.uartTxLine = g->uartTxLine,
		.masterClocks = g->masterClocks, .frameClocks = g->frameClocks, .irqClocks = g->irqClocks,
		.ayAddress = g->ayAddress,
		.cpu = g->cpu,
		.ay = g->ay, .samplePhase = g->samplePhase,
		.lcdClock = g->lcdClock, .lcdRowEvent = g->lcdRowEvent,
	};
	memcpy(state.ram, g->ram, sizeof(state.ram));
	memcpy(state.vram, g->vram, sizeof(state.vram));
	memcpy(state.ayRegisters, g->ayRegisters, sizeof(state.ayRegisters));
	memcpy(state.lcdAccum, g->lcdAccum, sizeof(state.lcdAccum));
	memcpy(state.frameBuffer, g->frameBuffer, sizeof(state.frameBuffer));
	memcpy(data, &state, sizeof(state));
	return true;
}

bool gamateLoadState(struct Gamate* g, const void* data, size_t size) {
	if (!data || size < sizeof(struct GamateState)) {
		return false;
	}
	struct GamateState state;
	memcpy(&state, data, sizeof(state));
	if (state.magic != STATE_MAGIC || state.version != STATE_VERSION || state.size != sizeof(state) ||
			state.romSize != g->romSize) {
		return false;
	}
	memcpy(g->ram, state.ram, sizeof(g->ram));
	memcpy(g->vram, state.vram, sizeof(g->vram));
	g->vramAddress = state.vramAddress;
	g->scrollX = state.scrollX;
	g->scrollY = state.scrollY;
	g->bank = state.bank;
	g->firstBank = state.firstBank;
	g->mapper = state.mapper <= GAMATE_MAPPER_4IN1 ? state.mapper : GAMATE_MAPPER_PLAIN;
	g->vramPlane = state.flags & 1;
	g->incrementRow = state.flags & 2;
	g->window = state.flags & 4;
	g->swapPlanes = state.flags & 8;
	g->displayBlank = state.flags & 16;
	g->displayStopped = state.flags & 32;
	g->irq = state.flags & 64;
	g->nmi = state.flags & 128;
	g->protectionPassed = state.protectionFlags & 1;
	g->protectionFailed = state.protectionFlags & 2;
	g->protectionReply = state.protectionFlags & 4;
	g->protectionPosition = state.protectionPosition;
	g->protectionByte = state.protectionByte;
	g->protectionSequence = state.protectionSequence;
	g->buttons = state.buttons;
	g->openBus = state.openBus;
	g->uartTx = state.uartTx;
	g->uartRx = state.uartRx;
	g->uartTxBits = state.uartTxBits;
	g->uartRxBits = state.uartRxBits;
	g->uartTxLine = state.uartTxLine;
	g->masterClocks = state.masterClocks;
	g->frameClocks = state.frameClocks;
	g->irqClocks = state.irqClocks;
	g->ayAddress = state.ayAddress;
	g->cpu = state.cpu;
	g->ay = state.ay;
	g->samplePhase = state.samplePhase;
	g->lcdClock = state.lcdClock;
	g->lcdRowEvent = state.lcdRowEvent;
	memcpy(g->lcdAccum, state.lcdAccum, sizeof(g->lcdAccum));
	memcpy(g->frameBuffer, state.frameBuffer, sizeof(g->frameBuffer));
	memcpy(g->ayRegisters, state.ayRegisters, sizeof(g->ayRegisters));
	updateAyPeriods(g);
	return true;
}
