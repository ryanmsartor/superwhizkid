/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "gamate/gamate.h"
#include "gamatebiosinput.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static struct Gamate machine, baseline;

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
	uint8_t* data = malloc(*size ? *size : 1);
	if (!data || fread(data, 1, *size, file) != *size) {
		free(data);
		data = NULL;
	}
	fclose(file);
	return data;
}

static bool bitSet(const uint8_t* map, unsigned address) {
	return (map[address >> 3] & (1u << (address & 7))) != 0;
}

static bool checkCall(uint32_t input, uint8_t p, uint8_t returnS, bool thorough) {
	struct Gamate* g = &machine;
	*g = baseline;
	g->ram[0x100 | returnS] = 0x60;
	g->ram[0x100 | (uint8_t) (returnS - 1)] = 0x1F;
	g->cpu.pc = 0xF033;
	g->cpu.s = (uint8_t) (returnS - 2);
	g->cpu.a = (uint8_t) (input >> 16);
	g->cpu.x = (uint8_t) (input >> 8);
	g->cpu.y = (uint8_t) input;
	g->cpu.p = p;
	g->cpu.irqMask = (p & 4) != 0;
	g->irqClocks = 0;
	g->irq = g->nmi = false;
	if (thorough) {
		gamateResetRamDiagnostics(g);
		gamateResetRomDiagnostics(g);
	}
	uint64_t start = g->masterClocks;
	bool returned = false;
	for (unsigned instruction = 0; instruction < 1000; instruction++) {
		unsigned cycles = gamateCpuStep(g);
		gamateRunClocks(g, cycles * 2);
		if (g->cpu.pc == 0x6020 && g->cpu.s == returnS) {
			returned = true;
			break;
		}
	}

	uint8_t digits[6];
	for (unsigned i = 0; i < 6; i++) digits[i] = (uint8_t) ((input >> (20 - i * 4)) & 15);
	bool blankSuffix = (uint8_t) input == 0xFF;
	unsigned limit = blankSuffix ? 4 : 5;
	unsigned first = 0;
	while (first < limit && !digits[first]) first++;
	uint8_t expected[6];
	for (unsigned i = 0; i < 6; i++) {
		expected[i] = i < first || (blankSuffix && i >= 4) ?
			0x20 : (uint8_t) (0x30 + digits[i]);
	}
	unsigned clocks = blankSuffix ? 190 + 44 * first :
		first == 5 ? 422 : 222 + 44 * first;
	uint8_t expectedP = (uint8_t) (0x20 | (p & 0x5C) |
		(blankSuffix && first == 4 ? 0 : 1));
	bool ok = returned && g->diagnostics.unsupportedOpcodes == baseline.diagnostics.unsupportedOpcodes &&
		g->cpu.a == 6 && g->cpu.x == (uint8_t) (input >> 8) &&
		g->cpu.y == first && g->cpu.p == expectedP && g->ram[0xA1] == 6 &&
		!memcmp(&g->ram[0xA2], expected, sizeof(expected)) && g->masterClocks - start == clocks &&
		g->vramAddress == baseline.vramAddress && g->vramPlane == baseline.vramPlane &&
		g->incrementRow == baseline.incrementRow && g->window == baseline.window &&
		g->swapPlanes == baseline.swapPlanes && g->displayBlank == baseline.displayBlank &&
		g->displayStopped == baseline.displayStopped && g->scrollX == baseline.scrollX &&
		g->scrollY == baseline.scrollY && g->ayAddress == baseline.ayAddress &&
		g->bank == baseline.bank && g->firstBank == baseline.firstBank &&
		g->diagnostics.ayWrites == baseline.diagnostics.ayWrites &&
		g->diagnostics.lcdWrites == baseline.diagnostics.lcdWrites &&
		g->diagnostics.vramWrites == baseline.diagnostics.vramWrites &&
		g->diagnostics.bankWrites == baseline.diagnostics.bankWrites &&
		g->diagnostics.uartClocks == baseline.diagnostics.uartClocks &&
		g->diagnostics.irqsTaken == baseline.diagnostics.irqsTaken &&
		g->diagnostics.nmisTaken == baseline.diagnostics.nmisTaken;
	if (thorough) {
		for (unsigned address = 0; address < sizeof(g->ram); address++) {
			bool stack = address >= 0x100 && address < 0x200;
			bool expectedRead = address == (0x100u | (uint8_t) (returnS - 3)) ||
				address == (0x100u | (uint8_t) (returnS - 1)) ||
				address == (0x100u | returnS);
			bool expectedWrite = (address >= 0xA1 && address <= 0xA7) ||
				address == (0x100u | (uint8_t) (returnS - 2));
			if (bitSet(g->diagnostics.ramReadBeforeWriteMap[1], address) != expectedRead ||
					bitSet(g->diagnostics.ramWritten, address) != expectedWrite ||
					(!stack && !expectedWrite && g->ram[address] != baseline.ram[address]))
				ok = false;
		}
		if (g->diagnostics.romTrackedReads != 1) ok = false;
	}
	if (!ok) {
		fprintf(stderr, "F033 mismatch: synthetic_input=%06X P=%02X S=%02X "
			"returned=%u A=%02X X=%02X Y=%02X P_out=%02X clocks=%llu\n",
			input, p, returnS, returned, g->cpu.a, g->cpu.x, g->cpu.y, g->cpu.p,
			(unsigned long long) (g->masterClocks - start));
	}
	return ok;
}

