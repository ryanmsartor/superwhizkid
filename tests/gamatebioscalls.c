/* Copyright (c) 2026 Ry
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
#include "gamate/gamate.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { MAX_EVENTS = 1024 };

struct Event {
	const char* direction;
	const char* kind;
	uint16_t source, target;
	uint8_t opcode, bank, firstBank;
	uint64_t count;
	unsigned firstFrame;
};

static uint8_t* readFile(const char* path, size_t* size) {
	FILE* file = fopen(path, "rb");
	if (!file) return NULL;
	if (fseek(file, 0, SEEK_END) || (*size = (size_t) ftell(file), fseek(file, 0, SEEK_SET))) {
		fclose(file);
		return NULL;
	}
	uint8_t* data = malloc(*size ? *size : 1);
	if (!data || fread(data, 1, *size, file) != *size) {
		free(data);
		data = NULL;
	}
	fclose(file);
	return data;
}

static bool isCartridge(uint16_t address) {
	return address >= 0x6000 && address < 0xE000;
}

static bool isBios(uint16_t address) {
	return address >= 0xE000;
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

static uint8_t opcodeAt(const struct Gamate* g, uint16_t address) {
	if (isCartridge(address)) return g->rom[cartridgeOffset(g, address)];
	if (isBios(address)) return g->bios[address & 0x0FFF];
	return 0xFF;
}

static const char* cartridgeToBiosKind(uint8_t opcode, bool irq, bool nmi) {
	if (nmi) return "nmi_vector";
	if (irq) return "irq_vector";
	switch (opcode) {
	case 0x00: return "brk_vector";
	case 0x20: return "jsr";
	case 0x4C: return "jmp";
	case 0x6C: return "jmp_indirect";
	case 0x60: return "rts_to_bios";
	case 0x40: return "rti_to_bios";
	default: return "other_to_bios";
	}
}

static const char* biosToCartridgeKind(uint8_t opcode) {
	switch (opcode) {
	case 0x20: return "callback_jsr";
	case 0x4C: return "jmp_to_cartridge";
	case 0x6C: return "jmp_indirect_to_cartridge";
	case 0x60: return "rts_to_cartridge";
	case 0x40: return "interrupt_return";
	default: return "other_to_cartridge";
	}
}

static bool addEvent(struct Event* events, size_t* eventCount, const char* direction,
		const char* kind, uint16_t source, uint16_t target, uint8_t opcode,
		uint8_t bank, uint8_t firstBank, unsigned frame) {
	for (size_t i = 0; i < *eventCount; i++) {
		struct Event* event = &events[i];
		if (!strcmp(event->direction, direction) && !strcmp(event->kind, kind) &&
				event->source == source && event->target == target && event->opcode == opcode &&
				event->bank == bank && event->firstBank == firstBank) {
			event->count++;
			return true;
		}
	}
	if (*eventCount == MAX_EVENTS) return false;
	events[*eventCount] = (struct Event) {
		.direction = direction, .kind = kind, .source = source, .target = target,
		.opcode = opcode, .bank = bank, .firstBank = firstBank,
		.count = 1, .firstFrame = frame,
	};
	(*eventCount)++;
	return true;
}

// GAMATEBIOSCALLS_PRESS="frame:mask:frames,..." uses active-high diagnostic masks; the emulated
// controller remains active low. Frame numbers are relative to cartridge entry.
static uint8_t buttonsForFrame(unsigned frame) {
	uint8_t buttons = 0xFF;
	const char* press = getenv("GAMATEBIOSCALLS_PRESS");
	while (press && *press) {
		char* end;
		unsigned from = (unsigned) strtoul(press, &end, 0);
		if (*end++ != ':') break;
		unsigned mask = (unsigned) strtoul(end, &end, 0);
		if (*end++ != ':') break;
		unsigned count = (unsigned) strtoul(end, &end, 0);
		if (frame >= from && frame < from + count) buttons &= (uint8_t) ~(mask & 0xFF);
		press = *end == ',' ? end + 1 : NULL;
	}
	return buttons;
}

int main(int argc, char** argv) {
	if (argc < 3 || argc > 4) {
		fprintf(stderr, "usage: %s gamate_bios.bin cartridge.bin [frames-after-entry]\n", argv[0]);
		return 2;
	}
	unsigned frames = argc == 4 ? (unsigned) strtoul(argv[3], NULL, 0) : 1000;
	if (!frames) {
		fprintf(stderr, "frame count must be positive\n");
		return 2;
	}
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

	bool entered = false;
	for (uint64_t instruction = 0; instruction < 20000000; instruction++) {
		unsigned cycles = gamateCpuStep(&g);
		gamateRunClocks(&g, cycles * 2);
		if (g.protectionPassed && g.cpu.pc == 0x6020) {
			entered = true;
			break;
		}
	}
	if (!entered) {
		fprintf(stderr, "BIOS did not reach cartridge entry: PC=$%04X\n", g.cpu.pc);
		gamateFree(&g);
		return 1;
	}

	struct Event events[MAX_EVENTS];
	size_t eventCount = 0;
	bool overflow = false;
	uint64_t cartridgeInstructions = 0, biosInstructions = 0;
	for (unsigned frame = 1; frame <= frames; frame++) {
		g.buttons = buttonsForFrame(frame);
		g.frameDone = false;
		while (!g.frameDone) {
			uint16_t source = g.cpu.pc;
			uint8_t opcode = opcodeAt(&g, source);
			uint8_t bank = g.bank, firstBank = g.firstBank;
			uint64_t oldIrqs = g.diagnostics.irqsTaken;
			uint64_t oldNmis = g.diagnostics.nmisTaken;
			unsigned cycles = gamateCpuStep(&g);
			uint16_t target = g.cpu.pc;
			if (isCartridge(source)) cartridgeInstructions++;
			else if (isBios(source)) biosInstructions++;
			if (isCartridge(source) && isBios(target)) {
				const char* kind = cartridgeToBiosKind(opcode,
					g.diagnostics.irqsTaken != oldIrqs, g.diagnostics.nmisTaken != oldNmis);
				overflow |= !addEvent(events, &eventCount, "cartridge_to_bios", kind,
					source, target, opcode, bank, firstBank, frame);
			} else if (isBios(source) && isCartridge(target)) {
				overflow |= !addEvent(events, &eventCount, "bios_to_cartridge",
					biosToCartridgeKind(opcode), source, target, opcode, bank, firstBank, frame);
			}
			gamateRunClocks(&g, cycles * 2);
		}
	}
	if (g.diagnostics.unsupportedOpcodes) {
		fprintf(stderr, "unsupported opcode $%02X at $%04X (%llu total)\n",
			g.diagnostics.lastUnsupportedOpcode, g.diagnostics.lastUnsupportedPc,
			(unsigned long long) g.diagnostics.unsupportedOpcodes);
		gamateFree(&g);
		return 1;
	}

	printf("direction\tkind\tsource\ttarget\topcode\tbank\tfirst_bank\tcount\tfirst_frame\n");
	for (size_t i = 0; i < eventCount; i++) {
		const struct Event* event = &events[i];
		printf("%s\t%s\t%04X\t%04X\t%02X\t%02X\t%02X\t%llu\t%u\n",
			event->direction, event->kind, event->source, event->target, event->opcode,
			event->bank, event->firstBank, (unsigned long long) event->count, event->firstFrame);
	}
	printf("# frames=%u cartridge_instructions=%llu bios_instructions=%llu events=%zu "
		"irqs=%llu nmis=%llu overflow=%u\n", frames,
		(unsigned long long) cartridgeInstructions, (unsigned long long) biosInstructions, eventCount,
		(unsigned long long) g.diagnostics.irqsTaken, (unsigned long long) g.diagnostics.nmisTaken,
		overflow);
	gamateFree(&g);
	return overflow ? 1 : 0;
}
