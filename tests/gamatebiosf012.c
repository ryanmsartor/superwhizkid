/* Copyright (c) 2026 Ry
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
/* Diagnostic-only corpus check for the observed BIOS $F012 tile-run contract. It runs a cartridge
 * for the standard post-entry window, compares every uninterrupted $F012 call with an independent
 * behavioral model, and reports only derived counts. No ROM or BIOS bytes are printed. */
#include "gamate/gamate.h"
#include "gamatebiosinput.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { MAX_READS = 8192, MAX_WRITES = 4096, SELECTOR_CLASSES = 6 };

struct Call {
	bool active;
	uint16_t expectedReturn, pointer;
	uint8_t s, x, y, p, lcd, width;
	uint8_t selectors[256];
	uint64_t startClocks, irqs, nmis;
	uint16_t reads[MAX_READS];
	size_t readCount;
	uint16_t writeOffsets[MAX_WRITES];
	uint8_t writeValues[MAX_WRITES];
	size_t writeCount;
	bool multipleWrites;
};

struct Totals {
	unsigned long calls, interrupted, checked, descriptorRom, descriptorRam, descriptorOther;
	unsigned long readMismatch, trailingOther, writeCount, writeAddress;
	unsigned long registers, status, carryOverflow, clockMismatch, copyMismatch, decimalEntry;
	bool inputSeen[256];
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

static size_t cartridgeOffset(const struct Gamate* g, uint16_t address) {
	size_t offset = address - 0x6000;
	if (g->mapper == GAMATE_MAPPER_4IN1 && offset < 0x4000) {
		offset = (size_t) g->firstBank * 0x4000 + offset;
	} else if (g->mapper != GAMATE_MAPPER_PLAIN && offset >= 0x4000) {
		offset = (size_t) g->bank * 0x4000 + (offset & 0x3FFF);
	}
	return offset < g->romSize ? offset : offset % g->romSize;
}

static uint8_t peek(const struct Gamate* g, uint16_t address) {
	if (address < 0x2000) return g->ram[address & 0x3FF];
	if (isCartridge(address)) return g->rom[cartridgeOffset(g, address)];
	if (address >= 0xE000) return g->bios[address & 0x0FFF];
	return 0;
}

/* Observed selector groups. Each group indexes eight-byte glyph rows in a fixed cartridge window;
 * group base page also determines which rows take an indexed page-crossing dummy read. */
static unsigned selectorClass(uint8_t selector) {
	if (selector < 0x20) return 0;
	if (selector == 0x20) return 1;
	if (selector <= 0x2E) return 2;
	if (selector <= 0x40) return 3;
	if (selector <= 0x60) return 4;
	return 5;
}

static unsigned tiles(const struct Call* call) {
	return call->width ? call->width : 256u;
}

/* The descriptor index is eight bits wide, so tile 255 of a width-zero run rereads the width. */
static uint16_t descriptorAddress(uint16_t pointer, unsigned tile) {
	return (uint16_t) (pointer + ((1 + tile) & 0xFF));
}

static uint16_t glyphBase(uint8_t selector) {
	if (selector <= 0x40) return 0x6021;
	if (selector <= 0x60) return 0x6079;
	return 0x6149;
}

static unsigned glyphIndex(uint8_t selector) {
	if (selector <= 0x20) return 1;
	if (selector <= 0x40) return ((selector - 0x2F) & 0x1F) + 1u;
	return ((selector - 1) & 0x1F) + 1u;
}

static uint16_t glyphAddress(uint8_t selector) {
	return (uint16_t) (glyphBase(selector) + 8 * glyphIndex(selector));
}

static unsigned crossingRows(uint8_t selector) {
	unsigned rows = 0;
	for (unsigned row = 0; row < 8; row++) {
		rows += ((glyphAddress(selector) + row) >> 8) != (glyphBase(selector) >> 8);
	}
	return rows;
}

static void captureRomRead(void* context, const struct Gamate* g, uint16_t address,
		size_t offset) {
	struct Call* call = context;
	(void) g;
	(void) offset;
	if (call->active && call->readCount < MAX_READS) call->reads[call->readCount++] = address;
}

static size_t predictReads(const struct Call* call, uint16_t* out) {
	size_t count = 0;
	bool romDescriptor = isCartridge(call->pointer);
	if (romDescriptor) out[count++] = call->pointer;
	for (unsigned tile = 0; tile < tiles(call); tile++) {
		uint8_t selector = call->selectors[tile];
		if (romDescriptor) {
			uint16_t descriptor = descriptorAddress(call->pointer, tile);
			if ((descriptor >> 8) != (call->pointer >> 8)) {
				out[count++] = (uint16_t) (descriptor - 0x100);
			}
			out[count++] = descriptor;
		}
		uint16_t base = glyphBase(selector), address = glyphAddress(selector);
		for (unsigned row = 0; row < 8; row++) {
			uint16_t data = (uint16_t) (address + row);
			if ((data >> 8) != (base >> 8)) out[count++] = (uint16_t) (data - 0x100);
			out[count++] = data;
		}
	}
	return count;
}

static unsigned tileClocks(uint8_t selector) {
	static const unsigned cost[SELECTOR_CLASSES] = { 440, 428, 456, 456, 440, 426 };
	return cost[selectorClass(selector)] + 2 * crossingRows(selector);
}

static uint64_t predictClocks(const struct Call* call) {
	uint64_t clocks = 68;
	for (unsigned tile = 0; tile < tiles(call); tile++) {
		clocks += tileClocks(call->selectors[tile]);
		clocks += 2 * ((descriptorAddress(call->pointer, tile) >> 8) != (call->pointer >> 8));
	}
	return clocks;
}

/* Carry depends only on the final selector. Overflow is updated by every selector above $20 and
 * otherwise carried from the previous tile or the caller's entry flags. */
static uint8_t predictCarryOverflow(const struct Call* call) {
	bool overflow = call->p & 0x40, carry = call->p & 0x01;
	for (unsigned tile = 0; tile < tiles(call); tile++) {
		uint8_t selector = call->selectors[tile];
		carry = (selector >= 0x20 && selector <= 0x2E) || (selector >= 0x81 && selector <= 0xA0) ||
			(selector >= 0xC1 && selector <= 0xE0);
		if (selector > 0x20) overflow = selector >= 0x80 && selector <= 0xE0;
	}
	return (uint8_t) ((carry ? 0x01 : 0) | (overflow ? 0x40 : 0));
}

struct Outcome {
	bool reads, trailing, writeCount, writeAddress, registers, status, carryOverflow, clocks;
	unsigned plane;
};

static struct Outcome evaluate(struct Totals* t, const struct Call* call, const struct Gamate* g,
		uint64_t clocks) {
	struct Outcome o = { 0 };
	static uint16_t predicted[MAX_READS];
	size_t predictedCount = predictReads(call, predicted);
	o.reads = predictedCount <= call->readCount &&
		!memcmp(predicted, call->reads, predictedCount * sizeof(*predicted));
	o.trailing = o.reads && call->readCount == predictedCount + 1 &&
		call->reads[predictedCount] == (uint16_t) (call->expectedReturn - 1);
	o.writeCount = call->writeCount == 8u * tiles(call) && !call->multipleWrites;
	o.writeAddress = o.writeCount;
	o.plane = o.writeCount && call->writeCount ? call->writeOffsets[0] & 1 : 0;
	for (size_t i = 0; o.writeCount && i < call->writeCount; i++) {
		unsigned tile = (unsigned) (i / 8), row = (unsigned) (i % 8);
		/* An eight-bit column counter starts at $80 + (X >> 3); bit 7 selects the plane. */
		uint8_t column = (uint8_t) (0x80 + (call->x >> 3) + tile);
		uint16_t address = (uint16_t) ((((call->y + row) & 0xFF) << 5) | (column & 0x1F));
		if (call->writeOffsets[i] != (uint16_t) ((address << 1) | (column >> 7))) {
			o.writeAddress = false;
		}
		uint8_t input = peek(g, (uint16_t) (glyphAddress(call->selectors[tile]) + row));
		uint8_t output = call->writeValues[i];
		if (!o.reads) continue;
		t->copyMismatch += input != output;
		t->inputSeen[input] = true;
	}
	uint8_t last = call->selectors[tiles(call) - 1];
	o.registers = g->cpu.a == call->width && g->cpu.y == call->width &&
		g->cpu.x == (uint8_t) (8 * glyphIndex(last)) && g->cpu.s == call->s;
	o.status = (g->cpu.p & 0x8E) == ((call->p & 0x0C) | 0x02);
	o.carryOverflow = (g->cpu.p & 0x41) == predictCarryOverflow(call);
	o.clocks = clocks == predictClocks(call);
	return o;
}

static void tally(struct Totals* t, const struct Outcome* o) {
	t->readMismatch += !o->reads;
	t->trailingOther += o->reads && !o->trailing;
	t->writeCount += !o->writeCount;
	t->writeAddress += o->writeCount && !o->writeAddress;
	t->registers += !o->registers;
	t->status += !o->status;
	t->carryOverflow += !o->carryOverflow;
	t->clockMismatch += !o->clocks;
}

static void beginCall(struct Call* call, const struct Gamate* g, uint16_t expectedReturn,
		uint8_t s, uint8_t p, uint64_t startClocks) {
	uint16_t pointer = (uint16_t) (g->ram[0] | g->ram[1] << 8);
	*call = (struct Call) {
		.active = true, .expectedReturn = expectedReturn, .pointer = pointer, .s = s,
		.x = g->cpu.x, .y = g->cpu.y, .p = p, .lcd = g->ram[0x15], .width = peek(g, pointer),
		.startClocks = startClocks,
		.irqs = g->diagnostics.irqsTaken, .nmis = g->diagnostics.nmisTaken,
	};
	for (unsigned i = 0; i < tiles(call); i++) {
		call->selectors[i] = peek(g, descriptorAddress(pointer, i));
	}
}

/* Executes one instruction, capturing VRAM writes for an active call. Returns true when the active
 * call returns to its caller. When detect is set, a cartridge JSR/JMP to $F012 starts a call. */
static bool step(struct Gamate* g, struct Call* call, bool detect) {
	uint16_t source = g->cpu.pc;
	uint8_t opcode = peek(g, source);
	uint8_t inS = g->cpu.s, inP = g->cpu.p;
	uint16_t writeOffset = (uint16_t) ((g->vramAddress << 1) | (g->vramPlane ? 1 : 0));
	uint64_t oldWrites = g->diagnostics.vramWrites;
	unsigned cycles = gamateCpuStep(g);
	uint16_t target = g->cpu.pc;
	if (call->active && g->diagnostics.vramWrites != oldWrites) {
		if (g->diagnostics.vramWrites != oldWrites + 1) call->multipleWrites = true;
		if (call->writeCount < MAX_WRITES) {
			call->writeOffsets[call->writeCount] = writeOffset;
			call->writeValues[call->writeCount++] = g->vram[writeOffset];
		}
	}
	bool returned = call->active && source >= 0xE000 && opcode == 0x60 &&
		target == call->expectedReturn && g->cpu.s == call->s;
	if (detect && !call->active && isCartridge(source) && target == 0xF012 &&
			(opcode == 0x20 || opcode == 0x4C)) {
		uint16_t expectedReturn = opcode == 0x20 ? (uint16_t) (source + 3) :
			(uint16_t) ((g->ram[0x100 | (uint8_t) (inS + 1)] |
				g->ram[0x100 | (uint8_t) (inS + 2)] << 8) + 1);
		beginCall(call, g, expectedReturn, opcode == 0x20 ? inS : (uint8_t) (inS + 2), inP,
			g->masterClocks + cycles * 2);
	}
	gamateRunClocks(g, cycles * 2);
	if (returned) call->active = false;
	return returned;
}

static void corpusCall(struct Totals* t, const struct Call* call, const struct Gamate* g,
		FILE* features, const char* title, const char* bios) {
	t->calls++;
	if (g->diagnostics.irqsTaken != call->irqs || g->diagnostics.nmisTaken != call->nmis) {
		t->interrupted++;
		return;
	}
	t->checked++;
	if (isCartridge(call->pointer)) t->descriptorRom++;
	else if (call->pointer < 0x2000) t->descriptorRam++;
	else t->descriptorOther++;
	uint64_t clocks = g->masterClocks - call->startClocks;
	struct Outcome o = evaluate(t, call, g, clocks);
	tally(t, &o);
	t->decimalEntry += (call->p & 0x08) != 0;
	if (features) {
		unsigned classes[SELECTOR_CLASSES] = { 0 }, crossings = 0, descriptorCrossings = 0;
		for (unsigned tile = 0; tile < tiles(call); tile++) {
			classes[selectorClass(call->selectors[tile])]++;
			crossings += crossingRows(call->selectors[tile]);
			descriptorCrossings += (descriptorAddress(call->pointer, tile) >> 8) != (call->pointer >> 8);
		}
		fprintf(features, "%s\t%s\t%c\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t%02X\t%02X\t%02X"
			"\t%02X\t%02X\t%llu\t%u\t%u\n",
			bios, title, isCartridge(call->pointer) ? 'R' : 'M', call->width,
			classes[0], classes[1], classes[2], classes[3], classes[4], classes[5], crossings,
			descriptorCrossings, call->x, call->y, call->lcd, call->p, g->cpu.p,
			(unsigned long long) clocks, o.reads, o.writeAddress);
	}
}

static void printTotals(const char* bios, const char* title, const struct Totals* t) {
	unsigned mapped = 0;
	for (unsigned i = 0; i < 256; i++) mapped += t->inputSeen[i];
	printf("%s\t%s\tcalls=%lu\tinterrupted=%lu\tchecked=%lu\trom_descriptor=%lu\t"
		"ram_descriptor=%lu\tother_descriptor=%lu\tread_mismatch=%lu\ttrailing_other=%lu\t"
		"write_count=%lu\twrite_address=%lu\tinput_values=%u\t"
		"registers=%lu\tstatus=%lu\tcarry_overflow=%lu\tclocks=%lu\tcopy_mismatch=%lu\t"
		"decimal_entry=%lu\n",
		bios, title, t->calls, t->interrupted, t->checked, t->descriptorRom, t->descriptorRam,
		t->descriptorOther, t->readMismatch, t->trailingOther, t->writeCount, t->writeAddress,
		mapped, t->registers, t->status, t->carryOverflow, t->clockMismatch,
		t->copyMismatch, t->decimalEntry);
}

struct Case {
	uint16_t pointer;
	uint8_t width, x, y, p, lcd;
	const uint8_t* selectors;
};

/* Runs one synthetic call from the authentic entry snapshot with a RAM-resident descriptor. */
static bool runCase(struct Gamate* g, const struct Gamate* base, struct Call* call,
		const struct Case* c, struct Outcome* outcome, struct Totals* t) {
	*g = *base;
	g->ram[0] = (uint8_t) c->pointer;
	g->ram[1] = (uint8_t) (c->pointer >> 8);
	g->ram[c->pointer & 0x3FF] = c->width;
	for (unsigned i = 0; i < (c->width ? c->width : 256u); i++) {
		g->ram[(c->pointer + 1 + i) & 0x3FF] = c->selectors[i];
	}
	g->ram[0x15] = c->lcd;
	g->cpu.x = c->x;
	g->cpu.y = c->y;
	g->cpu.p = c->p;
	g->cpu.irqMask = c->p & 0x04;
	uint16_t expectedReturn = (uint16_t) ((g->ram[0x100 | (uint8_t) (g->cpu.s + 1)] |
		g->ram[0x100 | (uint8_t) (g->cpu.s + 2)] << 8) + 1);
	beginCall(call, g, expectedReturn, (uint8_t) (g->cpu.s + 2), c->p, g->masterClocks);
	for (unsigned long i = 0; i < 1000000; i++) {
		if (step(g, call, false)) {
			t->calls++;
			if (g->diagnostics.irqsTaken != call->irqs || g->diagnostics.nmisTaken != call->nmis) {
				t->interrupted++;
				return false;
			}
			t->checked++;
			*outcome = evaluate(t, call, g, g->masterClocks - call->startClocks);
			return true;
		}
	}
	call->active = false;
	return false;
}

static int synthetic(struct Gamate* g, struct Call* call, const char* biosName) {
	const char* press = getenv("GAMATEBIOSCALLS_PRESS");
	bool found = false;
	for (unsigned frame = 1; frame <= 1000 && !found; frame++) {
		g->buttons = gamateBiosButtonsForFrame(frame, press);
		g->frameDone = false;
		while (!g->frameDone && !found) {
			step(g, call, false);
			found = g->cpu.pc == 0xF012;
		}
	}
	if (!found) {
		fprintf(stderr, "no $F012 call in the standard window\n");
		return 1;
	}
	struct Gamate* base = malloc(sizeof(*base));
	struct Totals* t = calloc(1, sizeof(*t));
	if (!base || !t) return 2;
	*base = *g;
	uint8_t authenticLcd = g->ram[0x15];
	uint8_t selectors[256];
	memset(selectors, 0, sizeof(selectors));
	struct Outcome o;
	unsigned long failures[8] = { 0 };
	bool verbose = getenv("GAMATEBIOSF012_VERBOSE") != NULL;
	unsigned long verboseLimit = verbose ? strtoul(getenv("GAMATEBIOSF012_VERBOSE"), NULL, 0) : 0;
#define CHECK(name, ok) do { \
		if (!(ok)) { \
			if (verbose && failures[name] < verboseLimit) printf("# fail %s w=%u ptr=%04X x=%02X y=%02X " \
				"p=%02X sel0=%02X sel1=%02X out_p=%02X out_x=%02X writes=%zu\n", #name, c.width, \
				c.pointer, c.x, c.y, c.p, selectors[0], selectors[1], g->cpu.p, g->cpu.x, \
				call->writeCount); \
			failures[name]++; \
		} \
	} while (0)
	enum { F_READS, F_WRITES, F_REGS, F_STATUS, F_CV, F_CLOCKS, F_RUN };
	struct Case c = { 0x0300, 1, 0x28, 0x58, 0x24, authenticLcd, selectors };
	static const uint8_t flags[] = { 0x20, 0x21, 0x24, 0x25, 0x60, 0x61, 0x64, 0x65, 0xA4, 0xE7 };

