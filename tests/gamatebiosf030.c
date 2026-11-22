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

static bool checkCall(uint16_t input, uint8_t a, uint8_t x, uint8_t y,
		uint8_t p, uint8_t scratch, uint8_t returnS) {
	struct Gamate* g = &machine;
	*g = baseline;
	g->ram[0x100 | returnS] = 0x60;
	g->ram[0x100 | (uint8_t) (returnS - 1)] = 0x1F;
	g->cpu.pc = 0xF030;
	g->cpu.s = (uint8_t) (returnS - 2);
	g->cpu.a = a;
	g->cpu.x = x;
	g->cpu.y = y;
	g->cpu.p = p;
	g->cpu.irqMask = (p & 4) != 0;
	g->ram[0x12] = (uint8_t) input;
	g->ram[0x13] = (uint8_t) (input >> 8);
	g->ram[0x14] = scratch;
	g->irqClocks = 0;
	g->irq = false;
	g->nmi = false;
	gamateResetRamDiagnostics(g);
	gamateResetRomDiagnostics(g);
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

	uint16_t state = input ? input : 0xFFFF;
	unsigned feedback = ((state >> 4) ^ (state >> 10)) & 1;
	uint16_t expected = (uint16_t) ((state << 1) | feedback);
	uint8_t expectedP = (uint8_t) (0x20 | (p & 0x4C) |
		(state >> 15) | ((expected & 0x80) ? 0x80 : 0) |
		((expected & 0xFF) ? 0 : 0x02));
	bool ok = returned && !g->diagnostics.unsupportedOpcodes &&
		g->cpu.a == (uint8_t) expected && g->cpu.x == x && g->cpu.y == y &&
		g->cpu.p == expectedP && g->ram[0x12] == (uint8_t) expected &&
		g->ram[0x13] == (uint8_t) (expected >> 8) &&
		g->ram[0x14] == (uint8_t) ((state >> 3) & 0x80) &&
		g->masterClocks - start == (input ? 174u : 188u);
	for (unsigned address = 0; address < sizeof(g->ram); address++) {
		bool stack = address >= 0x100 && address < 0x200;
		bool expectedRead = address == 0x12 || address == 0x13 ||
			address == (0x100u | (uint8_t) (returnS - 4)) ||
			address == (0x100u | (uint8_t) (returnS - 1)) ||
			address == (0x100u | returnS);
		bool expectedWrite = address == 0x12 || address == 0x13 || address == 0x14 ||
			address == (0x100u | (uint8_t) (returnS - 2)) ||
			address == (0x100u | (uint8_t) (returnS - 3));
		if (bitSet(g->diagnostics.ramReadBeforeWriteMap[1], address) != expectedRead ||
				bitSet(g->diagnostics.ramWritten, address) != expectedWrite ||
				(!stack && !expectedWrite && g->ram[address] != baseline.ram[address]))
			ok = false;
	}
	if (g->vramAddress != baseline.vramAddress || g->vramPlane != baseline.vramPlane ||
			g->incrementRow != baseline.incrementRow || g->window != baseline.window ||
			g->swapPlanes != baseline.swapPlanes || g->displayBlank != baseline.displayBlank ||
			g->displayStopped != baseline.displayStopped || g->scrollX != baseline.scrollX ||
			g->scrollY != baseline.scrollY || g->ayAddress != baseline.ayAddress ||
			g->bank != baseline.bank || g->firstBank != baseline.firstBank ||
			g->diagnostics.ayWrites != baseline.diagnostics.ayWrites ||
			g->diagnostics.lcdWrites != baseline.diagnostics.lcdWrites ||
			g->diagnostics.vramWrites != baseline.diagnostics.vramWrites ||
			g->diagnostics.bankWrites != baseline.diagnostics.bankWrites ||
			g->diagnostics.uartClocks != baseline.diagnostics.uartClocks ||
			g->diagnostics.irqsTaken != baseline.diagnostics.irqsTaken ||
			g->diagnostics.nmisTaken != baseline.diagnostics.nmisTaken ||
			g->diagnostics.romTrackedReads != 1) ok = false;
	if (!ok) {
		fprintf(stderr, "F030 mismatch: input=%04X A=%02X X=%02X Y=%02X P=%02X "
			"scratch=%02X returned=%u result=%02X%02X scratch_out=%02X "
			"P_out=%02X clocks=%llu rom_reads=%llu\n", input, a, x, y, p, scratch,
			returned, g->ram[0x13], g->ram[0x12], g->ram[0x14], g->cpu.p,
			(unsigned long long) (g->masterClocks - start),
			(unsigned long long) g->diagnostics.romTrackedReads);
	}
	return ok;
}