struct RomOffset {
	size_t offset;
	bool found;
};

static void captureOffset(void* context, const struct Gamate* g,
		uint16_t address, size_t offset) {
	struct RomOffset* result = context;
	(void) g;
	(void) address;
	result->offset = offset;
	result->found = true;
}

static bool installCallback(struct Gamate* g, bool nmi, uint16_t address, uint8_t value,
		bool reentrant, uint32_t nestedInput, bool clobber, uint16_t initializeAddress) {
	uint8_t callback[] = { 0xA9, value, 0x8D, (uint8_t) address, (uint8_t) (address >> 8), 0x60 };
	uint8_t nestedCallback[] = {
		0x8A, 0x48, 0x98, 0x48,
		0xA9, (uint8_t) (nestedInput >> 16), 0xA2, (uint8_t) (nestedInput >> 8),
		0xA0, (uint8_t) nestedInput, 0x20, 0x33, 0xF0,
		0x68, 0xA8, 0x68, 0xAA, 0x60
	};
	uint8_t clobberCallback[] = { 0xA9, 0x78, 0xA2, 0x34, 0xA0, value, 0x60 };
	uint8_t initializeCallback[] = {
		0xA9, 0x30, 0x8D, (uint8_t) initializeAddress, (uint8_t) (initializeAddress >> 8),
		0xA9, 0x78, 0xA2, 0x34, 0xA0, value, 0x60
	};
	const uint8_t* program = initializeAddress != 0xFFFF ? initializeCallback :
		clobber ? clobberCallback : reentrant ? nestedCallback : callback;
	size_t length = initializeAddress != 0xFFFF ? sizeof(initializeCallback) :
		clobber ? sizeof(clobberCallback) :
		reentrant ? sizeof(nestedCallback) : sizeof(callback);
	gamateResetRomDiagnostics(g);
	g->diagnostics.diagnosticInstructionPc = 0xF033;
	for (size_t i = 0; i < length; i++) {
		struct RomOffset offset = { 0, false };
		gamateSetRomDiagnosticWatch(g, captureOffset, &offset);
		(void) gamateRead(g, (uint16_t) ((nmi ? 0x6023 : 0x6026) + i));
		if (!offset.found || offset.offset >= g->romSize) {
			fprintf(stderr, "could not locate synthetic callback address\n");
			return false;
		}
		g->rom[offset.offset] = program[i];
	}
	gamateSetRomDiagnosticWatch(g, NULL, NULL);
	return true;
}

struct Intervention {
	uint8_t buffer[7], address, value;
	uint64_t at;
	bool pending, reentrant;
	uint32_t nestedInput;
};

static void expectedInterleaving(struct Intervention* state, uint32_t input,
		uint8_t* resultY, uint8_t* resultP);