	/* All single selectors with varied entry flags (decimal mode clear). */
	for (size_t f = 0; f < sizeof(flags); f++) {
		for (unsigned s = 0; s < 256; s++) {
			selectors[0] = (uint8_t) s;
			c.p = flags[f];
			if (!runCase(g, base, call, &c, &o, t)) { failures[F_RUN]++; continue; }
			CHECK(F_READS, o.reads && o.trailing);
			CHECK(F_WRITES, o.writeCount && o.writeAddress);
			CHECK(F_REGS, o.registers);
			CHECK(F_STATUS, o.status);
			CHECK(F_CV, o.carryOverflow);
			CHECK(F_CLOCKS, o.clocks);
		}
	}
	printf("%s\tsingle_selectors\tcases=%zu\treads=%lu\twrites=%lu\tregisters=%lu\tstatus=%lu\t"
		"carry_overflow=%lu\tclocks=%lu\tunfinished=%lu\n", biosName, sizeof(flags) * 256,
		failures[F_READS], failures[F_WRITES], failures[F_REGS], failures[F_STATUS],
		failures[F_CV], failures[F_CLOCKS], failures[F_RUN]);

	/* All ordered selector pairs. */
	memset(failures, 0, sizeof(failures));
	c.p = 0x24;
	c.width = 2;
	for (unsigned pair = 0; pair < 0x10000; pair++) {
		selectors[0] = (uint8_t) (pair >> 8);
		selectors[1] = (uint8_t) pair;
		if (!runCase(g, base, call, &c, &o, t)) { failures[F_RUN]++; continue; }
		CHECK(F_READS, o.reads && o.trailing);
		CHECK(F_WRITES, o.writeCount && o.writeAddress);
		CHECK(F_REGS, o.registers);
		CHECK(F_STATUS, o.status);
		CHECK(F_CV, o.carryOverflow);
		CHECK(F_CLOCKS, o.clocks);
	}
	printf("%s\tselector_pairs\tcases=65536\treads=%lu\twrites=%lu\tregisters=%lu\tstatus=%lu\t"
		"carry_overflow=%lu\tclocks=%lu\tunfinished=%lu\n", biosName, failures[F_READS],
		failures[F_WRITES], failures[F_REGS], failures[F_STATUS], failures[F_CV],
		failures[F_CLOCKS], failures[F_RUN]);

