/* Copyright (c) 2026 Ry
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
#include "gamate/gamate.h"
#include "gamatebiosinput.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { MAX_EVENTS = 16384, MAX_CALL_DEPTH = 32 };

struct Event {
	const char* direction;
	const char* kind;
	uint16_t source, target;
	uint8_t opcode, bank, firstBank;
	uint64_t count;
	unsigned firstFrame;
};

struct CallContext {
	uint16_t source, target, expectedReturn;
	uint16_t vramAddress;
	uint8_t opcode, bank, firstBank;
	uint8_t a, x, y, s, p;
	uint8_t waitCounter;
	uint32_t irqClocks;
	bool irqPending;
	uint8_t scrollX, scrollY, lcdFlags, ayAddress;
	uint8_t ram[0x400], vram[0x4000], ayRegisters[16];
	uint64_t startClocks, startIrqs, startNmis;
	uint64_t startAyWrites, startLcdWrites, startVramWrites;
	uint64_t startBankWrites, startUartClocks;
	unsigned frame;
};

struct ChangeSummary {
	unsigned count;
	uint16_t first, last;
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

static bool isServiceSlot(uint16_t address) {
	return address >= 0xF000 && address <= 0xF033 && !((address - 0xF000) % 3);
}

static uint16_t stackedReturn(const struct Gamate* g, uint8_t s) {
	uint8_t low = g->ram[(uint8_t) (s + 1) | 0x100];
	uint8_t high = g->ram[(uint8_t) (s + 2) | 0x100];
	return (uint16_t) (((uint16_t) high << 8 | low) + 1);
}

static struct ChangeSummary summarizeChanges(const uint8_t* before, const uint8_t* after,
		size_t size) {
	struct ChangeSummary summary = { 0, 0, 0 };
	for (size_t i = 0; i < size; i++) {
		if (before[i] == after[i]) continue;
		if (!summary.count) summary.first = (uint16_t) i;
		summary.last = (uint16_t) i;
		summary.count++;
	}
	return summary;
}

static uint16_t changedMask(const uint8_t* before, const uint8_t* after, size_t size) {
	uint16_t mask = 0;
	for (size_t i = 0; i < size; i++) if (before[i] != after[i]) mask |= (uint16_t) 1 << i;
	return mask;
}

static uint8_t lcdFlags(const struct Gamate* g) {
	return (g->vramPlane ? 0x01 : 0) | (g->incrementRow ? 0x02 : 0) |
		(g->window ? 0x04 : 0) | (g->swapPlanes ? 0x08 : 0) |
		(g->displayBlank ? 0x10 : 0) | (g->displayStopped ? 0x20 : 0);
}

static void beginCall(struct CallContext* call, const struct Gamate* g, uint16_t source,
		uint16_t target, uint16_t expectedReturn, uint8_t opcode, uint8_t bank,
		uint8_t firstBank, uint8_t a, uint8_t x, uint8_t y, uint8_t s, uint8_t p,
		uint64_t startClocks, unsigned frame) {
	*call = (struct CallContext) {
		.source = source, .target = target, .expectedReturn = expectedReturn,
		.opcode = opcode, .bank = bank, .firstBank = firstBank,
		.a = a, .x = x, .y = y, .s = s, .p = p,
		.waitCounter = g->ram[0x0B], .irqClocks = g->irqClocks, .irqPending = g->irq,
		.vramAddress = g->vramAddress, .scrollX = g->scrollX, .scrollY = g->scrollY,
		.lcdFlags = lcdFlags(g), .ayAddress = g->ayAddress,
		.startClocks = startClocks,
		.startIrqs = g->diagnostics.irqsTaken, .startNmis = g->diagnostics.nmisTaken,
		.startAyWrites = g->diagnostics.ayWrites, .startLcdWrites = g->diagnostics.lcdWrites,
		.startVramWrites = g->diagnostics.vramWrites,
		.startBankWrites = g->diagnostics.bankWrites,
		.startUartClocks = g->diagnostics.uartClocks, .frame = frame,
	};
	memcpy(call->ram, g->ram, sizeof(call->ram));
	memcpy(call->vram, g->vram, sizeof(call->vram));
	memcpy(call->ayRegisters, g->ayRegisters, sizeof(call->ayRegisters));
}

static void finishCall(FILE* file, const struct CallContext* call, const struct Gamate* g,
		uint64_t entryClocks, unsigned returnFrame) {
	struct ChangeSummary ram = summarizeChanges(call->ram, g->ram, sizeof(call->ram));
	struct ChangeSummary vram = summarizeChanges(call->vram, g->vram, sizeof(call->vram));
	uint16_t ayMask = changedMask(call->ayRegisters, g->ayRegisters,
		sizeof(call->ayRegisters));
	fprintf(file,
		"%04X\t%04X\t%04X\t%02X\t%02X\t%02X\t%u\t%u\t"
		"%llu\t%llu\t%02X\t%u\t%u\t"
		"%02X\t%02X\t%02X\t%02X\t%02X\t%02X\t%02X\t%02X\t%02X\t%02X\t"
		"%04X\t%02X\t%02X\t%02X\t%02X\t%02X\t%02X\t"
		"%04X\t%02X\t%02X\t%02X\t%02X\t%02X\t%02X\t"
		"%llu\t%u\t%04X\t%04X\t%u\t%04X\t%04X\t%04X\t"
		"%llu\t%llu\t%llu\t%llu\t%llu\t%llu\t%llu\n",
		call->source, call->target, call->expectedReturn, call->opcode,
		call->bank, call->firstBank, call->frame, returnFrame,
		(unsigned long long) call->startClocks,
		(unsigned long long) (g->masterClocks - entryClocks),
		call->waitCounter, call->irqClocks, call->irqPending,
		call->a, call->x, call->y, call->s, call->p,
		g->cpu.a, g->cpu.x, g->cpu.y, g->cpu.s, g->cpu.p,
		call->vramAddress, call->scrollX, call->scrollY, call->lcdFlags,
		call->ayAddress, call->bank, call->firstBank,
		g->vramAddress, g->scrollX, g->scrollY, lcdFlags(g),
		g->ayAddress, g->bank, g->firstBank,
		(unsigned long long) (g->masterClocks - entryClocks - call->startClocks),
		ram.count, ram.first, ram.last, vram.count, vram.first, vram.last, ayMask,
		(unsigned long long) (g->diagnostics.ayWrites - call->startAyWrites),
		(unsigned long long) (g->diagnostics.lcdWrites - call->startLcdWrites),
		(unsigned long long) (g->diagnostics.vramWrites - call->startVramWrites),
		(unsigned long long) (g->diagnostics.bankWrites - call->startBankWrites),
		(unsigned long long) (g->diagnostics.uartClocks - call->startUartClocks),
		(unsigned long long) (g->diagnostics.irqsTaken - call->startIrqs),
		(unsigned long long) (g->diagnostics.nmisTaken - call->startNmis));
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

int main(int argc, char** argv) {
	if (argc < 3 || argc > 5) {
		fprintf(stderr, "usage: %s gamate_bios.bin cartridge.bin [frames-after-entry] "
			"[completed-call-report]\n", argv[0]);
		return 2;
	}
	unsigned frames = argc >= 4 ? (unsigned) strtoul(argv[3], NULL, 0) : 1000;
	if (!frames) {
		fprintf(stderr, "frame count must be positive\n");
		return 2;
	}
	FILE* callFile = NULL;
	if (argc == 5) {
		callFile = fopen(argv[4], "w");
		if (!callFile) {
			fprintf(stderr, "could not create completed-call report\n");
			return 2;
		}
		fprintf(callFile, "source\ttarget\texpected_return\topcode\tbank\tfirst_bank\t"
			"entry_frame\treturn_frame\tentry_clock\treturn_clock\t"
			"in_wait_counter\tin_irq_clock\tin_irq_pending\t"
			"in_a\tin_x\tin_y\tin_s\tin_p\t"
			"out_a\tout_x\tout_y\tout_s\tout_p\t"
			"in_vram_address\tin_scroll_x\tin_scroll_y\tin_lcd_flags\tin_ay_address\t"
			"in_bank\tin_first_bank\tout_vram_address\tout_scroll_x\tout_scroll_y\t"
			"out_lcd_flags\tout_ay_address\tout_bank\tout_first_bank\tclocks\t"
			"ram_changed\tram_first\tram_last\tvram_changed\tvram_first\tvram_last\t"
			"ay_changed_mask\tay_writes\tlcd_writes\tvram_writes\tbank_writes\t"
			"uart_clocks\tirqs\tnmis\n");
	}
	FILE* bootIrqFile = NULL;
	const char* bootIrqPath = getenv("GAMATEBIOSCALLS_BOOT_IRQ_REPORT");
	if (bootIrqPath && *bootIrqPath) {
		bootIrqFile = fopen(bootIrqPath, "w");
		if (!bootIrqFile) {
			fprintf(stderr, "could not create boot IRQ report\n");
			if (callFile) fclose(callFile);
			return 2;
		}
		fprintf(bootIrqFile, "index\tmaster_clock\tinterrupted_pc\n");
	}
	size_t biosSize, romSize;
	uint8_t* bios = readFile(argv[1], &biosSize);
	uint8_t* rom = readFile(argv[2], &romSize);
	if (!bios || !rom) {
		fprintf(stderr, "could not read BIOS or cartridge\n");
		free(bios); free(rom);
		if (callFile) fclose(callFile);
		if (bootIrqFile) fclose(bootIrqFile);
		return 2;
	}
	struct Gamate g;
	gamateInit(&g);
	if (!gamateLoad(&g, rom, romSize, bios, biosSize)) {
		fprintf(stderr, "invalid BIOS or cartridge size\n");
		free(bios); free(rom);
		if (callFile) fclose(callFile);
		if (bootIrqFile) fclose(bootIrqFile);
		return 2;
	}
	free(bios); free(rom);

	bool entered = false;
	uint64_t bootIrqClocks[2] = { 0 };
	uint16_t bootIrqPcs[2] = { 0 };
	for (uint64_t instruction = 0; instruction < 20000000; instruction++) {
		uint64_t startClocks = g.masterClocks;
		uint64_t oldIrqs = g.diagnostics.irqsTaken;
		uint16_t interruptedPc = g.cpu.pc;
		unsigned cycles = gamateCpuStep(&g);
		if (g.diagnostics.irqsTaken != oldIrqs) {
			bootIrqClocks[0] = bootIrqClocks[1];
			bootIrqClocks[1] = startClocks;
			bootIrqPcs[0] = bootIrqPcs[1];
			bootIrqPcs[1] = interruptedPc;
			if (bootIrqFile) fprintf(bootIrqFile, "%llu\t%llu\t%04X\n",
				(unsigned long long) g.diagnostics.irqsTaken,
				(unsigned long long) startClocks, interruptedPc);
		}
		gamateRunClocks(&g, cycles * 2);
		if (g.protectionPassed && g.cpu.pc == 0x6020) {
			entered = true;
			break;
		}
	}
	if (!entered) {
		fprintf(stderr, "BIOS did not reach cartridge entry: PC=$%04X\n", g.cpu.pc);
		gamateFree(&g);
		if (callFile) fclose(callFile);
		if (bootIrqFile) fclose(bootIrqFile);
		return 1;
	}
	uint64_t entryClocks = g.masterClocks;
	uint32_t entryIrqClocks = g.irqClocks;

	struct Event events[MAX_EVENTS];
	size_t eventCount = 0;
	bool overflow = false;
	uint64_t cartridgeInstructions = 0, biosInstructions = 0;
	struct CallContext calls[MAX_CALL_DEPTH];
	size_t callDepth = 0;
	uint64_t completedCalls = 0, unpairedReturns = 0, callStackOverflow = 0;
	for (unsigned frame = 1; frame <= frames; frame++) {
		g.buttons = gamateBiosButtonsForFrame(frame, getenv("GAMATEBIOSCALLS_PRESS"));
		g.frameDone = false;
		while (!g.frameDone) {
			uint16_t source = g.cpu.pc;
			uint8_t opcode = opcodeAt(&g, source);
			uint8_t bank = g.bank, firstBank = g.firstBank;
			uint8_t inA = g.cpu.a, inX = g.cpu.x, inY = g.cpu.y, inS = g.cpu.s, inP = g.cpu.p;
			uint64_t startClocks = g.masterClocks;
			uint64_t oldIrqs = g.diagnostics.irqsTaken;
			uint64_t oldNmis = g.diagnostics.nmisTaken;
			unsigned cycles = gamateCpuStep(&g);
			uint16_t target = g.cpu.pc;
			if (isCartridge(source)) cartridgeInstructions++;
			else if (isBios(source)) biosInstructions++;
			bool directServiceCall = false;
			uint16_t expectedReturn = 0;
			bool completedCall = false;
			if (isCartridge(source) && isBios(target)) {
				const char* kind = cartridgeToBiosKind(opcode,
					g.diagnostics.irqsTaken != oldIrqs, g.diagnostics.nmisTaken != oldNmis);
				overflow |= !addEvent(events, &eventCount, "cartridge_to_bios", kind,
					source, target, opcode, bank, firstBank, frame);
				directServiceCall = isServiceSlot(target) && (opcode == 0x20 || opcode == 0x4C);
				if (directServiceCall) {
					expectedReturn = opcode == 0x20 ? source + 3 : stackedReturn(&g, inS);
				}
			} else if (isBios(source) && isCartridge(target)) {
				overflow |= !addEvent(events, &eventCount, "bios_to_cartridge",
					biosToCartridgeKind(opcode), source, target, opcode, bank, firstBank, frame);
				if (callFile && opcode == 0x60) {
					if (callDepth && calls[callDepth - 1].expectedReturn == target) completedCall = true;
					else unpairedReturns++;
				}
			}
			gamateRunClocks(&g, cycles * 2);
			if (completedCall) {
				finishCall(callFile, &calls[callDepth - 1], &g, entryClocks, frame);
				callDepth--;
				completedCalls++;
			}
			if (callFile && directServiceCall) {
				if (callDepth == MAX_CALL_DEPTH) callStackOverflow++;
				else beginCall(&calls[callDepth++], &g, source, target, expectedReturn, opcode,
					bank, firstBank, inA, inX, inY, inS, inP,
					startClocks + cycles * 2 - entryClocks, frame);
			}
		}
	}
	if (g.diagnostics.unsupportedOpcodes) {
		fprintf(stderr, "unsupported opcode $%02X at $%04X (%llu total)\n",
			g.diagnostics.lastUnsupportedOpcode, g.diagnostics.lastUnsupportedPc,
			(unsigned long long) g.diagnostics.unsupportedOpcodes);
		gamateFree(&g);
		if (callFile) fclose(callFile);
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
		"irqs=%llu nmis=%llu overflow=%u completed_calls=%llu open_calls=%zu "
		"unpaired_returns=%llu call_stack_overflow=%llu boot_entry_master=%llu "
		"boot_entry_irq_clock=%u boot_previous_irq_age=%llu boot_previous_irq_pc=%04X "
		"boot_last_irq_age=%llu boot_last_irq_pc=%04X\n", frames,
		(unsigned long long) cartridgeInstructions, (unsigned long long) biosInstructions, eventCount,
		(unsigned long long) g.diagnostics.irqsTaken, (unsigned long long) g.diagnostics.nmisTaken,
		overflow, (unsigned long long) completedCalls, callDepth,
		(unsigned long long) unpairedReturns, (unsigned long long) callStackOverflow,
		(unsigned long long) entryClocks, entryIrqClocks,
		(unsigned long long) (entryClocks - bootIrqClocks[0]), bootIrqPcs[0],
		(unsigned long long) (entryClocks - bootIrqClocks[1]), bootIrqPcs[1]);
	gamateFree(&g);
	if (callFile && fclose(callFile)) return 1;
	if (bootIrqFile && fclose(bootIrqFile)) return 1;
	return overflow || callStackOverflow || unpairedReturns ? 1 : 0;
}