static void intervene(struct Intervention* state, unsigned clocks) {
	if (state->pending && state->at <= clocks) {
		if (state->reentrant) {
			struct Intervention nested = { { 0 }, 0, 0, 0, false, false, 0 };
			uint8_t y, p;
			expectedInterleaving(&nested, state->nestedInput, &y, &p);
			memcpy(state->buffer, nested.buffer, sizeof(state->buffer));
		} else {
			state->buffer[state->address - 0xA1] = state->value;
		}
		state->pending = false;
	}
}

static void expectedInterleaving(struct Intervention* state, uint32_t input,
		uint8_t* resultY, uint8_t* resultP) {
	uint8_t a = (uint8_t) (input >> 16), x = (uint8_t) (input >> 8), y = (uint8_t) input;
	intervene(state, 20); state->buffer[2] = 0x30 + (a & 15);
	intervene(state, 58); state->buffer[1] = 0x30 + (a >> 4);
	intervene(state, 76); state->buffer[4] = 0x30 + (x & 15);
	intervene(state, 110); state->buffer[3] = 0x30 + (x >> 4);
	unsigned start;
	if (y == 0xFF) {
		intervene(state, 128); state->buffer[5] = 0x20;
		intervene(state, 134); state->buffer[6] = 0x20;
		start = 150;
	} else {
		intervene(state, 138); state->buffer[6] = 0x30 + (y & 15);
		intervene(state, 172); state->buffer[5] = 0x30 + (y >> 4);
		start = 182;
	}
	unsigned i = 0, carry = 1;
	for (; i < 5; i++) {
		unsigned at = start + i * 44;
		intervene(state, at);
		uint8_t byte = state->buffer[1 + i];
		if (byte != 0x30) {
			carry = byte >= 0x30;
			break;
		}
		intervene(state, at + 20);
		state->buffer[1 + i] = 0x20;
	}
	intervene(state, start + i * 44 + (i == 5 ? 2 : 22));
	state->buffer[0] = 6;
	if (state->pending) intervene(state, UINT32_MAX);
	*resultY = (uint8_t) i;
	*resultP = (uint8_t) (0x20 | carry);
}

enum InterleaveResult { INTERLEAVE_FAILED, INTERLEAVE_PAST_END, INTERLEAVE_MATCHED };

static enum InterleaveResult checkInterleaving(bool nmi, uint32_t input, uint8_t address,
		uint8_t value, unsigned point, uint8_t entryP, bool reentrant, uint32_t nestedInput) {
	struct Gamate* g = &machine;
	*g = baseline;
	if (!installCallback(g, nmi, address, value, reentrant, nestedInput, false, 0xFFFF))
		return INTERLEAVE_FAILED;
	uint8_t returnS = g->cpu.s;
	g->ram[0x100 | returnS] = 0x60;
	g->ram[0x100 | (uint8_t) (returnS - 1)] = 0x1F;
	memset(&g->ram[0xA1], 0xA5, 7);
	g->ram[0x0C] = 1;
	g->cpu.pc = 0xF033;
	g->cpu.s = (uint8_t) (returnS - 2);
	g->cpu.a = (uint8_t) (input >> 16);
	g->cpu.x = (uint8_t) (input >> 8);
	g->cpu.y = (uint8_t) input;
	g->cpu.p = entryP;
	g->cpu.irqMask = (entryP & 4) != 0;
	g->irqClocks = 0;
	g->irq = g->nmi = false;
	uint64_t start = g->masterClocks;
	uint64_t initialInterrupts = nmi ? g->diagnostics.nmisTaken : g->diagnostics.irqsTaken;
	unsigned completed = 0;
	bool injected = false, returned = false;
	struct Intervention state = {
		{ 0xA5, 0xA5, 0xA5, 0xA5, 0xA5, 0xA5, 0xA5 }, address, value, 0, true,
		reentrant, nestedInput
	};
	for (unsigned instruction = 0; instruction < 10000; instruction++) {
		if (!injected && completed == point) {
			state.at = g->masterClocks - start;
			if (nmi) g->nmi = true;
			else g->irq = true;
			injected = true;
		}
		unsigned cycles = gamateCpuStep(g);
		gamateRunClocks(g, cycles * 2);
		uint64_t taken = nmi ? g->diagnostics.nmisTaken : g->diagnostics.irqsTaken;
		if (taken == initialInterrupts) completed++;
		if (g->cpu.pc == 0x6020 && g->cpu.s == returnS) {
			returned = true;
			break;
		}
	}
	uint64_t taken = (nmi ? g->diagnostics.nmisTaken : g->diagnostics.irqsTaken) -
		initialInterrupts;
	if (returned && !injected && !taken) return INTERLEAVE_PAST_END;
	uint8_t expectedY, expectedP;
	expectedInterleaving(&state, input, &expectedY, &expectedP);
	expectedP |= entryP & 0x5C;
	if (!returned || !injected || taken != 1 ||
			g->diagnostics.unsupportedOpcodes != baseline.diagnostics.unsupportedOpcodes ||
			g->cpu.a != 6 || g->cpu.x != (uint8_t) (input >> 8) ||
			g->cpu.y != expectedY || g->cpu.p != expectedP ||
			memcmp(&g->ram[0xA1], state.buffer, sizeof(state.buffer))) {
		fprintf(stderr, "F033 interleaving mismatch: synthetic_input=%06X address=%02X "
			"value=%02X point=%u clocks=%llu returned=%u interrupts=%llu "
			"A=%02X X=%02X Y=%02X/%02X P=%02X/%02X buffer_matches=%u\n", input,
			address, value, point, (unsigned long long) state.at, returned,
			(unsigned long long) taken, g->cpu.a, g->cpu.x, g->cpu.y, expectedY,
			g->cpu.p, expectedP, !memcmp(&g->ram[0xA1], state.buffer, sizeof(state.buffer)));
		return INTERLEAVE_FAILED;
	}
	return INTERLEAVE_MATCHED;
}

