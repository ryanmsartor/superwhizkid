/* Copyright (c) 2026 Ry
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
#include "gamate.h"

// Instruction-granularity NMOS 6502 for the Gamate's NCR 65CX02. The machine bus remains separate
// so this core can be tested against flat memory independently of the LCD, cartridge and PSG.

enum { C = 1, Z = 2, I = 4, D = 8, B = 16, U = 32, V = 64, N = 128 };

static uint8_t read8(struct Gamate* g, uint16_t address) { return gamateRead(g, address); }
static void write8(struct Gamate* g, uint16_t address, uint8_t value) { gamateWrite(g, address, value); }
static uint8_t fetch(struct Gamate* g) { return read8(g, g->cpu.pc++); }
static uint16_t fetch16(struct Gamate* g) {
	uint8_t lo = fetch(g), hi = fetch(g);
	return lo | (uint16_t) hi << 8;
}
static uint16_t read16(struct Gamate* g, uint16_t address) {
	uint8_t lo = read8(g, address), hi = read8(g, address + 1);
	return lo | (uint16_t) hi << 8;
}
static uint16_t read16ZeroPage(struct Gamate* g, uint8_t address) {
	uint8_t lo = read8(g, address);
	uint8_t hi = read8(g, (uint8_t) (address + 1));
	return lo | (uint16_t) hi << 8;
}
static void push(struct Gamate* g, uint8_t value) { write8(g, 0x100 | g->cpu.s--, value); }
static uint8_t pull(struct Gamate* g) { return read8(g, 0x100 | ++g->cpu.s); }

static unsigned unsupported(struct Gamate* g, uint8_t opcode) {
	g->diagnostics.unsupportedOpcodes++;
	g->diagnostics.lastUnsupportedOpcode = opcode;
	g->diagnostics.lastUnsupportedPc = g->cpu.pc - 1;
	return 2;
}

static void flag(struct GamateCpu* c, uint8_t mask, bool set) {
	c->p = set ? c->p | mask : c->p & ~mask;
}
static void zn(struct GamateCpu* c, uint8_t value) {
	flag(c, Z, value == 0);
	flag(c, N, value & 0x80);
}
static uint16_t indexed(uint16_t base, uint8_t index, bool* crossed) {
	uint16_t result = base + index;
	*crossed = (base ^ result) & 0x100;
	return result;
}
static uint16_t uncorrectedPage(uint16_t base, uint16_t indexedAddress) {
	return (base & 0xFF00) | (indexedAddress & 0x00FF);
}

static void compare(struct GamateCpu* c, uint8_t left, uint8_t right) {
	flag(c, C, left >= right);
	zn(c, left - right);
}

// NMOS decimal mode derives Z from the binary result. ADC derives N/V after the low-nibble carry
// but before the high-nibble correction; SBC derives N/V from the binary result. These otherwise
// undocumented details matter for invalid BCD digits and are exhaustively tested.
static void adc(struct GamateCpu* c, uint8_t value) {
	unsigned carry = c->p & C;
	unsigned binary = c->a + value + carry;
	if (c->p & D) {
		unsigned low = (c->a & 0x0F) + (value & 0x0F) + carry;
		unsigned high = (c->a >> 4) + (value >> 4) + (low > 9);
		flag(c, Z, (uint8_t) binary == 0);
		flag(c, N, high & 8);
		flag(c, V, ((high << 4) ^ c->a) & 0x80 && !((c->a ^ value) & 0x80));
		if (low > 9) low += 6;
		if (high > 9) high += 6;
		flag(c, C, high > 0x0F);
		c->a = (high << 4) | (low & 0x0F);
	} else {
		flag(c, V, ~(c->a ^ value) & (c->a ^ binary) & 0x80);
		zn(c, binary);
		flag(c, C, binary > 0xFF);
		c->a = binary;
	}
}

static void sbc(struct GamateCpu* c, uint8_t value) {
	unsigned borrow = (c->p & C) ? 0 : 1;
	unsigned binary = c->a - value - borrow;
	flag(c, V, (c->a ^ value) & (c->a ^ binary) & 0x80);
	zn(c, binary);
	flag(c, C, binary < 0x100);
	if (c->p & D) {
		int low = (c->a & 0x0F) - (value & 0x0F) - (int) borrow;
		int high = (c->a >> 4) - (value >> 4);
		if (low < 0) {
			low -= 6;
			high--;
		}
		if (high < 0) high -= 6;
		c->a = (uint8_t) high << 4 | (low & 0x0F);
	} else {
		c->a = binary;
	}
}

static uint8_t shift(struct GamateCpu* c, unsigned operation, uint8_t value) {
	bool oldCarry = c->p & C;
	switch (operation) {
	case 0: flag(c, C, value & 0x80); value <<= 1; break; // ASL
	case 1: flag(c, C, value & 0x80); value = value << 1 | oldCarry; break; // ROL
	case 2: flag(c, C, value & 1); value >>= 1; break; // LSR
	default: flag(c, C, value & 1); value = value >> 1 | oldCarry << 7; break; // ROR
	}
	zn(c, value);
	return value;
}

static unsigned interrupt(struct Gamate* g, uint16_t vector, bool software) {
	struct GamateCpu* c = &g->cpu;
	if (!software) {
		// IRQ/NMI replace an opcode fetch, then repeat it while preparing the stack sequence.
		read8(g, c->pc);
		read8(g, c->pc);
	}
	push(g, c->pc >> 8);
	push(g, c->pc);
	push(g, (c->p & ~B) | U | (software ? B : 0));
	c->p = (c->p | I | U) & ~B;
	c->irqMask = true;
	c->pc = read16(g, vector);
	// Physical tests report that an IRQ-vector pull resets the free-running timer and its flag.
	if (vector == 0xFFFE) {
		g->irq = false;
		g->irqClocks = 0;
		if (!software) g->diagnostics.irqsTaken++;
	} else if (vector == 0xFFFA) {
		g->diagnostics.nmisTaken++;
	}
	return 7;
}

void gamateCpuReset(struct Gamate* g) {
	struct GamateCpu* c = &g->cpu;
	c->a = c->x = c->y = 0;
	// Model the documented power-on reset bus sequence from a zeroed internal CPU state: two
	// discarded PC reads, three stack reads with decrements, then the reset vector. No stack writes
	// occur. A warm external-reset input is not currently exposed by the Gamate machine API.
	c->pc = 0;
	c->s = 0;
	read8(g, c->pc);
	read8(g, c->pc);
	read8(g, 0x100 | c->s--);
	read8(g, 0x100 | c->s--);
	read8(g, 0x100 | c->s--);
	c->p = I | U;
	c->pc = read16(g, 0xFFFC);
	c->cycles = 7;
	c->stopped = false;
	c->irqMask = true;
}

static unsigned group1(struct Gamate* g, uint8_t opcode) {
	struct GamateCpu* c = &g->cpu;
	unsigned operation = opcode >> 5, mode = opcode >> 2 & 7, cycles;
	uint16_t address = 0;
	uint8_t value = 0;
	bool crossed = false, store = operation == 4;
	switch (mode) {
	case 0: { uint8_t zp = fetch(g); read8(g, zp); zp += c->x;
		address = read16ZeroPage(g, zp); cycles = 6; break; }
	case 1: address = fetch(g); cycles = 3; break;
	case 2: value = fetch(g); cycles = 2; break;
	case 3: address = fetch16(g); cycles = 4; break;
	case 4: { uint8_t zp = fetch(g); uint16_t base = read16ZeroPage(g, zp);
		address = indexed(base, c->y, &crossed);
		if (store || crossed) read8(g, uncorrectedPage(base, address));
		cycles = store ? 6 : 5 + crossed; break; }
	case 5: { uint8_t zp = fetch(g); read8(g, zp); address = (uint8_t) (zp + c->x); cycles = 4; break; }
	case 6: { uint16_t base = fetch16(g); address = indexed(base, c->y, &crossed);
		if (store || crossed) read8(g, uncorrectedPage(base, address));
		cycles = store ? 5 : 4 + crossed; break; }
	default: { uint16_t base = fetch16(g); address = indexed(base, c->x, &crossed);
		if (store || crossed) read8(g, uncorrectedPage(base, address));
		cycles = store ? 5 : 4 + crossed; break; }
	}
	if (store) {
		if (mode == 2) return 2; // $89 is not an NMOS STA instruction
		write8(g, address, c->a);
		return cycles;
	}
	if (mode != 2) value = read8(g, address);
	switch (operation) {
	case 0: c->a |= value; zn(c, c->a); break;
	case 1: c->a &= value; zn(c, c->a); break;
	case 2: c->a ^= value; zn(c, c->a); break;
	case 3: adc(c, value); break;
	case 5: c->a = value; zn(c, value); break;
	case 6: compare(c, c->a, value); break;
	case 7: sbc(c, value); break;
	default: break;
	}
	return cycles;
}

static unsigned group2(struct Gamate* g, uint8_t opcode) {
	struct GamateCpu* c = &g->cpu;
	unsigned operation = opcode >> 5, mode = opcode >> 2 & 7;
	uint16_t address = 0;
	uint8_t value;
	bool crossed = false;
	if (operation <= 3 && mode == 2) {
		c->a = shift(c, operation, c->a);
		return 2;
	}
	if (operation == 5 && mode == 0) {
		c->x = fetch(g); zn(c, c->x); return 2;
	}
	if (mode == 1) address = fetch(g);
	else if (mode == 3) address = fetch16(g);
	else if (mode == 5) { uint8_t zp = fetch(g); read8(g, zp);
		address = (uint8_t) (zp + ((operation == 4 || operation == 5) ? c->y : c->x)); }
	else if (mode == 7 && operation != 4) { uint16_t base = fetch16(g);
		address = indexed(base, operation == 5 ? c->y : c->x, &crossed);
		if (operation != 5 || crossed) read8(g, uncorrectedPage(base, address)); }
	else return unsupported(g, opcode);
	if (operation == 4) { write8(g, address, c->x); return mode == 3 ? 4 : 3 + (mode == 5); }
	if (operation == 5) {
		c->x = read8(g, address); zn(c, c->x);
		return mode == 1 ? 3 : mode == 3 ? 4 : mode == 5 ? 4 : 4 + crossed;
	}
	value = read8(g, address);
	// An NMOS 6502 RMW cycle writes the unmodified value before writing the result. This is visible
	// to Gamate peripherals whose action is triggered by an access rather than the data value.
	write8(g, address, value);
	if (operation <= 3) value = shift(c, operation, value);
	else if (operation == 6) { value--; zn(c, value); }
	else { value++; zn(c, value); }
	write8(g, address, value);
	return mode == 1 ? 5 : mode == 3 ? 6 : mode == 5 ? 6 : 7;
}

static unsigned branch(struct Gamate* g, bool take) {
	int8_t offset = (int8_t) fetch(g);
	if (!take) return 2;
	uint16_t old = g->cpu.pc;
	uint16_t target = old + offset;
	read8(g, old); // discarded fall-through opcode fetch
	bool crossed = (old ^ target) & 0x100;
	if (crossed) read8(g, uncorrectedPage(old, target));
	g->cpu.pc = target;
	return 3 + crossed;
}

static bool hasImpliedOpcodeFetch(uint8_t opcode) {
	if ((opcode & 0x9F) == 0x0A) return true; // ASL/ROL/LSR/ROR accumulator
	switch (opcode) {
	case 0x18: case 0x38: case 0x58: case 0x78: // flag operations
	case 0x88: case 0x8A: case 0x98: case 0x9A: // decrement/transfer
	case 0xA8: case 0xAA: case 0xB8: case 0xBA: // transfer/flag operations
	case 0xC8: case 0xCA: case 0xD8: case 0xE8: case 0xEA: case 0xF8:
		return true;
	default:
		return false;
	}
}

unsigned gamateCpuStep(struct Gamate* g) {
	struct GamateCpu* c = &g->cpu;
#ifdef GAMATE_ACCESS_DIAGNOSTICS
	g->diagnostics.diagnosticInstructionPc = c->pc;
#endif
	g->diagnostics.instructions++;
	if (c->stopped) return 1;
	if (g->nmi) {
		g->nmi = false;
#ifdef GAMATE_ACCESS_DIAGNOSTICS
		g->diagnostics.diagnosticInterrupt = 2;
#endif
		unsigned cycles = interrupt(g, 0xFFFA, false);
		c->cycles += cycles;
		return cycles;
	}
	if (g->irq && !c->irqMask) {
#ifdef GAMATE_ACCESS_DIAGNOSTICS
		g->diagnostics.diagnosticInterrupt = 1;
#endif
		unsigned cycles = interrupt(g, 0xFFFE, false);
		c->cycles += cycles;
		return cycles;
	}
	uint8_t opcode = fetch(g);
	if (hasImpliedOpcodeFetch(opcode)) read8(g, c->pc);
	bool oldIrqMask = c->p & I;
	bool delayedIrqMask = opcode == 0x28 || opcode == 0x58 || opcode == 0x78; // PLP, CLI, SEI
	unsigned cycles = 2;
	if ((opcode & 3) == 1 && opcode != 0x89) cycles = group1(g, opcode);
	else if ((opcode & 3) == 2 && opcode != 0x8A && opcode != 0x9A && opcode != 0xAA &&
			opcode != 0xBA && opcode != 0xCA && opcode != 0xEA) cycles = group2(g, opcode);
	else switch (opcode) {
	case 0x00:
#ifdef GAMATE_ACCESS_DIAGNOSTICS
		g->diagnostics.diagnosticInterrupt = 3;
#endif
		read8(g, c->pc++); cycles = interrupt(g, 0xFFFE, true); break;
	case 0x08: read8(g, c->pc); push(g, c->p | B | U); cycles = 3; break;
	case 0x10: cycles = branch(g, !(c->p & N)); break;
	case 0x18: c->p &= ~C; break;
	case 0x20: { uint8_t lo = fetch(g); uint16_t ret = c->pc; read8(g, 0x100 | c->s);
		push(g, ret >> 8); push(g, ret); c->pc = lo | (uint16_t) fetch(g) << 8; cycles = 6; break; }
	case 0x24: { uint8_t v = read8(g, fetch(g)); flag(c, Z, !(c->a & v)); flag(c, V, v & 0x40); flag(c, N, v & 0x80); cycles = 3; break; }
	case 0x28: read8(g, c->pc); read8(g, 0x100 | c->s); c->p = (pull(g) | U) & ~B; cycles = 4; break;
	case 0x2C: { uint8_t v = read8(g, fetch16(g)); flag(c, Z, !(c->a & v)); flag(c, V, v & 0x40); flag(c, N, v & 0x80); cycles = 4; break; }
	case 0x30: cycles = branch(g, c->p & N); break;
	case 0x38: c->p |= C; break;
	case 0x40: read8(g, c->pc); read8(g, 0x100 | c->s); c->p = (pull(g) | U) & ~B;
		{ uint8_t lo = pull(g); c->pc = lo | (uint16_t) pull(g) << 8; } cycles = 6;
#ifdef GAMATE_ACCESS_DIAGNOSTICS
		g->diagnostics.diagnosticInterrupt = 0;
#endif
		break;
	case 0x48: read8(g, c->pc); push(g, c->a); cycles = 3; break;
	case 0x4C: c->pc = fetch16(g); cycles = 3; break;
	case 0x50: cycles = branch(g, !(c->p & V)); break;
	case 0x58: c->p &= ~I; break;
	case 0x60: { read8(g, c->pc); read8(g, 0x100 | c->s); uint8_t lo = pull(g);
		uint16_t ret = lo | (uint16_t) pull(g) << 8; read8(g, ret); c->pc = ret + 1; cycles = 6; break; }
	case 0x68: read8(g, c->pc); read8(g, 0x100 | c->s); c->a = pull(g); zn(c, c->a); cycles = 4; break;
	case 0x6C: { uint16_t p = fetch16(g); uint8_t lo = read8(g, p);
		c->pc = lo | (uint16_t) read8(g, (p & 0xFF00) | (uint8_t) (p + 1)) << 8; cycles = 5; break; }
	case 0x70: cycles = branch(g, c->p & V); break;
	case 0x78: c->p |= I; break;
	case 0x84: write8(g, fetch(g), c->y); cycles = 3; break;
	case 0x88: c->y--; zn(c, c->y); break;
	case 0x8A: c->a = c->x; zn(c, c->a); break;
	case 0x8C: write8(g, fetch16(g), c->y); cycles = 4; break;
	case 0x90: cycles = branch(g, !(c->p & C)); break;
	case 0x94: { uint8_t zp = fetch(g); read8(g, zp); write8(g, (uint8_t) (zp + c->x), c->y); cycles = 4; break; }
	case 0x98: c->a = c->y; zn(c, c->a); break;
	case 0x9A: c->s = c->x; break;
	case 0xA0: c->y = fetch(g); zn(c, c->y); break;
	case 0xA4: c->y = read8(g, fetch(g)); zn(c, c->y); cycles = 3; break;
	case 0xA8: c->y = c->a; zn(c, c->y); break;
	case 0xAA: c->x = c->a; zn(c, c->x); break;
	case 0xAC: c->y = read8(g, fetch16(g)); zn(c, c->y); cycles = 4; break;
	case 0xB0: cycles = branch(g, c->p & C); break;
	case 0xB4: { uint8_t zp = fetch(g); read8(g, zp); c->y = read8(g, (uint8_t) (zp + c->x)); zn(c, c->y); cycles = 4; break; }
	case 0xB8: c->p &= ~V; break;
	case 0xBA: c->x = c->s; zn(c, c->x); break;
	case 0xBC: { bool crossed; uint16_t base = fetch16(g); uint16_t address = indexed(base, c->x, &crossed);
		if (crossed) read8(g, uncorrectedPage(base, address));
		c->y = read8(g, address); zn(c, c->y); cycles = 4 + crossed; break; }
	case 0xC0: compare(c, c->y, fetch(g)); break;
	case 0xC4: compare(c, c->y, read8(g, fetch(g))); cycles = 3; break;
	case 0xC8: c->y++; zn(c, c->y); break;
	case 0xCA: c->x--; zn(c, c->x); break;
	case 0xCC: compare(c, c->y, read8(g, fetch16(g))); cycles = 4; break;
	case 0xD0: cycles = branch(g, !(c->p & Z)); break;
	case 0xD8: c->p &= ~D; break;
	case 0xE0: compare(c, c->x, fetch(g)); break;
	case 0xE4: compare(c, c->x, read8(g, fetch(g))); cycles = 3; break;
	case 0xE8: c->x++; zn(c, c->x); break;
	case 0xEA: break;
	case 0xEC: compare(c, c->x, read8(g, fetch16(g))); cycles = 4; break;
	case 0xF0: cycles = branch(g, c->p & Z); break;
	case 0xF8: c->p |= D; break;
	default: cycles = unsupported(g, opcode); break;
	}
	c->p |= U;
	c->irqMask = delayedIrqMask ? oldIrqMask : (c->p & I) != 0;
	c->cycles += cycles;
	return cycles;
}
