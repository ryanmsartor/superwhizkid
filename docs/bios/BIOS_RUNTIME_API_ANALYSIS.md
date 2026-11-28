# Gamate BIOS runtime API analysis and specification

This document is the implementation-facing specification for the resident BIOS interface used after
cartridge entry at `$6020`. It defines callable service slots, interrupt callbacks, inputs, outputs,
side effects, timing formulas, and known interference behavior. Unless a section says otherwise,
contracts describe both recognized authentic BIOS revisions and use master clocks, where one CPU
cycle is two master clocks.

## BIOS documentation map

| Document | Canonical scope |
| --- | --- |
| This document | Post-`$6020` interrupt ABI and callable service contracts. |
| [`BIOS_BOOT_BEHAVIOR.md`](BIOS_BOOT_BEHAVIOR.md) | Reset, cartridge validation, failure paths, and transfer to `$6020`. |
| [`BIOS_DIFFERENCE_ANALYSIS.md`](BIOS_DIFFERENCE_ANALYSIS.md) | Structural and behavioral differences between the authentic revisions. |
| [`BIOS_ENTRY_RAM_ANALYSIS.md`](BIOS_ENTRY_RAM_ANALYSIS.md) | Low-RAM entry fields, derivation, perturbations, and cartridge consumers. |
| [`BIOS_REPLACEMENT_ANALYSIS.md`](BIOS_REPLACEMENT_ANALYSIS.md) | Complete `$6020` machine boundary and replacement/skip policy. |
| [`EXPERIMENTAL_BIOS.md`](EXPERIMENTAL_BIOS.md) | SWK prototype scope, build/use instructions, and current deficiencies. |
| [`../../src/gamate/SOURCES.md`](../../src/gamate/SOURCES.md) | Provenance, corpus coverage, validation history, uncertainty, and decision rationale. |

## Contract conventions

| Term | Meaning |
| --- | --- |
| Entry | CPU state immediately before the first instruction at the service slot executes. |
| Return | State after the service's `RTS` has returned to cartridge code. |
| Uninterrupted | No IRQ or NMI handler ran between service entry and return. |
| Preserved | Equal at entry and return; it does not imply the value was untouched internally. |
| Master clocks | The emulator's 4.433 MHz master-clock domain; divide by two for CPU cycles. |
| RAM address | Physical low-RAM offset. `$000-$3FF` mirrors through `$0000-$1FFF`. |
| VRAM address | The LCD controller's 13-bit byte address, with plane stated separately. |
| Zero loop count | Unless stated otherwise, an 8-bit decrement loop interprets zero as 256 iterations. |
| Status | Only listed flags are specified. Stored bit 5 remains set; the stored B bit is not a physical CPU signal. |

Timing formulas exclude interrupt-handler time. Time-dependent LCD, audio, IRQ, and UART state
continues to advance during every call even when a table says the service has no direct peripheral
effect. The authentic services do not generally mask interrupts; callers and replacement firmware
must therefore assume that shared registers, RAM, and hardware state can change at instruction
boundaries unless a linked detailed contract proves a narrower rule.

## Cartridge-facing interrupt ABI

| Event | BIOS work before callback | Enable condition | Cartridge callback | Final return |
| --- | --- | --- | --- | --- |
| Periodic IRQ | Increment big-endian counter `$00E-$011`; update the wait counter used by `$F003/$F006/$F009`. | RAM `$00C != 0` | `JSR $6026` | BIOS wrapper executes `RTI`. |
| Link-byte NMI | Read the completed UART byte into RAM `$00A`. | RAM `$00C != 0` | `JSR $6023` | BIOS wrapper executes `RTI`. |

Callback register behavior is observable ABI, not full context preservation:

| Path | A after callback | X after callback | Y after callback | P / PC / S |
| --- | --- | --- | --- | --- |
| IRQ callback enabled | Interrupted A restored | Callback X retained | Interrupted Y restored | Interrupted values restored in controlled probes. |
| NMI callback enabled | Interrupted A restored | Callback X retained | Callback Y retained | Interrupted values restored in controlled probes. |
| Callback disabled | Interrupted A restored | Interrupted X restored | Interrupted Y restored | Interrupted values restored in controlled probes. |

These rows cover normally returning callbacks. They do not specify callbacks that alter stacked
state, nest interrupts, or fail to return. `$00C` is boolean: zero suppresses dispatch and any
nonzero value enables it.

## Service table quick reference

Every observed direct entry is one of these 18 three-byte slots. Each linked slot leads to its
authoritative detailed behavior, formulas, edge cases, and interruption notes in the development log.