static bool checkCallbackContext(bool nmi, bool enabled, uint8_t entryP, uint8_t initialS) {
	struct Gamate* g = &machine;
	*g = baseline;
	if (!installCallback(g, nmi, 0, 0x56, false, 0, true, 0xFFFF)) return false;
	g->ram[0x0C] = enabled ? 1 : 0;
	g->cpu.pc = 0x6020;
	g->cpu.s = initialS;
	g->cpu.a = 0x12;
	g->cpu.x = 0xAB;
	g->cpu.y = 0xCD;
	g->cpu.p = entryP;
	g->cpu.irqMask = (entryP & 4) != 0;
	g->irqClocks = 0;
	g->irq = !nmi;
	g->nmi = nmi;
	uint64_t initialInterrupts = nmi ? g->diagnostics.nmisTaken : g->diagnostics.irqsTaken;
	bool returned = false;
	for (unsigned instruction = 0; instruction < 1000; instruction++) {
		unsigned cycles = gamateCpuStep(g);
		gamateRunClocks(g, cycles * 2);
		if (g->cpu.pc == 0x6020 && g->cpu.s == initialS) {
			returned = true;
			break;
		}
	}
	uint64_t taken = (nmi ? g->diagnostics.nmisTaken : g->diagnostics.irqsTaken) -
		initialInterrupts;
	bool ok = returned && taken == 1 && g->cpu.a == 0x12 &&
		g->cpu.x == (enabled ? 0x34 : 0xAB) &&
		g->cpu.y == (enabled && nmi ? 0x56 : 0xCD) && g->cpu.p == entryP &&
		g->diagnostics.unsupportedOpcodes == baseline.diagnostics.unsupportedOpcodes;
	if (!ok) {
		fprintf(stderr, "F033 callback context mismatch: enabled=%u P_in=%02X S_in=%02X "
			"returned=%u interrupts=%llu A=%02X X=%02X Y=%02X P=%02X\n",
			enabled, entryP, initialS, returned, (unsigned long long) taken,
			g->cpu.a, g->cpu.x, g->cpu.y, g->cpu.p);
	}
	return ok;
}

