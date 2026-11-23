/* SPDX-License-Identifier: GPL-2.0-or-later */

/* Purpose-written BRK and callback programs use private in-memory cartridge copies. */
#include "gamate/gamate.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { RETURN = 0x6100 };

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

static void installCapture(struct Gamate* g, bool absolute) {
	static const uint8_t direct[] = {
		0xAD, 0xFD, 0x01, 0x8D, 0x00, 0x03,
		0xAD, 0xFE, 0x01, 0x8D, 0x01, 0x03,
		0xAD, 0xFF, 0x01, 0x8D, 0x02, 0x03,
		0xA9, 0x5A, 0xA2, 0x34, 0xA0, 0x78, 0x60
	};
	if (absolute) {
		static const uint8_t stub[] = { 0x4C, 0x40, 0x60 };
		memcpy(g->rom + 0x26, stub, sizeof(stub));
		memcpy(g->rom + 0x40, direct, sizeof(direct));
	} else {
		memcpy(g->rom + 0x26, direct, sizeof(direct));
	}
}

static bool checkBrk(struct Gamate* g, uint8_t p, uint8_t s,
		bool enabled, bool absolute) {
	gamateReset(g);
	g->cpu.pc = 0x6020;
	g->cpu.a = 0xC1;
	g->cpu.x = 0x47;
	g->cpu.y = 0x19;
	g->cpu.s = s;
	g->cpu.p = p;
	g->cpu.irqMask = (p & 4) != 0;
	g->cpu.stopped = false;
	g->protectionPassed = true;
	g->protectionFailed = false;
	g->ram[0x0C] = enabled;
	g->ram[0x0B] = 23;
	g->ram[0x0E] = g->ram[0x0F] = g->ram[0x10] = g->ram[0x11] = 0xFF;
	g->ram[0x300] = g->ram[0x301] = g->ram[0x302] = 0;
	g->irq = g->nmi = false;
	g->irqClocks = 0;

	g->rom[0x20] = 0x00;
	g->rom[0x21] = 0x5A;
	g->rom[0x22] = 0x4C;
	g->rom[0x23] = (uint8_t) RETURN;
	g->rom[0x24] = (uint8_t) (RETURN >> 8);
	installCapture(g, absolute);

	uint64_t brkStart = g->masterClocks;
	bool returned = false;
	for (unsigned instruction = 0; instruction < 1000; instruction++) {
		unsigned cycles = gamateCpuStep(g);
		gamateRunClocks(g, cycles * 2);
		if (g->cpu.pc == RETURN && g->cpu.s == s) {
			returned = true;
			break;
		}
	}

	uint8_t stackedP = (uint8_t) (p | 0x30);
	uint8_t finalP = (uint8_t) (p | 0x20) & (uint8_t) ~0x10;
	bool capture = !enabled ||
		s != 0xFF ||
		(g->ram[0x300] == stackedP &&
		 g->ram[0x301] == 0x22 && g->ram[0x302] == 0x60);
	bool ok = returned &&
		g->cpu.a == 0xC1 &&
		g->cpu.x == (enabled ? 0x34 : 0x47) &&
		g->cpu.y == 0x19 &&
		g->cpu.s == s &&
		g->cpu.p == finalP &&
		g->ram[0x0B] == 24 &&
		g->diagnostics.irqsTaken == 0 &&
		g->diagnostics.nmisTaken == 0 &&
		capture &&
		g->masterClocks - brkStart > 0;
	if (!ok) {
		fprintf(stderr,
			"BRK mismatch enabled=%u abs=%u p=%02X s=%02X returned=%u PC=%04X "
			"A=%02X X=%02X Y=%02X S=%02X P=%02X 300=%02X 301=%02X 302=%02X "
			"0B=%02X 0E=%02X 0F=%02X 10=%02X 11=%02X clocks=%llu\n",
			enabled, absolute, p, s, returned, g->cpu.pc, g->cpu.a, g->cpu.x, g->cpu.y,
			g->cpu.s, g->cpu.p, g->ram[0x300], g->ram[0x301], g->ram[0x302],
			g->ram[0x0B], g->ram[0x0E], g->ram[0x0F], g->ram[0x10], g->ram[0x11],
			(unsigned long long) (g->masterClocks - brkStart));
	}
	return ok;
}

int main(int argc, char** argv) {
	if (argc != 2) {
		fprintf(stderr, "usage: %s gamate_bios.bin\n", argv[0]);
		return 2;
	}
	size_t biosSize;
	uint8_t* bios = readFile(argv[1], &biosSize);
	if (!bios) return 2;
	uint8_t rom[0x4000];
	memset(rom, 0, sizeof(rom));
	struct Gamate g;
	gamateInit(&g);
	if (!gamateLoad(&g, rom, sizeof(rom), bios, biosSize)) {
		fprintf(stderr, "load failed bios=%zu rom=%zu\n", biosSize, sizeof(rom));
		return 2;
	}
	free(bios);
	gamateReset(&g);

	uint64_t cases = 0;
	for (unsigned pBits = 0; pBits < 64; pBits++) {
		uint8_t p = (uint8_t) (0x20 |
			((pBits & 1) ? 1 : 0) |
			((pBits & 2) ? 2 : 0) |
			((pBits & 4) ? 4 : 0) |
			((pBits & 8) ? 8 : 0) |
			((pBits & 16) ? 64 : 0) |
			((pBits & 32) ? 128 : 0));
		for (unsigned s = 0; s < 256; s++) {
			for (unsigned enabled = 0; enabled < 2; enabled++) {
				for (unsigned absolute = 0; absolute < 2; absolute++) {
					if (!checkBrk(&g, p, (uint8_t) s,
							enabled != 0, absolute != 0)) return 1;
					cases++;
				}
			}
		}
	}
	printf("BRK matrix passed: %llu cases\n", (unsigned long long) cases);
	return 0;
}
