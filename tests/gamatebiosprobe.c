/* Copyright (c) 2026 Ry
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
#include "gamate/gamate.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

static bool parseHex(const char* text, unsigned long maximum, unsigned long* value) {
	char* end;
	*value = strtoul(text, &end, 16);
	return *text && !*end && *value <= maximum;
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

static uint32_t byteHash(const uint8_t* data, size_t size) {
	uint32_t hash = 2166136261U;
	for (size_t i = 0; i < size; i++) hash = (hash ^ data[i]) * 16777619U;
	return hash;
}

static uint8_t lcdFlags(const struct Gamate* g) {
	return (g->vramPlane ? 0x01 : 0) | (g->incrementRow ? 0x02 : 0) |
		(g->window ? 0x04 : 0) | (g->swapPlanes ? 0x08 : 0) |
		(g->displayBlank ? 0x10 : 0) | (g->displayStopped ? 0x20 : 0);
}

static void setLcdFlags(struct Gamate* g, uint8_t flags) {
	g->vramPlane = flags & 0x01;
	g->incrementRow = flags & 0x02;
	g->window = flags & 0x04;
	g->swapPlanes = flags & 0x08;
	g->displayBlank = flags & 0x10;
	g->displayStopped = flags & 0x20;
}

static bool applyOption(struct Gamate* g, const char* option) {
	const char* equal = strchr(option, '=');
	if (!equal) return false;
	unsigned long value;
	if (!parseHex(equal + 1, 0xFFFF, &value)) return false;
	size_t nameLength = (size_t) (equal - option);
#define OPTION(name, maximum, statement) do { \
	if (nameLength == sizeof(name) - 1 && !memcmp(option, name, sizeof(name) - 1)) { \
		if (value > (maximum)) return false; \
		statement; \
		return true; \
	} \
} while (0)
	OPTION("a", 0xFF, g->cpu.a = (uint8_t) value);
	OPTION("x", 0xFF, g->cpu.x = (uint8_t) value);
	OPTION("y", 0xFF, g->cpu.y = (uint8_t) value);
	OPTION("p", 0xFF, g->cpu.p = (uint8_t) value; g->cpu.irqMask = value & 4);
	OPTION("vram", 0x1FFF, g->vramAddress = (uint16_t) value);
	OPTION("scrollx", 0xFF, g->scrollX = (uint8_t) value);
	OPTION("scrolly", 0xFF, g->scrollY = (uint8_t) value);
	OPTION("lcd", 0x3F, setLcdFlags(g, (uint8_t) value));
	OPTION("ay", 0x0F, g->ayAddress = (uint8_t) value);
	OPTION("bank", 0xFF, g->bank = (uint8_t) value);
	OPTION("firstbank", 0xFF, g->firstBank = (uint8_t) value);
	OPTION("vram_fill", 0xFF, memset(g->vram, (uint8_t) value, sizeof(g->vram)));
#undef OPTION
	if (nameLength == 6 && !memcmp(option, "ram", 3)) {
		char addressText[4] = { option[3], option[4], option[5], 0 };
		unsigned long address;
		if (!parseHex(addressText, 0x3FF, &address) || value > 0xFF) return false;
		g->ram[address] = (uint8_t) value;
		return true;
	}
	return false;
}

int main(int argc, char** argv) {
	if (argc < 4) {
		fprintf(stderr, "usage: %s gamate_bios.bin cartridge.bin SERVICE [NAME=HEX ...]\n",
			argv[0]);
		return 2;
	}
	unsigned long service;
	if (!parseHex(argv[3], 0xFFFF, &service) || service < 0xF000 || service > 0xF033 ||
			(service - 0xF000) % 3) {
		fprintf(stderr, "service must be a slot from F000 through F033\n");
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
	for (int i = 4; i < argc; i++) {
		if (!applyOption(&g, argv[i])) {
			fprintf(stderr, "invalid probe option: %s\n", argv[i]);
			gamateFree(&g);
			return 2;
		}
	}
	uint8_t entryA = g.cpu.a, entryX = g.cpu.x, entryY = g.cpu.y;
	uint8_t entryS = g.cpu.s, entryP = g.cpu.p;
	uint16_t entryVramAddress = g.vramAddress;
	uint8_t entryScrollX = g.scrollX, entryScrollY = g.scrollY;
	uint8_t entryLcdFlags = lcdFlags(&g), entryAyAddress = g.ayAddress;
	uint8_t entryBank = g.bank, entryFirstBank = g.firstBank;
	uint8_t ram[sizeof(g.ram)], vram[sizeof(g.vram)], ay[sizeof(g.ayRegisters)];
	memcpy(ram, g.ram, sizeof(ram));
	memcpy(vram, g.vram, sizeof(vram));
	memcpy(ay, g.ayRegisters, sizeof(ay));
	uint64_t startClocks = g.masterClocks;
	uint64_t startAyWrites = g.diagnostics.ayWrites;
	uint64_t startLcdWrites = g.diagnostics.lcdWrites;
	uint64_t startVramWrites = g.diagnostics.vramWrites;
	uint64_t startBankWrites = g.diagnostics.bankWrites;
	uint64_t startUartClocks = g.diagnostics.uartClocks;
	uint64_t startIrqs = g.diagnostics.irqsTaken, startNmis = g.diagnostics.nmisTaken;
	uint16_t returnAddress = 0x6020, stacked = returnAddress - 1;
	g.ram[0x100 | entryS] = (uint8_t) (stacked >> 8);
	g.ram[0x100 | (uint8_t) (entryS - 1)] = (uint8_t) stacked;
	g.cpu.s = entryS - 2;
	gamateResetRamDiagnostics(&g);
	gamateResetVramDiagnostics(&g);
	g.cpu.pc = (uint16_t) service;
	g.irqClocks = 0;
	g.irq = false;
	bool returned = false;
	for (uint64_t instruction = 0; instruction < 10000000; instruction++) {
		unsigned cycles = gamateCpuStep(&g);
		gamateRunClocks(&g, cycles * 2);
		if (g.cpu.pc == returnAddress && g.cpu.s == entryS) {
			returned = true;
			break;
		}
	}
	if (!returned || g.diagnostics.unsupportedOpcodes) {
		fprintf(stderr, "service did not return cleanly: PC=$%04X S=$%02X\n", g.cpu.pc, g.cpu.s);
		gamateFree(&g);
		return 1;
	}
	struct ChangeSummary ramChanges = summarizeChanges(ram, g.ram, sizeof(ram));
	struct ChangeSummary vramChanges = summarizeChanges(vram, g.vram, sizeof(vram));
	uint16_t ayMask = 0;
	for (unsigned i = 0; i < sizeof(ay); i++) if (ay[i] != g.ayRegisters[i]) ayMask |= 1u << i;
	printf("# bios_ram_read_before_write:");
	for (unsigned address = 0; address < sizeof(g.ram); address++) {
		const uint8_t* bits = g.diagnostics.ramReadBeforeWriteMap[1];
		if (bits[address >> 3] & ((uint8_t) 1 << (address & 7))) printf(" %03X", address);
	}
	putchar('\n');
	printf("# ram_written:");
	for (unsigned address = 0; address < sizeof(g.ram); address++) {
		const uint8_t* bits = g.diagnostics.ramWritten;
		if (bits[address >> 3] & ((uint8_t) 1 << (address & 7))) printf(" %03X", address);
	}
	putchar('\n');
	printf("slot\tin_a\tin_x\tin_y\tin_s\tin_p\tout_a\tout_x\tout_y\tout_s\tout_p\t"
		"clocks\tram_changed\tram_first\tram_last\tram_hash\t"
		"vram_changed\tvram_first\tvram_last\tvram_hash\tvram_unique_written\t"
		"in_vram\tout_vram\tin_scroll_x\tout_scroll_x\tin_scroll_y\tout_scroll_y\t"
		"in_lcd\tout_lcd\tin_ay\tout_ay\tin_bank\tout_bank\tin_first_bank\tout_first_bank\t"
		"ay_mask\tay_writes\tlcd_writes\tvram_writes\tbank_writes\tuart_clocks\tirqs\tnmis\n");
	printf("%04lX\t%02X\t%02X\t%02X\t%02X\t%02X\t%02X\t%02X\t%02X\t%02X\t%02X\t"
		"%llu\t%u\t%04X\t%04X\t%08X\t%u\t%04X\t%04X\t%08X\t%u\t"
		"%04X\t%04X\t%02X\t%02X\t%02X\t%02X\t%02X\t%02X\t%02X\t%02X\t"
		"%02X\t%02X\t%02X\t%02X\t%04X\t%llu\t%llu\t%llu\t%llu\t%llu\t%llu\t%llu\n",
		service, entryA, entryX, entryY, entryS, entryP,
		g.cpu.a, g.cpu.x, g.cpu.y, g.cpu.s, g.cpu.p,
		(unsigned long long) (g.masterClocks - startClocks),
		ramChanges.count, ramChanges.first, ramChanges.last, byteHash(g.ram, sizeof(g.ram)),
		vramChanges.count, vramChanges.first, vramChanges.last, byteHash(g.vram, sizeof(g.vram)),
		g.diagnostics.vramUniqueWritten,
		entryVramAddress, g.vramAddress, entryScrollX, g.scrollX, entryScrollY, g.scrollY,
		entryLcdFlags, lcdFlags(&g), entryAyAddress, g.ayAddress, entryBank, g.bank,
		entryFirstBank, g.firstBank, ayMask,
		(unsigned long long) (g.diagnostics.ayWrites - startAyWrites),
		(unsigned long long) (g.diagnostics.lcdWrites - startLcdWrites),
		(unsigned long long) (g.diagnostics.vramWrites - startVramWrites),
		(unsigned long long) (g.diagnostics.bankWrites - startBankWrites),
		(unsigned long long) (g.diagnostics.uartClocks - startUartClocks),
		(unsigned long long) (g.diagnostics.irqsTaken - startIrqs),
		(unsigned long long) (g.diagnostics.nmisTaken - startNmis));
	gamateFree(&g);
	return 0;
}