static bool checkRegisterInterference(bool nmi, unsigned boundary, uint8_t callbackY,
		bool callbackInitializesTarget) {
	struct Gamate* g = &machine;
	*g = baseline;
	uint16_t target = 0xA2 + callbackY;
	if (!installCallback(g, nmi, 0, callbackY, false, 0, true,
			callbackInitializesTarget ? target : 0xFFFF)) return false;
	uint8_t returnS = 0xF0;
	g->ram[0x100 | returnS] = 0x60;
	g->ram[0x100 | (uint8_t) (returnS - 1)] = 0x1F;
	memset(&g->ram[0xA1], 0xA5, 7);
	g->ram[target] = 0x30;
	g->ram[0] = 0x31;
	g->ram[0x0C] = 1;
	g->cpu.pc = 0xF033;
	g->cpu.s = (uint8_t) (returnS - 2);
	g->cpu.a = 0x12;
	g->cpu.x = 0x78;
	g->cpu.y = 0x9A;
	g->cpu.p = 0x20;
	g->cpu.irqMask = false;
	g->irqClocks = 0;
	g->irq = g->nmi = false;
	uint64_t start = g->masterClocks;
	uint64_t initialInterrupts = nmi ? g->diagnostics.nmisTaken : g->diagnostics.irqsTaken;
	bool injected = false, returned = false;
	for (unsigned instruction = 0; instruction < 10000; instruction++) {
		if (!injected && g->masterClocks - start == boundary) {
			if (nmi) g->nmi = true;
			else g->irq = true;
			injected = true;
		}
		unsigned cycles = gamateCpuStep(g);
		gamateRunClocks(g, cycles * 2);
		if (g->cpu.pc == 0x6020 && g->cpu.s == returnS) {
			returned = true;
			break;
		}
	}
	uint64_t taken = (nmi ? g->diagnostics.nmisTaken : g->diagnostics.irqsTaken) -
		initialInterrupts;
	bool redirected = nmi && boundary == 182;
	uint8_t expectedY = redirected ? (uint8_t) (callbackY + 1) :
		nmi && boundary == 190 ? callbackY : 0;
	uint8_t expected[] = { 6, '1', '2', '7', '8', '9', ':' };
	if (boundary <= 82) expected[3] = '3';
	if (boundary <= 64) expected[4] = '4';
	if (nmi && boundary <= 126) {
		expected[5] = (uint8_t) ('0' + (callbackY >> 4));
		expected[6] = (uint8_t) ('0' + (callbackY & 15));
	}
	bool ok = returned && injected && taken == 1 && g->cpu.a == 6 &&
		g->cpu.x == 0x34 && g->cpu.y == expectedY && g->cpu.p == 0x21 &&
		g->ram[target] == (redirected ? 0x20 : 0x30) &&
		g->ram[0] == 0x31 &&
		!memcmp(&g->ram[0xA1], expected, sizeof(expected)) &&
		g->diagnostics.unsupportedOpcodes == baseline.diagnostics.unsupportedOpcodes;
	if (!ok) {
		fprintf(stderr, "F033 register-interference mismatch: boundary=%u callback_y=%02X "
			"target=%03X:%02X returned=%u interrupts=%llu A=%02X X=%02X Y=%02X P=%02X\n",
			boundary, callbackY, target, g->ram[target], returned, (unsigned long long) taken,
			g->cpu.a, g->cpu.x, g->cpu.y, g->cpu.p);
	}
	return ok;
}

