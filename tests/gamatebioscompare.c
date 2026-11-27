/* Copyright (c) 2026 Ry
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* Diagnostic-only per-frame comparison of two authentic BIOSes and the independent SWK firmware.
 * It reports derived hashes and state summaries without retaining ROM, BIOS, RAM, VRAM or pixels. */
#include "gamate/gamate.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct Snapshot {
	uint32_t video, vram, ram;
	uint16_t pc, vramAddress;
	uint8_t a, x, y, s, p, plane, scrollX, scrollY, lcd, bank, firstBank;
	uint8_t ay[16];
	uint64_t irqs, ayWrites, lcdWrites, vramWrites, controllerReads;
};

static uint8_t* readFile(const char* path, size_t* size) {
	FILE* file = fopen(path, "rb");
	if (!file) return NULL;
	if (fseek(file, 0, SEEK_END)) { fclose(file); return NULL; }
	long length = ftell(file);
	if (length < 0 || fseek(file, 0, SEEK_SET)) { fclose(file); return NULL; }
	uint8_t* data = malloc((size_t) length);
	if (!data || fread(data, 1, (size_t) length, file) != (size_t) length) {
		free(data);
		data = NULL;
	}
	fclose(file);
	*size = (size_t) length;
	return data;
}

static uint32_t hash(const void* data, size_t size) {
	const uint8_t* bytes = data;
	uint32_t value = 2166136261u;
	for (size_t i = 0; i < size; i++) value = (value ^ bytes[i]) * 16777619u;
	return value;
}

static uint8_t buttonsForFrame(unsigned frame, bool controllerCycle) {
	static const struct { unsigned from, mask, count; } pulses[] = {
		{ 80, 0x01, 12 }, { 120, 0x02, 12 }, { 160, 0x04, 12 }, { 200, 0x08, 12 },
		{ 240, 0x10, 12 }, { 280, 0x20, 12 }, { 320, 0x40, 60 }, { 400, 0x80, 20 },
		{ 460, 0x11, 12 }, { 500, 0x18, 12 }, { 540, 0x21, 12 }, { 580, 0x28, 12 },
		{ 700, 0x40, 60 }, { 800, 0x10, 12 }, { 840, 0x20, 12 },
	};
	uint8_t buttons = 0xFF;
	if (!controllerCycle) return buttons;
	for (size_t i = 0; i < sizeof(pulses) / sizeof(pulses[0]); i++)
		if (frame >= pulses[i].from && frame < pulses[i].from + pulses[i].count)
			buttons &= (uint8_t) ~pulses[i].mask;
	return buttons;
}

static bool boot(struct Gamate* g) {
	for (unsigned i = 0; i < 20000000; i++) {
		unsigned cycles = gamateCpuStep(g);
		gamateRunClocks(g, cycles * 2);
		if (g->protectionPassed && g->cpu.pc == 0x6020) return true;
	}
	return false;
}

static void snapshot(const struct Gamate* g, struct Snapshot* s) {
	s->video = hash(g->frameBuffer, sizeof(g->frameBuffer));
	s->vram = hash(g->vram, sizeof(g->vram));
	s->ram = hash(g->ram, sizeof(g->ram));
	s->pc = g->cpu.pc;
	s->a = g->cpu.a;
	s->x = g->cpu.x;
	s->y = g->cpu.y;
	s->s = g->cpu.s;
	s->p = g->cpu.p;
	s->vramAddress = g->vramAddress;
	s->plane = g->vramPlane;
	s->scrollX = g->scrollX;
	s->scrollY = g->scrollY;
	s->lcd = (uint8_t) (g->incrementRow | g->window << 1 | g->swapPlanes << 2 |
		g->displayBlank << 3 | g->displayStopped << 4);
	s->bank = g->bank;
	s->firstBank = g->firstBank;
	memcpy(s->ay, g->ayRegisters, sizeof(s->ay));
	s->irqs = g->diagnostics.irqsTaken;
	s->ayWrites = g->diagnostics.ayWrites;
	s->lcdWrites = g->diagnostics.lcdWrites;
	s->vramWrites = g->diagnostics.vramWrites;
	s->controllerReads = g->diagnostics.controllerReads;
}

static bool cpuEqual(const struct Snapshot* a, const struct Snapshot* b) {
	return a->pc == b->pc && a->a == b->a && a->x == b->x && a->y == b->y &&
		a->s == b->s && a->p == b->p;
}

static bool hardwareEqual(const struct Snapshot* a, const struct Snapshot* b) {
	return a->vramAddress == b->vramAddress && a->plane == b->plane &&
		a->scrollX == b->scrollX && a->scrollY == b->scrollY && a->lcd == b->lcd &&
		a->bank == b->bank && a->firstBank == b->firstBank &&
		!memcmp(a->ay, b->ay, sizeof(a->ay));
}

static bool activityEqual(const struct Snapshot* a, const struct Snapshot* b) {
	return a->irqs == b->irqs && a->ayWrites == b->ayWrites &&
		a->lcdWrites == b->lcdWrites && a->vramWrites == b->vramWrites &&
		a->controllerReads == b->controllerReads;
}

