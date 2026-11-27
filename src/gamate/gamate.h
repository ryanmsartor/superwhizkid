/* Copyright (c) 2026 Ry
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef SUPERWHIZKID_GAMATE_H
#define SUPERWHIZKID_GAMATE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum { GAMATE_WIDTH = 160, GAMATE_HEIGHT = 150, GAMATE_CLOCKS_PER_FRAME = 72900 };
enum { GAMATE_IRQ_PERIOD = 32768 };
enum { IRQ_PERIOD = GAMATE_IRQ_PERIOD };
enum { GAMATE_BIOS_SIZE = 0x1000, GAMATE_MAX_ROM_SIZE = 0x80000 };

enum GamateMapper { GAMATE_MAPPER_PLAIN, GAMATE_MAPPER_BANKED, GAMATE_MAPPER_4IN1 };

enum GamateDebugStopReason {
	GAMATE_DEBUG_STOP_NONE,
	GAMATE_DEBUG_STOP_BREAKPOINT,
	GAMATE_DEBUG_STOP_RUN_TO,
	GAMATE_DEBUG_STOP_STEP_OVER,
	GAMATE_DEBUG_STOP_STEP_OUT,
	GAMATE_DEBUG_STOP_IRQ,
	GAMATE_DEBUG_STOP_NMI,
	GAMATE_DEBUG_STOP_BRK,
	GAMATE_DEBUG_STOP_UNSUPPORTED_OPCODE,
	GAMATE_DEBUG_STOP_BIOS_TRAP,
	GAMATE_DEBUG_STOP_UNSUPPORTED = GAMATE_DEBUG_STOP_UNSUPPORTED_OPCODE
};

enum GamateDebugRunMode {
	GAMATE_DEBUG_MODE_NONE,
	GAMATE_DEBUG_MODE_RUN_TO,
	GAMATE_DEBUG_MODE_STEP_OVER,
	GAMATE_DEBUG_MODE_STEP_OUT
};

typedef void (*GamateLinkBitCallback)(void* context, bool bit);

#ifdef GAMATE_ACCESS_DIAGNOSTICS
struct Gamate;
typedef void (*GamateRamAccessCallback)(void* context, const struct Gamate* gamate,
	bool write, uint16_t address, uint8_t previous, uint8_t value);
typedef void (*GamateRomReadCallback)(void* context, const struct Gamate* gamate,
	uint16_t address, size_t physicalOffset);
typedef void (*GamateLcdWriteCallback)(void* context, const struct Gamate* gamate,
	uint8_t reg, uint8_t value);
typedef void (*GamateWatchpointCallback)(void* context, const struct Gamate* gamate,
	bool write, bool dummy, uint16_t address, uint8_t previous, uint8_t value);
#endif

enum GamateWatchpointMode { GAMATE_WATCHPOINT_READ, GAMATE_WATCHPOINT_WRITE, GAMATE_WATCHPOINT_ACCESS };

struct GamateCpu {
	uint16_t pc;
	uint8_t a, x, y, s, p;
	uint64_t cycles;
	bool stopped, irqMask;
};

struct GamateDiagnostics {
	uint64_t instructions, unsupportedOpcodes, irqsTaken, nmisTaken;
	uint64_t ayWrites, lcdWrites, vramWrites, bankWrites;
#ifdef GAMATE_ACCESS_DIAGNOSTICS
	uint64_t vramTrackedWrites, vramReads, vramReadBeforeWrite;
	uint64_t vramDisplayReads, vramDisplayReadBeforeWrite;
	uint64_t ramTrackedWrites[3], ramReads[3], ramReadBeforeWrite[3];
	uint64_t romTrackedReads;
#endif
	uint64_t uartClocks, uartReceivedBits;
	uint64_t ayRegisterWrites[16], ayActiveSamples[3];
	uint64_t controllerReads, pressedControllerReads;
	uint16_t lastUnsupportedPc;
	uint8_t lastUnsupportedOpcode;
#ifdef GAMATE_ACCESS_DIAGNOSTICS
	bool vramCoverageEnabled;
	uint16_t vramUniqueWritten, vramUniqueReadBeforeWrite, vramUniqueDisplayReadBeforeWrite;
	uint8_t vramWritten[0x4000 / 8];
	uint8_t vramReadBeforeWriteMap[0x4000 / 8];
	uint8_t vramDisplayReadBeforeWriteMap[0x4000 / 8];
	bool ramCoverageEnabled;
	uint16_t diagnosticInstructionPc;
	uint16_t ramWatchAddress;
	uint8_t diagnosticInterrupt;
	bool ramWatchEnabled;
	GamateRamAccessCallback ramAccessCallback;
	void* ramAccessContext;
	bool watchpointDummyAccess;
	uint8_t watchpointRead[0x10000 / 8];
	uint8_t watchpointWrite[0x10000 / 8];
	uint8_t watchpointAccess[0x10000 / 8];
	GamateWatchpointCallback watchpointCallback;
	void* watchpointContext;
	GamateRomReadCallback romReadCallback;
	GamateLcdWriteCallback lcdWriteCallback;
	void* lcdWriteContext;
	void* romReadContext;
	uint16_t ramUniqueWritten, ramUniqueReadBeforeWrite[3];
	uint8_t ramWritten[0x400 / 8];
	uint8_t ramReadBeforeWriteMap[3][0x400 / 8];
	bool romCoverageEnabled;
	uint32_t romUniqueRead;
	uint8_t romRead[GAMATE_MAX_ROM_SIZE / 8];
#endif
};

struct GamateAy {
	uint32_t toneCounter[3], noiseCounter, envelopeCounter;
	uint32_t noiseLfsr;
	int8_t envelopeStep;
	uint8_t envelopeAttack;
	bool toneLevel[3], noiseLevel;
	bool envelopeHold, envelopeAlternate, envelopeHolding;
	int32_t previousInput[2], highPass[2];
};

struct GamateDebug {
	uint8_t breakpoints[0x10000 / 8];
	enum GamateDebugRunMode mode;
	uint16_t runToAddress, stepAddress;
	uint8_t stepStackPointer;
	enum GamateDebugStopReason stopReason;
	bool breakOnIrq, breakOnNmi, breakOnBrk;
	bool breakOnUnsupportedOpcode, breakOnBiosTrap;
};

struct Gamate {
	struct GamateCpu cpu;
	uint8_t ram[0x400];
	uint8_t vram[0x4000];
	uint8_t bios[GAMATE_BIOS_SIZE];
	uint8_t* rom;
	size_t romSize;

	uint16_t vramAddress;
	uint8_t scrollX, scrollY;
	uint8_t bank, firstBank;
	bool vramPlane, incrementRow, window, swapPlanes, displayBlank, displayStopped;
	enum GamateMapper mapper;
	bool protectionPassed, protectionFailed, protectionReply;
	uint8_t protectionPosition, protectionByte, protectionSequence;

	uint8_t buttons, openBus;
	uint8_t uartTx, uartRx, uartTxBits, uartRxBits;
	bool uartTxLine;
	GamateLinkBitCallback linkBitCallback;
	void* linkBitContext;
	uint64_t masterClocks;
	uint32_t frameClocks, irqClocks;
	bool frameDone, irq, nmi;

	uint8_t ayAddress;
	uint8_t ayRegisters[16];
	uint32_t ayTonePeriod[3], ayNoisePeriod, ayEnvelopePeriod;
	struct GamateAy ay;
	int16_t* audioOut;
	size_t audioCapacity, audioCount;
	unsigned sampleRate;
	uint64_t samplePhase;

	uint16_t frameBuffer[GAMATE_WIDTH * GAMATE_HEIGHT];
	uint8_t lcdAccum[GAMATE_WIDTH * GAMATE_HEIGHT];
	uint16_t lcdClock, lcdRowEvent;
	// Observational counters for bring-up tools; deliberately excluded from save states.
	struct GamateDiagnostics diagnostics;
	// Debugger configuration and transient run state; deliberately excluded from save states.
	struct GamateDebug debug;
};

void gamateInit(struct Gamate* gamate);
bool gamateLoad(struct Gamate* gamate, const uint8_t* rom, size_t romSize,
	const uint8_t* bios, size_t biosSize);
void gamateSetMapper(struct Gamate* gamate, enum GamateMapper mapper);
void gamateFree(struct Gamate* gamate);
void gamateReset(struct Gamate* gamate);

uint8_t gamateRead(struct Gamate* gamate, uint16_t address);
void gamateWrite(struct Gamate* gamate, uint16_t address, uint8_t value);
// Debugger access. None of these change machine state, openBus, or diagnostics counters.
// gamatePeek returns the value a gamateRead would return, without advancing VRAM, UART, or
// protection state.
uint8_t gamatePeek(const struct Gamate* gamate, uint16_t address);
// Writes the storage behind an address: RAM, cartridge ROM ($6000-$DFFF), or BIOS ($E000-$FFFF).
// Hardware registers are not writable here. Returns false for addresses with no such storage.
// Save states do not capture ROM or BIOS bytes, so these edits persist across state loads and are
// discarded only when content is reloaded or the bytes are edited again.
bool gamatePoke(struct Gamate* gamate, uint16_t address, uint8_t value);
// Physical cartridge ROM offset that a read of address maps to under the current bank, or SIZE_MAX
// outside the cartridge window ($6000-$DFFF).
size_t gamateRomOffset(const struct Gamate* gamate, uint16_t address);
void gamateRunClocks(struct Gamate* gamate, unsigned clocks);
void gamateRunFrame(struct Gamate* gamate);
// Connect the outgoing UART data sampled by each local transmit-clock access.
// The callback and context are transport state and are deliberately not serialized.
void gamateSetLink(struct Gamate* gamate, GamateLinkBitCallback callback, void* context);
// Supply one rising receiver-clock edge and its data bit from a future link transport.
void gamateUartReceiveBit(struct Gamate* gamate, bool bit);
// Immediate diagnostic preview; normal execution builds the picture progressively in run clocks.
void gamateRenderSnapshot(struct Gamate* gamate);
#ifdef GAMATE_ACCESS_DIAGNOSTICS
// Restarts diagnostic-only memory access coverage without changing machine state. These APIs and
// their storage do not exist in normal core builds.
void gamateResetVramDiagnostics(struct Gamate* gamate);
void gamateResetRamDiagnostics(struct Gamate* gamate);
void gamateResetRomDiagnostics(struct Gamate* gamate);
void gamateSetRamDiagnosticWatch(struct Gamate* gamate, uint16_t address,
	GamateRamAccessCallback callback, void* context);
void gamateSetRomDiagnosticWatch(struct Gamate* gamate,
	GamateRomReadCallback callback, void* context);
void gamateSetLcdDiagnosticWatch(struct Gamate* gamate,
	GamateLcdWriteCallback callback, void* context);
void gamateSetWatchpoint(struct Gamate* gamate, uint16_t address,
	enum GamateWatchpointMode mode, GamateWatchpointCallback callback, void* context);
void gamateClearWatchpoint(struct Gamate* gamate, uint16_t address,
	enum GamateWatchpointMode mode);
void gamateClearWatchpoints(struct Gamate* gamate);
bool gamateWatchpointEnabled(const struct Gamate* gamate, uint16_t address,
	enum GamateWatchpointMode mode);
#endif
void gamateSetAudio(struct Gamate* gamate, int16_t* output, size_t frameCapacity, unsigned sampleRate);
void gamateCpuReset(struct Gamate* gamate);
unsigned gamateCpuStep(struct Gamate* gamate);

// Executes one instruction and advances peripheral clocks by the instruction's master-clock count.
unsigned gamateStep(struct Gamate* gamate);
// Runs until a configured stop condition fires.
enum GamateDebugStopReason gamateDebugRun(struct Gamate* gamate);
void gamateDebugSetBreakpoint(struct Gamate* gamate, uint16_t address, bool enabled);
void gamateDebugClearBreakpoint(struct Gamate* gamate, uint16_t address);
void gamateDebugClearBreakpoints(struct Gamate* gamate);
bool gamateDebugBreakpointEnabled(const struct Gamate* gamate, uint16_t address);
void gamateDebugSetBreakOnInterrupt(struct Gamate* gamate, bool irq, bool nmi, bool brk);
void gamateDebugSetBreakOnUnsupportedOpcode(struct Gamate* gamate, bool enabled);
void gamateDebugSetBreakOnBiosTrap(struct Gamate* gamate, bool enabled);
enum GamateDebugStopReason gamateDebugRunTo(struct Gamate* gamate, uint16_t address);
enum GamateDebugStopReason gamateDebugStepOver(struct Gamate* gamate);
enum GamateDebugStopReason gamateDebugStepOut(struct Gamate* gamate);
enum GamateDebugStopReason gamateDebugGetStopReason(const struct Gamate* gamate);

void gamateSetBreakpoint(struct Gamate* gamate, uint16_t address);
void gamateClearBreakpoint(struct Gamate* gamate, uint16_t address);
void gamateClearBreakpoints(struct Gamate* gamate);
void gamateSetBreakOnIrq(struct Gamate* gamate, bool enabled);
void gamateSetBreakOnNmi(struct Gamate* gamate, bool enabled);
void gamateSetBreakOnBrk(struct Gamate* gamate, bool enabled);
void gamateSetBreakOnUnsupported(struct Gamate* gamate, bool enabled);
void gamateSetBreakOnBiosTrap(struct Gamate* gamate, bool enabled);
void gamateSetBreakOnInterrupt(struct Gamate* gamate, bool irq, bool nmi, bool brk);

size_t gamateStateSize(const struct Gamate* gamate);
bool gamateSaveState(const struct Gamate* gamate, void* data, size_t size);
bool gamateLoadState(struct Gamate* gamate, const void* data, size_t size);

#endif