	/* Widths 1-255 with coordinate wraps and RAM descriptors that cross pages or the RAM mirror. */
	memset(failures, 0, sizeof(failures));
	static const uint16_t pointers[] = { 0x0300, 0x02F0, 0x03F8, 0x0020 };
	static const uint8_t xs[] = { 0x00, 0x07, 0x28, 0xF8, 0xFF };
	static const uint8_t ys[] = { 0x00, 0x58, 0xF9, 0xFF };
	unsigned long cases = 0;
	for (unsigned w = 1; w < 256; w++) {
		for (size_t pi = 0; pi < sizeof(pointers) / sizeof(*pointers); pi++) {
			uint16_t pointer = pointers[pi];
			/* Descriptor must not overlap the pointer, cached LCD byte, scratch, or the stack. */
			bool overlaps = false;
			for (unsigned i = 0; i <= w; i++) {
				unsigned a = (pointer + i) & 0x3FF;
				overlaps |= a <= 0x01 || a == 0x15 || (a >= 0x18 && a <= 0x1B) ||
					(a >= 0x100 && a < 0x200);
			}
			if (overlaps) continue;
			for (unsigned i = 0; i < w; i++) {
				selectors[i] = (uint8_t) (i * 37 + w * 11 + pi);
			}
			c.width = (uint8_t) w;
			c.pointer = pointer;
			c.x = xs[(w + pi) % sizeof(xs)];
			c.y = ys[(w / 3 + pi) % sizeof(ys)];
			cases++;
			if (!runCase(g, base, call, &c, &o, t)) { failures[F_RUN]++; continue; }
			CHECK(F_READS, o.reads && o.trailing);
			CHECK(F_WRITES, o.writeCount && o.writeAddress);
			CHECK(F_REGS, o.registers);
			CHECK(F_STATUS, o.status);
			CHECK(F_CV, o.carryOverflow);
			CHECK(F_CLOCKS, o.clocks);
		}
	}
	printf("%s\twidths_wraps_pages\tcases=%lu\treads=%lu\twrites=%lu\tregisters=%lu\tstatus=%lu\t"
		"carry_overflow=%lu\tclocks=%lu\tunfinished=%lu\n", biosName, cases, failures[F_READS],
		failures[F_WRITES], failures[F_REGS], failures[F_STATUS], failures[F_CV],
		failures[F_CLOCKS], failures[F_RUN]);