int main(int argc, char** argv) {
	if (argc < 5 || argc > 7) {
		fprintf(stderr, "usage: %s cartridge umc_bios bit_bios swk_bios "
			"[no_input|controller_cycle] [frames=1000]\n", argv[0]);
		return 2;
	}
	bool controllerCycle = argc >= 6 && !strcmp(argv[5], "controller_cycle");
	if (argc >= 6 && strcmp(argv[5], "no_input") && !controllerCycle) {
		fprintf(stderr, "campaign must be no_input or controller_cycle\n");
		return 2;
	}
	unsigned frames = argc == 7 ? (unsigned) strtoul(argv[6], NULL, 0) : 1000;
	if (!frames) return 2;

	size_t romSize, biosSize[3];
	uint8_t* rom = readFile(argv[1], &romSize);
	uint8_t* bios[3];
	struct Gamate* g = calloc(3, sizeof(*g));
	for (unsigned i = 0; i < 3; i++) bios[i] = readFile(argv[2 + i], &biosSize[i]);
	if (!rom || !bios[0] || !bios[1] || !bios[2] || !g) {
		fprintf(stderr, "could not read inputs or allocate machines\n");
		free(rom); for (unsigned i = 0; i < 3; i++) free(bios[i]); free(g);
		return 2;
	}
	for (unsigned i = 0; i < 3; i++) {
		gamateInit(&g[i]);
		if (!gamateLoad(&g[i], rom, romSize, bios[i], biosSize[i]) || !boot(&g[i])) {
			fprintf(stderr, "machine %u did not reach cartridge entry\n", i);
			free(rom); for (unsigned j = 0; j < 3; j++) free(bios[j]); free(g);
			return 1;
		}
	}
	free(rom);
	for (unsigned i = 0; i < 3; i++) free(bios[i]);

	static const char* names[] = { "video", "vram", "ram", "cpu", "hardware", "activity" };
	unsigned firstPair[6] = { 0 }, firstSwk[6] = { 0 }, matchingFrames[6] = { 0 };
	bool previous[2] = { false, false };
	printf("# campaign=%s frames=%u audio_samples=excluded ay_phase=excluded master_clock=excluded\n",
		controllerCycle ? "controller_cycle" : "no_input", frames);
	printf("kind\tmetric\tvalue1\tvalue2\tvalue3\tvalue4\tvalue5\n");
	for (unsigned frame = 1; frame <= frames; frame++) {
		struct Snapshot s[3];
		for (unsigned i = 0; i < 3; i++) {
			g[i].buttons = buttonsForFrame(frame, controllerCycle);
			g[i].frameDone = false;
			while (!g[i].frameDone) {
				unsigned cycles = gamateCpuStep(&g[i]);
				gamateRunClocks(&g[i], cycles * 2);
			}
			snapshot(&g[i], &s[i]);
		}
		bool pair[6] = { s[0].video == s[1].video, s[0].vram == s[1].vram,
			s[0].ram == s[1].ram, cpuEqual(&s[0], &s[1]), hardwareEqual(&s[0], &s[1]),
			activityEqual(&s[0], &s[1]) };
		bool swk[6] = { s[2].video == s[0].video || s[2].video == s[1].video,
			s[2].vram == s[0].vram || s[2].vram == s[1].vram,
			s[2].ram == s[0].ram || s[2].ram == s[1].ram,
			cpuEqual(&s[2], &s[0]) || cpuEqual(&s[2], &s[1]),
			hardwareEqual(&s[2], &s[0]) || hardwareEqual(&s[2], &s[1]),
			activityEqual(&s[2], &s[0]) || activityEqual(&s[2], &s[1]) };
		for (unsigned i = 0; i < 6; i++) {
			if (!pair[i] && !firstPair[i]) firstPair[i] = frame;
			if (!swk[i] && !firstSwk[i]) firstSwk[i] = frame;
			if (swk[i]) matchingFrames[i]++;
		}
		if (frame == 1 || swk[0] != previous[0])
			printf("transition\tvideo\t%u\t%u\t%08X\t%08X\t%08X\n", frame, swk[0],
				s[0].video, s[1].video, s[2].video);
		if (frame == 1 || swk[1] != previous[1])
			printf("transition\tvram\t%u\t%u\t%08X\t%08X\t%08X\n", frame, swk[1],
				s[0].vram, s[1].vram, s[2].vram);
		previous[0] = swk[0]; previous[1] = swk[1];
	}
	printf("# summary values: authentic_pair_first_difference swk_first_difference "
		"swk_matching_frames\n");
	for (unsigned i = 0; i < 6; i++)
		printf("summary\t%s\t%u\t%u\t%u\t0\t0\n", names[i], firstPair[i],
			firstSwk[i], matchingFrames[i]);
	for (unsigned i = 0; i < 3; i++) gamateFree(&g[i]);
	free(g);
	return 0;
}