static uint8_t status(unsigned bits) {
	return (uint8_t) (0x20 | ((bits & 1) ? 0x01 : 0) |
		((bits & 2) ? 0x02 : 0) | ((bits & 4) ? 0x04 : 0) |
		((bits & 8) ? 0x08 : 0) | ((bits & 16) ? 0x40 : 0) |
		((bits & 32) ? 0x80 : 0));
}

struct RomOffset {
	size_t offset;
	bool found;
};

static void captureOffset(void* context, const struct Gamate* g,
		uint16_t address, size_t physicalOffset) {
	struct RomOffset* result = context;
	(void) g;
	(void) address;
	result->offset = physicalOffset;
	result->found = true;
}

struct FinalRead {
	uint64_t start;
	bool seen;
};

static void captureFinalRead(void* context, const struct Gamate* g, bool write,
		uint16_t address, uint8_t previous, uint8_t value) {
	struct FinalRead* read = context;
	(void) address;
	(void) previous;
	(void) value;
	if (!write && g->masterClocks - read->start >= 150) read->seen = true;
}

static bool installCallback(struct Gamate* g, bool nmi, const uint8_t* callback, size_t length) {
	gamateResetRomDiagnostics(g);
	g->diagnostics.diagnosticInstructionPc = 0xF030;
	for (size_t i = 0; i < length; i++) {
		struct RomOffset offset = { 0, false };
		gamateSetRomDiagnosticWatch(g, captureOffset, &offset);
		(void) gamateRead(g, (uint16_t) ((nmi ? 0x6023 : 0x6026) + i));
		if (!offset.found || offset.offset >= g->romSize) {
			fprintf(stderr, "could not locate synthetic callback address\n");
			return false;
		}
		g->rom[offset.offset] = callback[i];
	}
	gamateSetRomDiagnosticWatch(g, NULL, NULL);
	return true;
}