	/* Width zero draws 256 tiles. */
	c = (struct Case) { 0x0200, 0, 0x28, 0x58, 0x24, authenticLcd, selectors };
	for (unsigned i = 0; i < 256; i++) selectors[i] = (uint8_t) (i * 73 + 5);
	bool zeroOk = runCase(g, base, call, &c, &o, t) && o.reads && o.trailing && o.writeCount &&
		o.writeAddress && o.registers && o.status && o.carryOverflow && o.clocks;
	printf("%s\twidth_zero_as_256\tcases=1\tfailures=%u\treads=%u/%u\twrites=%u/%u\t"
		"registers=%u\tstatus=%u\tcarry_overflow=%u\tclocks=%u\n", biosName, !zeroOk, o.reads,
		o.trailing, o.writeCount, o.writeAddress, o.registers, o.status, o.carryOverflow, o.clocks);

	/* Decimal mode at entry. */
	memset(failures, 0, sizeof(failures));
	c = (struct Case) { 0x0300, 1, 0x28, 0x58, 0x2C, authenticLcd, selectors };
	for (unsigned s = 0; s < 256; s++) {
		selectors[0] = (uint8_t) s;
		if (!runCase(g, base, call, &c, &o, t)) { failures[F_RUN]++; continue; }
		CHECK(F_READS, o.reads && o.trailing);
		CHECK(F_WRITES, o.writeCount && o.writeAddress);
		CHECK(F_REGS, o.registers);
		CHECK(F_STATUS, o.status);
		CHECK(F_CV, o.carryOverflow);
		CHECK(F_CLOCKS, o.clocks);
	}
	printf("%s\tdecimal_entry\tcases=256\treads=%lu\twrites=%lu\tregisters=%lu\tstatus=%lu\t"
		"carry_overflow=%lu\tclocks=%lu\tunfinished=%lu\n", biosName, failures[F_READS],
		failures[F_WRITES], failures[F_REGS], failures[F_STATUS], failures[F_CV],
		failures[F_CLOCKS], failures[F_RUN]);

