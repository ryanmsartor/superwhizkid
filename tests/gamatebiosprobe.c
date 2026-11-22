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

struct ChangeSummary {
	unsigned count;
	uint16_t first, last;
};

enum { ROM_TRACE_LIMIT = 16384 };

struct RomTrace {
	size_t offsets[ROM_TRACE_LIMIT];
	uint16_t addresses[ROM_TRACE_LIMIT];
	size_t count, total;
};

struct RamTrace {
	unsigned reads, writes;
	uint8_t lastValue;
};

struct LcdTrace {
	uint8_t registers[ROM_TRACE_LIMIT], values[ROM_TRACE_LIMIT];
	size_t count, total;
};

static void captureLcdWrite(void* context, const struct Gamate* g,
		uint8_t reg, uint8_t value) {
	struct LcdTrace* trace = context;
	(void) g;
	if (trace->count < ROM_TRACE_LIMIT) {
		trace->registers[trace->count] = reg;
		trace->values[trace->count++] = reg == 1 || reg == 4 || reg == 5 ? value : 0;
	}
	trace->total++;
}

static void captureRomRead(void* context, const struct Gamate* g,
		uint16_t address, size_t physicalOffset) {
	struct RomTrace* trace = context;
	(void) g;
	if (trace->count < ROM_TRACE_LIMIT) {
		trace->addresses[trace->count] = address;
		trace->offsets[trace->count++] = physicalOffset;
	}
	trace->total++;
}

static void captureRamAccess(void* context, const struct Gamate* g, bool write,
		uint16_t address, uint8_t previous, uint8_t value) {
	struct RamTrace* trace = context;
	(void) g;
	(void) address;
	(void) previous;
	if (write) trace->writes++;
	else trace->reads++;
	trace->lastValue = value;
}

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

static bool coverageBit(const uint8_t* bits, unsigned offset) {
	return bits[offset >> 3] & ((uint8_t) 1 << (offset & 7));
}

static void printVramCoverageRuns(const char* label, const uint8_t* bits) {
	printf("# %s:", label);
	for (unsigned plane = 0; plane < 2; plane++) {
		for (unsigned address = 0; address < 0x2000;) {
			if (!coverageBit(bits, (address << 1) | plane)) {
				address++;
				continue;
			}
			unsigned first = address;
			do address++;
			while (address < 0x2000 && coverageBit(bits, (address << 1) | plane));
			if (address == first + 1) printf(" P%u:%04X", plane, first);
			else printf(" P%u:%04X-%04X", plane, first, address - 1);
		}
	}
	putchar('\n');
}

static void printRomCoverageRuns(const struct Gamate* g) {
	printf("# bios_rom_reads: total=%llu unique=%u runs=",
		(unsigned long long) g->diagnostics.romTrackedReads, g->diagnostics.romUniqueRead);
	for (size_t offset = 0; offset < g->romSize;) {
		if (!coverageBit(g->diagnostics.romRead, (unsigned) offset)) {
			offset++;
			continue;
		}
		size_t first = offset;
		do offset++;
		while (offset < g->romSize && coverageBit(g->diagnostics.romRead, (unsigned) offset));
		if (offset == first + 1) printf(" %05zX", first);
		else printf(" %05zX-%05zX", first, offset - 1);
	}
	putchar('\n');
}

static void printRomSequence(const struct RomTrace* trace) {
	printf("# bios_rom_sequence: captured=%zu total=%zu offsets=", trace->count, trace->total);
	for (size_t i = 0; i < trace->count; i++) printf(" %05zX", trace->offsets[i]);
	if (trace->count < trace->total) printf(" ...");
	printf("\n# bios_rom_sequence_cpu:");
	for (size_t i = 0; i < trace->count; i++) printf(" %04X", trace->addresses[i]);
	putchar('\n');
}

static void printVramBytes(const struct Gamate* g) {
	for (unsigned address = 0; address < 0x2000; address++) {
		for (unsigned plane = 0; plane < 2; plane++) {
			unsigned offset = (address << 1) | plane;
			if (!coverageBit(g->diagnostics.vramWritten, offset)) continue;
			printf("# vram_byte: plane=%u address=%04X value=%02X\n",
				plane, address, g->vram[offset]);
		}
	}
}