static bool checkInterrupt(unsigned scenario, bool nmi) {
	/* Purpose-written callbacks are installed only in the probe's private cartridge copy. */
	static const uint8_t observe[] = {
		0xA5, 0x12, 0x8D, 0x00, 0x03, 0xA5, 0x13, 0x8D, 0x01, 0x03, 0x60
	};
	static const uint8_t changeScratch[] = { 0xA9, 0x80, 0x85, 0x14, 0x60 };
	static const uint8_t changeLow[] = { 0xA9, 0x80, 0x85, 0x12, 0x60 };
	static const uint8_t nestedCall[] = { 0x20, 0x30, 0xF0, 0x60 };
	const uint8_t* callback = scenario == 0 ? observe : scenario == 1 ? changeScratch :
		scenario == 2 ? changeLow : nestedCall;
	size_t length = scenario == 0 ? sizeof(observe) :
		scenario == 1 ? sizeof(changeScratch) : scenario == 2 ? sizeof(changeLow) :
		sizeof(nestedCall);
	struct Gamate* g = &machine;
	*g = baseline;
	if (!installCallback(g, nmi, callback, length)) return false;
	uint8_t returnS = g->cpu.s;
	g->ram[0x100 | returnS] = 0x60;
	g->ram[0x100 | (uint8_t) (returnS - 1)] = 0x1F;
	g->cpu.pc = 0xF030;
	g->cpu.s = (uint8_t) (returnS - 2);
	g->cpu.a = 0x5A;
	g->cpu.x = 0xC3;
	g->cpu.y = 0x69;
	g->cpu.p = 0x20;
	g->cpu.irqMask = false;
	g->ram[0x0C] = 1;
	g->ram[0x12] = scenario == 3 ? 0x00 : 0x34;
	g->ram[0x13] = scenario == 3 ? 0x04 : 0x12;
	g->ram[0x14] = 0xA5;
	g->ram[0x300] = g->ram[0x301] = 0xFF;
	g->irqClocks = 0;
	g->irq = false;
	g->nmi = false;
	struct FinalRead read = { g->masterClocks, false };
	if (scenario == 2) {
		gamateResetRamDiagnostics(g);
		gamateSetRamDiagnosticWatch(g, 0x12, captureFinalRead, &read);
	}
	uint64_t interruptStart = nmi ? g->diagnostics.nmisTaken : g->diagnostics.irqsTaken;
	bool injected = false, returned = false;
	for (unsigned instruction = 0; instruction < 10000; instruction++) {
		unsigned cycles = gamateCpuStep(g);
		gamateRunClocks(g, cycles * 2);
		if (!injected && ((scenario == 0 && g->ram[0x12] == 0x69) ||
				(scenario == 1 && g->ram[0x14] == 0) || (scenario == 2 && read.seen) ||
				(scenario == 3 && g->ram[0x14] == 0x80))) {
			if (nmi) g->nmi = true;
			else g->irq = true;
			injected = true;
		}
		if (g->cpu.pc == 0x6020 && g->cpu.s == returnS) {
			returned = true;
			break;
		}
	}
	uint8_t expectedLow = scenario == 0 ? 0x69 : scenario == 1 ? 0x68 :
		scenario == 2 ? 0x80 : 0x03;
	uint8_t expectedA = scenario == 2 ? 0x69 : expectedLow;
	uint64_t taken = (nmi ? g->diagnostics.nmisTaken : g->diagnostics.irqsTaken) -
		interruptStart;
	bool ok = injected && returned && taken == 1 &&
		g->ram[0x12] == expectedLow && g->ram[0x13] == (scenario == 3 ? 0x10 : 0x24) &&
		g->ram[0x14] == (scenario == 1 || scenario == 3 ? 0x80 : 0) &&
		g->cpu.a == expectedA && g->cpu.x == 0xC3 && g->cpu.y == 0x69 &&
		g->cpu.p == 0x20 && (scenario || (g->ram[0x300] == 0x69 && g->ram[0x301] == 0x12));
	if (!ok) {
		fprintf(stderr, "F030 interrupt mismatch: scenario=%u injected=%u returned=%u "
			"interrupts=%llu result=%02X%02X scratch=%02X observed=%02X%02X\n",
			scenario, injected, returned, (unsigned long long) taken,
			g->ram[0x13], g->ram[0x12], g->ram[0x14], g->ram[0x301], g->ram[0x300]);
	}
	return ok;
}

struct Intervention {
	uint8_t ram[3], address, value;
	uint64_t at;
	bool pending, nested;
};

static void applyIntervention(struct Intervention* state) {
	if (state->nested) {
		uint16_t value = (uint16_t) (state->ram[0] | state->ram[1] << 8);
		if (!value) value = 0xFFFF;
		unsigned feedback = ((value >> 4) ^ (value >> 10)) & 1;
		uint16_t next = (uint16_t) ((value << 1) | feedback);
		state->ram[0] = (uint8_t) next;
		state->ram[1] = (uint8_t) (next >> 8);
		state->ram[2] = (uint8_t) ((value >> 3) & 0x80);
	} else {
		state->ram[state->address - 0x12] = state->value;
	}
	state->pending = false;
}

static void intervene(struct Intervention* state, unsigned clocks) {
	if (state->pending && state->at <= clocks) applyIntervention(state);
}

static void expectedInterleaving(struct Intervention* state, uint8_t* a, uint8_t* p) {
	intervene(state, 26);
	uint8_t low = state->ram[0];
	intervene(state, 32);
	uint8_t high = state->ram[1];
	bool repair = !low && !high;
	unsigned delay = repair ? 14 : 0;
	if (repair) {
		intervene(state, 46);
		state->ram[0] = 0xFF;
		intervene(state, 52);
		state->ram[1] = 0xFF;
	}
	intervene(state, 44 + delay);
	uint8_t scratch = (uint8_t) ((state->ram[1] & 4) << 5);
	intervene(state, 74 + delay);
	state->ram[2] = scratch;
	intervene(state, 80 + delay);
	unsigned feedback = (state->ram[0] >> 4) & 1;
	intervene(state, 102 + delay);
	feedback ^= (state->ram[2] >> 7) & 1;
	intervene(state, 112 + delay);
	unsigned carry = state->ram[0] >> 7;
	state->ram[0] = (uint8_t) ((state->ram[0] << 1) | feedback);
	intervene(state, 122 + delay);
	unsigned finalCarry = state->ram[1] >> 7;
	state->ram[1] = (uint8_t) ((state->ram[1] << 1) | carry);
	intervene(state, 156 + delay);
	*a = state->ram[0];
	if (state->pending) applyIntervention(state);
	*p = (uint8_t) (0x20 | finalCarry | ((*a & 0x80) ? 0x80 : 0) | (*a ? 0 : 2));
}