int main(int argc, char** argv) {
	if (argc != 4) {
		fprintf(stderr, "usage: %s BIOS CARTRIDGE "
			"matrix|full-binary|full-decimal|interleave-irq|interleave-nmi|"
			"interleave-flags-irq|interleave-flags-nmi|reentrant-irq|reentrant-nmi|"
			"context-irq|context-nmi|register-irq|register-nmi|"
			"register-early-irq|register-early-nmi|register-range-nmi\n", argv[0]);
		return 2;
	}
	bool matrix = !strcmp(argv[3], "matrix");
	bool binary = !strcmp(argv[3], "full-binary");
	bool decimal = !strcmp(argv[3], "full-decimal");
	bool irq = !strcmp(argv[3], "interleave-irq");
	bool nmi = !strcmp(argv[3], "interleave-nmi");
	bool flagsIrq = !strcmp(argv[3], "interleave-flags-irq");
	bool flagsNmi = !strcmp(argv[3], "interleave-flags-nmi");
	bool nestedIrq = !strcmp(argv[3], "reentrant-irq");
	bool nestedNmi = !strcmp(argv[3], "reentrant-nmi");
	bool contextIrq = !strcmp(argv[3], "context-irq");
	bool contextNmi = !strcmp(argv[3], "context-nmi");
	bool registerEarly = !strcmp(argv[3], "register-early-irq") ||
		!strcmp(argv[3], "register-early-nmi");
	bool registerRange = !strcmp(argv[3], "register-range-nmi");
	bool registerIrq = !strcmp(argv[3], "register-irq") ||
		!strcmp(argv[3], "register-early-irq");
	bool registerNmi = !strcmp(argv[3], "register-nmi") ||
		!strcmp(argv[3], "register-early-nmi") || registerRange;
	if (!matrix && !binary && !decimal && !irq && !nmi && !flagsIrq && !flagsNmi &&
			!nestedIrq && !nestedNmi && !contextIrq && !contextNmi &&
			!registerIrq && !registerNmi) {
		fprintf(stderr, "invalid mode: %s\n", argv[3]);
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
	gamateInit(&machine);
	if (!gamateLoad(&machine, rom, romSize, bios, biosSize)) {
		fprintf(stderr, "invalid BIOS or cartridge size\n");
		free(bios); free(rom);
		return 2;
	}
	free(bios); free(rom);
	bool entered = false;
	for (unsigned instruction = 0; instruction < 20000000; instruction++) {
		unsigned cycles = gamateCpuStep(&machine);
		gamateRunClocks(&machine, cycles * 2);
		if (machine.protectionPassed && machine.cpu.pc == 0x6020) {
			entered = true;
			break;
		}
	}
	if (!entered) {
		fprintf(stderr, "BIOS did not reach cartridge entry\n");
		gamateFree(&machine);
		return 1;
	}
	unsigned frame = 1;
	const char* press = getenv("GAMATEBIOSCALLS_PRESS");
	machine.frameDone = false;
	machine.buttons = gamateBiosButtonsForFrame(frame, press);
	bool found = false;
	for (unsigned instruction = 0; instruction < 100000000; instruction++) {
		if (machine.frameDone) {
			machine.frameDone = false;
			machine.buttons = gamateBiosButtonsForFrame(++frame, press);
		}
		unsigned cycles = gamateCpuStep(&machine);
		gamateRunClocks(&machine, cycles * 2);
		if (machine.cpu.pc == 0xF033) {
			found = true;
			break;
		}
	}
	if (!found) {
		fprintf(stderr, "cartridge did not reach first F033 occurrence\n");
		gamateFree(&machine);
		return 1;
	}
	machine.cpu.s += 2;
	baseline = machine;
	uint64_t tested = 0;
	if (matrix) {
		const uint8_t suffixes[] = { 0x00, 0x99, 0xFF };
		for (unsigned word = 0; word < 65536; word++) {
			for (unsigned i = 0; i < sizeof(suffixes); i++) {
				uint32_t input = (word << 8) | suffixes[i];
				for (unsigned d = 0; d < 2; d++) {
					if (!checkCall(input, (uint8_t) (0x24 | (d << 3)), baseline.cpu.s, false))
						goto failure;
					tested++;
				}
			}
		}
		uint32_t input = 0x123456;
		for (unsigned i = 0; i < 65536; i++) {
			input = (input * 1664525u + 1013904223u) & 0xFFFFFFu;
			for (unsigned d = 0; d < 2; d++) {
				if (!checkCall(input, (uint8_t) (0x24 | (d << 3)), baseline.cpu.s, false))
					goto failure;
				tested++;
			}
		}
		const uint32_t cases[] = { 0, 0x123456, 0x0000FF, 0xFFFFFF, 1 };
		for (unsigned i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
			for (unsigned value = 0; value < 256; value++) {
				if (!checkCall(cases[i], (uint8_t) value, baseline.cpu.s, true) ||
						!checkCall(cases[i], 0x24, (uint8_t) value, true))
					goto failure;
				tested += 2;
			}
		}
	} else if (binary || decimal) {
		for (uint32_t input = 0; input < 0x1000000; input++) {
			if (!checkCall(input, decimal ? 0x2C : 0x24, baseline.cpu.s, false))
				goto failure;
			tested++;
		}
	} else if (contextIrq || contextNmi) {
		for (unsigned enabled = 0; enabled < 2; enabled++) {
			for (unsigned s = 0; s < 256; s++) {
				if (!checkCallbackContext(contextNmi, enabled != 0, 0x20, (uint8_t) s))
					goto failure;
				tested++;
			}
			for (unsigned flags = 0; flags < 64; flags++) {
				uint8_t p = (uint8_t) (0x20 | (flags & 3) | ((flags & 4) ? 4 : 0) |
					((flags & 8) ? 8 : 0) | ((flags & 16) ? 0x40 : 0) |
					((flags & 32) ? 0x80 : 0));
				if (!contextNmi && (p & 4)) continue;
				if (!checkCallbackContext(contextNmi, enabled != 0, p, baseline.cpu.s))
					goto failure;
				tested++;
			}
		}
	} else if (registerIrq || registerNmi) {
		const unsigned early[] = { 0, 68, 86 };
		const unsigned late[] = { 178, 182, 190 };
		const unsigned *boundaries = registerEarly ? early : late;
		if (registerRange) {
			for (unsigned y = 6; y <= 255; y++) {
				if (!checkRegisterInterference(true, 182, (uint8_t) y, true))
					goto failure;
				tested++;
			}
		} else {
			for (unsigned i = 0; i < 3; i++) {
				for (unsigned target = 0; target < 2; target++) {
					if (!checkRegisterInterference(registerNmi, boundaries[i],
							target ? 0x5E : 0x56, false))
						goto failure;
					tested++;
				}
			}
		}
	} else if (!nestedIrq && !nestedNmi) {
		const uint32_t inputs[] = { 0, 0x123456, 0x0000FF, 0x000100, 0xFFFFFF, 1, 0x0000FE };
		const uint8_t values[] = { 0, 0x20, 0x2F, 0x30, 0x31, 0xFF };
		for (unsigned i = 0; i < sizeof(inputs) / sizeof(inputs[0]); i++) {
			for (unsigned address = 0xA1; address <= 0xA7; address++) {
				for (unsigned v = 0; v < sizeof(values); v++) {
					bool finished = false;
					for (unsigned point = 0; point < 200; point++) {
						bool useNmi = nmi || flagsNmi;
						uint8_t entryP = flagsNmi ? 0x6C : flagsIrq ? 0x68 : 0x20;
						enum InterleaveResult result = checkInterleaving(useNmi, inputs[i],
							(uint8_t) address, values[v], point, entryP, false, 0);
						if (result == INTERLEAVE_FAILED) goto failure;
						if (result == INTERLEAVE_PAST_END) {
							finished = true;
							break;
						}
						tested++;
					}
					if (!finished) {
						fprintf(stderr, "F033 interleaving boundary limit exceeded\n");
						goto failure;
					}
				}
			}
		}
	} else {
		const uint32_t inputs[] = { 0, 0x123456, 0x0000FF, 0x000100, 0xFFFFFF, 1, 0x0000FE };
		for (unsigned i = 0; i < sizeof(inputs) / sizeof(inputs[0]); i++) {
			for (unsigned j = 0; j < sizeof(inputs) / sizeof(inputs[0]); j++) {
				bool finished = false;
				for (unsigned point = 0; point < 200; point++) {
					enum InterleaveResult result = checkInterleaving(nestedNmi, inputs[i],
						0xA1, 0, point, 0x20, true, inputs[j]);
					if (result == INTERLEAVE_FAILED) goto failure;
					if (result == INTERLEAVE_PAST_END) {
						finished = true;
						break;
					}
					tested++;
				}
				if (!finished) {
					fprintf(stderr, "F033 reentrant boundary limit exceeded\n");
					goto failure;
				}
			}
		}
	}
	uint64_t expectedCalls = matrix ? 526848u : binary || decimal ? 16777216u :
		nestedIrq || nestedNmi ? 3276u : contextIrq ? 576u : contextNmi ? 640u :
		registerRange ? 250u : registerIrq || registerNmi ? 6u : 19656u;
	if (tested != expectedCalls) {
		fprintf(stderr, "F033 incomplete test: %llu calls, expected %llu\n",
			(unsigned long long) tested, (unsigned long long) expectedCalls);
		goto failure;
	}
	printf("F033 %s: %llu calls matched\n", argv[3], (unsigned long long) tested);
	gamateFree(&machine);
	return 0;
failure:
	gamateFree(&machine);
	return 1;
}