	/* Cached LCD-control byte: plane selection, retained mode, and output identity. */
	c = (struct Case) { 0x0300, 2, 0x28, 0x58, 0x24, 0, selectors };
	selectors[0] = 0x35;
	selectors[1] = 0x52;
	unsigned long lcdFailures = 0, planeCounts[2][2] = { { 0 } }, modeMismatch = 0;
	unsigned long outputDiff = 0;
	uint8_t reference[16];
	bool haveReference = false;
	for (unsigned v = 0; v < 256; v++) {
		c.lcd = (uint8_t) v;
		if (!runCase(g, base, call, &c, &o, t)) { lcdFailures++; continue; }
		if (!o.reads || !o.writeCount || !o.writeAddress || !o.registers || !o.clocks) {
			lcdFailures++;
			continue;
		}
		planeCounts[(v & 0x80) != 0][o.plane]++;
		uint8_t mode = (uint8_t) ((g->incrementRow ? 0x40 : 0) | (g->window ? 0x20 : 0) |
			(g->swapPlanes ? 0x10 : 0) | (g->displayStopped ? 0x01 : 0) |
			(g->displayBlank ? 0x80 : 0));
		if (mode != (uint8_t) ((v | 0x40) & 0xF1)) modeMismatch++;
		if (!haveReference) {
			memcpy(reference, call->writeValues, sizeof(reference));
			haveReference = true;
		} else if (memcmp(reference, call->writeValues, sizeof(reference))) {
			outputDiff++;
		}
	}
	printf("%s\tlcd_cache\tcases=256\tfailures=%lu\tbit7_plane=%lu/%lu/%lu/%lu\t"
		"final_mode_not_input_or_40_and_F1=%lu\toutput_differs=%lu\n", biosName, lcdFailures,
		planeCounts[0][0], planeCounts[0][1], planeCounts[1][0], planeCounts[1][1],
		modeMismatch, outputDiff);
	/* Output transform: exhaustive input values in a private in-memory ROM copy, reported only as
	 * structural properties. The original byte is restored before any other case runs. */
	uint8_t transform[8][256];
	bool rowEqual = true, xEqual = true, lcdEqual = true, transformOk = true;
	c = (struct Case) { 0x0300, 1, 0x28, 0x58, 0x24, authenticLcd, selectors };
	selectors[0] = 0x30;
	for (unsigned row = 0; row < 8 && transformOk; row++) {
		size_t offset = cartridgeOffset(base, (uint16_t) (glyphAddress(0x30) + row));
		uint8_t original = base->rom[offset];
		for (unsigned v = 0; v < 256; v++) {
			base->rom[offset] = (uint8_t) v;
			transformOk &= runCase(g, base, call, &c, &o, t) && o.writeCount;
			transform[row][v] = call->writeValues[row];
			if (row == 0) {
				for (unsigned x = 1; x < 8; x++) {
					struct Case shifted = c;
					shifted.x = (uint8_t) (c.x + x);
					transformOk &= runCase(g, base, call, &shifted, &o, t);
					xEqual &= call->writeValues[0] == transform[0][v];
				}
				struct Case lcd = c;
				lcd.lcd = (uint8_t) (authenticLcd ^ 0xFF);
				transformOk &= runCase(g, base, call, &lcd, &o, t);
				lcdEqual &= call->writeValues[0] == transform[0][v];
			}
		}
		base->rom[offset] = original;
		if (row) rowEqual &= !memcmp(transform[row], transform[0], 256);
	}
	bool seen[256] = { false }, bijective = true, affine = true;
	for (unsigned v = 0; v < 256; v++) {
		bijective &= !seen[transform[0][v]];
		seen[transform[0][v]] = true;
		for (unsigned w = 0; w < 256; w++) {
			affine &= (transform[0][v ^ w] ^ transform[0][0]) ==
				(uint8_t) (transform[0][v] ^ transform[0][w]);
		}
	}
	/* Input-bit to output-bit dependence matrix: bit j of row i set when toggling input bit i can
	 * change output bit j for some input value. */
	uint8_t dependence[8] = { 0 };
	for (unsigned bit = 0; bit < 8; bit++) {
		for (unsigned v = 0; v < 256; v++) {
			dependence[bit] |= transform[0][v] ^ transform[0][v ^ (1u << bit)];
		}
	}
	printf("%s\ttransform\tcomplete=%u\trow_independent=%u\tx_low_bits_independent=%u\t"
		"lcd_cache_independent=%u\tbijective=%u\tgf2_affine=%u\tzero_maps_to=%02X\t"
		"input_bit_to_output_mask=", biosName, transformOk, rowEqual, xEqual, lcdEqual, bijective,
		affine, transform[0][0]);
	for (unsigned bit = 0; bit < 8; bit++) printf("%s%02X", bit ? "," : "", dependence[bit]);
	putchar('\n');
	unsigned mapped = 0;
	for (unsigned i = 0; i < 256; i++) mapped += t->inputSeen[i];
	printf("%s\tsynthetic_totals\tcalls=%lu\tinterrupted=%lu\tinput_values=%u\t"
		"copy_mismatch=%lu\n", biosName, t->calls, t->interrupted, mapped,
		t->copyMismatch);
#undef CHECK
	free(base);
	free(t);
	return 0;
}