static void printRomTrace(const struct RomTrace* trace) {
	printf("# bios_rom_trace: captured=%zu total=%zu runs=", trace->count, trace->total);
	for (size_t i = 0; i < trace->count;) {
		size_t length = 1;
		while (i + length < trace->count &&
				trace->offsets[i + length] == trace->offsets[i + length - 1] + 1) length++;
		size_t repeats = 1;
		while (i + (repeats + 1) * length <= trace->count &&
				!memcmp(&trace->offsets[i], &trace->offsets[i + repeats * length],
					length * sizeof(trace->offsets[0]))) repeats++;
		if (length == 1) printf(" %05zX", trace->offsets[i]);
		else printf(" %05zX-%05zX", trace->offsets[i], trace->offsets[i + length - 1]);
		if (repeats > 1) printf("x%zu", repeats);
		i += length * repeats;
	}
	if (trace->count < trace->total) printf(" ...");
	putchar('\n');
}

static void printLcdTrace(const struct LcdTrace* trace) {
	printf("# lcd_trace: captured=%zu total=%zu events=", trace->count, trace->total);
	for (size_t i = 0; i < trace->count;) {
		uint8_t reg = trace->registers[i];
		if (reg == 7) {
			size_t end = i + 1;
			while (end < trace->count && trace->registers[end] == 7) end++;
			printf(" 7x%zu", end - i);
			i = end;
		} else {
			if (reg == 1 || reg == 4 || reg == 5) printf(" %u:%02X", reg, trace->values[i]);
			else printf(" %u", reg);
			i++;
		}
	}
	if (trace->count < trace->total) printf(" ...");
	putchar('\n');
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
	// These perturb only the probe's private loaded copies; source files are never modified.
	if (nameLength == 11 && !memcmp(option, "romxor", 6)) {
		char offsetText[6] = {
			option[6], option[7], option[8], option[9], option[10], 0
		};
		unsigned long offset;
		if (!parseHex(offsetText, GAMATE_MAX_ROM_SIZE - 1, &offset) ||
				offset >= g->romSize || value > 0xFF) return false;
		g->rom[offset] ^= (uint8_t) value;
		return true;
	}
	if (nameLength == 9 && !memcmp(option, "ramxor", 6)) {
		char addressText[4] = { option[6], option[7], option[8], 0 };
		unsigned long address;
		if (!parseHex(addressText, 0x3FF, &address) || value > 0xFF) return false;
		g->ram[address] ^= (uint8_t) value;
		return true;
	}
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
		fprintf(stderr, "usage: %s gamate_bios.bin cartridge.bin SERVICE "
			"[occurrence=HEX] [rom_trace] [rom_sequence] [lcd_trace] [vram_bytes] [vram_reads] "
			"[ram_watch=HEX] [NAME=HEX ...]\n",
			argv[0]);
		return 2;
	}
	unsigned long service;
	if (!parseHex(argv[3], 0xFFFF, &service) || service < 0xF000 || service > 0xF033 ||
			(service - 0xF000) % 3) {
		fprintf(stderr, "service must be a slot from F000 through F033\n");
		return 2;
	}
	unsigned long occurrence = 0;
	unsigned long ramWatchAddress = 0;
	bool romTraceEnabled = false;
	bool romSequenceEnabled = false;
	bool lcdTraceEnabled = false;
	bool vramBytesEnabled = false;
	bool vramReadsEnabled = false;
	bool ramWatchEnabled = false;
	for (int i = 4; i < argc; i++) {
		if (!strcmp(argv[i], "rom_trace")) {
			romTraceEnabled = true;
			continue;
		}
		if (!strcmp(argv[i], "rom_sequence")) {
			romTraceEnabled = romSequenceEnabled = true;
			continue;
		}
		if (!strcmp(argv[i], "lcd_trace")) {
			lcdTraceEnabled = true;
			continue;
		}
		if (!strcmp(argv[i], "vram_bytes")) {
			vramBytesEnabled = true;
			continue;
		}
		if (!strcmp(argv[i], "vram_reads")) {
			vramReadsEnabled = true;
			continue;
		}
		if (!strncmp(argv[i], "ram_watch=", 10)) {
			if (ramWatchEnabled || !parseHex(argv[i] + 10, 0x3FF, &ramWatchAddress)) {
				fprintf(stderr, "ram_watch must be one hexadecimal RAM address\n");
				return 2;
			}
			ramWatchEnabled = true;
			continue;
		}
		if (strncmp(argv[i], "occurrence=", 11)) continue;
		if (occurrence || !parseHex(argv[i] + 11, 0xFFFF, &occurrence) || !occurrence) {
			fprintf(stderr, "occurrence must be a nonzero hexadecimal call number\n");
			return 2;
		}
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
	unsigned frame = 1;
	const char* press = getenv("GAMATEBIOSCALLS_PRESS");
	g.frameDone = false;
	g.buttons = gamateBiosButtonsForFrame(frame, press);
	if (occurrence) {
		unsigned long seen = 0;
		for (uint64_t instruction = 0; instruction < 100000000; instruction++) {
			if (g.frameDone) {
				g.frameDone = false;
				g.buttons = gamateBiosButtonsForFrame(++frame, press);
			}
			unsigned cycles = gamateCpuStep(&g);
			gamateRunClocks(&g, cycles * 2);
			if (g.cpu.pc == service && ++seen == occurrence) break;
		}
		if (g.cpu.pc != service || seen != occurrence) {
			fprintf(stderr, "cartridge did not reach service occurrence %lX\n", occurrence);
			gamateFree(&g);
			return 1;
		}
	}
	for (int i = 4; i < argc; i++) {
		if (!strncmp(argv[i], "occurrence=", 11) || !strcmp(argv[i], "rom_trace") ||
				!strcmp(argv[i], "rom_sequence") || !strcmp(argv[i], "lcd_trace") ||
				!strcmp(argv[i], "vram_bytes") ||
				!strcmp(argv[i], "vram_reads") ||
				!strncmp(argv[i], "ram_watch=", 10)) continue;
		if (!applyOption(&g, argv[i])) {
			fprintf(stderr, "invalid probe option: %s\n", argv[i]);
			gamateFree(&g);
			return 2;
		}
	}
	uint8_t entryA = g.cpu.a, entryX = g.cpu.x, entryY = g.cpu.y;
	uint8_t returnS = occurrence ? (uint8_t) (g.cpu.s + 2) : g.cpu.s;
	uint8_t entryS = returnS, entryP = g.cpu.p;
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
	uint16_t returnAddress;
	if (occurrence) {
		uint16_t stacked = g.ram[0x100 | (uint8_t) (g.cpu.s + 1)] |
			(uint16_t) g.ram[0x100 | (uint8_t) (g.cpu.s + 2)] << 8;
		returnAddress = stacked + 1;
	} else {
		returnAddress = 0x6020;
		uint16_t stacked = returnAddress - 1;
		g.ram[0x100 | entryS] = (uint8_t) (stacked >> 8);
		g.ram[0x100 | (uint8_t) (entryS - 1)] = (uint8_t) stacked;
		g.cpu.s = entryS - 2;
		g.cpu.pc = (uint16_t) service;
	}
	gamateResetRamDiagnostics(&g);
	gamateResetVramDiagnostics(&g);
	gamateResetRomDiagnostics(&g);
	struct RomTrace romTrace = { 0 };
	struct RamTrace ramTrace = { 0 };
	struct LcdTrace lcdTrace = { 0 };
	if (romTraceEnabled) gamateSetRomDiagnosticWatch(&g, captureRomRead, &romTrace);
	if (lcdTraceEnabled) gamateSetLcdDiagnosticWatch(&g, captureLcdWrite, &lcdTrace);
	if (ramWatchEnabled) gamateSetRamDiagnosticWatch(&g, (uint16_t) ramWatchAddress,
		captureRamAccess, &ramTrace);
	g.irqClocks = 0;
	g.irq = false;
	bool returned = false;
	for (uint64_t instruction = 0; instruction < 10000000; instruction++) {
		unsigned cycles = gamateCpuStep(&g);
		gamateRunClocks(&g, cycles * 2);
		if (g.cpu.pc == returnAddress && g.cpu.s == returnS) {
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
	printVramCoverageRuns("vram_written_runs", g.diagnostics.vramWritten);
	if (vramReadsEnabled) {
		printf("# vram_reads: total=%llu before_write=%llu\n",
			(unsigned long long) g.diagnostics.vramReads,
			(unsigned long long) g.diagnostics.vramReadBeforeWrite);
		printVramCoverageRuns("vram_read_before_write_runs",
			g.diagnostics.vramReadBeforeWriteMap);
	}
	printRomCoverageRuns(&g);
	if (romTraceEnabled) printRomTrace(&romTrace);
	if (romSequenceEnabled) printRomSequence(&romTrace);
	if (lcdTraceEnabled) printLcdTrace(&lcdTrace);
	if (vramBytesEnabled) printVramBytes(&g);
	if (ramWatchEnabled) {
		printf("# ram_watch: address=%03lX reads=%u writes=%u final=%02X\n",
			ramWatchAddress, ramTrace.reads, ramTrace.writes, ramTrace.lastValue);
	}
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