| Slot | Operation | Primary inputs | Primary outputs | Uninterrupted timing |
| --- | --- | --- | --- | --- |
| [`$F000`](OPEN_SOURCE_BIOS_DEVELPOMENT_LOG.md#fixed-bitmap-writer-f000) | Fixed 64-byte bitmap writer | RAM `$015` LCD control | A/X/Y=`$38/$40/$20`; 64 VRAM writes | 3,278 clocks |
| [`$F003`](OPEN_SOURCE_BIOS_DEVELPOMENT_LOG.md#irq-gated-counter-wait-f003) | Wait until `$00B >= 8` | RAM `$00B` | A=`$00`; `$00B=0` | 42-clock ready path; otherwise `8-n` IRQs |
| [`$F006`](OPEN_SOURCE_BIOS_DEVELPOMENT_LOG.md#irq-gated-counter-wait-f006) | Wait until `$00B >= 5` | RAM `$00B` | A=`$00`; `$00B=0` | 42-clock ready path; otherwise `5-n` IRQs |
| [`$F009`](OPEN_SOURCE_BIOS_DEVELPOMENT_LOG.md#irq-gated-counter-wait-f009) | Wait until `$00B >= 24` | RAM `$00B` | A=`$00`; `$00B=0` | 42-clock ready path; otherwise `24-n` IRQs |
| [`$F00C`](OPEN_SOURCE_BIOS_DEVELPOMENT_LOG.md#two-plane-vram-zero-writer-and-lcd-control-service-f00c) | Clear both VRAM planes | RAM `$015` LCD control | A=`$40 \| $015`; X=`$00`; Y=`$C8` | 247,326 clocks |
| [`$F00F`](OPEN_SOURCE_BIOS_DEVELPOMENT_LOG.md#fixed-plane-0-rectangle-clear-f00f) | Clear fixed 31×25-byte plane-0 rectangle | RAM `$015` | A/X/Y=`$00`; 775 VRAM writes | 15,930 clocks |
| [`$F012`](OPEN_SOURCE_BIOS_DEVELPOMENT_LOG.md#fixed-height-tile-run-contract-f012) | Draw an eight-row glyph run | X/Y, descriptor pointer `$000-$001`, `$015` | VRAM glyph run; see contract | Per-selector formula below |
| [`$F015`](OPEN_SOURCE_BIOS_DEVELPOMENT_LOG.md#two-plane-bitmap-copy-contract-f015) | Copy a rectangle into both planes | X/Y, pointers `$000-$003`, `$015`, `$01C-$01D` | Two VRAM rectangles | `234 + columns*(132 + 76*rows) + page penalties` |
| [`$F018`](OPEN_SOURCE_BIOS_DEVELPOMENT_LOG.md#command-stream-tile-row-blitter-f018) | Draw a two-plane row/run command stream | Stream pointer `$000-$001`; plane bases `$006-$009` | Variable VRAM output | Stream-dependent formula below |
| [`$F01B`](OPEN_SOURCE_BIOS_DEVELPOMENT_LOG.md#plane-0-row-run-stream-service-f01b) | Draw plane-0 row/run stream | Stream pointer `$000-$001`; `$015` | Plane-0 VRAM output | `15362 + 140*rows + 134*pairs + 18*(bytes-pairs) + penalties` |
| [`$F01E`](OPEN_SOURCE_BIOS_DEVELPOMENT_LOG.md#caller-supplied-two-plane-graphics-service-f01e) | Draw two-plane tile-literal stream | Stream pointer `$000-$001`; plane bases `$006-$009`; `$015` | Two-plane VRAM output | `144 + 260*rows + 392*selectors + data/penalty terms` |
| [`$F021`](OPEN_SOURCE_BIOS_DEVELPOMENT_LOG.md#plane-0-row-literal-stream-service-f021) | Draw plane-0 literal-row stream | Stream pointer `$000-$001`; `$015` | Plane-0 VRAM output | `15362 + 140*rows + 66*bytes + penalties` |
| [`$F024`](OPEN_SOURCE_BIOS_DEVELPOMENT_LOG.md#strided-ram-initializer-f024) | Initialize two fields in 16 records | D must be clear | 32 RAM bytes become `$FF`; A/Y=`$80`, X=`$00` | 824 clocks |
| [`$F027`](OPEN_SOURCE_BIOS_DEVELPOMENT_LOG.md#sprite-and-background-compositor-f027) | Redraw backgrounds and composite objects | Records `$021-$0A0`, graphics bases `$006-$009`, `$015` | VRAM plus scratch buffers | `6,326 + operation costs` |
| [`$F02A`](OPEN_SOURCE_BIOS_DEVELPOMENT_LOG.md#plane-0-rectangle-fill-f02a) | Fill plane-0 rectangle | A/X/Y, `$01C-$01D` | Rectangle filled with A | `126 + 58*columns + 18*columns*rows` |
| [`$F02D`](OPEN_SOURCE_BIOS_DEVELPOMENT_LOG.md#plane-1-rectangle-fill-f02d) | Fill plane-1 rectangle | A/X/Y, `$015`, `$01C-$01D` | Rectangle filled with A | `120 + 58*columns + 18*columns*rows` |
| [`$F030`](OPEN_SOURCE_BIOS_DEVELPOMENT_LOG.md#pseudorandom-state-transition-f030) | Advance 16-bit pseudorandom state | RAM `$012-$013` | A and `$012-$013` become next state; `$014` scratch | 174 clocks; 188 for zero input |
| [`$F033`](OPEN_SOURCE_BIOS_DEVELPOMENT_LOG.md#packed-digit-formatter-f033) | Format six packed nibbles | A/X/Y | `$0A1-$0A7`; A=`$06`, Y=first digit | `222 + 44*first`, with special cases below |

The labels describe measured effects rather than original firmware names. A replacement should
implement the linked contract, not infer behavior from the short label alone.

## Detailed contracts and evidence

Detailed trace methodology, corpus observations, perturbation results, timing derivations, executable models, corrections, and open research are maintained in the [open-source BIOS development log](OPEN_SOURCE_BIOS_DEVELPOMENT_LOG.md). The links below are the canonical detailed references for each service.

| Service | Detailed reference |
| --- | --- |
| `$F000` | [Fixed bitmap writer](OPEN_SOURCE_BIOS_DEVELPOMENT_LOG.md#fixed-bitmap-writer-f000) |
| `$F003`, `$F006`, `$F009` | [IRQ-gated waits](OPEN_SOURCE_BIOS_DEVELPOMENT_LOG.md#irq-gated-counter-wait-f003) |
| `$F00C`, `$F00F` | [VRAM clear services](OPEN_SOURCE_BIOS_DEVELPOMENT_LOG.md#two-plane-vram-zero-writer-and-lcd-control-service-f00c) |
| `$F012`, `$F015` | [Glyph run](OPEN_SOURCE_BIOS_DEVELPOMENT_LOG.md#fixed-height-tile-run-contract-f012) and [bitmap copy](OPEN_SOURCE_BIOS_DEVELPOMENT_LOG.md#two-plane-bitmap-copy-contract-f015) |
| `$F018`, `$F01B`, `$F01E`, `$F021` | [Graphics-stream services](OPEN_SOURCE_BIOS_DEVELPOMENT_LOG.md#command-stream-tile-row-blitter-f018) |
| `$F024` | [Strided RAM initializer](OPEN_SOURCE_BIOS_DEVELPOMENT_LOG.md#strided-ram-initializer-f024) |
| `$F027` | [Sprite and background compositor](OPEN_SOURCE_BIOS_DEVELPOMENT_LOG.md#sprite-and-background-compositor-f027) |
| `$F02A`, `$F02D` | [Rectangle fill services](OPEN_SOURCE_BIOS_DEVELPOMENT_LOG.md#plane-0-rectangle-fill-f02a) |
| `$F030`, `$F033` | [Pseudorandom transition](OPEN_SOURCE_BIOS_DEVELPOMENT_LOG.md#pseudorandom-state-transition-f030) and [formatter](OPEN_SOURCE_BIOS_DEVELPOMENT_LOG.md#packed-digit-formatter-f033) |

For tracer methodology, call-surface coverage, and unresolved hypotheses, see [detailed evidence and remaining analysis](OPEN_SOURCE_BIOS_DEVELPOMENT_LOG.md#detailed-runtime-api-evidence-and-contracts).


## Limits

The baseline is execution evidence for the paths reached in 1,000 unattended frames. It cannot prove
that an unobserved input combination, later level, failure path, or link session uses no additional
entry behavior. The tracer sees boundary crossings, not internal calls made entirely within BIOS
space. IRQ return destinations are deliberately retained in the raw local report but are not treated
as BIOS API entries.

The combined report remains outside the repository and contains filenames and derived addresses but
no firmware or cartridge payload. Source provenance and use are recorded in
[`../../src/gamate/SOURCES.md`](../../src/gamate/SOURCES.md).