int main(int argc, char** argv) {
	if (argc < 3 || argc > 5) {
		fprintf(stderr, "usage: %s gamate_bios.bin cartridge.bin [frames-after-entry|synthetic] "
			"[private-feature-report]\n", argv[0]);
		return 2;
	}
	bool syntheticMode = argc >= 4 && !strcmp(argv[3], "synthetic");
	unsigned frames = argc >= 4 && !syntheticMode ? (unsigned) strtoul(argv[3], NULL, 0) : 1000;
	FILE* features = NULL;
	if (argc == 5 && !(features = fopen(argv[4], "a"))) {
		fprintf(stderr, "could not open feature report\n");
		return 2;
	}
	size_t biosSize, romSize;
	uint8_t* bios = readFile(argv[1], &biosSize);
	uint8_t* rom = readFile(argv[2], &romSize);
	struct Gamate* g = malloc(sizeof(*g));
	struct Call* call = calloc(1, sizeof(*call));
	struct Totals* totals = calloc(1, sizeof(*totals));
	if (!bios || !rom || !g || !call || !totals) {
		fprintf(stderr, "could not read BIOS or cartridge\n");
		return 2;
	}
	gamateInit(g);
	if (!gamateLoad(g, rom, romSize, bios, biosSize)) {
		fprintf(stderr, "invalid BIOS or cartridge size\n");
		return 2;
	}
	free(bios);
	free(rom);
	bool entered = false;
	for (uint64_t instruction = 0; instruction < 20000000; instruction++) {
		unsigned cycles = gamateCpuStep(g);
		gamateRunClocks(g, cycles * 2);
		if (g->protectionPassed && g->cpu.pc == 0x6020) {
			entered = true;
			break;
		}
	}
	if (!entered) {
		fprintf(stderr, "BIOS did not reach cartridge entry\n");
		return 1;
	}
	gamateResetRomDiagnostics(g);
	gamateSetRomDiagnosticWatch(g, captureRomRead, call);
	const char* title = strrchr(argv[2], '/') ? strrchr(argv[2], '/') + 1 : argv[2];
	const char* biosName = strrchr(argv[1], '/') ? strrchr(argv[1], '/') + 1 : argv[1];
	int result = 0;
	if (syntheticMode) {
		result = synthetic(g, call, biosName);
	} else {
		const char* press = getenv("GAMATEBIOSCALLS_PRESS");
		for (unsigned frame = 1; frame <= frames; frame++) {
			g->buttons = gamateBiosButtonsForFrame(frame, press);
			g->frameDone = false;
			while (!g->frameDone) {
				if (step(g, call, true)) corpusCall(totals, call, g, features, title, biosName);
			}
		}
		printTotals(biosName, title, totals);
	}
	if (features) fclose(features);
	gamateFree(g);
	free(g);
	free(call);
	free(totals);
	return result;
}
