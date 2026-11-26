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
| [`../src/gamate/SOURCES.md`](../src/gamate/SOURCES.md) | Provenance, corpus coverage, validation history, uncertainty, and decision rationale. |

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
boundaries unless a detailed section proves a narrower rule.

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

Every observed direct entry is one of these 18 three-byte slots. “Detailed contract” links to the
authoritative behavior, formulas, edge cases, and interruption notes in this document.

| Slot | Operation | Primary inputs | Primary outputs | Uninterrupted timing |
| --- | --- | --- | --- | --- |
| [`$F000`](#fixed-bitmap-writer-f000) | Fixed 64-byte bitmap writer | RAM `$015` LCD control | A/X/Y=`$38/$40/$20`; 64 VRAM writes | 3,278 clocks |
| [`$F003`](#irq-gated-counter-wait-f003) | Wait until `$00B >= 8` | RAM `$00B` | A=`$00`; `$00B=0` | 42-clock ready path; otherwise `8-n` IRQs |
| [`$F006`](#irq-gated-counter-wait-f006) | Wait until `$00B >= 5` | RAM `$00B` | A=`$00`; `$00B=0` | 42-clock ready path; otherwise `5-n` IRQs |
| [`$F009`](#irq-gated-counter-wait-f009) | Wait until `$00B >= 24` | RAM `$00B` | A=`$00`; `$00B=0` | 42-clock ready path; otherwise `24-n` IRQs |
| [`$F00C`](#two-plane-vram-zero-writer-and-lcd-control-service-f00c) | Clear both VRAM planes | RAM `$015` LCD control | A=`$40 \| $015`; X=`$00`; Y=`$C8` | 247,326 clocks |
| [`$F00F`](#fixed-plane-0-rectangle-clear-f00f) | Clear fixed 31×25-byte plane-0 rectangle | RAM `$015` | A/X/Y=`$00`; 775 VRAM writes | 15,930 clocks |
| [`$F012`](#fixed-height-tile-run-contract-f012) | Draw an eight-row glyph run | X/Y, descriptor pointer `$000-$001`, `$015` | VRAM glyph run; see contract | Per-selector formula below |
| [`$F015`](#two-plane-bitmap-copy-contract-f015) | Copy a rectangle into both planes | X/Y, pointers `$000-$003`, `$015`, `$01C-$01D` | Two VRAM rectangles | `234 + columns*(132 + 76*rows) + page penalties` |
| [`$F018`](#command-stream-tile-row-blitter-f018) | Draw a two-plane row/run command stream | Stream pointer `$000-$001`; plane bases `$006-$009` | Variable VRAM output | Stream-dependent formula below |
| [`$F01B`](#plane-0-row-run-stream-service-f01b) | Draw plane-0 row/run stream | Stream pointer `$000-$001`; `$015` | Plane-0 VRAM output | `15362 + 140*rows + 134*pairs + 18*(bytes-pairs) + penalties` |
| [`$F01E`](#caller-supplied-two-plane-graphics-service-f01e) | Draw two-plane tile-literal stream | Stream pointer `$000-$001`; plane bases `$006-$009`; `$015` | Two-plane VRAM output | `144 + 260*rows + 392*selectors + data/penalty terms` |
| [`$F021`](#plane-0-row-literal-stream-service-f021) | Draw plane-0 literal-row stream | Stream pointer `$000-$001`; `$015` | Plane-0 VRAM output | `15362 + 140*rows + 66*bytes + penalties` |
| [`$F024`](#strided-ram-initializer-f024) | Initialize two fields in 16 records | D must be clear | 32 RAM bytes become `$FF`; A/Y=`$80`, X=`$00` | 824 clocks |
| [`$F027`](#sprite-and-background-compositor-f027) | Redraw backgrounds and composite objects | Records `$021-$0A0`, graphics bases `$006-$009`, `$015` | VRAM plus scratch buffers | `6,326 + operation costs` |
| [`$F02A`](#plane-0-rectangle-fill-f02a) | Fill plane-0 rectangle | A/X/Y, `$01C-$01D` | Rectangle filled with A | `126 + 58*columns + 18*columns*rows` |
| [`$F02D`](#plane-1-rectangle-fill-f02d) | Fill plane-1 rectangle | A/X/Y, `$015`, `$01C-$01D` | Rectangle filled with A | `120 + 58*columns + 18*columns*rows` |
| [`$F030`](#pseudorandom-state-transition-f030) | Advance 16-bit pseudorandom state | RAM `$012-$013` | A and `$012-$013` become next state; `$014` scratch | 174 clocks; 188 for zero input |
| [`$F033`](#packed-digit-formatter-f033) | Format six packed nibbles | A/X/Y | `$0A1-$0A7`; A=`$06`, Y=first digit | `222 + 44*first`, with special cases below |

The labels describe measured effects rather than original firmware names. A replacement should
implement the linked contract, not infer behavior from the short label alone.

## Supporting evidence and validation

This section summarizes how the contract was derived. It is non-normative; implementers can skip to
[Detailed service contracts](#detailed-service-contracts). Detailed provenance and the reasons for
the selected coverage policies are maintained in [`../src/gamate/SOURCES.md`](../src/gamate/SOURCES.md).

### Execution-aware tracer

`gamatebioscalls` runs an authentic BIOS and cartridge to `$6020`, then records control-flow
transitions across the cartridge/BIOS boundary. Unlike a raw byte scan, it observes executed
instructions and classifies:

- Hardware IRQ and NMI vector entry.
- Direct `JSR`, absolute `JMP`, and indirect `JMP` into BIOS space.
- `BRK` vectoring.
- Cartridge callback returns into the BIOS wrapper.
- BIOS calls to `$6023` or `$6026`.
- BIOS `RTI`, `RTS`, or jumps back into cartridge space.

Mapper bank and first-bank selection are captured with each event, so source addresses in the upper
cartridge window remain attributable. The tracer reads opcodes directly from the already loaded ROM
or BIOS image without adding emulated bus accesses. Its TSV output contains addresses, event type,
bank, counts, and first-observed frame, but no ROM or BIOS bytes beyond the single executed opcode
needed to classify a transition.

`tools/gamate-bios-call-sweep.sh` runs the tracer over both BIOS revisions and every cartridge in a
local collection. Its default observation window is 1,000 post-entry frames. Optional input pulses
can be supplied through `GAMATEBIOSCALLS_PRESS` using the same `frame:mask:duration` form as the boot
diagnostic.

### Call-surface baseline

The baseline now covers all 62 catalogued cartridges under both BIOS revisions for 1,000 post-entry
frames. The final four titles were added as an eight-case extension to the established matrix:

- 124/124 combinations completed without tracer overflow or execution failure.
- Every cartridge made at least one direct call into the BIOS.
- Every direct entry targeted one of 18 slots from `$F000` through `$F033`, spaced exactly three
  bytes apart.
- The complete direct-event multiset was identical across BIOS revisions: cartridge, source address,
  mapper bank, target slot, transition type, call count, and first-observed frame all matched.
- Per BIOS, the run observed 163,208 direct cartridge-to-BIOS calls from 1,109 distinct
  source/bank/target event rows.
- Direct entries were `JSR` except for tail jumps from Nightmare of Santa Claus to `$F027` and from
  Tennis to `$F015`. No indirect jump, `BRK`, fall-through, or target outside the service table was
  observed.
- All 62 titles received periodic cartridge IRQ callbacks at `$6026` during the window.
- No `$6023` NMI callback occurred because the unattended sweep supplied no link traffic.

The identical call surface demonstrates a stable BIOS ABI. The two firmware revisions may implement
or relocate helpers differently behind the table, but commercial cartridges use the same public
entry slots and produce the same observed call schedule.

### Observed slot use

Counts below are for one BIOS revision; the other revision produced an identical table. “Titles” is
the number of distinct cartridges reaching the slot, and “calls” is the aggregate over the 1,000
frame window.

| Service slot | Titles | Calls | First frame | Observed entry form |
| --- | ---: | ---: | ---: | --- |
| `$F000` | 8 | 8 | 6 | `JSR` |
| `$F003` | 30 | 3,725 | 2 | `JSR` |
| `$F006` | 26 | 3,233 | 5 | `JSR` |
| `$F009` | 41 | 2,052 | 3 | `JSR` |
| `$F00C` | 61 | 129 | 1 | `JSR` |
| `$F00F` | 39 | 103 | 1 | `JSR` |
| `$F012` | 39 | 19,518 | 5 | `JSR` |
| `$F015` | 42 | 20,912 | 5 | `JSR`, tail `JMP` |
| `$F018` | 15 | 24 | 5 | `JSR` |
| `$F01B` | 13 | 18 | 5 | `JSR` |
| `$F01E` | 19 | 110 | 5 | `JSR` |
| `$F021` | 14 | 46 | 5 | `JSR` |
| `$F024` | 39 | 65 | 1 | `JSR` |
| `$F027` | 25 | 3,598 | 5 | `JSR`, tail `JMP` |
| `$F02A` | 22 | 4,779 | 1 | `JSR` |
| `$F02D` | 22 | 104,545 | 3 | `JSR` |
| `$F030` | 8 | 181 | 408 | `JSR` |
| `$F033` | 1 | 162 | 408 | `JSR` |

The former 300-frame sweep window would have missed `$F030` and `$F033`. `$F033` was observed only
in Tornado during this baseline; `$F030` appeared in eight titles. This directly motivated changing
the standard compatibility and BIOS-call sweep defaults to 1,000 frames.

Subsequent RAM tracing identifies `$F030` as the BIOS pseudorandom service. It preserves X and Y,
repairs a zero input state before advancing a 16-bit feedback state at `$0012-$0013`, and returns
the new `$0012` low byte in A. A nonzero input can itself produce zero. `$0014` is scratch feedback
overwritten by each call. This name is based on measured state transitions and cartridge use, not
on another emulator's label.

The table alone does not establish semantics for the other slots. Names such as drawing, delay,
formatting, or input must not be assigned until register, RAM, and hardware effects are measured
independently.

### Replacement-firmware implications

The minimum viable replacement BIOS is substantially larger than reset, protection, and interrupt
dispatch. Every observed cartridge depends on the `$F000-$F033` table, and most use multiple slots.
A BIOS-free implementation must therefore provide:

1. All 18 observed service entry points at their stable addresses, or a demonstrably equivalent
   high-level interception boundary.
2. Calling conventions, register/flag preservation, RAM effects, and hardware side effects for each
   used service.
3. Tail-call behavior for `$F015` and `$F027`, not only ordinary subroutine returns.
4. Periodic IRQ dispatch to `$6026` and link NMI dispatch to `$6023`.
5. Correct interaction between BIOS helpers and interrupts that occur while a helper is running.

The corpus-wide use of the table favors a small resident clean-room firmware over setting PC directly
to `$6020`. A direct jump without the table would fail immediately in all 62 titles observed here.

### Interrupt callback validation

The earlier claim in the boot analysis that both BIOS wrappers restore the entire interrupted CPU
context was incorrect. A purpose-written callback replaced A, X, and Y at a synthetic cartridge
instruction boundary, then returned normally. Under both BIOS revisions, the IRQ path restored
interrupted A and Y but retained the callback's X; the NMI path restored A but retained its X and Y.
P, PC, and S were restored in these probes. When `$00C` disabled the callback, A, X, Y, P, PC, and S
were preserved by both paths. The controlled matrix covered all 256 stack pointers, both callback
enable states, and all N/V/D/I/Z/C combinations for NMI or those with I clear for IRQ, in all 32
first-call cartridge/BIOS contexts (38,912 cases). These observations used a private in-memory
cartridge copy, and NMI was injected directly rather than delivered by UART. Callbacks that modify
the interrupted stack, generate nested interrupts, or depend on other machine state are not covered
by this result.

## Detailed service contracts

The following sections define the current executable contracts. The two method summaries explain
how calls were paired and classified; each service section then owns its exact formulas, edge cases,
and unresolved behavior.

### Call/return pairing method

Pair each direct cartridge-to-service-table entry with its matching BIOS-to-cartridge return while
allowing IRQ/NMI callbacks to cross the boundary in between. For every completed call, retain only
derived behavioral metadata:

- entry and exit CPU registers, flags, and stack pointer;
- expected return boundary and whether execution returned there;
- elapsed master clocks and entry/return frames;
- changed-byte counts and address extents for RAM and VRAM, without retaining byte contents;
- changed AY-register mask and AY, LCD, VRAM, mapper, UART, IRQ, and NMI counter deltas.
- entry and exit VRAM address, scroll, LCD mode flags, AY address, and mapper-bank state.

The detailed completed-call report remains local and separate from the compact boundary-event report.
This is now implemented as the optional final output argument to `gamatebioscalls` and
`tools/gamate-bios-call-sweep.sh`. Calls still executing when the observation window ends are reported
as open rather than incorrectly paired. Direct tail jumps obtain their expected return from the
existing cartridge stack frame.

The full paired sweep completed all 124 cartridge/BIOS cases under a stricter validator that
fails a run on any unmatched BIOS return or pairing-stack overflow:

- 326,334 service calls completed within the observation windows: 163,167 per BIOS revision.
- Another 41 calls per BIOS began before the final frame boundary and remained legitimately open.
- Every slot's completed-call count matched exactly between BIOS revisions.
- For a given cartridge, source, slot, mapper state, and input/output register tuple, the complete
  signature multiset was identical between BIOS revisions.
- Phase-sensitive frames, elapsed clocks, interrupt inclusion, and resulting memory/peripheral
  summaries sometimes differed, as expected from the known 58-master-clock cartridge-entry offset.

The balanced register signatures strengthen the conclusion that both BIOS revisions expose the same
calling convention. They do not yet identify the meaning of any slot.

| Slot | Completed calls | Clock range | Calls changing VRAM | Calls changing AY registers | Calls spanning IRQ/NMI |
| --- | ---: | ---: | ---: | ---: | ---: |
| `$F02D` | 209,088 | 196–114,978 | 42,694 | 60 | 1,584 |
| `$F015` | 41,824 | 594–23,158 | 34,596 | 62 | 3,034 |
| `$F012` | 39,034 | 494–10,692 | 7,276 | 36 | 2,516 |
| `$F02A` | 9,558 | 202–114,436 | 8,956 | 8 | 396 |
| `$F003` | 7,424 | 42–268,662 | 0 | 1,842 | 7,362 |
| `$F027` | 7,192 | 6,326–309,980 | 3,064 | 472 | 4,826 |
| `$F006` | 6,454 | 42–168,820 | 0 | 1,352 | 6,262 |

These are observational categories, not semantic labels. In particular, a call that spans an IRQ can
include callback memory or peripheral effects in its summary. The interrupt counters make those cases
separable during clustering. `tools/gamate-bios-call-summary.py` reproduces the per-slot aggregate
table from a local completed-call report.

### Signature-clustering method

Group completed calls by service slot, input/output register tuple, memory-change shape, hardware
effects, and timing range. Begin with `$F02D`, `$F015`, and `$F012`, which together dominate the
observed call volume, then cover `$F02A`, `$F003`, `$F006`, and `$F027`. Treat clusters as candidate
calling conventions, not semantic names, until targeted perturbations confirm them.

This stage is now in progress. The completed-call report and summary tool retain hardware-state
transitions in addition to write counters, allowing a helper that changes LCD configuration to be
distinguished from one whose writes merely occur while the prior configuration remains active.
Selected-slot summaries also report register preservation, status-bit behavior, timing grouped by
input A, and the most frequent hardware-state transitions.

### Pseudorandom state transition: `$F030`

For an uninterrupted call, interpret RAM `$012` as the low byte and `$013` as the high byte of
an unsigned 16-bit state. Let `s` be that input, except replace zero with `$FFFF` before applying
the following transition. Bits are numbered from zero:

```
feedback = bit(s, 4) XOR bit(s, 10)
next = ((s << 1) | feedback) & $FFFF
RAM[$012] = next & $FF
RAM[$013] = next >> 8
RAM[$014] = bit(s, 10) << 7
```

The zero repair applies to the **input**, not the output. For example, `$8000` becomes `$0000`;
the next call repairs that input and returns `$FFFE`. Thus the previously stated invariant that
the service maintains a nonzero state was incorrect.

| Property | Uninterrupted contract |
| --- | --- |
| Inputs | RAM `$012-$013`; entry A, X, Y, carry, and the previous `$014` do not affect the transition. |
| Return | A is the new `$012`; X, Y, and S are preserved. |
| Status | C is bit 15 of `s` after zero repair; N and Z describe returned A; I, D, and V are preserved. Incoming N, Z, and C do not affect the result. |
| Scratch | `$014` is overwritten with `$00` or `$80` as above and read during the call. No other non-stack RAM is written; only `$012-$013` are read before being written. |
| Stack | For a normal JSR returning with S, the tested calls write stack locations `S-2` and `S-3` modulo 256 and read the stacked return address. The read-before-write map also includes `S-4`; stack contents are not a stable public interface and must not be copied as firmware data. |
| Timing | 174 master clocks (87 CPU cycles) for nonzero input; 188 clocks when the input is zero. |
| Hardware effects | No VRAM, LCD, AY-register, mapper, UART, scroll, or hardware-address-state changes. Time-dependent hardware continues to advance. |

`gamatebiosf030` checks this equation for every one of the 65,536 input states, using two distinct
register/status/scratch profiles, at the first `$F030` caller context of each of the 27 cartridge
images observed in the four existing 1,000-frame campaigns, under both BIOSes. These are 26 distinct
titles, including both Bomb Blaster dumps. In each context it also tests every A, X, Y, and prior
`$014` byte and all 64 combinations of N, V, D, I, Z, and C for seven selected states. All
7,489,152 calls matched (138,688 per context). Stack access and wrapping
were checked separately at every S for those seven states in one synthetic return context per BIOS:
1,792 calls per revision. The diagnostic uses an independently constructed return address after
locating each authentic context; it does not preserve or publish firmware or cartridge payload.

The selected campaigns were no input for Dino Ball, Dino Bibo, Fortune 'n Luck, Hot Hero, Magic
Jigsaw, Tornado, Witty Apee, and World Cup Soccer; long Start for Baseball, both Bomb Blaster
dumps, Cosmic Fighter, Devil Castle, Enchanted Bricks, Flipuzzle, Further Adventures of Hannibal
The Cat, Insect War, Mighty Tank, Pipemania, Riddle of the Ancient Tomb, Snowman Legend, and
Vindicators; menu actions for Brick Blaster, Nightmare of Santa Claus, and Tennis; and controller
cycle for Fist of Thunder and QuizFighter. Each schedule's occurrence was selected from its existing
1,000-frame campaign. The schedules use `frame:mask:duration` pulses:

- Long Start: `90:0x40:60,400:0x40:60,750:0x40:60`.
- Menu actions: `90:0x80:20,130:0x40:60,260:0x10:12,310:0x20:12,500:0x40:30,580:0x10:12,630:0x20:12`.
- Controller cycle: `80:0x01:12,120:0x02:12,160:0x04:12,200:0x08:12,240:0x10:12,280:0x20:12,320:0x40:60,400:0x80:20,460:0x11:12,500:0x18:12,540:0x21:12,580:0x28:12,700:0x40:60,800:0x10:12,840:0x20:12`.

Build the diagnostic with `cmake --build build --target gamatebiosf030`; invoke it as
`build/gamatebiosf030 BIOS_FILE CARTRIDGE_FILE full` for the exhaustive matrix. `sample` selects a
smaller state set, `stack` checks stack wrapping, and `irq` and `nmi` run synthetic-callback cases.
`interleave-irq` and `interleave-nmi` compare a controlled single-write interrupt matrix with the
event model below; `interleave-all-irq` and `interleave-all-nmi` extend the matrix to every byte
value. `interleave-flags-irq` and `interleave-flags-nmi` test the D/V-set profiles described below.
`nested-irq` and `nested-nmi` test a callback that calls `$F030` once before returning.
An optional decimal occurrence argument selects an authentic call before testing; input schedules
use `GAMATEBIOSCALLS_PRESS` as in the other BIOS diagnostics. These modes require local,
user-supplied BIOS and cartridge files and are not part of the production core.

Exhaustive enumeration of the resulting mathematical transition graph gives seven cycles of
lengths 7, 85, 85, 85, 595, 595, and 595, with a maximum transient of 10 calls. The observed boot
seed `$358A` lies on a 595-call cycle. This is a property of the specified transition, not a
physical-hardware measurement or a claim that every possible title uses each state.

#### Interrupt interaction

The operation is not atomic. With input `$1234`, a deliberately injected IRQ after the low byte
was updated allowed a purpose-written cartridge callback to observe the intermediate state
`$1269`; the uninterrupted and eventual result was `$2469`. In a second experiment, an IRQ after
the service wrote `$014` let a synthetic callback set `$014=$80` before it was read; the result
changed from `$2469` to `$2468`. In a third, the callback set `$012=$80` after the service's final
read: A returned `$69` while the final RAM state was `$2480`. In a fourth, a callback reentered
`$F030` after the outer call wrote scratch for input `$0400`. The final state was `$1003`, rather
than `$1002` from two uninterrupted sequential calls.

Each scenario was reproduced at all 27 cartridge first-call contexts under both BIOSes, using either
an injected IRQ or an injected NMI (432 interrupted calls). The callback was installed only in a
private in-memory cartridge copy after the authentic call was reached. It is not commercial
cartridge data; the NMI tests inject the interrupt directly and do not simulate UART link traffic.

Separate address watches on seven synthetic state inputs under both BIOSes observed the low-byte
update before the high-byte update, with `$014` written and then read before those updates. The
zero-input path first writes `$FF` to both state bytes. An additional exploratory matrix injected
one IRQ at each modeled instruction boundary for inputs `$0000`, `$1234`, and `$8000`, with a
callback setting one of `$012-$014` to `$80`: 288 calls per BIOS in one context. Derived output
signatures matched between revisions but varied with injection point, including cases where A and
the final low byte differed. These observations identify real intermediate state and preclude an
atomic read/modify/write substitute when callbacks can access these bytes. They do not define
every possible IRQ/NMI injection point, callback mutation, nesting case, or instruction-level
timing. A replacement must preserve the necessary interrupt-visible ordering; full equivalence for
arbitrary asynchronous callbacks remains unverified.

The following event model was derived independently from those watches and controlled callback
writes. Times are emulator instruction-start offsets in master clocks, excluding interrupt work;
they are **not** physical bus-cycle measurements. The zero-repair path uses the offsets in the
second column.

| Nonzero path | Zero-repair path | Observed event or modeled effect |
| ---: | ---: | --- |
| 26, 32 | 26, 32 | Sample `$012`, then `$013`; if both samples are zero, take the repair path. |
| — | 46, 52 | Write `$FF` to `$012`, then `$013`. |
| 44, 74 | 58, 88 | Sample `$013`; write its bit 2 as bit 7 of `$014`. |
| 80, 102 | 94, 116 | Sample `$012` and then `$014`; XOR their bits 4 and 7 for feedback. |
| 112, 122 | 126, 136 | Shift the then-current `$012` left with feedback; shift the then-current `$013` left with the former low byte's bit 7. |
| 156 | 170 | Load A from the then-current `$012`; N/Z describe A and C is the former high byte's bit 7. |
| 174 | 188 | Return, absent interrupt work. |

The model describes the tested intervention class: one IRQ or NMI callback writes one of
`$012-$014` once and otherwise returns normally, with the write applied at the interrupted
instruction boundary. It was checked with eight synthetic input states, five callback byte values,
each target byte, and every modeled instruction boundary in each of the 27 cartridge contexts
under both BIOSes, using IRQ and NMI separately. Each combination was run with D/V clear and set;
the D/V-set NMI profile also entered with I set. All 813,240 interrupted calls matched. A second
matrix used the same eight states, three targets, boundaries, contexts, BIOSes, and IRQ/NMI, but
exercised all 256 callback byte values with entry P=`$20`. All 20,818,944 cases matched. Neither
matrix proves behavior for arbitrary callback programs, nested asynchronous interrupts, or bus-cycle
timing. In particular, callbacks that inspect or alter the interrupted stack and CPU context,
generate other interrupts, or affect other RAM or hardware have not been exhaustively modeled.

A separate reentrancy matrix tested one callback that invokes `$F030` uninterrupted and returns,
without further interrupt injection. The prediction applies the uninterrupted transition to
`$012-$014` at the interrupted outer boundary, then resumes the event model. For the same eight
synthetic states, every outer instruction boundary, entry P=`$20`, IRQ and NMI, 27 cartridge
contexts, and both BIOSes, all 27,108 cases matched A, X, Y, S, P, and `$012-$014`. This covers
one level of service reentry, not arbitrary asynchronous interrupt nesting.

### Fixed bitmap writer: `$F000`

`gamatebiosprobe` boots an authentic BIOS and cartridge to the normal `$6020` entry boundary, changes
only explicitly named entry-state fields, and invokes one public service slot with a synthetic
subroutine return. It reports derived register, timing, address-range, hash, and write-count data; it
does not emit BIOS instructions, RAM contents, or VRAM contents. IRQ timing is reset before the call
so that a narrow service probe is not contaminated by a periodic cartridge callback.

A controlled `$F000` matrix varied A, X, Y, status, incoming VRAM address, scroll, LCD flags, AY
selection, mapper state, VRAM fill, and RAM `$015`. Both BIOS revisions produced identical derived
results throughout the matrix.

| Property | Controlled contract |
| --- | --- |
| Input RAM | `$015` is the only non-stack RAM location read before it is written. It is the cached raw LCD-control value. |
| Return | A/X/Y become `$38/$40/$20`; S is restored; status becomes `$22` for tested entry states. |
| Timing | 3,278 master clocks (1,639 CPU cycles), with no interrupt included. |
| VRAM | 64 writes to 64 unique addresses; final address `$128B`. With a common initial fill, the final full-VRAM hash and changed-address extent were identical across all tested inputs and both BIOSes. |
| LCD state | Writes cached control from `$015` while forcing row increment; the service's drawing sequence determines the final plane. Other cached mode bits remain effective. |
| Preserved hardware state | Scroll X/Y, AY selection, current bank, and first bank. |
| Other hardware effects | No AY-register, mapper, UART, IRQ, or NMI effects. |

The invariant final VRAM result supports the narrow name **fixed bitmap writer**. It does not identify
what the bitmap depicts, and the bitmap bytes are deliberately neither retained nor published. A
clean-room replacement must not copy that firmware-resident asset: it can use an independently
authored image, or omit the drawing only after compatibility testing demonstrates that callers do not
depend on the pixels. The observed `$015` dependency and LCD/VRAM side effects remain part of the ABI
even if the replacement artwork differs.

### Packed-digit formatter: `$F033`

The unattended 1,000-frame sweep observed `$F033` only in Tornado, but the existing input-driven
1,000-frame campaigns expose 16 cartridge images: Boom, Brick Blaster, Devil Castle, Enchanted
Bricks, Famous 7, Fist of Thunder, Magic Jigsaw, Marauder, Mars Voyage, Money Maze, Monster Pitfall,
Myth of Asamia, Nightmare of Santa Claus, Pipemania, Tornado, and Treasure Hunter. This corrects the
earlier description of Tornado as the only observed caller.

The candidate uninterrupted formatter model below extends the valid packed-BCD interface to
synthetic invalid nibbles and a special Y input. Let the six nibbles of A, X, Y, in high-to-low
order, be `d[0]` through `d[5]`.

1. If input Y is `$FF`, set the last two characters to spaces and consider only `d[0..3]`.
   Otherwise consider all six nibbles.
2. Set `first` to the index of the first nonzero considered nibble, except that the search stops
   at index 4 for Y=`$FF` and at index 5 otherwise.
3. Before `first`, write spaces. Starting at `first`, write `$30 + d[i]` for each considered
   nibble; the two suffix spaces for Y=`$FF` remain spaces.

| Property | Uninterrupted model |
| --- | --- |
| Output | `$06` at `$0A1`, followed by the six characters at `$0A2-$0A7`. Invalid nibbles `$A-$F` yield `$3A-$3F`, except that Y=`$FF` produces two trailing spaces. |
| Return | A=`$06`; X and S are preserved; Y=`first`. For Y=`$FF` and A=X=`$00`, `first` is 4 and all six characters are spaces. |
| Status | N and Z clear. C clears only for Y=`$FF` and A=X=`$00`; otherwise C sets. B, I, D, and V are preserved, and bit 5 is set. |
| Timing | For Y other than `$FF`, `222 + 44 * first` master clocks when `first < 5`, or 422 when `first = 5`. For Y=`$FF`, `190 + 44 * first` clocks. |
| RAM | No non-stack RAM is read before being written; only `$0A1-$0A7` are written outside the stack. |
| Stack | For a normal JSR returning with S, the tested calls write `S-2` and read-before-write `S-3`, `S-1`, and S, modulo 256 within the stack page. |
| Hardware | No VRAM, LCD, AY, mapper, UART, scroll, or hardware-address-state changes; time-dependent hardware continues to advance. |

For example, `$12/$34/$56` produces `123456`, `$00/$00/$01` produces five spaces and `1`, and
`$00/$00/$FF` produces six spaces. The latter is not equivalent to formatting the invalid nibbles
`$F/$F`. The previous blanket statement that decimal mode changes this service was not supported by
the new controlled comparisons; D-set behavior is included in the model rather than excluded.

Across the four existing completed-call campaigns, 2,470 uninterrupted `$F033` calls matched the
candidate register, status, and timing model; 42 interrupt-spanning calls were excluded. These
campaign totals may overlap, and none of these calls used Y=`$FF`, invalid nibbles, or D set. The
formatter output bytes themselves are not present in the completed-call reports.

`gamatebiosf033 matrix` tested 526,848 synthetic calls in the first `$F033` context of each of the
16 cartridge images under each BIOS revision: 16,859,136 matches. Per context, it tested all
65,536 A/X pairs with Y=`$00`, `$99`, and `$FF`, plus 65,536 mixed 24-bit values, each with D
clear and set. For five additional inputs it exercised every entry-status byte and every stack
pointer, checking non-stack RAM access, stack wrapping, and the cartridge return dummy read.
Uninterrupted output, registers, timing, and peripheral effects were checked in every call.
Tornado used the no-input campaign; the other 15 used long Start pulses
`90:0x40:60,400:0x40:60,750:0x40:60`. The inputs were synthetic and the cartridge and BIOS files
remained read-only.

The uninterrupted model also matched all 16,777,216 A/X/Y combinations under each BIOS revision
with D clear and with D set: 67,108,864 calls in total. D-clear runs used the Tornado no-input
context; D-set runs used the Brick Blaster long-Start context. Entry I was set in these exhaustive
runs. They checked output, registers, status, timing, and peripheral effects, but do not replace
the broader context, stack, status, or interrupt matrices.

Build the diagnostic with `cmake --build build --target gamatebiosf033` and invoke
`build/gamatebiosf033 BIOS_FILE CARTRIDGE_FILE matrix`. `full-binary` and `full-decimal` enumerate
the 24-bit input domain with D clear and set, respectively. `interleave-irq`, `interleave-nmi`,
`interleave-flags-irq`, `interleave-flags-nmi`, `reentrant-irq`, and `reentrant-nmi` test
synthetic callbacks; `context-irq` and `context-nmi` test callback register preservation, and
`register-irq`, `register-nmi`, `register-early-irq`, `register-early-nmi`, and
`register-range-nmi` test register interference below. The `GAMATEBIOSCALLS_PRESS`
environment variable selects the input schedule, and each mode starts at the first observed `$F033`
call. These diagnostics require user-supplied local BIOS and cartridge files and do not alter the
production core.

#### Formatter interrupt-visible ordering

The output buffer is not written atomically. Address watches on five synthetic inputs at the first
call context of all 16 cartridge images under both BIOSes gave identical traces. The following
event model describes the observed output-buffer operations; offsets are emulator instruction-start
master clocks from service entry, excluding interrupt work, not physical bus-cycle measurements.

| Offset | Observable event |
| ---: | --- |
| 20, 58 | Write the characters derived from the low and high nibbles of A to `$0A3`, then `$0A2`. |
| 76, 110 | Write the characters derived from the low and high nibbles of X to `$0A5`, then `$0A4`. |
| 138, 172 | For Y other than `$FF`, write its low and high characters to `$0A7`, then `$0A6`. |
| 128, 134 | For Y=`$FF`, write spaces to `$0A6`, then `$0A7`. |
| 182 or 150 | Begin examining `$0A2` for Y other than `$FF`, or for Y=`$FF` respectively. |

Examine successive positions through `$0A6`, 44 clocks apart. If the current character equals
`$30`, it is replaced by a space 20 clocks after its examination; otherwise stop. The final length
write to `$0A1` occurs 22 clocks after the first nonzero-character examination, or two clocks
after the hypothetical next examination if the first five characters were zero. Carry is set if
the stopping character is at least `$30`, or if all five examined characters were zero. The
candidate model treats an interrupt callback's single buffer-byte write as occurring at the
interrupted outer instruction boundary.

Synthetic single-write IRQ and NMI callbacks matched this model for seven inputs, six callback
values (`$00`, `$20`, `$2F`, `$30`, `$31`, and `$FF`), all seven buffer addresses, and every
outer instruction boundary in all 32 caller/BIOS contexts (1,257,984 interrupted calls), with entry
P=`$20`. An equally sized matrix set D and V on entry; its NMI profile also set I. All 2,515,968
cases matched.
Callbacks were installed only in a private in-memory cartridge copy; direct NMI injection did not
simulate UART link traffic. This does not establish arbitrary callback programs, nested
asynchronous interrupts, physical bus timing, or callbacks that inspect or modify stacked state.

Another matrix used a callback that saves X/Y, invokes `$F033` once with a synthetic input, restores
X/Y, and returns without further interrupts. Applying the uninterrupted inner formatter result at the
outer interruption boundary and then resuming this event model matched all 209,664 cases: each of the
same seven outer and inner inputs, every outer instruction boundary, IRQ/NMI, and all 32 contexts,
with entry P=`$20`. This is one level of service reentry, not arbitrary asynchronous nesting.

Callbacks that change registers are outside that single-write model. In a synthetic call with
A/X/Y=`$12/$78/$9A`, an NMI callback that sets X=`$34` and Y=`$56` at outer offset 182 redirects
the formatter's next indexed RAM access. When RAM `$0F8` was deliberately initialized to `$30`,
that location became `$20` and Y returned `$57`, even though `$0F8` is outside the normal output
buffer. NMI at the adjacent tested boundaries, offsets 178 and 190, did not change `$0F8`. The
corresponding IRQ controls did not redirect the access because the IRQ wrapper restored Y. All
192 probes across those three offsets, both interrupt types, and all 32 caller/BIOS contexts
matched. A replacement cannot assume the uninterrupted RAM write set applies when callbacks clobber
registers. The example uses synthetic data and direct NMI injection, not a link-driven hardware
observation. With `$0F8` left at its authentic caller-context value instead, the same NMI returned
carry clear in 10 of the 32 contexts and set in the other 22; the underlying RAM bytes were not
retained.

With the same synthetic input and an NMI callback setting Y=`$5E`, a second controlled probe used
S=`$F0`, set stack-page `$100` to `$30`, and set `$000` to `$31`. At offset 182, `$100` became `$20`
and Y returned `$5F`; `$000` remained `$31`. Thus this redirected access can reach stack-page RAM,
not merely wrap within zero page. The adjacent-boundary and IRQ controls did not change `$100`.
All 384 tests of both target addresses, three boundaries, IRQ/NMI, and 32 caller/BIOS contexts
matched. The test did not overwrite a live return address and does not establish the consequences
of doing so.

For the same outer input and NMI boundary, a synthetic callback set Y to each value `$06-$FF` and
initialized RAM at `$0A2 + Y` to `$30` immediately before returning. Across all 32 caller/BIOS
contexts, each of the 8,000 cases left that target as `$20` and returned Y=`(Y + 1) & $FF`.
The targets span `$0A8-$1A1`; the `$FF` case wraps the returned Y to zero. The callback
initialization matters: an earlier probe found an NMI wrapper could change a targeted location
before the callback ran. These tests avoid live stack entries and do not establish the behavior
of overwriting them.

Earlier callback boundaries can also change the formatted digits. With the same input and callback
X=`$34`, an IRQ or NMI at offset 0 yielded X-derived characters `34`; at offset 68 they were
the mixed pair `38`; at offset 86 they remained `78`. An NMI additionally replaced the
Y-derived characters with the callback's Y digits, whereas an IRQ restored the original Y digits.
All 384 tests across both callback-Y values, three boundaries, both interrupt types, and all 32
contexts matched. These are selected timing examples, not an exhaustive model of register clobber.

### Strided RAM initializer: `$F024`

`$F024` is a fixed strided-RAM initializer when entered under the commercial calling convention with
decimal mode clear. It operates on two interleaved 16-byte address sequences:

- `$023, $02B, ... $09B` (16 locations, stride 8); and
- `$028, $030, ... $0A0` (16 locations, stride 8).

| Property | Controlled contract with D clear |
| --- | --- |
| Input registers | Tested A/X/Y values are ignored. |
| RAM effect | Reads and then writes all 32 listed locations, leaving every one as `$FF`; no other non-stack RAM is accessed. |
| Return | A and Y become `$80`; X becomes `$00`; S is restored. |
| Status | Z and V are set; N and C are clear; I is preserved. |
| Timing | 824 master clocks (412 CPU cycles), independent of tested registers and initial target bytes. |
| Hardware effects | None: VRAM/LCD state, scroll, AY selection/registers, mapper state, UART, IRQ, and NMI remain unchanged. |

Both BIOS revisions produced identical results. The address shape is consistent with initializing two
fields across sixteen eight-byte records, but the owning data structure and field meanings are not
yet established, so the narrower name **strided RAM initializer** is retained. Setting D changes the
loop arithmetic and result; no observed commercial call enters that mode, and D-set behavior is not
part of the replacement contract.

### Fixed plane-0 rectangle clear: `$F00F`

`$F00F` is a fixed plane-0 VRAM rectangle clear. Logical plane/address coverage from the controlled
probe establishes 25 rows of 31 consecutive bytes:

- rows 230 through 254;
- byte columns 0 through 30 (pixel columns 0 through 247); and
- plane 0 only.

Equivalently, it writes zero to `$1CC0-$1CDE`, then the corresponding 31-byte span every `$20`
addresses through `$1FC0-$1FDE`. It deliberately leaves byte column 31 and row 255 untouched.

| Property | Controlled contract |
| --- | --- |
| Input registers | Tested A/X/Y values are ignored. |
| Input RAM | Reads cached LCD control at `$015`; no other inherited non-stack RAM is read. |
| VRAM effect | Writes zero to 775 unique plane-0 locations forming the fixed 31-byte × 25-row rectangle. Initial VRAM contents do not affect coverage or output. |
| Return | A, X, and Y become zero; S is restored; final VRAM address is `$1FFE`. |
| Status | C and Z are set; N is clear; I, D, and V are preserved across tested entry states. |
| Timing | 15,930 master clocks (7,965 CPU cycles). |
| LCD state | Selects plane 0 and forces row increment while retaining the other cached mode bits from `$015`. Scroll is preserved. |
| Scratch RAM | Leaves `$018-$01A/$01C-$01D` as `$1F,$E6,$00/$FF,$19` and `$0A8` as zero. |
| Other hardware effects | No AY-register, mapper, UART, IRQ, or NMI effects. |

Both BIOS revisions produced identical register, RAM, VRAM, timing, and hardware results. The cleared
rectangle's later software purpose is not yet established, so the contract names the measured VRAM
operation without labeling it as a screen, font, sprite, or workspace clear.

### Command-stream tile-row blitter: `$F018`

Directly invoking `$F018` from cartridge-entry state does not supply the RAM parameter block it
expects. `gamatebiosprobe` therefore supports `occurrence=N`: it runs the cartridge to the Nth
authentic entry into the selected service, retains the real stack and caller RAM, applies any named
perturbations, and measures the matching return. This mode turns Tornado's formerly non-returning
synthetic probe into a balanced call.

With IRQs masked to separate the service from callbacks, Tornado's first `$F018` call has these
direct effects:

- 231,396 master clocks;
- 5,120 VRAM writes and 6,401 LCD-register writes;
- both planes, byte columns 0-19, over rows 0-31 and 56-151;
- `$FF` output over the first 32-row region and zero over the later 96-row region for its authentic
  source inputs;
- final VRAM address `$1313`, with plane 1 and row increment selected; and
- no AY, mapper, UART, or interrupt effects in the masked probe.

Entry A/X/Y perturbations do not change this result. Caller RAM supplies the real inputs. In Tornado,
`$000-$001` points to a 77-byte cartridge control stream. `$006-$009` supply two pointer-like lookup
bases: changing either low byte shifts one eight-byte cartridge-read span and changes the associated
plane output; making the two bases equal makes the two plane results correspondingly equal.

Diagnostic-only ROM coverage records physical offsets, never cartridge values. The authentic Tornado
call performs 5,275 BIOS-initiated cartridge reads from only 94 unique offsets: the 77-byte stream,
two eight-byte lookup spans, and one additional byte. The total is consistent with two passes over
the control stream plus one lookup read per output byte.

An opt-in `rom_trace` probe records the order of those physical offsets without values. Tornado's
complete trace resolves the broad structure:

- the same control stream is traversed twice, once per bitplane;
- emitted graphics are fetched in eight-byte records;
- each emitted eight-row band consists of 20 successive fetches of one such record, matching the
  20 byte-wide VRAM coverage;
- 16 emitted bands account exactly for the 128 written rows per plane; and
- a longer control-only interval coincides with the untouched 24-row gap.

Ordered traces from Baseball, Enchanted Bricks, Galaxy Invaders, Mighty Tank, and Witty Apee show the
same general interleaving of a sequential control stream with eight-byte graphics records. Complex
screens select many records rather than repeating one. Bitwise, in-memory perturbations of Tornado's
control fields resolve the stream grammar without retaining its original bytes:

| Field | Measured interpretation | Perturbation result |
| --- | --- | --- |
| Row selector | Absolute eight-pixel-high VRAM band | A one-bit change moves that band's writes by the corresponding tile-row distance. |
| Tile index | Eight-byte record at `plane_base + index * 8` | A one-bit change shifts only that run's eight-byte ROM-read span by the predicted amount. |
| Run length | Horizontal repetition count in byte columns | Bit changes add or remove exactly eight VRAM writes per column per active plane. |
| `$FF` | End of the current row's run list | Changing it causes subsequent fields to be consumed as another run until a later delimiter resynchronizes parsing. |
| `$FE` | End of the complete stream | Changing the final marker prevents the authentic call from returning cleanly within the probe bound. |

A row begins with its selector, followed by zero or more `(tile_index, run_length)` pairs and `$FF`.
After `$FF`, the next byte is another row selector or `$FE`. Tile index zero is transparent: it
advances the horizontal position by the run length without reading graphics or writing VRAM. This
explains Tornado's 24-row untouched interval: those rows consist entirely of transparent runs rather
than a special skip opcode. Nonzero indices fetch one eight-byte vertical tile record and repeat it
horizontally. Baseball's trace repeatedly fetches only the next two control bytes between different
eight-byte records, independently confirming that one row may contain multiple run pairs. The BIOS
traverses the complete stream again with the other plane's base pointer.

This identifies `$F018` as a command-stream-driven, two-plane tile-row blitter. The stream supports
transparent and repeated-tile runs, but no independently observed command currently establishes a
more general compression scheme; the earlier generic “decompressor” label is therefore withdrawn.

The operation is not fixed-size across software. First authentic calls from six representative titles
range from 640 to 5,120 VRAM writes and 40,716 to 236,184 master clocks. The caller comparison below
tests whether its parameter-block roles remain consistent across that variation.

#### Caller parameter-word comparison

Low-byte XOR perturbations establish the same three pointer roles in every known first-call `$F018`
caller. “Geometry preserved” means clocks and VRAM-write count remain at baseline while the output
hash changes; small ROM-read-count changes can occur when a one-byte base shift crosses an indexed
record boundary.

| Cartridge | `$000-$001` control pointer bit-0 XOR | `$006-$007` first plane base | `$008-$009` second plane base |
| --- | --- | --- | --- |
| 4-in-1 | Malformed; no clean bounded return | Geometry preserved; output changed | Geometry preserved; output changed |
| Baseball | Runaway/misaligned work before return | Geometry preserved; output changed | Geometry preserved; output changed |
| Devil Castle | Runaway/misaligned work before return | Geometry preserved; output changed | Geometry preserved; output changed |
| Enchanted Bricks | Runaway/misaligned work before return | Geometry preserved; output changed | Geometry preserved; output changed |
| Galaxy Invaders | Malformed; no clean bounded return | Geometry preserved; output changed | Geometry preserved; output changed |
| GP Race | Runaway/misaligned work before return | Geometry preserved; output changed | Geometry preserved; output changed |
| Kung-Fu Fighter | Immediate termination with no VRAM writes | Geometry preserved; output changed | Geometry preserved; output changed |
| Mighty Tank | Runaway/misaligned work before return | Geometry preserved; output changed | Geometry preserved; output changed |
| Tornado | Malformed; no clean bounded return | Geometry preserved; output changed | Geometry preserved; output changed |
| Vindicators | Runaway/misaligned work before return | Geometry preserved; output changed | Geometry preserved; output changed |
| Witty Apee | Immediate termination with no VRAM writes | Geometry preserved; output changed | Geometry preserved; output changed |

For Tornado, the ordered physical reads make the base-pointer result especially direct: perturbing
`$006` shifts only the first eight-byte graphics span by one byte, and perturbing `$008` shifts only
the second. The stream span does not move. Conversely, perturbing `$000` affects stream parsing rather
than either graphics base. The cross-title matrix establishes these as stable caller ABI fields rather
than a Tornado-specific layout.

Full-byte entry perturbations of `$002-$005` leave clocks, ordered ROM reads, VRAM coverage, write
count, and output unchanged in all 11 callers. Access coverage separates the pairs: `$002-$003` are
overwritten deterministically by every measured call and therefore serve as service scratch/output;
`$004-$005` are neither read nor written and remain caller-preserved. They are not required `$F018`
inputs in the observed first-call population.

### Cross-corpus command-stream structure

The completed-call inventory identifies 15 `$F018` caller titles. Their first authentic calls were
probed under both BIOS revisions with I set to isolate the service (30 contexts). In every context,
the stream parsed from the first cartridge-ROM read using the row/run grammar above, and the
resulting active-column count predicted the measured VRAM writes exactly:
`16 * sum(run_length for nonzero tile indices)`.

Across the 15 per-title streams, there were 249 rows and 1,244 run pairs. Of those pairs, 257 used
the transparent tile index; the streams described 2,791 active and 5,238 total byte columns. Each
stream contained 6-24 rows and 19-194 run pairs. Row spans ranged from 11 through 32 columns; no row
selector exceeded 31 and no run length was zero. All 30 contexts matched the write formula, and the
measured address traces, stream structure, timing, returned registers, write counts, and VRAM hashes
matched between BIOS revisions in all 15 title pairs.

This first-call sweep extended the grammar beyond the previously inspected callers. The executable
model and broader later-call investigation below resolve its previously open ordering, returns,
scratch effects and timing. The sweep used address-only traces and transient local ROM reads to
derive counts; no stream or graphics payload was saved or published.

#### `$F018` executable contract

The diagnostic-only `gamatebiosf018` model reproduces the observed two-pass contract at LCD-port
granularity. The rules below describe terminating streams whose reads use ordinary memory and do
not alias the service's mutable scratch or stack. They are execution evidence from the emulator,
not independent physical measurements.

Inputs and stream:

- `$000-$001` is the little-endian stream pointer. `$006-$007` and `$008-$009` are the plane-0
  and plane-1 tile bases. `$015` is the cached LCD control byte. Entry A, X, Y and C are not
  inputs to the graphics operation.
- The stream consists of rows. A row starts with a row selector, then zero or more
  `(tile_index, count)` pairs, followed by `$FF`. The next byte is another row selector; `$FE`
  in that position ends the entire stream. `$FF` is a valid row selector, and `$FE` is a valid
  tile index; `$FF` is reserved when a tile index is expected.
- Tile index zero is transparent: it advances the column by `count` and emits nothing. A zero
  count therefore advances by zero. A nonzero index emits the selected tile `count` times; a zero
  count emits it 256 times.
- For each plane, the eight source bytes for tile `t` are read from `base + 8*t` through
  `base + 8*t + 7`, with 16-bit address wrapping. They are fetched again for each repeated column,
  not cached. Source graphics and the stream are supplied by the caller.
- Cartridge reads follow that stream/tile order in each pass. Indexed page crossings add the
  corresponding same-page dummy read before the effective read. A final cartridge read at the
  pulled return address is the CPU's RTS dummy read, not stream or tile data.

LCD port sequence:

1. Write `$015 | $40` to LCD register 1, forcing increment-by-row mode.
2. Process the complete stream once with the first tile base, then restart it with the second.
   The initial eight-bit column is `$00` on the first pass and `$80` on the second, and it is reset
   to that value at each row.
3. For each nontransparent output column, write register 4 = column, register 5 = `(row << 3) &
   $FF`, and then the tile's eight bytes to register 7. Increment the eight-bit column after each
   tile. Transparent pairs add their count to the column without issuing LCD writes.

The two passes normally select planes 0 and 1 via register 4 bit 7. This is not a clipping
guarantee: columns past `$7F` can change that bit, and the column wraps modulo 256. Other register-4
bits and the forced increment mode are interpreted by the LCD; the service does not constrain the
stream to the visible 20 columns. With D set, the transparent-column addition uses NMOS decimal
arithmetic; ordinary per-tile column increments remain binary.

Returns and RAM:

- A returns `$FE`; Y is the low byte of the terminator's stream index; S is restored. X returns
  `$FF` if any nonzero tile was emitted, otherwise it retains its entry value.
- `$000` is preserved, and `$001` is the entry high byte plus `terminator_index >> 8`, modulo
  256. Both bytes are written even for an immediately terminating stream.
- `$018` and `$019` are written when a row exists. At return they contain, respectively, the
  final second-pass column and the final row selector.
- `$002-$003`, `$01A` and `$01B` are written when a pair exists. At return, `$002-$003` is the
  second tile base with its high byte incremented by `last_tile_index >> 5`, `$01A` is
  `(last_tile_index << 3) & $FF`, and `$01B` is the last count for a transparent pair or zero
  for an opaque pair. Bytes not written retain their entry values.
- No other non-stack RAM is written in the tested calls. The largest observed masked stack depth
  was six bytes below entry S, excluding the caller's return address. No AY or bank-register
  writes or UART clocks were observed; UART transmit state and scroll registers were preserved.

Status, using the emulator's stored status-byte representation:

`P_out = (P_in & $1C) | $23 | V`

I and D are preserved, N is clear, and Z and C are set. Bit 4 is the emulator's stored,
non-physical B bit. V begins with entry V and is updated in stream order across both passes:

- A transparent pair sets V to the NMOS signed-overflow result of adding its count to the current
  column with carry clear. D selects decimal addition and its NMOS overflow behavior.
- A nonzero tile index of `$20` or greater clears V. A nonzero index below `$20` leaves V
  unchanged. Rows with no pairs do not change V.

Timing, in master clocks, for a call without an interrupt:

```text
144 + 260*rows + 472*pairs + 64*sum(tile_index >> 5)
    + 660*opaque_columns + 4*stream_crossings + 16*stream_wraps
    + 2*tile_crossing_reads
```

`rows`, `pairs`, and `sum(tile_index >> 5)` count the stream once, not once per pass.
`opaque_columns` sums nonzero-tile counts, interpreting zero as 256. `stream_wraps` is
`terminator_index >> 8`; `stream_crossings` counts each stream index `i` through the terminator
for which `(pointer & $FF) + (i & $FF) > $FF`. `tile_crossing_reads` counts, across both passes
and every repeated tile, the individual source-byte indexes `j = 0..7` for which
`(base & $FF) + ((tile_index << 3) & $FF) + j > $FF`. The high-byte tile adjustment does not
change that crossing test. The decimal flag and `$015` do not independently change these clock
terms.

#### Validation and limits

- The four standard 1,000-frame input campaigns covered all 62 cartridges under both BIOSes.
  At every reached entry the checker replayed a snapshot with I set: 326 calls, 163 per BIOS, in
  25 titles. All returned with zero ordered cartridge-read, LCD-port, register/status, non-stack
  RAM, checked-peripheral, or clock mismatches. No entry had D set.
- Replays with the original interrupt state also matched all port and return-state checks; 246
  spanned an IRQ or NMI. Elapsed time was nonnegative relative to the masked model, and calls
  without an interrupt had identical timing. This does not establish behavior when a handler
  itself changes the stream, scratch memory or LCD ports.
- Under both BIOSes and in two title contexts, `selftest` passed 1,373 cases covering all tile
  indices, row selectors, LCD-control values, count bytes and source low bytes, plus zero-page and
  other RAM sources and multi-page streams. `selftest-open` passed 1,280 cases covering all entry
  status bytes with pending interrupts cleared for the short synthetic calls. `selftest-flags`
  passed all 65,536 pairs of transparent counts in each of binary and decimal mode. An additional
  1,200 seeded varied streams matched each BIOS in Tornado.
- The model checks ordinary RAM, cartridge and BIOS reads; it explicitly rejects memory-mapped
  I/O and mutable scratch or stack aliases as sources. No claim is made for those aliases,
  unterminated streams, unobserved callback interference, or every possible future game path.
  The BIOS/ROM files remained read-only; synthetic ROM contents existed only in a private copy.
  Comparisons retained derived results rather than firmware, cartridge or graphics payload.

### Plane-0 row-run stream service: `$F01B`

`$F01B` is fully defined for every observed calling context and for the synthetic edge classes
listed below. It clears a fixed 25-row band of plane 0, then decodes a caller-supplied row/run
stream into LCD port writes. An independent model in `tests/gamatebiosrowstream.c` (service
argument `F01B`) reproduces the BIOS's LCD port-write sequence, returned registers and status, RAM
side effects, stack use, and exact elapsed clocks.

#### Contract

Inputs:

- RAM `$000-$001` is a little-endian pointer to the stream. The stream may be in cartridge ROM or
  RAM. Byte `i` is read from `pointer + i` through an indexed-indirect walk; `$000` is never
  written.
- RAM `$015` is the caller's cached LCD control byte.
- Entry A, X, and Y are ignored. Entry C is ignored. Entry D affects row arithmetic, described
  below; every authentic call enters with D clear.

LCD port sequence, all at `$5000-$5007`:

1. Write `$015` verbatim to register 1.
2. Clear rows `$E6-$FE` (25 rows). For each row in ascending order, write register 5 = row,
   register 4 = `$00`, then 32 writes of `$00` to register 7.
3. Decode the stream:
   - Byte 0 is a row offset. Offset `$FE` terminates the stream.
   - Any other offset selects absolute row `$E6 + offset`, computed with carry clear. The service
     writes register 5 = row, then register 4 = `$00`.
   - The next byte is a value. Value `$FF` is an escape: the following byte is a new row offset,
     which is again absolute from `$E6`, not relative to the previous row. Offset `$FE`
     terminates.
   - Any other value is followed by a count byte. The service writes the value to register 7
     `count` times, with count `$00` meaning 256. It then reads the next value.

Consequences:

- Register 4 is always `$00`, so every write targets plane 0 from the start of the selected row.
- Value `$FF` cannot be emitted as data.
- Row offsets wrap modulo 256. For example, offset `$19` selects row `$FF`, and offset `$1A`
  selects row `$00`.
- Register-1 bits, including the increment-by-row bit 6, apply to the same port sequence. The
  BIOS does not alter its port sequence when that bit is set.

Returns and side effects:

- A=`$FE` and X=`$00`. Y is the low byte of the terminator's stream index. S is restored.
- The service uses at most 2 stack bytes below its entry S, not counting the caller's return
  address.
- `$001` becomes the entry value plus `index >> 8`, where `index` is the terminator's stream
  index. It is written only when the index wrapped past `$FF`.
- `$01A` is written only if at least one value/count pair was decoded. It then holds the last
  emitted run value.
- No other RAM below `$400` is written outside the stack page.
- No AY or bank-register writes occur, the UART is not clocked, UART transmit state is
  unchanged, and the scroll registers are not written.

Status:

`P_out = (P_in & $1C) | $23 | V`

- N is clear; Z and C are set; I and D are preserved.
- V is the signed overflow of the last `$E6 + offset` row addition, computed with carry clear.
  With D clear, V is set for offsets `$80-$99`.
- Bit 4 is the emulator's stored, non-physical B bit, carried through from entry.

Timing in master clocks, with CPU cycles in parentheses:

```text
15362 (7681)
+ 140 (70) * row_commands
+ 134 (67) * pairs
+  18 (9)  * (run_bytes - pairs)
+   2 (1)  * crossing_reads
+   8 (4)  * Y_wraps
```

- `row_commands` counts non-terminating offsets.
- `pairs` counts decoded value/count pairs.
- `run_bytes` is the sum of counts, with `$00` counted as 256.
- `crossing_reads` counts stream indexes `i` from 0 through the terminator index for which
  `(pointer & $FF) + (i & $FF) > $FF`. These are the indexed-indirect page-crossing penalty reads.
- `Y_wraps` is `index >> 8`.
- `$015` and decimal mode do not affect timing.
- Interrupts add only handler time.

Decimal mode:

- The service does not clear D.
- With D set, each row is the NMOS decimal-mode addition of `$E6 + offset` with carry clear, and V
  follows the same decimal sequence.
- Every other port write, return value, RAM effect, and timing value is unchanged.

No authentic caller enters with D set. A replacement BIOS therefore needs binary row addition only
to match the corpus. D-set behavior also depends on the emulator's CPU decimal implementation,
which is not a physical measurement.

Interrupts:

- The service does not mask interrupts.
- All authentic calls enter with I clear, and most take IRQs mid-service.
- An interrupt changes neither the service's port sequence nor its register and status results.
  This assumes the game's own handler does not use the LCD ports, which held in the corpus.

Practical note: a cartridge address read immediately before return, such as `$A0BE` or `$794C`
depending on the title, is the CPU's RTS dummy read at the pulled return address. It is not a
service input.

#### Validation

- **Authentic corpus:**
  - Coverage: the four standard 1,000-frame input campaigns (no input, long Start, menu actions,
    and controller cycle) across all 62 cartridges and both BIOS revisions, 496 runs.
  - At every `$F01B` entry, an I-masked copy of the machine was replayed and compared with the model.
  - Calls and titles: 242 calls, 121 per BIOS, in 23 titles.
  - Results: zero port, register, status, RAM, peripheral, or clock mismatches.
  - Stream properties: all streams came from cartridge ROM. The maximum stream used 334 pairs, 800
    run bytes, 25 row commands, 2 Y wraps, and 395 crossing reads.
  - Two-wrap streams appear in Nightmare of Santa Claus and Treasure Hunter.
  - No authentic call produced a row-addition overflow or entered with D set.
- **Unmasked replay:**
  - Coverage: the 23 caller titles under the same campaigns.
  - Results: all 242 calls entered with I clear, 224 spanned at least one IRQ or NMI, and all
    returned. Every call had zero port, register, or status mismatches.
  - Timing: extra elapsed time was 0 to 1,628 master clocks, consistent with handler execution
    alone.
- **Synthetic streams:**
  - Method: the checker snapshots Baseball's first entry and replays caller-style streams through
    the authentic BIOS under both revisions.
  - Coverage:
    - 400 random RAM streams;
    - 200 RAM streams with random entry A, X, Y, and P (D clear) and random `$015`;
    - 120 private-copy ROM streams with up to 4 Y wraps and 1,244 crossing reads;
    - 120 `$015` values with bits 1-3 set;
    - 120 D-set entries.
  - Edge cases: these cases include count `$00`, escape-only rows, offset `$FF`, row wrap, and
    overflowing offsets.
  - Results: every case matches under both BIOS revisions.

#### Corrections to earlier `$F01B` findings

- The earlier formula mapping `$015` to final LCD flags was not a BIOS transformation. The BIOS
  writes `$015` verbatim to register 1. The formula was the earlier probe's re-encoding of the
  emulator's decoded register-1 fields. Bits 1-3 had no observed effect because the emulator does
  not model them, not because the BIOS discards them.
- The "sparse" bit-6 coverage was LCD increment-by-row mode applied to the same port sequence. It
  was not a separate BIOS addressing mode.
- The common `$1CC0-$1FDF` region is the fixed 25-row clear of rows `$E6-$FE`.
- Cosmic Fighter's extent to `$1FF3`, and caller-dependent final VRAM pointers, come from stream
  content. One example is an offset selecting row `$FF`.
- The `$000`/`$001` XOR results follow from pointer semantics. Shifting the pointer redirects
  decoding into unrelated bytes, which explains the large and caller-dependent changes in clocks
  and registers.
- The earlier status formula `(P_in & $0C) | $23` is the V-clear case of the full formula.
- The format, source mapping, return-Y rule, and timing previously listed as unresolved are
  resolved above.
- Earlier counts of 34 calls in 12 titles came from a narrower no-input call inventory. The full
  campaign corpus contains 242 calls in 23 titles, including Incantational Couple.

#### Limits

- The contract is derived from the emulator's execution of the authentic BIOS revisions. Timing
  is the emulator's validated CPU and bus model, not a separate physical measurement of this
  service.
- D-set behavior inherits the emulator's NMOS-style decimal arithmetic and is outside every
  observed caller.
- Behavior when a stream never terminates is not part of the contract.
- The fixed-window campaigns do not prove that other titles never call `$F01B`.
- Retained evidence consists of derived counts, model comparisons, and synthetic
  stream descriptions. No BIOS, ROM, stream, RAM, or VRAM payload bytes were retained.
- Synthetic ROM streams were written only into the checker's private in-memory copy.

### Caller-supplied two-plane graphics service: `$F01E`

The historical observations in this section are extended by the executable contract below.

The 1,000-frame slot inventory records 110 `$F01E` entries per BIOS across 19 titles. The paired
balanced-call corpus contains 216 calls, 108 per BIOS across 18 titles. All reported fields match
between revisions. A returns `$FE`, X returns `$FF`, and S is preserved in every completed call;
Y varies across callers. Calls report 0-7,984 VRAM writes, with LCD writes exactly
`5 * (VRAM writes / 4) + 1`. Since 212 of the 216 completed calls span IRQs, these aggregate counts
and elapsed times include callback work.

Controlled first-call probes in Famous 7, Time Warrior, Volcano Panic, and Robin Hood set I to
isolate the service and were paired across both BIOS revisions. The measured service time varies
from 52,184 clocks in Famous 7 to 453,760 in Time Warrior, consistent with work driven by caller
data rather than a fixed-cost helper. The isolated probes had no AY, mapper, UART, IRQ, or NMI
effects. Separate A, X, and Y perturbations in Famous 7 and Time Warrior did not change their
measured graphics output or ROM-read profile. In these two contexts, Y's return value was also
independent of the tested entry Y; its result differs by context, so no general return rule is
established.

The access maps in Time Warrior, Volcano Panic, and Robin Hood read `$000-$001`, `$006-$009`, and
`$015`; the first pair is also rewritten, while `$006-$009` and `$015` are preserved. In Time Warrior,
one-bit XORs to `$000-$001` change the ROM-read coverage and operation. XORing bit 0 of `$000`
specifically produces a 144-clock return with no VRAM writes, while changing `$001` selects a
different read/output profile. These tests were consistent with `$000-$001` supplying a ROM-side
control or descriptor address, but did not establish its format.

In Time Warrior, `$006-$007` and `$008-$009` act as separate 16-bit source bases for the two VRAM
planes. One-bit XORs to either byte of the first pair changed the streamed output hash for plane 0
only; XORs to either byte of the second pair changed plane 1 only. Each change preserved the tested
write count and coverage while shifting the corresponding ROM-read coverage. Output bytes were
streamed directly into per-plane hashes and counts; no graphics payload was retained.

`$015` is an active LCD-control input in the tested contexts: XORing its low bit changes the final
LCD flags from `$03` to `$23` in both Famous 7 and Time Warrior, without changing their VRAM output
hashes or write counts. The bit's physical-hardware meaning remains unmeasured. At this stage the
exact control-stream grammar, source units and record layout, return-Y rule, and status-flag
contract were not yet established.
Controlled whole-RAM hashes differed between BIOS revisions in the four probed titles despite
matching access maps and measured operation fields. The paired-snapshot follow-up below localizes
those differences to pre-existing, unchanged stack bytes. These are emulator observations, not
physical-hardware measurements.

#### Cross-corpus source-byte perturbations

The four existing 1,000-frame campaign reports identify `$F01E` calls in 24 titles. For each title,
the earliest campaign with calls under both BIOS revisions supplied the first authentic call
context. I-masked baselines and independent XOR-1 perturbations to `$006`, `$007`, `$008`, and
`$009` produced 240 probes, all of which returned.

Each tested byte was read before being written in all 48 title/BIOS contexts and was never written.
Perturbing `$006` or `$007` changed the plane-0 output hash in 46 contexts and never changed the
plane-1 hash. Perturbing `$008` or `$009` changed the plane-1 output hash in 46 contexts and never
changed the plane-0 hash. No perturbation changed either plane's address coverage or the VRAM/LCD
write counts, and returned registers were unchanged. The address-sequence summary changed in 46
contexts for each byte. Timing changed in 42 contexts for `$006` (deltas -8 to +36 master clocks)
and 40 for `$008` (deltas -10 to +36); `$007` and `$009` did not change timing.

Baseline and perturbed checked output signatures, per-plane hashes and coverage, RAM access maps,
and captured ROM-address sequence summaries agreed between BIOS revisions in all 24 title pairs.
Whole-RAM hashes and changed-byte summaries were excluded from those comparisons. Two of the 48
sequence comparisons per byte involved a trace over the diagnostic capture limit, so full sequence
equality is not claimed for those comparisons. The results support `$006-$007` as plane-0 source
inputs and `$008-$009` as plane-1 source inputs across this caller set, but did not establish the
exact address transform or stream grammar. VRAM bytes were hashed as they streamed from the
diagnostic and were not retained. These are emulator observations, not hardware measurements.

#### Cross-corpus `$000-$001` perturbations

The same 24 first-call contexts were probed with I set using a baseline and independent XOR-1
perturbations to `$000` and `$001` (144 probes total). All returned. Each byte was read before
being written and was rewritten by the service in all 48 title/BIOS contexts.

Both perturbations changed the address-sequence summary and timing in all 48 contexts. XORing
`$000` changed both plane hashes and both planes' coverage in 44 contexts, changed write counts in
32, and changed the returned register signature in all 48. Its timing deltas ranged from -453,616
to +1,084 master clocks. XORing `$001` changed both plane hashes, both coverages, and write counts
in all 48 contexts; the returned register signature changed in 28. Its timing deltas ranged from
-412,844 to +47,848,702 master clocks. These are deliberately perturbed inputs, not normal-call
timing bounds.

Baseline checked outputs and address-sequence summaries agreed between BIOS revisions in all 24
title pairs. Perturbed `$000` comparisons agreed in all 24 pairs; `$001` comparisons agreed in 23.
Whole-RAM hashes and changed-byte summaries were excluded. Two of the 48 `$000` sequence
comparisons and 14 of the 48 `$001` comparisons involved a trace over the diagnostic capture limit;
full address-sequence equality is not claimed for those comparisons. Both bytes are behaviorally
influential and rewritten, but these low-bit tests did not establish whether they encoded a pointer,
descriptor, or mutable parser state.

#### `$F01E` executable contract

For terminating streams whose reads use ordinary memory and do not alias mutable scratch or stack,
the diagnostic-only `gamatebiosf01e` model specifies the observed service as follows. Its grammar
is related to `$F018`, but each selector here represents exactly one column and has no count byte.

Inputs and stream:

- `$000-$001` is the little-endian stream pointer; `$006-$007` and `$008-$009` are the first- and
  second-pass tile bases. `$015` is the cached LCD control byte. Entry A, X, Y, and C do not affect
  graphics output.
- A row begins with a row selector followed by zero or more tile selectors and `$FF`. The next byte
  begins another row; `$FE` in row-selector position terminates the stream. `$FF` is a valid row
  selector, and `$FE` is a valid tile selector.
- A zero tile selector advances the column by one without writing a tile. Any other selector `t`
  emits eight bytes read from `base + 8*t` through `base + 8*t + 7`, with 16-bit wrapping.
  The stream and selected graphics are read again during the second pass.
- Cartridge reads follow stream and tile order, including indexed page-crossing dummy reads.
  An additional read at the pulled return address is the CPU's RTS dummy read, not stream data.

LCD port sequence:

1. Write `$015 | $40` to LCD register 1, forcing increment-by-row mode.
2. Process the entire stream using the first tile base, then restart it using the second. At each
   row, start the eight-bit column at `$00` for the first pass or `$80` for the second.
3. For each nonzero tile selector, write register 4 = column, register 5 = `(row << 3) & $FF`,
   then the eight source bytes to register 7. Increment the column modulo 256 after each tile.
   A zero selector advances the column without issuing LCD writes.

Register-4 bit 7 normally selects the plane, but the service does not clip columns: exceeding
`$7F` can change the selected plane. With D set, the zero-selector column addition uses NMOS
decimal arithmetic; ordinary output-column increments are binary.

Returns and side effects:

- A=`$FE`; Y is the low byte of the terminator's stream index; S is restored. X=`$FF` if any
  nonzero tile was emitted, otherwise entry X is retained.
- `$000` is preserved, and `$001` becomes the entry high byte plus `terminator_index >> 8`, modulo
  256. Both bytes are written even for immediate termination.
- If any row exists, `$018` and `$019` contain the final second-pass column and row selector.
- If any tile selector exists, `$002-$003` contains the second tile base with its high byte
  incremented by `last_selector >> 5`, `$01A` is `(last_selector << 3) & $FF`, and `$01B` is
  one for a zero selector or zero for a nonzero selector. Unwritten bytes retain their entry values.
- No other non-stack RAM writes were observed. The maximum measured masked stack depth was six
  bytes below entry S. No AY or bank-register writes or UART clocks were observed; UART transmit
  state and scroll registers were preserved.

Status, using the emulator's stored status-byte representation:

`P_out = (P_in & $1C) | $23 | V`

I and D are preserved, N is clear, and Z and C are set. Bit 4 is the emulator's stored,
non-physical B bit. V begins with entry V and updates in stream order over both passes:

- A zero selector sets V to the NMOS signed-overflow result of adding one to the current column
  with carry clear. D selects decimal addition and its NMOS overflow behavior.
- A selector of `$20` or greater clears V. A nonzero selector below `$20` and a row without
  selectors leave V unchanged.

Timing in master clocks without interrupts:

```text
144 + 260*rows + 392*selectors + 64*sum(selector >> 5)
    + 660*opaque_columns + 4*stream_crossings + 16*stream_wraps
    + 2*tile_crossing_reads
```

Rows, selectors and the selector sum count the stream once, not once per pass. `opaque_columns`
is the number of nonzero selectors. `stream_wraps = terminator_index >> 8`. `stream_crossings`
counts stream indexes `i` through the terminator for which
`(pointer & $FF) + (i & $FF) > $FF`. `tile_crossing_reads` counts individual tile-byte reads
across both passes for which `(base & $FF) + ((selector << 3) & $FF) + j > $FF`, with `j` from
0 through 7. Neither D nor `$015` changes these clock terms.

#### `$F01E` validation and limits

- The four standard 1,000-frame campaigns covered all 62 cartridges under both BIOSes. The 808
  calls, 404 per BIOS in 25 titles, all matched the masked model for ordered cartridge addresses,
  LCD port values, return registers and status, non-stack RAM writes, selected peripheral state,
  and elapsed clocks. None entered with D set. The four earlier no-input title/BIOS jobs were
  included without rerunning them.
- All 808 original-interrupt-state replays matched port and return-state checks; 736 spanned an
  IRQ or NMI. Elapsed time was nonnegative relative to the masked model, and calls without an
  interrupt had identical timing.
- Both BIOSes passed 838 synthetic boundary cases, including zero-page, ordinary RAM, and BIOS
  tile sources; 1,280 entry-status cases; 1,024 transparent column/decimal/overflow cases; and
  1,200 seeded varied streams in Time Warrior.
- Paired first-call snapshots in Famous 7, Time Warrior, Volcano Panic, and Robin Hood differed
  only in one or two stack bytes before the call. Those bytes remained unchanged, and the calls
  introduced no new RAM differences, explaining the earlier whole-RAM hash discrepancies in
  these four contexts. No stack contents were published.
- These are execution observations in the emulator, not physical measurements. The checker
  excludes side-effectful I/O and mutable scratch or stack aliases as sources. Unterminated streams
  and interrupt handlers that modify the source, scratch, or LCD ports are outside the tested
  contract. No firmware or cartridge payload is retained or published.

### Plane-0 row-literal stream service: `$F021`

`$F021` is fully defined for every observed calling context and for the synthetic edge classes
listed below. It is the literal-byte sibling of [`$F01B`](#plane-0-row-run-stream-service-f01b):
the LCD preamble, row commands, escape, terminator, pointer handling, return registers, and status
rule are the same, but each data byte is written once instead of being decoded as a value/count
pair. The independent model in `tests/gamatebiosrowstream.c` (service argument `F021`) reproduces
the BIOS's LCD port-write sequence, returned registers and status, RAM side effects, stack use, and
exact elapsed clocks.

#### Contract

Inputs:

- RAM `$000-$001` is a little-endian pointer to the stream. The stream may be in cartridge ROM or
  RAM, and it is read sequentially from `pointer + i`. `$000` is never written.
- RAM `$015` is written verbatim to LCD register 1.
- Entry A, X, Y, and C are ignored. Entry D affects row arithmetic exactly as in `$F01B`; every
  authentic call enters with D clear.

LCD port sequence:

1. Write `$015` to register 1.
2. Clear rows `$E6-$FE`. For each row, write register 5 = row, register 4 = `$00`, then 32 writes of
   `$00` to register 7.
3. Decode the stream:
   - Byte 0 is a row offset. Offset `$FE` terminates the stream.
   - Any other offset selects absolute row `$E6 + offset`, computed with carry clear. The service
     writes register 5 = row, then register 4 = `$00`.
   - Each following byte other than `$FF` is written once to register 7. These bytes may include
     `$FE`.
   - `$FF` is an escape: the next byte is a new absolute row offset, or `$FE` to terminate.

The value `$FF` therefore cannot be written as data. All output goes to plane 0, starting at
column 0 of each selected row. Register-1 bits such as increment-by-row change how the LCD applies
the same port sequence; the BIOS does not alter the sequence.

Returns and side effects:

- A=`$FE` and X=`$00`. Y is the low byte of the terminator's stream index. S is restored.
- `$001` advances by `index >> 8` and is written only when the index wraps.
- No other RAM below `$400` is written outside the stack page. Unlike `$F01B`, `$F021` does not
  write `$01A`.
- The service uses at most 2 stack bytes below its entry S, not counting the caller's return
  address.
- No AY or bank-register writes occur, the UART is not clocked, UART transmit state is unchanged,
  and the scroll registers are not written.

Status:

`P_out = (P_in & $1C) | $23 | V`

V is the signed overflow of the last row addition, computed exactly as in `$F01B`.

Timing in master clocks, with CPU cycles in parentheses:

```text
15362 (7681)
+ 140 (70) * row_commands
+  66 (33) * literal_bytes
+   2 (1)  * crossing_reads
+   8 (4)  * Y_wraps
```

- `literal_bytes` counts bytes written after row commands.
- Crossing reads and Y wraps are defined as in `$F01B`.
- `$015` and decimal mode do not affect timing.
- Interrupts add only handler time.

Interrupts:

- The service does not mask interrupts, and authentic calls enter with I clear.
- An interrupt changes neither the port sequence nor the register and status results. This assumes
  the game's handler does not use the LCD ports, which held in the corpus.

#### Validation

- **Authentic corpus:**
  - Coverage: the four standard 1,000-frame input campaigns across all 62 cartridges and both BIOS
    revisions, 496 runs.
  - Calls and titles: 406 calls, 203 per BIOS, in 19 titles.
  - Results: an I-masked replay at every entry had zero port, register, status, RAM, peripheral, or
    clock mismatches.
  - Stream properties: every stream was in cartridge ROM. The largest call used 25 row commands,
    800 literal bytes, 3 Y wraps, and 387 crossing reads. No authentic call entered with D set or
    produced an overflowing row addition.
- **Unmasked replay:**
  - Results: all 406 calls entered with I clear, and 280 spanned interrupts. Every call returned
    with zero port, register, or status mismatches.
  - Timing: interrupts added at most 1,918 master clocks of handler time.
- **Synthetic streams:**
  - Method: the checker snapshots Volcano Panic's first entry and replays streams under both BIOS
    revisions.
  - Coverage:
    - 150 RAM streams with random entry A, X, Y, P (D clear), and `$015`;
    - 150 private-copy ROM streams with up to 4 Y wraps and 1,151 crossing reads;
    - 150 D-set entries.
  - Edge cases: these include escape-only rows, offset `$FF`, literal `$FE`, row wrap, and
    overflowing offsets.
  - Results: every case matches.
- **Earlier inventory:** that inventory listed 14 caller titles. The broader campaigns also find
  `$F021` calls in Baseball, Incantational Couple, Legend of Dragon Knight, Pipemania, and the 4-in-1
  cartridge.

#### Corrections to earlier `$F021` findings

- The `$015` "flag mapping" and bit-6 "sparse addressing pattern" are the same artifacts corrected
  for `$F01B`. The BIOS writes `$015` verbatim to register 1, and bit 6 is LCD increment-by-row mode
  applied to an unchanged sequence.
- The `$000`/`$001` XOR results follow from pointer redirection. An XOR on `$000` moves the start by
  one byte. An XOR on `$001` moves it by a page.
- The common `$1CC0-$1FDF` coverage is the fixed 25-row clear.
- Per-title Y, write counts, and timing follow from stream content through the formula above.
- The previously unresolved source grammar, ROM mapping, return-Y rule, status flags, and timing are
  resolved above.
- Devil Castle's use of both services is explained by their shared preamble and row grammar. They
  are distinct entry points with different data encodings, not aliases.

#### Limits

- Timing reflects the emulator's validated CPU and bus model, not a separate physical measurement.
- D-set behavior lies outside all authentic calls and inherits the emulator's decimal arithmetic.
- Non-terminating streams are not part of the contract.
- The fixed campaigns do not prove that other titles never call `$F021`.
- The retained evidence consists of derived counts and purpose-written synthetic streams. No BIOS,
  ROM, stream, RAM, or VRAM payload was retained or published.

### Fixed-height tile-run contract: `$F012`

`$F012` draws a horizontal run of eight-row glyph tiles from a descriptor. The contract below is
resolved from emulator observation. It reproduces all 36,518 uninterrupted authentic calls in the
1,000-frame corpus and 293,780 synthetic calls (four title/BIOS runs of 73,445 calls each). No mismatches were
found in cartridge read addresses, VRAM write addresses and planes, VRAM output, registers, status
flags, or exact master-clock duration.

#### Calling convention

| Input | Meaning |
| --- | --- |
| RAM `$000-$001` | Little-endian pointer `P` to the descriptor. The descriptor may be in cartridge ROM or work RAM. |
| X | Destination pixel X. Only `X >> 3` is used; the low three bits have no effect. |
| Y | Destination pixel row of the first glyph row. |
| A | Ignored. |
| RAM `$015` | Cached LCD-control byte. Each tile writes `$015 | $40` (row-increment mode) to the LCD control register; the value is retained after return. |
| D flag | Must be clear. Decimal-mode entry is unsupported (see below). |

The descriptor is a width byte `W` at `P`, followed by one selector byte per tile. Selector `t`
(0-based) is read from `P + ((1 + t) & $FF)`: the index is eight bits wide. `W = 0` draws 256
tiles, and its final tile rereads the width byte as selector `$00`. If a descriptor read falls on a
different page from `P`, the read is preceded by a dummy read 256 bytes lower and costs two extra
master clocks. Descriptors in RAM and ROM take the same time.

#### Glyph lookup

Each selector chooses one eight-byte glyph from a font in the cartridge's fixed `$6000` window,
immediately after the normal `$6020` entry area. The font is supplied by the cartridge and is not
part of the BIOS. Its contents are not reproduced here. With index `i`, the glyph begins at
`base + 8 * i`:

| Selector | Glyph index `i` | `base` |
| --- | --- | --- |
| `$00-$20` | 1 | `$6021` |
| `$21-$40` | `((s - $2F) & $1F) + 1` | `$6021` |
| `$41-$60` | `((s - 1) & $1F) + 1` | `$6079` |
| `$61-$FF` | `((s - 1) & $1F) + 1` | `$6149` |

Selectors `$00-$20` all use glyph 1. `$2F-$40` map to indices 1-18, and `$21-$2E` wrap to indices
19-32. `$41-$60` index one 32-glyph segment. `$61-$FF` index another segment, and that mapping
repeats every 32 selectors. Row `r` reads `base + 8 * i + r`. A glyph row on a different page from
`base` is preceded by a dummy read 256 bytes lower and costs two extra master clocks. One trailing
read at the return address minus 1 follows during `RTS`.

#### Output

- **Copy:** Glyph byte `r` is copied unchanged to VRAM. This identity transform was tested on all
  256 input values. It does not depend on the row, the low bits of X, or `$015`.
- **Destination:** Tile `t` writes rows `r = 0..7` in order, to row `(Y + r) & $FF` and byte column
  `c & $1F`. Here `c` is an eight-bit counter that starts at `$80 + (X >> 3)` and increments by one
  per tile. The VRAM address is `((Y + r) & $FF) << 5 | (c & $1F)`.
- **Plane:** Bit 7 of `c` selects the plane. Runs start on plane 1. A run switches to plane 0 when
  `c` wraps past `$FF`; with `X >> 3 = 31`, for example, the switch occurs at tile 97. The plane is
  not taken from `$015`.
- **No transparency:** Every tile writes all eight destination bytes. An all-zero glyph row still
  overwrites the destination.
- **LCD writes:** Each tile makes 11 LCD writes: one control write, two address writes, and eight
  data writes.

#### Returned state

| Observable | Contract |
| --- | --- |
| A, Y | The width byte (`$00` for 256-tile runs). |
| X | `(8 * i_last) & $FF`, where `i_last` is the glyph index of the final selector. |
| S | Restored. Stack bytes `$1F9-$1FB` are written. |
| N, Z | N clear, Z set. |
| I, D | Preserved. |
| C | Depends only on the final selector: set for `$20-$2E`, `$81-$A0`, and `$C1-$E0`; otherwise clear. Entry C has no effect. |
| V | Updated by each selector above `$20`: set for `$80-$E0`, otherwise clear. Selectors `$00-$20` leave V unchanged, so it can carry from an earlier tile or from entry. |
| Scratch | RAM `$018-$01B`. |
| Other hardware | Scroll, AY, mapper, and UART state are preserved. |

#### Timing

Duration, in master clocks from the first service instruction through `RTS` and excluding the
caller's `JSR`, is:

`68 + sum(tile_cost) + 2 * (glyph page-crossing rows + descriptor page crossings)`

| Selector | `tile_cost` |
| --- | ---: |
| `$00-$1F` | 440 |
| `$20` | 428 |
| `$21-$40` | 456 |
| `$41-$60` | 440 |
| `$61-$FF` | 426 |

CPU cycles are half these values. Timing does not depend on glyph contents, X, Y, `$015`, the
descriptor's memory type, or entry flags other than D.

#### Decimal mode

With D set at entry, 41 of the 256 single selectors change their reads and returned registers. The
affected selectors are `$21-$2E`, `$30-$3E`, `$40`, and `$50-$F0` in steps of `$10`. Of these, 12
also change timing and 14 change C or V. Both BIOS revisions agree. No authentic call entered with D
set, so a replacement BIOS may treat D-set entry as undefined.

#### Evidence

- **Corpus:** `gamatebiosf012` ran every caller title under both BIOS revisions for the standard
  1,000-frame window: 39 titles and 78 runs. It reproduced the 39,034 balanced calls in the
  completed-call corpus, 19,517 per revision. Of the 36,518 uninterrupted calls, 35,564 used ROM
  descriptors and 954 used RAM descriptors. Six titles used RAM descriptors, and two used only RAM
  descriptors. All 36,518 matched the model exactly, including clocks and C/V, with no D-set entries.
  Per revision, authentic selectors fell in `$20` (11,293 tiles), `$2F-$40` (16,193), `$41-$60`
  (51,066), and `$61-$FF` (11,264). Widths ranged from 1 through 18. The 2,516 calls that span an IRQ
  or NMI were excluded because their summaries include callback work.
- **Synthetic matrix:** The matrix was run on two titles under both BIOS revisions. In each run, the
  first authentic call was snapshotted, a RAM descriptor was installed, and the service was replayed
  for:
  - every single selector with 10 entry-flag sets (2,560 cases);
  - all 65,536 ordered selector pairs;
  - 740 width, column-wrap, and page-crossing cases;
  - width 0;
  - every `$015` value;
  - every selector under decimal entry.

  The transform test rewrote one glyph byte at a time in a private in-memory ROM copy, which was
  restored afterward. All four runs passed with zero failures and produced identical decimal-mode
  deviations.
- **Earlier studies:** First-call address traces, RAM-bit toggles, single-byte XOR perturbations, and
  the Mighty Boxer descriptor study are consistent with this contract. The earlier eight-read and
  16-read tile forms are the page-crossing dummy reads described above. Earlier timing deltas came
  from selector classes and page crossings.

#### Corrections to earlier findings

- **Identity copy:** Output is an identity copy of the glyph byte. The earlier statement that the
  Mighty Boxer mapping was "not a copy or bit reversal" was wrong. The exhaustive in-memory perturbation test
  supersedes it. The pairing error in the earlier analysis was not re-audited.
- **No 16-entry mapping:** There is no separate 16-entry or 89-entry lookup table.
- **Plane source:** The plane comes from bit 7 of the column counter, not from `$015`.
- **RAM descriptors:** The two "one-tile contexts without descriptor reads" used RAM descriptors.
  The cartridge-ROM trace could not see them. RAM `$000-$001` is a direct pointer: in the earlier
  experiment, toggling a pointer bit moved the descriptor, not the glyph run, so no linear shift of
  the glyph reads was expected.
- **BIOS-specific trace:** The single BIOS-specific trace under `$001` XOR `$01` is consistent with
  the moved pointer reading RAM whose prior contents differ between BIOS revisions. This explanation
  is inferred and was not separately traced. The service itself agreed between revisions in every
  controlled comparison.

#### Limits

- **Source of evidence:** All results are emulator observations, not physical-hardware
  measurements.
- **Selectors outside the corpus:** Costs and flags for selectors `$00-$1F` and `$21-$2E` come only
  from the synthetic tests.
- **Untested conditions:** Interrupt-spanning behavior and D-set entry are not part of the
  contract.
- **Clean-room boundary:** The model was derived from addresses, write positions, counts, timing,
  and equality comparisons. No BIOS instructions were reconstructed. No glyph, BIOS, or cartridge
  bytes were retained or published.

`tools/gamate-bios-f012-summary.py` checks the write shape, return registers, status invariants,
stack balance, and unrelated hardware effects against the completed-call report. That report has no
selector or glyph-address columns, so exact timing and glyph addressing are checked by
`gamatebiosf012`. The validator reports zero mismatches across all 36,518 uninterrupted calls.

### Two-plane bitmap-copy contract: `$F015`

The combined completed-call corpus contains 41,824 balanced `$F015` calls, evenly split between BIOS
revisions. Of those, 38,790 do not span an IRQ or NMI. Every non-interrupted call fits one copy,
register, and timing contract; the other 3,034 are excluded from direct timing classification because
their summaries include interrupt callback work.

| Input | Meaning |
| --- | --- |
| X | Starting horizontal pixel coordinate; destination byte column is `X >> 3`. |
| Y | Starting destination row. |
| RAM `$000-$001` | First-plane source pointer. It is replaced with the second pointer during the call. |
| RAM `$002-$003` | Second-plane source pointer; preserved. |
| RAM `$015` | Cached raw LCD-control value; mode bits are retained and row increment is forced. |
| RAM `$01C` | Horizontal pixel extent; byte-column count is `$01C >> 3`. |
| RAM `$01D` | Vertical row count. |

Entry A is ignored. The routine copies a contiguous, column-major source rectangle into plane 0,
then repeats from the second pointer into plane 1. For each byte column it programs the plane, column,
and starting row, then streams the column's source bytes with row increment enabled. Existing VRAM is
overwritten rather than merged. As in `$F02D`, either 8-bit loop count becoming zero executes 256
iterations rather than skipping the operation.

For `columns = $01C >> 3`, `rows = $01D`, and `bytes_per_plane = columns * rows`:

| Observable | Contract |
| --- | --- |
| ROM data reads | `2 * bytes_per_plane`, followed by the cartridge-side `RTS` dummy read described below. |
| VRAM writes | `2 * bytes_per_plane`, one complete rectangle per plane. |
| LCD writes | `1 + 2 * columns * (2 + rows)`. |
| Baseline clocks | `234 + columns * (132 + 76 * rows)`. |
| Page-cross penalty | Two additional master clocks for each indexed source read that crosses a 6502 page. |
| Registers | X preserved; A returns the final copied byte; Y returns `bytes_per_plane & $FF`. |
| Status | C and N clear, Z set; I, D, and V preserved. |
| Stack | A normal JSR balances S. A cartridge tail-JMP into the service consumes the pre-existing return address, as expected for the service's final RTS. |
| Scratch | `$018/$019` hold raw plane/column and starting row; `$01C` becomes the byte-column count; `$01E/$01F` are loop/index scratch. `$000-$001` finish equal to the second source pointer. |
| Other hardware | Scroll, AY, mapper, and UART state are preserved; final LCD state selects plane 1 with row increment. |

Observed clean shapes use widths of 1–20 byte columns and heights of 2–72 rows. The most common
shape is 1×8 bytes per plane (24,148 calls). Baseball provides a controlled 4×8 example; Mighty Boxer
provides 6×32 and wrapping-Y examples. Source-pointer perturbations shift only the corresponding
contiguous ROM span, and VRAM-fill perturbations confirm unconditional overwrite behavior. Both BIOS
revisions produce identical controlled ROM reads, timing, output, registers, and LCD effects.

Each authentic trace contains one additional cartridge read after the source spans. It occurs at the
stacked return address during the BIOS `RTS`, so it is the modeled NMOS dummy bus read rather than a
source byte or inline argument.

`tools/gamate-bios-f015-summary.py` derives dimensions from LCD/VRAM write counts and validates the
copy shape, baseline timing, bounded page-cross penalties, registers, status, stack convention, and
unrelated hardware effects. It reports zero mismatches across all 38,790 available non-interrupted
calls.

### Plane-0 rectangle fill: `$F02A`

A controlled Baseball call identifies `$F02A` as a column-major rectangle fill with a caller
supplied byte, normally in plane 0. RAM `$01C` supplies the horizontal pixel extent and RAM `$01D`
supplies the vertical row count. The byte-column count is `$01C >> 3`; both BIOS revisions agree on
the call.

For `columns = $01C >> 3` and `rows = $01D`, with ordinary nonzero counts:

| Observable | Contract |
| --- | --- |
| VRAM writes | `columns * rows`, all in plane 0. |
| LCD writes | `1 + columns * (2 + rows)`. |
| Master clocks | `126 + 58 * columns + 18 * columns * rows`. |
| Registers | A and S are preserved; X and Y return zero. |
| Status | C and N clear, Z sets, I and D are preserved, and V is preserved. |
| Scratch | `$018-$01A` hold service scratch and `$0A8` receives the plane-0 selector. |
| Other hardware | Scroll, AY, mapper, and UART state are preserved. |

The baseline 1×1 call writes A once at `(X >> 3, Y)` and takes 202 master clocks. Width and height
perturbations produce the corresponding column-major coverage and the LCD/write/timing equations
above. The expanded form is equivalent to `126 + columns * (58 + 18 * rows)` for ordinary
nonzero dimensions. As with the 8-bit loops in the neighboring fill services, a derived zero count is interpreted
through 256 iterations; a 256×256 controlled probe therefore reaches 65,536 writes, but the general
normal-dimension formula should remain the primary contract. The plane-0 description applies to
ordinary nonzero widths: a zero-derived 256-column probe beginning at X=`$F8`, Y=`$FE`, with one
row, wrote all 32 byte columns on **both** planes under each BIOS revision. All 256 LCD column
selectors followed the raw eight-bit counter across bit 7 and its wrap; describing that extreme case
as plane-0-only is incorrect. This observation does not establish every zero-count combination.

The validated completed-call corpus contains 9,414 `$F02A` calls, evenly split between BIOS revisions.
Excluding 394 interrupt-spanning calls leaves 9,020 uninterrupted calls. An added
`tools/gamate-bios-f02a-summary.py` derives dimensions independently from LCD/VRAM write counts and
reports zero mismatches for timing, register/status behavior, and unrelated AY/bank/UART activity.
The only measured ROM read is the stacked cartridge `RTS` dummy address, not a semantic data read.

### Sprite and background compositor: `$F027`

The validated completed-call report contains 7,026 `$F027` calls, evenly split between BIOS revisions.
Of those, 2,366 do not span an IRQ or NMI. Early controlled calls exposed caller-dependent effects;
the later isolated-record model below resolves their shared graphics structure.

The controlled comparison covers first authentic occurrences in four titles and both BIOS revisions:
Baseball, Fortune 'n Luck, Nightmare of Santa Claus, and Time Warrior. The first three each perform
32 VRAM writes and 106 LCD writes, but take 14,202, 14,026, and 16,490 clocks respectively. Time
Warrior performs 160 VRAM writes and 515 LCD writes in 47,024 clocks. Six further titles were tested
at their first VRAM-writing occurrence (the second service occurrence): Money Maze, Pipemania, Mars
Voyage, Witty Apee, Devil Castle, and Kill Shot. Across the ten selected productive calls, VRAM
writes range from 32 to 1,488 and LCD writes from 106 to 2,930. A, X, Y, and S are preserved in all
ten; status changes vary. The ordered ROM reads and perturbation effects differ by caller.

In Baseball, low-byte XOR at `$006` shifts one eight-byte cartridge read span by one byte, while XOR
at `$008` shifts a different eight-byte span. XOR at `$007` and `$009` moves the corresponding
observed spans to another 256-byte page. These effects support address inputs in Baseball's call context. They do not generalize as fixed
one-span pointers: XOR of `$006` shifts one address span in Baseball and Nightmare, shifts both spans
in Fortune 'n Luck, and changes several read spans in the other tested callers. XOR of `$008` shifts
one span in Baseball and Nightmare, while in Time Warrior it adds a read and two clocks. Those
probes alone did not establish the shared roles of `$006-$009`, which are identified below.

The table gives VRAM/LCD write counts for baseline and selected RAM perturbations. Each perturbation
was applied independently; the `$023/$028/$030` columns show setting that byte to zero.

| Caller | Baseline | `$023=0` | `$028=0` | `$030=0` |
| --- | ---: | ---: | ---: | ---: |
| Baseball | 32/106 | 128/409 | 176/281 | 176/281 |
| Fortune 'n Luck | 32/106 | 128/409 | 176/281 | 176/281 |
| Nightmare of Santa Claus | 32/106 | 128/409 | 176/281 | 176/281 |
| Time Warrior | 160/515 | 160/515 | 304/690 | 304/690 |
| Money Maze | 256/848 | 352/1,151 | 400/1,023 | 400/1,023 |
| Pipemania | 96/187 | 192/490 | 240/362 | 240/362 |
| Mars Voyage | 480/1,515 | 480/1,515 | 624/1,690 | 624/1,690 |
| Witty Apee | 960/1,912 | 960/1,912 | 960/1,912 | 960/1,912 |
| Devil Castle | 96/303 | 96/303 | 240/478 | 240/478 |
| Kill Shot | 1,488/2,930 | 1,584/3,233 | 1,632/3,105 | 1,632/3,105 |

Thus these early probes showed caller-dependent effects, without yet recovering the record format.
Both BIOS revisions matched for every measured title/perturbation pair.

Entry A, X, Y, and S are preserved in these ten selected baseline calls. The ten-title evidence
initially suggested a caller-RAM-driven blitter family without a recovered graphics format. The
record fields and their effects are now identified below.

#### Expanded caller comparison

The first productive `occurrence=1` was probed with IRQs masked in 14 additional titles:
Box Forum, Brick Blaster, Dino Ball, Enchanted Bricks, GP Race, Galaxy Invaders, Kung-Fu Fighter,
Magic Jigsaw, Mighty Tank, Mini Golf, Myth of Asamia, Tough Guy, Vindicators, and Volcano Panic.
The later four-title corpus extension also includes Incantational Couple; both of its authentic
`$F027` occurrences were probed. Across these 16 contexts, both BIOS revisions matched in return
registers/status, timing, ordered cartridge-address reads, VRAM coverage, and output hash. The new
contexts range from 32 to 1,248
VRAM writes and 13,962 to 172,762 master clocks.

Single-bit XORs at each byte `$006-$009` and zeroing `$023`, `$028`, or `$030` were then tested in
Box Forum, Dino Ball, Galaxy Invaders, Tough Guy, GP Race, and both Incantational Couple contexts,
under both BIOS revisions. All seven bytes were read before first write and left unwritten in these
contexts. Every pointer-byte XOR changed the ordered cartridge-read sequence, but its effect on read
count and timing varied; for example, changing `$007` in Galaxy Invaders added 168 reads and 8,912
master clocks while preserving VRAM write geometry. The three zeroed fields were likewise
caller-dependent: they sometimes changed only reads/timing/output, sometimes changed VRAM/LCD
geometry, and sometimes had no observed effect. `$030=0` changed geometry in GP Race and the two
Incantational Couple calls, but not in the other sampled contexts. Both BIOS revisions agreed on
every tested perturbation.

Address watches in the five varied titles and both Incantational Couple contexts show a repeatable
read-count shape under both BIOSes: `$006/$007` are each read twice as often as `$008/$009`, with
equal counts within each byte pair; `$023`, `$028`, and `$030` are read once, twice, and twice,
respectively. None of these seven addresses is written by the service in those calls. Together with
the independent address perturbations, this is consistent with `$006-$007` and `$008-$009` serving
as two caller-supplied 16-bit cartridge-address words. Their graphics-plane roles and the record gates
are established by the subsequent synthetic tests.

These probes alone established caller-owned inputs but not a common parameter schema. The isolated
record experiments below now identify a shared graphics-table structure and the two pointer roles.
The analysis uses only derived counts, hashes, coverage, and ROM addresses; RAM-watch final values
and source payloads are omitted.

An IRQ-masked matrix varied `$021` through `$00`, each single-bit value, and `$FF` for the first
call in each of the 25 titles observed to call `$F027` in the 1,000-frame unattended inventory.
Seven second-call contexts were also tested: Devil Castle, Incantational Couple, Kill Shot, Mars
Voyage, Money Maze, Pipemania, and Witty Apee. In all 640 probes, the service read `$021` once and
did not write it; both BIOS revisions agreed on the recorded non-RAM-hash results. ROM address
sequences, VRAM and LCD write counts, and returned CPU registers/status were unchanged by these
variations in each context. Twelve of the 32 contexts changed VRAM coverage; the other 20 had
unchanged coverage, final VRAM hash, and measured timing for these values. The final VRAM address
did vary in Devil Castle's second call, so it is not generally preserved across this input matrix.

In 11 of the 12 coverage-sensitive contexts, the written-address set decomposes into a fixed set
and 96 moving bytes: three byte columns for each of 16 rows in each plane. The moving columns
advance by `$021 >> 3`, wrapping within the 32-byte VRAM row. In Mars Voyage's second call,
overlap prevents the same simple decomposition at its original vertical position; the combined
position probes below separate those writes. For Galaxy Invaders' and Time Warrior's first calls,
all 256 `$021` values were tested under both BIOSes: the same decomposition held, and the
master-clock count was respectively
`172762 + 1516 * ($021 & 7)` and `46796 + 1516 * ($021 & 7)`. The ten-value matrix exhibited the
same timing increment in all 12 coverage-sensitive contexts and no increment in the other 20.
These are context-specific measurements with the inherited VRAM contents, not a general service
timing formula.

The same 32 contexts and ten-value matrix were tested independently for `$022` under both BIOSes.
All 640 probes read it once without writing it; coverage changed in the same 12 contexts and
remained unchanged in the other 20. In all 32, the tested values left measured timing, ROM address
sequence, VRAM/LCD write counts, and returned CPU registers/status unchanged. For each sensitive
context, the coverage decomposed into a fixed set and 96 moving bytes, with the moving rows
advanced by `$022` modulo 256. Galaxy Invaders' and Time Warrior's first calls were also tested at
all 256 values under both BIOSes (1,024 probes), with no coverage-model or timing mismatches.

A combined matrix used `$021={00,07,08,FF}` and `$022={00,01,80,FF}` in each of the 12 sensitive
contexts under both BIOSes (384 probes). All matched a fixed set plus the 96-byte region translated
by `$021 >> 3` columns modulo 32 and `$022` rows modulo 256. Recovering the moving region from a
non-overlapping position also explains the earlier Mars Voyage coverage ambiguity. ROM address
sequences and write counts stayed unchanged, and timing was the context's zero-position time plus
`1516 * ($021 & 7)` throughout this matrix with inherited VRAM. These observations bound two
positioning inputs in these paths, not the full command or timing grammar.

In Time Warrior's first call with VRAM seeded to zero, direct in-memory equality comparisons
verified that each moving three-byte row in both planes equals the zero-offset row shifted right
by `$021 & 7`; its starting byte advances by `$021 >> 3` with row wrapping. This was checked for
`$00-$09`, `$7F`, `$F8`, and `$FF` under both BIOSes. Seeding VRAM to `$FF` changed the resulting
rows, so this is not evidence for unconditional replacement or a fully specified blending rule.
A further zero-filled comparison combined six horizontal values `$00,$03,$07,$08,$0F,$FF` with
eleven vertical values `$00,$01,$07,$0F,$10,$20,$40,$E0,$F0,$F8,$FF`, checking all 16 rows and both
planes under both BIOSes. All 4,224 row/plane comparisons matched the translated, horizontally
shifted zero-position output, including row and column wrap. Positions overlapping fixed writes
were not included in this pixel comparison.

For the same Time Warrior call, further I-masked probes used uniform VRAM fills `$00`, `$FF`, each
single-bit value, `$55`, `$AA`, `$5A`, and `$A5`. With `$021=$00-$07, $022=$20` or
`$021=$F8-$FF, $022=$FF`, all 224 paired comparisons between BIOSes agreed on every written
output byte (448 probes). For each fixed context and written address, the tested fills matched
`output(fill) = output($00) | (fill & output($FF))`, including the 96-byte moving region and the
other written addresses. These comparisons used bytes only in memory. Uniform fills cannot establish
whether an individual destination bit depends only on that bit, nor do they identify the source
format or prove a general blending rule.

The same proposed equation was checked with 12 uniform fills (`$00`, `$FF`, the eight single-bit
values, `$55`, and `$AA`) at inherited positions in the 32 contexts above, with P set to `$24` and
both BIOSes: 768 probes. Of the 32 contexts, 26 wrote VRAM. Coverage was independent of the tested
fills, and the paired revisions agreed on written output and checked return/hardware fields.
However, 28,906 of 139,296 byte comparisons failed the equation, and timing varied with fill in
some contexts. A six-fill localization (`$00`, `$FF`, `$01`, `$80`, `$55`, `$AA`) found failures
in ten contexts with the Bit BIOS (192 probes): first calls in Box Forum, Brick Blaster, Dino Ball,
Enchanted Bricks, Galaxy Invaders, Mini Golf, and Tough Guy, and second calls in Kill Shot,
Pipemania, and Witty Apee. Only timing varied among the checked non-pixel observables in this
localization. For example, Box Forum's first call took 96,206, 99,534, 104,526, and 108,686 clocks
for fills `$00`, `$55`, `$AA`, and `$FF`, respectively, under both revisions. In that call some output bits changed
even when the corresponding input bit was the same in the compared fills. Thus neither the simple
per-bit model nor background-independent timing is universal, even though it holds in the selected
Time Warrior probes. Uniform fills do not isolate the source of the cross-bit or cross-address
dependence.

#### Isolated record and rendering model

Further I-masked experiments isolate the table at `$021-$0A0` into 16 consecutive eight-byte
records. The first eight records draw 16-by-16-pixel objects; the remaining eight draw
8-by-8-pixel objects. With every table byte set to `$FF`, there are no VRAM or LCD accesses.
Setting individual bytes to zero identified two independent activity gates in each record. In the
tested cases the record fields are:

| Offset | Observed role |
| --- | --- |
| `+0`, `+1` | Current pixel X and Y. |
| `+2` | Current graphics selector; `$FF` disables current-object drawing. |
| `+3`, `+4` | No effect on the isolated rendering cases tested so far. |
| `+5`, `+6` | Background-redraw pixel X and Y. |
| `+7` | `$FF` disables background redraw; other tested values enable it. |

RAM `$006-$007` and `$008-$009` are little-endian bases for plane-0 and plane-1 graphics,
respectively. In the tested binary-mode cases, selector `s` supplies the eight rows at
`base + 8*s`. A small object uses one selector. A large object uses the two-by-two selector
arrangement `s, s+1` on the top and `s+2, s+3` on the bottom, with selector arithmetic wrapping
at 256. For current-object drawing, `$FF` is a disable marker rather than a drawable selector;
`$FF` remains a valid background-map selector.

An enabled background redraw covers a fixed 3-by-3 tile rectangle for a large record or a 2-by-2
tile rectangle for a small one, regardless of the low position bits. Its selector map is in VRAM
plane 0: for tile offset `(c,r)`, the map coordinate is column `((X >> 3) + c) & 31` and row
`($E6 + (Y >> 3) + r) & $FF`. The destination is the corresponding tile at column
`((X >> 3) + c) & 31` and pixel row `((Y & $F8) + 8*r) & $FF`. Both planes are overwritten
with all eight source rows. Selectors are captured before that record's redraw, including tested
cases where the destination overlaps the map.

For current-object drawing, the two source planes are shifted right by `X & 7` and written over
the existing destination at byte column `X >> 3` and pixel row Y, wrapping columns and rows.
For each destination bit, two zero source bits leave both background bits unchanged; otherwise
the two source bits replace the background bits. Equivalently, with shifted source bytes
`s0,s1` and destination bytes `d0,d1`, each output plane is
`sN | (dN & ~(s0 | s1))`. A large object reads and writes three bytes per row in each plane
(96 reads and 96 writes); a small object reads and writes two bytes per row in each plane
(32 reads and 32 writes), even when the horizontal shift is zero.

Controlled overlaps indicate background redraws run in record order before current-object draws,
which also run in record order. This explains why the earlier uniform-background equation failed:
background redraw interprets plane-0 map bytes as tile selectors, rather than blending each output
bit independently.

#### LCD and cartridge accesses

The following ordered LCD-write sequence has been checked against the behavioral model in
synthetic cases and eligible authentic replays. The placement of register-6 reads is inferred
from access counts, coverage and output comparisons, not an ordered read trace. `C` is RAM
`$015 | $40`; `x` and `y` are the relevant record coordinates; `q` is 3 for a large record and
2 for a small one. A register-6
read obtains a VRAM byte and a register-7 write stores one. Column register values below are raw
eight-bit values; the LCD uses their low five bits for the column and bit 7 for the plane.

1. For each enabled background redraw, write `C` to register 1. In column-major order, select map
   row `$E6 + (y >> 3) + r` in register 5 and column `(x >> 3) + c` in register 4, then read
   register 6, for `c,r = 0..q-1`. Rows wrap at 256.
2. For each plane and then column, write register 4 = `(x >> 3) + c + $80*plane` and
   register 5 = `y & $F8`, then write the `8*q` source rows to register 7, in increasing tile
   row and pixel-row order.
3. For each enabled current object, write `C` to register 1. For each plane, column, and pixel
   row, select the destination using register 5 = `y + row` and register 4 =
   `(x >> 3) + column + $80*plane`, then read register 6. Rows wrap at 256.
4. Write `C` to register 1 twice for a large object or once for a small one. For each plane and
   column, write register 5 = `y`, register 4 = `(x >> 3) + column + $80*plane`, then the
   16 or 8 composited rows to register 7.

The redraw pass precedes the object pass. Each pass visits records in ascending order. Within
each operation, source reads are plane-major, then column-major, then tile-row-major, with eight
rows per tile. A page-crossing indexed source read is preceded by a dummy read 256 bytes below
its final CPU address. The `RTS` also makes a dummy read at the stacked return address when it
falls in the cartridge window. These describe the tested emulator bus, not a physical measurement.

In isolated probes, RAM `$015` was read once per background redraw, three times per large sprite
and twice per small sprite, matching the respective register-1 write counts. For each sprite size
under both BIOSes, changing `$015` at an instruction boundary immediately after its first control
write changed the subsequent control writes. An interrupt-aware replacement cannot safely treat
`C` as one entry-time snapshot for an entire sprite; the exact read timestamps remain open.
The same isolated probes read each byte of the plane-0 graphics base twice per selected tile
and each byte of the plane-1 base once per tile. In a separate synthetic small-redraw test,
changing `$006` after the first VRAM write left the first tile sourced from the old base but
made the next tile use the new base under both BIOSes. A whole-call pointer snapshot is therefore
insufficient when those inputs can change concurrently; the exact pointer-read schedule is open.
A further synthetic test enabled and positioned the second sprite record after the first sprite's
first VRAM write; both revisions then drew that second sprite. Thus later sprite records also
cannot generally be treated as an entry-time snapshot.

#### Binary-mode timing and returns

For uninterrupted calls with safe graphics sources, define `T(s)` as 64 times `s >> 5`, plus two
master clocks for each source-row read that crosses an indexed page. For each plane base `B`, row
`r = 0..7` crosses when `(B & $FF) + 8*(s & 31) + r > $FF`. Count both planes.

Start at 6,326 master clocks and add, for every enabled operation:

| Operation | Additional clocks |
| --- | --- |
| Large background redraw | `9,084 + sum(T(s))` over its nine captured map selectors |
| Small background redraw | `4,270 + sum(T(s))` over its four captured map selectors |
| Large current object | `23,326 + 1,516*(X & 7) + sum(T(s))` over its four wrapped selectors |
| Small current object | `7,556 + 540*(X & 7) + T(s)` |

This formula accounts for the previously observed background-dependent timing: map selectors
determine both the graphics and the indexed-page costs. The all-`$FF` table takes 6,326 clocks
and issues no LCD accesses.

In the checked calls A, X, and Y are preserved, and S is restored to the caller. With D clear,
N and Z reflect the restored A, C is set, V is clear, and I and the stored B bit are preserved.
The checked calls leave scroll, AY registers and address, mapper registers, and UART clock accesses
unchanged. LCD state remains unchanged for an empty table; otherwise the final LCD control is
`$015 | $40` and the final address/plane follows the last register-7 write in the sequence above.
Observed non-stack RAM writes are confined to `$000-$003`, `$018-$01F`, and `$300-$3C7`;
the final `$300-$307` bytes matched the final table record in the eligible replays and synthetic
cases.
For an all-disabled table in binary mode, 16 synthetic cases per BIOS also found no writes to
`$000-$003`, `$018-$01F`, or `$308-$3C7`; all eight bytes of `$300-$307` were written and
received the final record.

#### Scratch state and observed consumers

The following final-state model applies to uninterrupted binary-mode calls with stable inputs and
graphics sources unaffected by service writes or read side effects. Start with the entry values;
process enabled redraws in ascending record order, then enabled objects in ascending record order.
Within `$000-$003` and `$018-$01F`, each operation changes only the listed locations. Let `B1`
be the plane-1 graphics base, `x,y` the operation's coordinates, and `s` the last selector in that
operation: the last captured map selector for a redraw, the object selector for a small sprite,
or `(selector + 3) & $FF` for a large sprite.

| RAM | Either operation | Redraw | Object |
| --- | --- | --- | --- |
| `$000-$001` | | unchanged | `$03B0` for large, `$03A0` for small, little-endian |
| `$002` | `B1 & $FF` | | |
| `$003` | `((B1 >> 8) + (s >> 5)) & $FF` | | |
| `$018` | `$80 \| (x >> 3)` | | |
| `$019` | | `y & $F8` | `y` |
| `$01A` | `s` | | |
| `$01B` | | `0` | `1` |
| `$01C` | `0` | | |
| `$01D` | | `0` | unchanged |
| `$01E` | | `x >> 3` | unchanged |
| `$01F` | | `($E6 + (y >> 3)) & $FF` | unchanged |

For each redraw, the `q*q` captured map selectors also overwrite `$308 + c*q + r`, where `q` is
3 or 2 for a large or small record and `c,r = 0..q-1`. The selection uses the map before that
redraw writes VRAM. Later redraws and objects can overwrite the same slots; bytes not written by
any operation retain their entry values.

For each current-object draw in the checked cases, the service also stores the destination before
and after compositing in RAM. Let `h` be the object's height, 16 or 8, and let `c` range over
its three or two destination byte columns. For plane `p = 0,1` and row `r = 0..h-1`, the pre-draw
destination byte is stored at `$308 + 48*p + h*c + r`; the composited byte is stored at
`$368 + 48*p + h*c + r`. The destination coordinates wrap as in the rendering model above.
If later sprites write the same buffer slot, the last such write determines its final value.
In 28,516 eligible masked corpus replays, all buffer slots modeled for drawn sprites matched.
The expanded model compares final values and write sets throughout `$000-$003`, `$018-$01F`
and `$308-$3C7` in 256 overlapping and 2,052 selector/sentinel synthetic cases per BIOS,
with varied initial scratch values.
Another 1,024 isolated redraw cases per BIOS test the column-major selector layout. These
equations describe final values and whether a byte was written, not the timing or intermediate
values of the scratch accesses.
The expanded model also matched all 28,518 masked replays across 62 cartridges, both BIOSes and
four 1,000-frame campaigns, with no skips or mismatches. This run privately included BIOS-window
graphics sources in the two Mini Golf calls discussed below.

For an uninterrupted call, synthetic write-set checks also found contiguous stack writes beginning
at entry S and descending with eight-bit wrap. The span was nine bytes for an empty table, 11
for an isolated redraw of either size, 13 for an isolated large object, and 18 for an isolated
small object. The greatest applicable span matched 256 multi-record cases per BIOS. The isolated
tests covered four entry S values including wrap, four positions and four selectors in records
0, 7, 8 and 15. The span also matched all 7,196 IRQ-masked replays from 62 cartridges and both
BIOSes in the 1,000-frame no-input campaign. Stack contents and individual access times are not
specified by this observation; interrupts can add further stack writes.

The scratch-consumption probe observes bytes written during each authentic call and cartridge-code
reads before an intervening write, until the next `$F027` entry or campaign end. Across the four
1,000-frame campaigns, 62 cartridges and both BIOSes, it tracked 28,482 returns, including
19,846 interrupt-spanning returns; 34 calls remained in flight at the end of their campaigns.
Of 496 cartridge/BIOS/campaign combinations, 24 had apparent read-before-write accesses. Replaying
those combinations distinguished indexed-store dummy reads from read-modify-write instructions:
only Tough Guy's controller-cycle campaign consumed scratch values, at `$39C-$3A3`, once per
address under each BIOS. The last writes to those bytes during the observed call were attributed
to BIOS code outside the tracked interrupt context. At that return they matched rows 4–11 of
plane 1, column 0 of the final large object, as predicted by the buffer model.

This is bounded execution evidence, not a universal caller-clobber guarantee. Interrupt handlers
and code outside the cartridge window are not independently covered as consumers; the recorded
write set may include interrupt-handler writes; a window ends at the next `$F027` entry; and
interrupt-context attribution is diagnostic rather than a proof about nested interrupts.

#### Partial event timing

In isolated I-masked tests with all other record gates disabled, a single enabled operation in
record `r = 0..15` first writes LCD register 1 at the following emulator instruction-boundary
master-clock offsets from service entry:

- Background redraw: `166 + 44*r + (r >= 8 ? 6 : 0)`.
- Current object: `1,132 + 340*r + (r >= 8 ? 44 : 0)`.

The tests used X = 0, Y = `$20`, binary mode and synthetic RAM graphics, and both BIOS revisions
agreed for all 32 cases. These are isolated first-event observations, not a schedule for later
events, multiple active records or interrupt-spanning calls, and they are not physical bus timing.

For the first observed calls in Tough Guy, Time Warrior and Box Forum, paired probes forced the
same binary-mode status except for I and reset the periodic IRQ phase at entry. With I clear,
each call took one or two IRQs and had the same final VRAM hash and VRAM-write count as its
I-masked counterpart under both BIOSes. Whole-RAM hashes differed, so this is not machine-state
equivalence. First calls in Pipemania and GP Race took no IRQ in the same setup and serve only
as controls. These few reset-phase probes do not define general interrupt interference.

A broader paired replay cloned each authentic binary-mode entry with its original IRQ phase:
one copy retained its original interrupt state, while the other masked and suppressed IRQs.
Across 62 cartridges, both BIOSes and four 1,000-frame campaigns, all 28,518 entries returned
in both copies. Of these, 19,880 original-state copies took IRQs and none took an NMI. For every
IRQ-spanning pair, final VRAM, compositor scratch, tracked inputs, A/X/Y/S and status other
than I, LCD register state and mapper registers matched, while other non-stack RAM differed.
The 8,638 pairs without an interrupt matched in these categories and elapsed time. There were
no decimal or pending-NMI skips or timeouts. A diagnostic self-test checks the difference
categories and their deliberate exclusion of stack and I.

These are paired emulator replays, not evidence that interrupts are universally harmless: the
comparison excludes stack contents and does not establish ordered access times, full AY/UART
state, injected NMI behavior, altered handlers or physical-hardware behavior. An IRQ-aware
replacement must still allow for live inputs and handler interference.

`tests/gamatebiosf027.c` checks these models using purpose-written RAM graphics and private
in-memory cartridge overlays, without publishing source or output bytes. Under each BIOS revision,
1,024 isolated sprite cases and 1,024 isolated background cases covered every record, all horizontal
bit offsets at ordinary and wrapped columns, two vertical positions, and varied synthetic data.
Another 256 combined cases tested multiple records, overlapping operations and map/destination
aliasing. A further 2,052 cases exercised all 256 selectors in records 0, 7, 8 and 15, including
the current-object disable marker and background selector `$FF`. All compared complete final VRAM
and access counts; the combined and selector cases additionally checked ordered LCD writes, ordered
cartridge reads, returned status and exact timing with varied LCD controls and source-page offsets.
No mismatches were found.

The four standard 1,000-frame campaigns covered 62 cartridges and both BIOSes. Of 28,518 service
entries, 28,516 matched the predicted final VRAM, access counts, ordered LCD writes, ordered
cartridge reads, registers, bounded RAM side effects, selected peripheral state, and exact
masked-replay timing; two accesses beyond the cartridge window were conservatively excluded from
the model.
Both exclusions are the same Mini Golf long-Start call under the two BIOS revisions; a modeled
tile range extends into the BIOS window, ending at `$E0E7`. Its BIOS-sourced output was not
included in that cartridge-only replay.
An opt-in private-memory replay using the local BIOS bytes as graphics input subsequently matched
all 65 Mini Golf long-Start calls per revision, including the formerly skipped call. In that call,
an otherwise identical model with BIOS-window source bytes replaced by zero produced different
final VRAM under each revision. Thus an independent replacement cannot assume BIOS-window source
contents are immaterial in this observed context. The comparison does not expose firmware bytes
or establish that a replacement must reproduce the authentic BIOS's contents.
The replay masks IRQs and is not a measurement of interrupt-spanning call timing. Controlled
decimal-mode probes did not follow this binary-mode model, including an apparently disabled table
that still performed VRAM writes. D-clear entry is therefore a requirement for this specification,
not an assertion that decimal mode behaves identically.

The complete ABI is still open: later bus-event timestamps, intermediate scratch and stack contents,
other live input dependencies, unsupported decimal-mode behavior and interrupt interference
require further work.
The model does not yet establish behavior for side-effectful graphics sources.
Galaxy Invaders reads its inherited
`$021` before later setting it for its first service call; the entry-state implications are in
[`BIOS_ENTRY_RAM_ANALYSIS.md`](BIOS_ENTRY_RAM_ANALYSIS.md).

### IRQ-gated counter wait: `$F003`

The validated report contains 7,424 `$F003` calls. Its 62 uninterrupted calls are evenly split
between the two BIOS revisions and span 19 titles. Every uninterrupted call takes 42 master clocks
and has the same register, RAM-write, status, and hardware-effect shape.

| Observable | Contract |
| --- | --- |
| Entry A | Ignored; A returns zero. |
| Entry RAM `$00B` | Values `$00-$07` wait (8 to 1 IRQs); values `$08-$FF` return immediately. |
| RAM `$00B` on return | Written as zero. |
| Registers | X, Y, and S are preserved. |
| Status | C and Z set; N clear; B, D, I, and V preserved. |
| Ready-path timing | 42 master clocks when the wait condition is already satisfied. |
| Other hardware | No VRAM, LCD, AY, mapper, or UART effects in uninterrupted calls. |

An exhaustive entry-byte matrix tested all 256 `$00B` values in the first authentic call context of
each of the 30 titles observed using `$F003`, under both BIOS revisions (15,360 probes). Values
`$00-$07` took exactly `8 - n` periodic IRQs, where `n` is the entry value; every value `$08-$FF`
returned on the 42-master-clock ready path without an IRQ. All probes returned with `$00B=0`, A=`$00`,
X/Y/S preserved, and status `(P_in & $7C) | $03`. Ready-path probes wrote only `$00B` and had no
peripheral effects. The counter was changed only in the probe's private in-memory machine state.

This establishes the complete byte-domain contract across every observed caller's first call
context. Later call contexts were not individually perturbed. Wait-path timing varies with
title-specific IRQ callback work; RAM and peripheral changes during IRQ-spanning calls must not be
attributed to `$F003` without separating callback effects. The callback's intermediate counter
updates were not traced directly. These are emulated observations, not physical timing measurements.

### IRQ-gated counter wait: `$F006`

The validated report contains 6,454 `$F006` calls; 192 uninterrupted calls are evenly split between
the two BIOS revisions and span 15 titles. Every uninterrupted call takes 42 master clocks and has
the same register, RAM-write, status, and hardware-effect shape.

| Observable | Contract |
| --- | --- |
| Entry A | Ignored; A returns zero. |
| Entry RAM `$00B` | Values `$00-$04` wait (`5 - n` IRQs); values `$05-$FF` return immediately. |
| RAM `$00B` on return | Written as zero. |
| Registers | X, Y, and S are preserved. |
| Status | C and Z set; N clear; I, D, and V preserved. |
| Ready-path timing | 42 master clocks when the wait condition is already satisfied. |
| Other hardware | No VRAM, LCD, AY, mapper, or UART effects in uninterrupted calls. |

An exhaustive entry-byte matrix tested all 256 `$00B` values in the first authentic call context of
each of the 26 titles observed using `$F006`, under both BIOS revisions (13,312 probes). Values
`$00-$04` took exactly `5 - n` periodic IRQs, where `n` is the entry value; every value `$05-$FF`
returned on the 42-master-clock ready path without an IRQ. All probes returned with `$00B=0`, A=`$00`,
X/Y/S preserved, and status `(P_in & $7C) | $03`. Ready-path probes wrote only `$00B` and had no
peripheral effects. The counter was changed only in the probe's private in-memory machine state.

This establishes the complete byte-domain contract across every observed caller's first call
context; later call contexts were not individually perturbed. With IRQs masked and `$00B=0`, earlier
probes in Dinosaur Park and Witty Apee did not observe a clean return within a 10-million-instruction
bound. The IRQ path therefore depends on IRQ progress, but the callback's intermediate counter
updates were not traced. Wait-path timing varies with IRQ phase and callback work; RAM and peripheral
changes during IRQ-spanning calls must not be attributed to `$F006` without separating callback
effects. These are emulated observations, not physical timing measurements.

### IRQ-gated counter wait: `$F009`

The available completed-call report contains 4,070 balanced `$F009` calls across 41 titles, 2,035
per BIOS revision. Of these, 4,054 span an IRQ or NMI; the 16 uninterrupted calls, from six titles,
are evenly split between BIOS revisions. All 16 take 42 master clocks and have the same register,
RAM, status, and hardware-effect signature. Across all 4,070 completed calls, A returns zero, X/Y/S are
preserved, and C/Z are set, N is clear, and B/D/I/V are preserved. X preservation is an observed
corpus result, not a guarantee when a callback changes X (see the synthetic probe below).

| Observable | Contract |
| --- | --- |
| Entry A | Ignored; A returns zero. |
| Entry RAM `$00B` | Values `$00-$17` wait `24 - n` IRQs (`n` is the numeric entry value); `$18-$FF` take the ready path. |
| RAM `$00B` on return | Written as zero. |
| Registers | X, Y, and S are preserved on the ready path and in observed corpus calls. A waiting call can expose a callback-modified X. |
| Status | C and Z set; N clear; B, D, I, and V preserved. |
| Ready-path timing | 42 master clocks when the wait condition is already satisfied. |
| Other hardware | No peripheral effects in uninterrupted calls. IRQ-spanning calls include callback effects. |

An exhaustive entry-byte matrix tested all 256 `$00B` values in the first authentic call context
of every title observed using `$F009`: 41 titles under both BIOS revisions (20,992 probes). Values
`$00-$17` took exactly `24 - n` periodic IRQs, where `n` is the entry value; every value `$18-$FF`
returned on the 42-master-clock ready path without an IRQ, wrote only `$00B`, and produced no VRAM,
LCD, scroll, AY, mapper, or UART effects. Every probe returned with `$00B=0`, A=`$00`, X/Y/S
preserved, C/Z set, N clear, and B/D/I/V preserved; none raised an NMI. The counter was changed only
in the probe's private in-memory machine state.

This establishes the complete byte-domain contract across every observed caller's first call
context. Later call contexts in each title were not individually perturbed. The callback's
intermediate counter updates were not traced directly. Wait-path elapsed clocks vary with
title-specific IRQ callback work; RAM and peripheral changes in IRQ-spanning calls must not be
attributed to `$F009`. These are emulated observations, not physical timing measurements.

A purpose-written synthetic cartridge supplied an IRQ callback that changed A, X, and Y during a
one-IRQ `$F009` wait. With both BIOS revisions, X returned as the callback's new value while Y was
restored, A returned zero, and S was restored. The previous unconditional X-preservation statement
therefore applies only to the uninterrupted/observed caller contexts, not arbitrary callbacks.
The synthetic cartridge used the documented header identifier, but no commercial program or asset
bytes.

### Two-plane VRAM zero-writer and LCD-control service: `$F00C`

The paired completed-call corpus contains 248 `$F00C` calls: 124 per BIOS revision across 57 titles.
Every call writes 12,800 VRAM bytes and 13,602 LCD registers. The corpus includes calls spanning
seven or eight IRQs, so those totals are not by themselves a clean measure of service-only timing or
peripheral effects.

The first call in each of the 57 titles was probed under both BIOS revisions, with the input status
preserved except for setting I and with VRAM prefilled to `$FF`. All 114 probes had the same main
effect profile: A returns `$40`, the output LCD flags are `$03`, the VRAM pointer ends at `$1900`,
12,800 distinct VRAM locations are written in 247,326 master clocks, and the cartridge ROM read
count is one. Detailed traces in Baseball, Dino Ball, and Volcano Panic identify that single read as
the stacked return-address dummy fetch, not a cartridge data input. Per-plane write coverage is
`$0000-$18FF` in both planes. A streaming value-count check in Baseball found zero-valued output
bytes for `$015` inputs `$00`, `$40`, and `$FF` under both BIOSes, without retaining the VRAM payload.

The first-call RAM access map in those detailed probes reads `$015` and stack bytes; it writes
`$01E` and stack bytes. Separate A, X, and Y perturbations in Baseball did not change the measured
operation. Across the tested `$015` patterns, A returns `$40` OR the `$015` input, X returns `$00`,
Y returns `$C8`, and S is preserved. With I set, C is set, Z is clear, and N follows returned A;
I, D, and V were preserved under the tested input flags. Timing remained 247,326 clocks across the
tested register and `$015` values.

Controlled one-bit `$015` perturbations in Baseball, paired across both BIOS revisions, show that
bits 0, 4, 5, and 7 respectively add `$20`, `$08`, `$04`, and `$10` to the outgoing LCD flags.
Bits 1-3 did not change LCD flags or VRAM coverage in those probes. Bit 6 changes the clear's
address pattern: instead of every address through `$18FF`, each plane receives a write at every
`$20`-byte interval through `$1CC0` (231 distinct addresses per plane), while the total remains
12,800 writes and the final pointer is `$1CE0`. This sparse path also matched in Dino Ball and
Volcano Panic under both BIOSes.

Combined `$015` values `$00`, `$3F`, `$40`, `$41`, `$50`, `$60`, `$C0`, `$7F`, and `$FF` were
tested in Baseball. The flag contributions compose independently:
`out_lcd = $03 | (bit 0 ? $20 : 0) | (bit 4 ? $08 : 0) | (bit 5 ? $04 : 0) | (bit 7 ? $10 : 0)`.
Bit 6 does not add an LCD flag; it selects the `$20`-stride address pattern. For every tested value
with bit 6 set (`$40`, `$41`, `$50`, `$60`, `$C0`, `$7F`, and `$FF`), the same flags and address
coverage were reproduced under both BIOS revisions. In sparse mode, the 462 unique locations are
231 addresses per plane, with identical coverage in both planes.

A full `$015` byte sweep (`$00-$FF`) was then run at Baseball's first `$F00C` occurrence under
both BIOS revisions, for 512 isolated probes. Every value matched the observed contract: A is
`$40 | RAM[$015]`; the LCD-flag formula above holds; bit 6 alone selects the sparse address set;
and X, Y, S, status, timing, write counts, VRAM coverage, and final pointer all matched their
expected invariants. Each probe took 247,326 master clocks and issued 12,800 VRAM writes and 13,602
LCD writes. The two BIOS revisions had no non-hash field or coverage differences, and their
whole-RAM hashes matched for every input value. This closes the `$015` byte-value map for this
authentic call context, but does not establish the hardware meaning of the stride or behavior in
every caller context.

The only alternate A result in the completed-call corpus is `$60`, in both BIOSes for Time Warrior's
second `$F00C` call. Probing that authentic occurrence shows outgoing LCD flags `$07`; changing
`$015` to zero changes A to `$40` and the flags to `$03`, while setting `$015` to `$20` reproduces
the authentic result. This confirms that `$015` is an active LCD-control input, not merely scratch.

Four title pairs have different whole-RAM hashes between BIOS revisions, although their reported
registers, counts, extents, and the checked read/write address sets match. The paired-snapshot
follow-up below localizes the differences to pre-existing, unchanged stack bytes. The ordered-port
analysis explains the bit-6 stride in the emulator; physical-hardware validation remains open.
Controlled calls mask IRQs to isolate the service; this does not imply that a replacement should
suppress the IRQs that occur during authentic calls.

#### `$F00C` executable contract

The diagnostic-only `gamatebiosf00c` checker models the complete ordered LCD write sequence,
register/status results, scratch effects, and service-only timing:

1. Read the cached LCD-control input from RAM `$015` and write that value OR `$80` to LCD register 1.
2. Write `$00` to scratch `$01E`. For each row `r = 0..199` in ascending order, read `$01E`,
   write that byte to register 4, write `r` to register 5, then write 32 zero bytes to register 7.
3. Write `$80` to `$01E`. Repeat the same 200 rows.
4. Read `$015` again, write that value OR `$40` to LCD register 1, and return.

Without interference, each plane receives 6,400 zero writes; the service issues 13,602 LCD writes.
The byte `$01E` is read once per row to obtain the value written to register 4 and, absent
interference, ends at `$80`. No other non-stack RAM writes were observed; `$015` is the only
non-stack byte read before any service write. The largest observed masked stack depth was two
bytes below entry S. A cartridge read at the pulled return address is the CPU's RTS dummy read,
not a graphics input.

The service does not change bit 6 in the first register-1 write. In this emulator, that bit controls
the LCD data-port increment, explaining the earlier sparse coverage without a separate BIOS
clear algorithm: when bit 6 is clear, the writes cover `$0000-$18FF` in each plane. When it is
set, each row's 32 data writes advance by `$20`; their union is the 231 addresses
`$0000,$0020,...,$1CC0` in each plane, with repeated overwrites. Other VRAM bytes are preserved.
The final pointer is `$1900` or `$1CE0`, respectively. The physical LCD behavior is not established
by this emulator-based result.

Returns and preserved state:

- A is the second `$015` value OR `$40`, X=`$00`, Y=`$C8`; S is restored. Entry A, X, Y, C and the
  existing VRAM and LCD addresses do not change the port sequence.
- In the emulator's stored status-byte representation,
  `P_out = (P_in & $5C) | $21 | (A & $80)`: I, D, V and the nonphysical B bit are preserved;
  N follows A, Z clears, and C sets.
- The final register-1 value is the second `$015` value OR `$40`; without interference, register 4
  has selected plane 1. Scroll registers, bank selection, and UART transmit state are preserved.
  No AY writes, UART clocks, or CPU VRAM reads were observed.

Without interrupts the call takes exactly 247,326 master clocks in the emulator, independent of
the tested control and status values. At the emulator's instruction-start diagnostic granularity,
its LCD-write timeline from service entry is:

```text
initial register 1: 16
plane p, row r: base = 56 + 123636*p + 618*r
    read $01E:  base - 6
    register 4: base
    register 5: base + 8
    register 7, byte j: base + 24 + 18*j       (j = 0..31)
final register 1: 247306
return: 247326
```

`p` is 0 or 1 and `r` is 0 through 199. The `$015` reads occur at clocks 10 and 247,300;
the `$01E` writes occur at clocks 28 and 123,664.
These event timestamps are instruction-boundary observations, not measured physical bus-cycle
edges. The service does not mask interrupts; interrupt handlers can extend the call or interfere
with `$015`, scratch state and LCD writes. Controlled mutations in both directions across bit 6
confirmed that the initial write and clear use the first `$015` value, while the final write and
returned A use the second. A separate mutation of `$01E` during the second plane affected
subsequent row-select writes; the service does not restore the modified value after that plane begins.

#### `$F00C` validation and limits

- Four standard 1,000-frame campaigns covered all 62 cartridges under both BIOSes. Across 61
  titles, all 1,596 calls (798 per BIOS) matched the masked port sequence, scratch effects,
  return state, checked peripherals, and total time. All original-interrupt-state replays matched
  the port and return-state checks; 1,572 spanned IRQ or NMI. No authentic entry had D set.
  Two already completed no-input jobs were included without rerunning them.
- An additional no-input pass checked every modeled LCD event time and final VRAM byte for 258
  calls, 129 per BIOS, across 61 titles. Synthetic cases in Baseball covered all 256 `$015`
  values and all 256 entry-status bytes at both possible A signs, with IRQ delivery suppressed
  during those synthetic runs. Both BIOSes passed 768 cases each; register values, stack positions,
  VRAM fills, and scroll positions were varied as well.
- Paired first-call snapshots across those 61 titles found four with differing whole-RAM hashes:
  Famous 7, Galaxy Invaders, One Million Whys, and QuizFighter. Each pair differed by one stack
  byte already present at entry; those bytes stayed unchanged and no new RAM differences arose.
  This localizes the earlier four paired hash discrepancies without publishing stack contents.
- Controlled mid-call `$015` and `$01E` mutations confirmed the two separate control reads and
  the per-row scratch dependency under both BIOSes. These are emulator observations, not physical
  measurements; arbitrary interfering handlers and exact physical bus-cycle timing remain untested.

### Plane-1 rectangle fill: `$F02D`

The combined 1,000-frame completed-call corpus contains 209,088 balanced `$F02D` calls split evenly
between BIOS revisions. Of those, 207,504 do not span an IRQ or NMI. Every non-interrupted call fits
the same register, write-count, and timing contract; the remaining 1,584 are excluded from direct
timing classification because their summaries include interrupt callback work.

Controlled authentic and synthetic-caller probes resolve these inputs:

| Input | Meaning |
| --- | --- |
| A | Byte value written to every destination. |
| X | Starting horizontal pixel coordinate; the starting VRAM byte column is `X >> 3`. |
| Y | Starting VRAM row. |
| RAM `$015` | Cached raw LCD-control value. The service retains its blank/window/plane-swap/stop bits and forces row increment. |
| RAM `$01C` | Horizontal pixel extent. The byte-column count is `$01C >> 3`. |
| RAM `$01D` | Vertical row count. |

The routine is column-major. It writes the cached LCD control once, forcing row increment. For each
byte column it programs plane 1, that column, and starting row Y, then writes A for `$01D` successive
rows. It advances its raw low-address byte to the next column and repeats. Ordinary callers use
nonzero dimensions, but both loops are 8-bit decrement loops: a derived column count of zero or row
count of zero executes 256 iterations. This also preserves raw register overflow behavior for invalid
or unusually large dimensions rather than clipping to the 160×150 viewport.

For `columns = $01C >> 3` and `rows = $01D`, interpreted with zero as 256 for their respective loops:

| Observable | Contract |
| --- | --- |
| VRAM writes | `columns * rows`, normally all to plane 1. |
| LCD writes | `1 + columns * (2 + rows)`: one control write, two address writes per column, then the data writes. |
| Master clocks | `120 + 58 * columns + 18 * columns * rows`. |
| Registers | A and S preserved; X and Y return zero. |
| Status | C and N clear, Z set; I, D, and V preserved. |
| Scratch | `$018` is the raw plane/column byte and finishes one column past the last start; `$019` caches Y, `$01A` caches A, and `$0A8` receives the plane-1 selector. |
| Other hardware | Scroll, AY, mapper, and UART state are preserved. |

For a derived 256-loop count in the replacement implementation, plane 1 adds `118` instead of
`120` in both 256×1 and 1×256 controlled shapes. The observed synthetic clocks are 19,574 and
4,784 respectively; the corresponding plane-0 values are 19,582 and 4,792.

The most common observed shape is 1×1 byte (205,796 clean calls), but the corpus includes widths up
to 21 byte columns and heights up to 200 rows. Mighty Boxer's first authentic call demonstrates a
20×48-byte fill. Both BIOS revisions produce identical controlled timing, VRAM coverage, output,
registers, scratch, and LCD state.

The diagnostic ROM coverage reports one cartridge read per call. Its address is the stacked return
address and it occurs during the BIOS `RTS`; this is the NMOS RTS dummy bus read already modeled by
the CPU, not a semantic cartridge parameter. `$F02D` therefore has no observed cartridge-data input.

`tools/gamate-bios-f02d-summary.py` validates this contract against one or more local completed-call
reports. It derives dimensions independently from LCD/VRAM write counts, checks the timing equation,
register/status behavior, and unrelated hardware effects, and emits no memory or firmware contents.
It reports zero mismatches across all 207,504 currently available non-interrupted calls.

## Remaining analysis

### Extend reachable paths

Apply repeatable Start, A, B, and directional schedules, including Tornado's known long Start press,
and compare coverage with the unattended baseline. Retain a targeted trace only when it reaches a new
slot, signature, return form, or hardware effect. Separately inject controlled UART input into World
Cup Soccer to exercise the NMI wrapper and `$6023` callback.

The deterministic controller campaign is now implemented by
`tools/gamate-bios-input-campaign.sh`. It runs four 1,000-frame schedules:

1. no input;
2. three long Start presses, including durations suitable for sparse polling;
3. Select/Start menu transitions followed by separate A and B actions;
4. every individual control plus legal direction-and-button combinations.

`tools/gamate-bios-call-coverage.py` compares each candidate with the accumulated coverage union. It
separates boundary events, direct service slots, cartridge/bank call sites, detailed call-site
signatures, and normalized ABI signatures. Frame numbers, elapsed time, and exact counter totals are
excluded from signature identity so that timing drift alone does not count as new coverage. Reports
for schedules that add neither a call site nor an ABI signature are discarded by the campaign tool.

The initial controller-cycle run passed all 116 cartridge/BIOS cases and added 979 service call sites,
5,817 detailed call-site signatures, and 4,027 normalized ABI signatures relative to unattended
execution. All 58 titles added boundary coverage and 57 added completed-call signatures. Repeated
long Start added another 250 call sites, 5,748 detailed signatures, and 3,042 normalized signatures
relative to the unattended/controller-cycle union. The menu/action schedule then added 37 call sites,
4,303 detailed signatures, and 2,350 normalized signatures relative to that expanded union.

No controller schedule exposed a nineteenth service slot, an NMI callback, or a callback target other
than periodic IRQ `$6026`. This supports—but cannot prove—that controller-driven software uses the
same 18-slot BIOS ABI. The large incremental signature counts show that unattended execution was not
sufficient for calling-convention analysis and that schedule timing materially changes reached code.

Basketball initially exceeded the tracer's 1,024-entry boundary-event aggregate after input reached
gameplay. The run had no pairing error; it produced 1,070 distinct boundary rows, largely because IRQ
returns reached many new cartridge locations. The diagnostic capacity is now 16,384 entries, and the
affected case and complete controller-cycle matrix pass without overflow.

### Probe narrow hypotheses

Use purpose-written cartridges and controlled entry-state perturbations only after call clusters
suggest a narrow contract. Vary one register or state field at a time and test named outcomes rather
than copying authentic working memory. This stage should establish which boot fields and helper
effects are functionally required by a clean-room resident replacement.

The first implementation of this stage is `gamatebiosprobe`, described in the `$F000` contract above.
Its option syntax permits controlled CPU-register, RAM, VRAM-fill, LCD, scroll, AY-selection, and
mapper-state perturbations. Its diagnostic output reports VRAM write coverage as logical plane/address
runs rather than interleaved storage offsets and BIOS-initiated cartridge reads as physical ROM-offset
runs. `occurrence=N` selects authentic caller context before applying perturbations. Public
documentation should retain only behavioral summaries needed to specify an ABI; local full-state
hashes are comparison aids rather than distributable firmware data.

### Separate mapper investigation

Mapper activation is a separate question from the service ABI. Its hardware evidence, lazy-activation
proposal, and required full-corpus checks are tracked in
[`MAPPER_DETECTION.md`](MAPPER_DETECTION.md); mapper accesses should not be treated as BIOS helper
semantics.

## Limits

The baseline is execution evidence for the paths reached in 1,000 unattended frames. It cannot prove
that an unobserved input combination, later level, failure path, or link session uses no additional
entry behavior. The tracer sees boundary crossings, not internal calls made entirely within BIOS
space. IRQ return destinations are deliberately retained in the raw local report but are not treated
as BIOS API entries.

The combined report remains outside the repository and contains filenames and derived addresses but
no firmware or cartridge payload. Source provenance and use are recorded in
[`../src/gamate/SOURCES.md`](../src/gamate/SOURCES.md).

