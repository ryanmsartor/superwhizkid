/* Copyright (c) 2026 Ry
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
#include "gamate/gamate.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint32_t pictureHash(const struct Gamate* g) {
	uint32_t hash = 2166136261U;
	for (size_t i = 0; i < GAMATE_WIDTH * GAMATE_HEIGHT; i++) {
		hash ^= g->frameBuffer[i];
		hash *= 16777619U;
	}
	return hash;
}

static uint32_t byteHash(const void* data, size_t size) {
	uint32_t hash = 2166136261U;
	const uint8_t* bytes = data;
	while (size--) {
		hash ^= *bytes++;
		hash *= 16777619U;
	}
	return hash;
}

static bool pictureHasInk(const struct Gamate* g) {
	for (size_t i = 0; i < GAMATE_WIDTH * GAMATE_HEIGHT; i++) {
		if (g->frameBuffer[i]) return true;
	}
	return false;
}

static uint32_t audioHash(const int16_t* audio, size_t frames) {
	uint32_t hash = 2166136261U;
	for (size_t i = 0; i < frames * 2; i++) {
		hash ^= (uint16_t) audio[i];
		hash *= 16777619U;
	}
	return hash;
}

static uint8_t* readFile(const char* path, size_t* size) {
	FILE* file = fopen(path, "rb");
	if (!file) return NULL;
	if (fseek(file, 0, SEEK_END) || (*size = (size_t) ftell(file), fseek(file, 0, SEEK_SET))) {
		fclose(file);
		return NULL;
	}
	uint8_t* data = malloc(*size);
	if (!data || fread(data, 1, *size, file) != *size) {
		free(data);
		data = NULL;
	}
	fclose(file);
	return data;
}

// GAMATEBOOT_PRESS="frame:mask:frames,..." presses active-high mask bits in the diagnostic input
// description (the emulated controller bus remains active low). Frame numbers are post-entry.
static uint8_t buttonsForFrame(unsigned frame) {
	uint8_t buttons = 0xFF;
	const char* press = getenv("GAMATEBOOT_PRESS");
	while (press && *press) {
		char* end;
		unsigned from = (unsigned) strtoul(press, &end, 0);
		if (*end++ != ':') break;
		unsigned mask = (unsigned) strtoul(end, &end, 0);
		if (*end++ != ':') break;
		unsigned count = (unsigned) strtoul(end, &end, 0);
		if (frame >= from && frame < from + count) buttons &= (uint8_t) ~mask;
		press = *end == ',' ? end + 1 : NULL;
	}
	return buttons;
}

static bool perturbEntryVram(struct Gamate* g) {
	const char* pattern = getenv("GAMATEBOOT_ENTRY_VRAM");
	if (!pattern || !*pattern || !strcmp(pattern, "authentic")) return true;
	if (!strcmp(pattern, "zero")) {
		memset(g->vram, 0x00, sizeof(g->vram));
	} else if (!strcmp(pattern, "ones")) {
		memset(g->vram, 0xFF, sizeof(g->vram));
	} else if (!strcmp(pattern, "planes")) {
		for (size_t i = 0; i < sizeof(g->vram); i++) g->vram[i] = i & 1 ? 0x55 : 0xAA;
	} else if (!strcmp(pattern, "address")) {
		for (size_t i = 0; i < sizeof(g->vram); i++) {
			g->vram[i] = (uint8_t) (i ^ i >> 5 ^ i >> 9);
		}
	} else {
		fprintf(stderr, "unknown GAMATEBOOT_ENTRY_VRAM pattern: %s\n", pattern);
		return false;
	}
	printf("entry VRAM replaced with diagnostic pattern: %s\n", pattern);
	return true;
}

static bool perturbEntryRam(struct Gamate* g) {
	const char* pattern = getenv("GAMATEBOOT_ENTRY_RAM");
	if (!pattern || !*pattern || !strcmp(pattern, "authentic")) return true;
	if (!strncmp(pattern, "zero-page", 9) && pattern[9] >= '0' && pattern[9] <= '3' && !pattern[10]) {
		memset(g->ram + (pattern[9] - '0') * 0x100, 0x00, 0x100);
		printf("entry RAM replaced with diagnostic pattern: %s\n", pattern);
		return true;
	}
	if (!strncmp(pattern, "zero-block-", 11) && strlen(pattern) == 13) {
		char* end;
		unsigned block = (unsigned) strtoul(pattern + 11, &end, 16);
		if (!*end && block < 16) {
			memset(g->ram + block * 0x40, 0x00, 0x40);
			printf("entry RAM replaced with diagnostic pattern: %s\n", pattern);
			return true;
		}
	}
	uint8_t callback = g->ram[0x00C];
	uint8_t checksum[4] = { g->ram[0x0E0], g->ram[0x0E1], g->ram[0x0E5], g->ram[0x0E6] };
	uint8_t stack[0x100];
	memcpy(stack, g->ram + 0x100, sizeof(stack));
	if (!strcmp(pattern, "zero") || !strcmp(pattern, "known-zero") ||
			!strcmp(pattern, "known-stack-zero")) {
		memset(g->ram, 0x00, sizeof(g->ram));
	} else if (!strcmp(pattern, "ones") || !strcmp(pattern, "known-ones")) {
		memset(g->ram, 0xFF, sizeof(g->ram));
	} else if (!strcmp(pattern, "address")) {
		for (size_t i = 0; i < sizeof(g->ram); i++) g->ram[i] = (uint8_t) (i ^ i >> 3);
	} else {
		fprintf(stderr, "unknown GAMATEBOOT_ENTRY_RAM pattern: %s\n", pattern);
		return false;
	}
	if (!strncmp(pattern, "known-", 6)) {
		g->ram[0x00C] = callback;
		g->ram[0x0E0] = checksum[0];
		g->ram[0x0E1] = checksum[1];
		g->ram[0x0E5] = checksum[2];
		g->ram[0x0E6] = checksum[3];
	}
	if (!strcmp(pattern, "known-stack-zero")) memcpy(g->ram + 0x100, stack, sizeof(stack));
	printf("entry RAM replaced with diagnostic pattern: %s\n", pattern);
	return true;
}

#ifdef GAMATE_ACCESS_DIAGNOSTICS
static void printRamAddressMap(const char* label, const uint8_t* bits) {
	printf("RAM read-before-write addresses (%s):", label);
	for (unsigned address = 0; address < 0x400; address++) {
		if (bits[address >> 3] & ((uint8_t) 1 << (address & 7))) printf(" %03X", address);
	}
	putchar('\n');
}
#endif

int main(int argc, char** argv) {
	if (argc < 3 || argc > 4) {
		fprintf(stderr, "usage: %s gamate_bios.bin cartridge.bin [frames-after-entry]\n", argv[0]);
		return 2;
	}
	unsigned framesWanted = argc == 4 ? (unsigned) strtoul(argv[3], NULL, 0) : 1000;
	size_t biosSize, romSize;
	uint8_t* bios = readFile(argv[1], &biosSize);
	uint8_t* rom = readFile(argv[2], &romSize);
	if (!bios || !rom) {
		fprintf(stderr, "could not read BIOS or cartridge\n");
		free(bios); free(rom);
		return 2;
	}
	struct Gamate g;
	gamateInit(&g);
	if (!gamateLoad(&g, rom, romSize, bios, biosSize)) {
		fprintf(stderr, "invalid BIOS or cartridge size\n");
		free(bios); free(rom);
		return 2;
	}
	free(bios); free(rom);
	printf("reset PC=$%04X, cartridge=%zu bytes, mapper=%d\n", g.cpu.pc, g.romSize, g.mapper);

	bool reportedSequence = false, reportedUnlock = false;
	uint64_t instructions;
	bool entered = false;
	for (instructions = 0; instructions < 20000000; instructions++) {
		uint16_t pc = g.cpu.pc;
		unsigned cycles = gamateCpuStep(&g);
		gamateRunClocks(&g, cycles * 2);
		if (!reportedSequence && g.protectionSequence == 15) {
			reportedSequence = true;
			printf("protection key accepted after %llu instructions at PC=$%04X\n",
				(unsigned long long) instructions + 1, pc);
		}
		if (!reportedUnlock && g.protectionPassed) {
			reportedUnlock = true;
			printf("cartridge unlocked after %llu instructions at PC=$%04X\n",
				(unsigned long long) instructions + 1, pc);
		}
		if (reportedUnlock && g.cpu.pc == 0x6020) {
			printf("BIOS reached cartridge entry $6020 after %llu instructions (%llu CPU cycles)\n",
				(unsigned long long) instructions + 1, (unsigned long long) g.cpu.cycles);
			entered = true;
			break;
		}
	}
	if (!entered) {
		fprintf(stderr, "boot did not reach $6020 after %llu instructions: PC=$%04X, protection=%u/%u%s\n",
			(unsigned long long) instructions, g.cpu.pc, g.protectionSequence, g.protectionPosition,
			g.protectionPassed ? " unlocked" : " locked");
		gamateFree(&g);
		return 1;
	}
	printf("entry state: master=%llu frame=%u irq=%u lcd=%u/%u RAM=%08X VRAM=%08X "
		"scroll=%u/%u lcdcon=%u%u%u%u%u\n",
		(unsigned long long) g.masterClocks, g.frameClocks, g.irqClocks, g.lcdRowEvent, g.lcdClock,
		byteHash(g.ram, sizeof(g.ram)), byteHash(g.vram, sizeof(g.vram)), g.scrollX, g.scrollY,
		g.incrementRow, g.window, g.swapPlanes, g.displayBlank, g.displayStopped);
	if (!perturbEntryVram(&g)) {
		gamateFree(&g);
		return 2;
	}
	if (!perturbEntryRam(&g)) {
		gamateFree(&g);
		return 2;
	}
	if (getenv("GAMATEBOOT_VRAM_COVERAGE") || getenv("GAMATEBOOT_ENTRY_VRAM")) {
		gamateResetVramDiagnostics(&g);
	}
	if (getenv("GAMATEBOOT_RAM_COVERAGE") || getenv("GAMATEBOOT_ENTRY_RAM")) {
		gamateResetRamDiagnostics(&g);
	}
	// Diagnostic experiment only: remove the oscillator/display phase inherited from the BIOS while
	// preserving CPU, RAM, VRAM, registers, and cartridge state. Normal emulation never does this.
	if (getenv("GAMATEBOOT_NORMALIZE_ENTRY_PHASE")) {
		g.masterClocks = g.frameClocks = g.irqClocks = 0;
		g.lcdClock = g.lcdRowEvent = 0;
		g.samplePhase = 0;
		g.irq = false;
		memset(&g.ay, 0, sizeof(g.ay));
		g.ay.noiseLfsr = 0x1FFFF;
		g.ay.noiseLevel = true;
		g.ay.envelopeStep = 15;
		memset(g.lcdAccum, 0, sizeof(g.lcdAccum));
		memset(g.frameBuffer, 0, sizeof(g.frameBuffer));
		puts("entry phase normalized for diagnostic comparison");
	}

	unsigned nonblank = 0, audible = 0;
	unsigned firstAudible = 0, firstControllerRead = 0, firstPressedRead = 0;
	unsigned peak = 0;
	int16_t audio[2048 * 2];
	gamateSetAudio(&g, audio, 2048, 48000);
	uint64_t longestSamePc = 0, samePc = 0;
	uint16_t previousPc = 0xFFFF;
	for (unsigned frame = 1; frame <= framesWanted; frame++) {
		g.buttons = buttonsForFrame(frame);
		uint64_t controllerReadsBefore = g.diagnostics.controllerReads;
		uint64_t pressedReadsBefore = g.diagnostics.pressedControllerReads;
		g.frameDone = false;
		g.audioCount = 0;
		while (!g.frameDone) {
			uint16_t pc = g.cpu.pc;
			unsigned cycles = gamateCpuStep(&g);
			gamateRunClocks(&g, cycles * 2);
			if (g.cpu.pc == pc && pc == previousPc) {
				if (++samePc > longestSamePc) longestSamePc = samePc;
			} else {
				samePc = 0;
			}
			previousPc = pc;
		}
		if (pictureHasInk(&g)) nonblank++;
		bool hasAudio = false;
		size_t audioFrames = g.audioCount < g.audioCapacity ? g.audioCount : g.audioCapacity;
		for (size_t i = 0; i < audioFrames * 2; i++) {
			int value = audio[i] < 0 ? -(int) audio[i] : audio[i];
			if (value) hasAudio = true;
			if ((unsigned) value > peak) peak = value;
		}
		if (hasAudio) {
			audible++;
			if (!firstAudible) firstAudible = frame;
		}
		if (!firstControllerRead && g.diagnostics.controllerReads != controllerReadsBefore) firstControllerRead = frame;
		if (!firstPressedRead && g.diagnostics.pressedControllerReads != pressedReadsBefore) firstPressedRead = frame;
		if (frame == 1 || frame == 60 || frame == framesWanted) {
			printf("frame %u: video=%08X audio=%08X/%zu PC=$%04X bank=%u/%u\n", frame,
				pictureHash(&g), audioHash(audio, audioFrames), audioFrames, g.cpu.pc, g.firstBank, g.bank);
		}
		if (g.diagnostics.unsupportedOpcodes) break;
	}
	printf("post-entry: mapper=%d, nonblank=%u/%u, audible=%u/%u peak=%u, IRQs=%llu, AY writes=%llu, "
		"LCD writes=%llu, VRAM writes=%llu, UART clocks=%llu, bank changes=%llu, longest self-loop=%llu\n",
		g.mapper, nonblank, framesWanted, audible, framesWanted, peak,
		(unsigned long long) g.diagnostics.irqsTaken, (unsigned long long) g.diagnostics.ayWrites,
		(unsigned long long) g.diagnostics.lcdWrites,
		(unsigned long long) g.diagnostics.vramWrites, (unsigned long long) g.diagnostics.uartClocks,
		(unsigned long long) g.diagnostics.bankWrites,
		(unsigned long long) longestSamePc);
	printf("AY registers:");
	for (unsigned i = 0; i < 16; i++) printf(" %02X", g.ayRegisters[i]);
	printf("\nAY writes/register:");
	for (unsigned i = 0; i < 16; i++) printf(" %llu", (unsigned long long) g.diagnostics.ayRegisterWrites[i]);
	printf("\nAY active samples: A=%llu B=%llu C=%llu\n",
		(unsigned long long) g.diagnostics.ayActiveSamples[0],
		(unsigned long long) g.diagnostics.ayActiveSamples[1],
		(unsigned long long) g.diagnostics.ayActiveSamples[2]);
	printf("controller reads: %llu (%llu while pressed)\n",
		(unsigned long long) g.diagnostics.controllerReads,
		(unsigned long long) g.diagnostics.pressedControllerReads);
	printf("first events: audio=%u controller-read=%u pressed-read=%u\n",
		firstAudible, firstControllerRead, firstPressedRead);
	printf("final state: PC=%04X RAM=%08X VRAM=%08X bank=%u/%u scroll=%u/%u "
		"vram-position=%04X/%u\n", g.cpu.pc, byteHash(g.ram, sizeof(g.ram)),
		byteHash(g.vram, sizeof(g.vram)), g.firstBank, g.bank, g.scrollX, g.scrollY,
		g.vramAddress, g.vramPlane);
	printf("VRAM coverage: writes=%llu unique-written=%u cpu-reads=%llu "
		"read-before-write=%llu/%u display-reads=%llu display-before-write=%llu/%u\n",
		(unsigned long long) g.diagnostics.vramTrackedWrites, g.diagnostics.vramUniqueWritten,
		(unsigned long long) g.diagnostics.vramReads,
		(unsigned long long) g.diagnostics.vramReadBeforeWrite,
		g.diagnostics.vramUniqueReadBeforeWrite,
		(unsigned long long) g.diagnostics.vramDisplayReads,
		(unsigned long long) g.diagnostics.vramDisplayReadBeforeWrite,
		g.diagnostics.vramUniqueDisplayReadBeforeWrite);
	printf("RAM coverage: writes=%llu/%llu/%llu unique-written=%u "
		"reads=%llu/%llu/%llu read-before-write=%llu/%u,%llu/%u,%llu/%u\n",
		(unsigned long long) g.diagnostics.ramTrackedWrites[0],
		(unsigned long long) g.diagnostics.ramTrackedWrites[1],
		(unsigned long long) g.diagnostics.ramTrackedWrites[2], g.diagnostics.ramUniqueWritten,
		(unsigned long long) g.diagnostics.ramReads[0],
		(unsigned long long) g.diagnostics.ramReads[1],
		(unsigned long long) g.diagnostics.ramReads[2],
		(unsigned long long) g.diagnostics.ramReadBeforeWrite[0],
		g.diagnostics.ramUniqueReadBeforeWrite[0],
		(unsigned long long) g.diagnostics.ramReadBeforeWrite[1],
		g.diagnostics.ramUniqueReadBeforeWrite[1],
		(unsigned long long) g.diagnostics.ramReadBeforeWrite[2],
		g.diagnostics.ramUniqueReadBeforeWrite[2]);
	printRamAddressMap("cartridge", g.diagnostics.ramReadBeforeWriteMap[0]);
	printRamAddressMap("BIOS", g.diagnostics.ramReadBeforeWriteMap[1]);
	printRamAddressMap("other", g.diagnostics.ramReadBeforeWriteMap[2]);
	if (g.diagnostics.unsupportedOpcodes) {
		fprintf(stderr, "unsupported opcode $%02X at $%04X (%llu total)\n",
			g.diagnostics.lastUnsupportedOpcode, g.diagnostics.lastUnsupportedPc,
			(unsigned long long) g.diagnostics.unsupportedOpcodes);
		gamateFree(&g);
		return 1;
	}
	gamateFree(&g);
	return nonblank ? 0 : 1;
}
