/* SPDX-License-Identifier: GPL-2.0-or-later */

/* Exercise the diagnostic's static policy without loading firmware. */
#define main gamateboot_main
#include "gamateboot.c"
#undef main

#include <assert.h>

int main(void) {
	assert(!experimentalBiosTrap(0xF000));
	assert(!strcmp(experimentalBiosTrap(0xF036), "unsupported BIOS service or link NMI"));
	assert(!strcmp(experimentalBiosTrap(0xF039), "cartridge protection failed"));
	assert(!strcmp(experimentalBiosTrap(0xF03C), "cartridge identity failed"));
	assert(!strcmp(experimentalBiosTrap(0xF03F), "cartridge checksum failed"));
	assert(!reportExperimentalBiosTrap(false, 0xF036, 0xF027));
	struct Gamate* g = calloc(1, sizeof(*g));
	assert(g);
	memset(g->ram, 0xA5, sizeof(g->ram));
	assert(!setenv("GAMATEBOOT_ENTRY_RAM", "semantic-ones", 1));
	assert(!setenv("GAMATEBOOT_ENTRY_IRQ_COUNTER", "12345678", 1));
	assert(!setenv("GAMATEBOOT_ENTRY_LFSR", "9ABC", 1));
	assert(perturbEntryRam(g));
	for (size_t i = 0x20; i < sizeof(g->ram); i++) assert(g->ram[i] == 0xFF);
	assert(g->ram[0x004] == 0x47 && g->ram[0x00C] == 1);
	for (size_t i = 0x005; i <= 0x00A; i++) assert(g->ram[i] == 0xFF);
	assert(g->ram[0x00E] == 0x12 && g->ram[0x00F] == 0x34);
	assert(g->ram[0x010] == 0x56 && g->ram[0x011] == 0x78);
	assert(g->ram[0x012] == 0xBC && g->ram[0x013] == 0x9A);
	assert(g->ram[0x000] == 0 && g->ram[0x01F] == 0);

	memset(g->ram, 0xA5, sizeof(g->ram));
	assert(!setenv("GAMATEBOOT_ENTRY_RAM", "semantic-ones-keep-block-03", 1));
	assert(perturbEntryRam(g));
	for (size_t i = 0x20; i < sizeof(g->ram); i++)
		assert(g->ram[i] == (i >= 0xC0 && i < 0x100 ? 0xA5 : 0xFF));

	memset(g->ram, 0xA5, sizeof(g->ram));
	assert(!setenv("GAMATEBOOT_ENTRY_RAM", "semantic-ones-keep-block-00", 1));
	assert(perturbEntryRam(g));
	assert(g->ram[0x004] == 0x47 && g->ram[0x01F] == 0);
	for (size_t i = 0x20; i < 0x40; i++) assert(g->ram[i] == 0xA5);

	memset(g->ram, 0xA5, sizeof(g->ram));
	assert(!setenv("GAMATEBOOT_ENTRY_RAM", "semantic-ones-keep-chunk-02", 1));
	assert(perturbEntryRam(g));
	for (size_t i = 0x20; i < sizeof(g->ram); i++)
		assert(g->ram[i] == (i < 0x30 ? 0xA5 : 0xFF));

	memset(g->ram, 0xA5, sizeof(g->ram));
	assert(!setenv("GAMATEBOOT_ENTRY_RAM", "semantic-ones-keep-byte-021", 1));
	assert(perturbEntryRam(g));
	for (size_t i = 0x20; i < sizeof(g->ram); i++)
		assert(g->ram[i] == (i == 0x21 ? 0xA5 : 0xFF));

	memset(g->ram, 0xA5, sizeof(g->ram));
	assert(!setenv("GAMATEBOOT_ENTRY_RAM", "semantic-ones-zero-021", 1));
	assert(perturbEntryRam(g));
	for (size_t i = 0x20; i < sizeof(g->ram); i++)
		assert(g->ram[i] == (i == 0x21 ? 0 : 0xFF));

	memset(g->ram, 0xA5, sizeof(g->ram));
	assert(!setenv("GAMATEBOOT_ENTRY_RAM", "semantic-low32", 1));
	assert(perturbEntryRam(g));
	for (size_t i = 0x20; i < sizeof(g->ram); i++) assert(g->ram[i] == 0xA5);

	memset(g->ram, 0xA5, sizeof(g->ram));
	assert(!setenv("GAMATEBOOT_ENTRY_RAM", "semantic-ones", 1));
	assert(!setenv("GAMATEBOOT_ENTRY_LFSR", "0", 1));
	assert(!perturbEntryRam(g));
	for (size_t i = 0; i < sizeof(g->ram); i++) assert(g->ram[i] == 0xA5);
	assert(!setenv("GAMATEBOOT_ENTRY_LFSR", "9ABC", 1));
	assert(!setenv("GAMATEBOOT_ENTRY_RAM", "semantic-ones-keep-block-10", 1));
	assert(!perturbEntryRam(g));
	for (size_t i = 0; i < sizeof(g->ram); i++) assert(g->ram[i] == 0xA5);
	free(g);
	return 0;
}