enum InterleaveResult { INTERLEAVE_FAILED, INTERLEAVE_PAST_END, INTERLEAVE_MATCHED };

static enum InterleaveResult checkInterleaving(bool nmi, uint16_t input, uint8_t address,
		uint8_t value, unsigned point, uint8_t entryP, bool nested) {
	uint8_t callback[] = { 0xA9, value, 0x85, address, 0x60 };
	static const uint8_t nestedCallback[] = { 0x20, 0x30, 0xF0, 0x60 };
	struct Gamate* g = &machine;
	*g = baseline;
	if (!installCallback(g, nmi, nested ? nestedCallback : callback,
			nested ? sizeof(nestedCallback) : sizeof(callback))) return INTERLEAVE_FAILED;
	uint8_t returnS = g->cpu.s;
	g->ram[0x100 | returnS] = 0x60;
	g->ram[0x100 | (uint8_t) (returnS - 1)] = 0x1F;
	g->cpu.pc = 0xF030;
	g->cpu.s = (uint8_t) (returnS - 2);
	g->cpu.a = 0x5A;
	g->cpu.x = 0xC3;
	g->cpu.y = 0x69;
	g->cpu.p = entryP;
	g->cpu.irqMask = (entryP & 4) != 0;
	g->ram[0x0C] = 1;
	g->ram[0x12] = (uint8_t) input;
	g->ram[0x13] = (uint8_t) (input >> 8);
	g->ram[0x14] = 0xA5;
	g->irqClocks = 0;
	g->irq = false;
	g->nmi = false;
	uint64_t start = g->masterClocks;
	uint64_t initialInterrupts = nmi ? g->diagnostics.nmisTaken : g->diagnostics.irqsTaken;
	unsigned completed = 0;
	bool injected = false, returned = false;
	struct Intervention state = {
		{ (uint8_t) input, (uint8_t) (input >> 8), 0xA5 }, address, value, 0, true, nested
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
	uint8_t expectedA, expectedP;
	expectedInterleaving(&state, &expectedA, &expectedP);
	expectedP |= entryP & 0x4C;
	if (!returned || !injected || taken != 1 || g->ram[0x12] != state.ram[0] ||
			g->ram[0x13] != state.ram[1] || g->ram[0x14] != state.ram[2] ||
			g->cpu.a != expectedA || g->cpu.x != 0xC3 || g->cpu.y != 0x69 ||
			g->cpu.p != expectedP) {
		fprintf(stderr, "F030 interleaving mismatch: state=%04X address=%02X value=%02X "
			"point=%u clocks=%llu returned=%u interrupts=%llu\n", input, address, value,
			point, (unsigned long long) state.at, returned, (unsigned long long) taken);
		return INTERLEAVE_FAILED;
	}
	return INTERLEAVE_MATCHED;
}

int main(int argc, char** argv) {
	if (argc != 4 && argc != 5) {
		fprintf(stderr, "usage: %s BIOS CARTRIDGE MODE [occurrence]\n", argv[0]);
		return 2;
	}
	bool full = !strcmp(argv[3], "full");
	bool stack = !strcmp(argv[3], "stack");
	bool irq = !strcmp(argv[3], "irq");
	bool nmi = !strcmp(argv[3], "nmi");
	bool interleaveIrq = !strcmp(argv[3], "interleave-irq");
	bool interleaveNmi = !strcmp(argv[3], "interleave-nmi");
	bool allIrq = !strcmp(argv[3], "interleave-all-irq");
	bool allNmi = !strcmp(argv[3], "interleave-all-nmi");
	bool flagsIrq = !strcmp(argv[3], "interleave-flags-irq");
	bool flagsNmi = !strcmp(argv[3], "interleave-flags-nmi");
	bool nestedIrq = !strcmp(argv[3], "nested-irq");
	bool nestedNmi = !strcmp(argv[3], "nested-nmi");
	if (!full && !stack && !irq && !nmi && !interleaveIrq && !interleaveNmi && !allIrq && !allNmi &&
			!flagsIrq && !flagsNmi && !nestedIrq && !nestedNmi &&
			strcmp(argv[3], "sample")) {
		fprintf(stderr, "invalid mode: %s\n", argv[3]);
		return 2;
	}
	char* end = NULL;
	unsigned long occurrence = argc == 5 ? strtoul(argv[4], &end, 10) : 0;
	if (argc == 5 && (!*argv[4] || *end || !occurrence || occurrence > 65535)) {
		fprintf(stderr, "occurrence must be a nonzero decimal call number\n");
		return 2;
	}
	size_t biosSize, romSize;
	uint8_t* bios = readFile(argv[1], &biosSize);
	uint8_t* rom = readFile(argv[2], &romSize);
	if (!bios || !rom) {
		fprintf(stderr, "could not read BIOS or cartridge\n");
		free(bios);
		free(rom);
		return 2;
	}
	gamateInit(&machine);
	if (!gamateLoad(&machine, rom, romSize, bios, biosSize)) {
		fprintf(stderr, "invalid BIOS or cartridge size\n");
		free(bios);
		free(rom);
		return 2;
	}
	free(bios);
	free(rom);
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
	if (occurrence) {
		unsigned frame = 1;
		unsigned long seen = 0;
		const char* press = getenv("GAMATEBIOSCALLS_PRESS");
		machine.frameDone = false;
		machine.buttons = gamateBiosButtonsForFrame(frame, press);
		for (unsigned instruction = 0; instruction < 100000000; instruction++) {
			if (machine.frameDone) {
				machine.frameDone = false;
				machine.buttons = gamateBiosButtonsForFrame(++frame, press);
			}
			unsigned cycles = gamateCpuStep(&machine);
			gamateRunClocks(&machine, cycles * 2);
			if (machine.cpu.pc == 0xF030 && ++seen == occurrence) break;
		}
		if (machine.cpu.pc != 0xF030 || seen != occurrence) {
			fprintf(stderr, "cartridge did not reach F030 occurrence\n");
			gamateFree(&machine);
			return 1;
		}
		machine.cpu.s += 2;
	}
	baseline = machine;
	unsigned tested = 0;
	for (unsigned state = 0; state <
			(stack || irq || nmi || interleaveIrq || interleaveNmi || allIrq || allNmi ||
				flagsIrq || flagsNmi || nestedIrq || nestedNmi ?
				0u : full ? 65536u : 32u);
			state++) {
		uint16_t input = full ? (uint16_t) state :
			(state == 0 ? 0 : state <= 16 ? (uint16_t) (1u << (state - 1)) :
				(uint16_t) (0x9E37u * state));
		for (unsigned variant = 0; variant < 2; variant++) {
			uint8_t a = variant ? 0xA5 : 0x5A;
			uint8_t x = variant ? 0x3C : 0xC3;
			uint8_t y = variant ? 0x96 : 0x69;
			uint8_t p = status(variant ? 63 : 4);
			uint8_t scratch = variant ? 0xFF : 0;
			if (!checkCall(input, a, x, y, p, scratch, baseline.cpu.s)) {
				gamateFree(&machine);
				return 1;
			}
			tested++;
		}
	}
	if (full) {
		const uint16_t states[] = { 0, 1, 0x0010, 0x0400, 0x8000, 0xFFFF, 0x1234 };
		for (size_t i = 0; i < sizeof(states) / sizeof(states[0]); i++) {
			for (unsigned value = 0; value < 256; value++) {
				if (!checkCall(states[i], (uint8_t) value, 0xC3, 0x69, 0x24, 0x5A, baseline.cpu.s) ||
						!checkCall(states[i], 0x5A, (uint8_t) value, 0x69, 0x24, 0x5A, baseline.cpu.s) ||
						!checkCall(states[i], 0x5A, 0xC3, (uint8_t) value, 0x24, 0x5A, baseline.cpu.s) ||
						!checkCall(states[i], 0x5A, 0xC3, 0x69, 0x24, (uint8_t) value, baseline.cpu.s) ||
						(value < 64 && !checkCall(states[i], 0x5A, 0xC3, 0x69,
							status(value), 0x5A, baseline.cpu.s))) {
					gamateFree(&machine);
					return 1;
				}
				tested += value < 64 ? 5 : 4;
			}
		}
	}
	if (stack) {
		const uint16_t states[] = { 0, 1, 0x0010, 0x0400, 0x8000, 0xFFFF, 0x1234 };
		for (size_t i = 0; i < sizeof(states) / sizeof(states[0]); i++) {
			for (unsigned s = 0; s < 256; s++) {
				if (!checkCall(states[i], 0x5A, 0xC3, 0x69, 0x24, 0x5A, (uint8_t) s)) {
					gamateFree(&machine);
					return 1;
				}
				tested++;
			}
		}
	}
	if (irq || nmi) {
		for (unsigned scenario = 0; scenario < 4; scenario++) {
			if (!checkInterrupt(scenario, nmi)) {
				gamateFree(&machine);
				return 1;
			}
			tested++;
		}
	}
	if (interleaveIrq || interleaveNmi || allIrq || allNmi || flagsIrq || flagsNmi) {
		const uint16_t states[] = { 0, 1, 0x0010, 0x0400, 0x8000, 0xFFFF, 0x1234, 0x5555 };
		const uint8_t values[] = { 0, 4, 0x10, 0x80, 0xFF };
		bool allValues = allIrq || allNmi;
		for (size_t i = 0; i < sizeof(states) / sizeof(states[0]); i++) {
			for (unsigned address = 0x12; address <= 0x14; address++) {
				unsigned count = allValues ? 256u : sizeof(values) / sizeof(values[0]);
				for (unsigned v = 0; v < count; v++) {
					bool finished = false;
					for (unsigned point = 0; point < 100; point++) {
						bool useNmi = interleaveNmi || allNmi || flagsNmi;
						uint8_t entryP = flagsNmi ? 0x6C : flagsIrq ? 0x68 : 0x20;
						enum InterleaveResult result = checkInterleaving(useNmi, states[i],
							(uint8_t) address, allValues ? (uint8_t) v : values[v],
							point, entryP, false);
						if (result == INTERLEAVE_FAILED) {
							gamateFree(&machine);
							return 1;
						}
						if (result == INTERLEAVE_PAST_END) {
							finished = true;
							break;
						}
						tested++;
					}
					if (!finished) {
						fprintf(stderr, "F030 interleaving boundary limit exceeded\n");
						gamateFree(&machine);
						return 1;
					}
				}
			}
		}
	}
	if (nestedIrq || nestedNmi) {
		const uint16_t states[] = { 0, 1, 0x0010, 0x0400, 0x8000, 0xFFFF, 0x1234, 0x5555 };
		for (size_t i = 0; i < sizeof(states) / sizeof(states[0]); i++) {
			bool finished = false;
			for (unsigned point = 0; point < 100; point++) {
				enum InterleaveResult result = checkInterleaving(nestedNmi, states[i], 0x12,
					0, point, 0x20, true);
				if (result == INTERLEAVE_FAILED) {
					gamateFree(&machine);
					return 1;
				}
				if (result == INTERLEAVE_PAST_END) {
					finished = true;
					break;
				}
				tested++;
			}
			if (!finished) {
				fprintf(stderr, "F030 nested boundary limit exceeded\n");
				gamateFree(&machine);
				return 1;
			}
		}
	}
	unsigned expectedCalls = full ? 138688u : stack ? 1792u : irq || nmi ? 4u :
		interleaveIrq || interleaveNmi || flagsIrq || flagsNmi ? 3765u :
		allIrq || allNmi ? 192768u : nestedIrq || nestedNmi ? 251u : 64u;
	if (tested != expectedCalls) {
		fprintf(stderr, "F030 incomplete test: %u calls, expected %u\n", tested, expectedCalls);
		gamateFree(&machine);
		return 1;
	}
	printf("F030 %s: %u calls matched\n", argv[3], tested);
	gamateFree(&machine);
	return 0;
}
