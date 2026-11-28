/* Copyright (c) 2026 Ry
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
#include "gamate/gamate.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint8_t memory[0x10000];

// Flat bus used only by the external CPU conformance runner. Linking cpu.c directly against this
// file ensures the exact production CPU executes without involving the Gamate memory map.
uint8_t gamateRead(struct Gamate* gamate, uint16_t address) {
	(void) gamate;
	return memory[address];
}

void gamateWrite(struct Gamate* gamate, uint16_t address, uint8_t value) {
	(void) gamate;
	memory[address] = value;
}

// cpu.c also supports the production debugger and trace recorder. The functional-test runner keeps
// tracing disabled, but supplies its flat-bus equivalents so it can link cpu.c independently of the
// Gamate machine implementation.
uint8_t gamatePeek(const struct Gamate* gamate, uint16_t address) {
	(void) gamate;
	return memory[address];
}

void gamateTraceRecord(struct Gamate* gamate, const struct GamateTraceEntry* entry) {
	(void) gamate;
	(void) entry;
}

uint8_t gamateDisassemble(const struct Gamate* gamate, uint16_t address,
		char* output, size_t outputSize,
		GamateDisassemblyLabelCallback labelCallback, void* labelContext) {
	(void) gamate;
	(void) address;
	(void) output;
	(void) outputSize;
	(void) labelCallback;
	(void) labelContext;
	return 0;
}

int main(int argc, char** argv) {
	if (argc != 2) {
		fprintf(stderr, "usage: %s 6502_functional_test.bin\n", argv[0]);
		return 2;
	}
	FILE* file = fopen(argv[1], "rb");
	if (!file) {
		perror(argv[1]);
		return 2;
	}
	size_t size = fread(memory, 1, sizeof(memory), file);
	int extra = fgetc(file);
	fclose(file);
	if (size != sizeof(memory) || extra != EOF) {
		fprintf(stderr, "%s: expected a 65536-byte test image\n", argv[1]);
		return 2;
	}

	struct Gamate machine;
	memset(&machine, 0, sizeof(machine));
	machine.cpu.pc = 0x0400;
	machine.cpu.s = 0xFD;
	machine.cpu.p = 0x24;
	uint16_t previous = 0xFFFF;
	unsigned repeats = 0;
	for (uint64_t instructions = 0; instructions < 100000000; instructions++) {
		uint16_t pc = machine.cpu.pc;
		gamateCpuStep(&machine);
		if (machine.cpu.pc == pc && pc == previous) {
			if (++repeats == 16) {
				if (pc == 0x3469) {
					printf("6502 functional test passed in %llu instructions\n",
						(unsigned long long) instructions + 1);
					return 0;
				}
				fprintf(stderr, "6502 functional test trapped at $%04X (test case $%02X)\n",
					pc, memory[0x0200]);
				fprintf(stderr, "A=$%02X X=$%02X Y=$%02X P=$%02X inputs=$%02X,$%02X,$%02X expected=$%02X/$%02X\n",
					machine.cpu.a, machine.cpu.x, machine.cpu.y, machine.cpu.p,
					memory[0x0D], memory[0x0E], memory[0x12], memory[0x0F], memory[0x10]);
				return 1;
			}
		} else {
			repeats = 0;
		}
		previous = pc;
	}
	fprintf(stderr, "6502 functional test timed out at $%04X (test case $%02X)\n",
		machine.cpu.pc, memory[0x0200]);
	return 1;
}
