# BIOS runtime API analysis

Commercial Gamate software continues to execute authentic BIOS code after cartridge entry. This
document records execution-boundary evidence for that runtime dependency and defines the remaining
work needed to specify an independently authored replacement ABI.

## Execution-aware tracer

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

## Complete unattended baseline

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

## Observed service-slot use

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

## Replacement-firmware implications

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

## Interrupt callback register effects

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

## Next analysis stages

The tracer establishes the service surface. Work should proceed in the following order so that each
stage produces evidence needed by the next rather than merely accumulating longer traces.

### Stage 1: pair calls with returns

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

### Stage 2: cluster signatures

Group completed calls by service slot, input/output register tuple, memory-change shape, hardware
effects, and timing range. Begin with `$F02D`, `$F015`, and `$F012`, which together dominate the
observed call volume, then cover `$F02A`, `$F003`, `$F006`, and `$F027`. Treat clusters as candidate
calling conventions, not semantic names, until targeted perturbations confirm them.

This stage is now in progress. The completed-call report and summary tool retain hardware-state
transitions in addition to write counters, allowing a helper that changes LCD configuration to be
distinguished from one whose writes merely occur while the prior configuration remains active.
Selected-slot summaries also report register preservation, status-bit behavior, timing grouped by
input A, and the most frequent hardware-state transitions.

### Pseudorandom service: `$F030`

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

### Second resolved contract: `$F000`

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

### Fourth resolved contract: `$F024`

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

### Fifth resolved contract: `$F00F`

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

### Caller-context investigation: `$F018`

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

This extends the stream grammar beyond the previously inspected callers, but it covers only the
first authentic call in each title. The precise interleaving of control bytes and graphics-record
reads, the repeated stream/base triples, and later invocation contexts remain open. The sweep used
address-only traces and transient local ROM reads to derive counts; no stream or graphics payload
was saved or published.

### Plane-0 row-run stream service: `$F01B`

`$F01B` is fully defined for every observed calling context and for the synthetic edge classes
listed below. It clears a fixed 25-row band of plane 0, then decodes a caller-supplied row/run
stream into LCD port writes. An independent model in `tests/gamatebiosf01b.c` reproduces the BIOS's
LCD port-write sequence, returned registers and status, RAM side effects, and exact elapsed clocks.

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
different read/output profile. This is consistent with `$000-$001` supplying a ROM-side control or
descriptor address, but the format is not resolved.

In Time Warrior, `$006-$007` and `$008-$009` act as separate 16-bit source bases for the two VRAM
planes. One-bit XORs to either byte of the first pair changed the streamed output hash for plane 0
only; XORs to either byte of the second pair changed plane 1 only. Each change preserved the tested
write count and coverage while shifting the corresponding ROM-read coverage. Output bytes were
streamed directly into per-plane hashes and counts; no graphics payload was retained.

`$015` is an active LCD-control input in the tested contexts: XORing its low bit changes the final
LCD flags from `$03` to `$23` in both Famous 7 and Time Warrior, without changing their VRAM output
hashes or write counts. The bit's hardware meaning is not established. The exact control-stream
grammar, source units and record layout, return-Y rule, and status-flag contract remain open.
Controlled whole-RAM hashes differed between BIOS revisions in the four probed titles despite
matching access maps and measured operation fields; the cause is unresolved. These are emulator
observations, not physical-hardware measurements.

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
inputs and `$008-$009` as plane-1 source inputs across this caller set, but the exact address
transform and stream grammar remain unresolved. VRAM bytes were hashed as they streamed from the
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
influential and rewritten, but these low-bit tests do not establish whether they encode a pointer,
descriptor, or mutable parser state. Their roles and the stream grammar remain unresolved.

### ROM-backed single-plane graphics service: `$F021`

The 1,000-frame inventory records 46 `$F021` calls per BIOS across 14 titles. The paired
balanced-call corpus contains 92 calls. Every call returns A=`$FE`, X=`$00`, and preserves S; Y
varies. Sixty-six calls span an IRQ, so corpus timing and memory-effect summaries include callback
work. The paired reports match in every field except `ram_changed` for one IRQ-spanning Volcano
Panic call from `$AF24` at frame 350 (12 versus 13 changed bytes). Because that call spans an IRQ,
the extra RAM change cannot be attributed to `$F021`.

Controlled first-call probes in Volcano Panic and Famous 7 set I and were run under both BIOS
revisions. They produce plane-0 VRAM writes only, with no AY, mapper, UART, IRQ, or NMI effects.
Volcano Panic takes 25,174 clocks and issues 933 VRAM writes, covering 800 unique bytes in
`$1CC0-$1FDF`; Famous 7 takes 19,882 clocks and issues 860 writes over the same 800-byte coverage.
Their LCD-write counts are 998 and 919, respectively, and their returned Y values are `$93` and
`$44`. The ROM-address traces contain 176 reads in Volcano Panic and 70 in Famous 7. The access maps
and all other reported summaries match between BIOS revisions in these probes, but whole-RAM hashes
differ; the cause is unresolved. These per-title times are examples, not a timing formula.

In the Volcano Panic context, `$000-$001` are read and preserved. Bit-0 XOR perturbations change
the ROM-read and output profiles: XORing `$000` changes Y from `$93` to `$94`, changes the ROM-read
count from 176 to 177, and changes the write profile from 933 writes over 800 addresses to 934 over
820 addresses. XORing `$001` produces 567 ROM reads (539 unique offsets), 1,189 VRAM writes over
800 addresses, and 42,348 clocks. This pair is caller-supplied control or source data; its exact
format and address transformation remain unknown. Changing entry A, X, or Y does not change the
tested output or ROM-read profile. Changing the entry VRAM pointer to `$0000` or `$1FFF` also leaves
the output unchanged in this context. Scroll values are preserved.

RAM `$015` is read and preserved, and its value affects LCD flags. In Volcano Panic, tested values
`$01`, `$10`, `$20`, and `$80` produce output flags `$20`, `$08`, `$04`, and `$10`; `$02`, `$04`,
and `$08` produce `$00`, while `$3F` produces `$2C`. Setting bit 6 selects a distinct sparse
addressing pattern: in both Volcano Panic and Famous 7, plane-0 writes cover 56 addresses in
`$0000-$03A0` and `$1CC0-$1FE0` at `$20` intervals, and the final LCD flags are `$02`. The final
VRAM pointer is `$0140` in Volcano Panic and `$01A0` in Famous 7, so this mode's full meaning is not
established.

These are emulator observations from controlled, authentic call contexts, not physical-hardware
measurements. The service's source-data grammar, selector-to-ROM-address mapping, return-Y rule,
status-flag contract, and exact timing remain unresolved. ROM traces retained addresses only;
graphics values and BIOS/ROM payloads were not retained.

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

### Plane-0 rectangle-fill contract: `$F02A`

A controlled Baseball call identifies `$F02A` as a column-major plane-0 rectangle fill with a caller
supplied byte. RAM `$01C` supplies the horizontal pixel extent and RAM `$01D` supplies the vertical
row count. The byte-column count is `$01C >> 3`; both BIOS revisions agree on the call.

For `columns = $01C >> 3` and `rows = $01D`, with ordinary nonzero counts:

| Observable | Contract |
| --- | --- |
| VRAM writes | `columns * rows`, all in plane 0. |
| LCD writes | `1 + columns * (2 + rows)`. |
| Master clocks | `126 + columns * (58 + 18 * rows)`. |
| Registers | A and S are preserved; X and Y return zero. |
| Status | C and N clear, Z sets, I and D are preserved, and V is preserved. |
| Scratch | `$018-$01A` hold service scratch and `$0A8` receives the plane-0 selector. |
| Other hardware | Scroll, AY, mapper, and UART state are preserved. |

The baseline 1×1 call writes A once at `(X >> 3, Y)` and takes 202 master clocks. Width and height
perturbations produce the corresponding column-major coverage and the LCD/write/timing equations
above. As with the 8-bit loops in the neighboring fill services, a derived zero count is interpreted
through 256 iterations; a 256×256 controlled probe therefore reaches 65,536 writes, but the general
normal-dimension formula should remain the primary contract.

The validated completed-call corpus contains 9,414 `$F02A` calls, evenly split between BIOS revisions.
Excluding 394 interrupt-spanning calls leaves 9,020 uninterrupted calls. An added
`tools/gamate-bios-f02a-summary.py` derives dimensions independently from LCD/VRAM write counts and
reports zero mismatches for timing, register/status behavior, and unrelated AY/bank/UART activity.
The only measured ROM read is the stacked cartridge `RTS` dummy address, not a semantic data read.

### Caller-dependent blitter: `$F027`

The validated completed-call report contains 7,026 `$F027` calls, evenly split between BIOS revisions.
Of those, 2,366 do not span an IRQ or NMI. A controlled first occurrence in Baseball resolves the
caller pointer effects but not a title-independent ABI.

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
one span in Baseball and Nightmare, while in Time Warrior it adds a read and two clocks. The exact
shared roles of `$006-$009` therefore remain unresolved.

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

Thus these RAM bytes affect the operation in caller-dependent ways, not as a recovered title-independent
geometry format. Both BIOS revisions matched for every measured title/perturbation pair.

Entry A, X, Y, and S are preserved in these ten selected baseline calls. The ten-title evidence
supports a caller-RAM-driven blitter family, not a single recovered text or graphics format. Exact
pointer roles, control-field meanings, encoding, terminator grammar, and timing remain open. The
addresses `$023`, `$028`, and `$030` are empirical perturbation sites, not semantic field names.

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
as two caller-supplied 16-bit cartridge-address words. Their precise data roles remain
caller-dependent; the three single-byte inputs still have no established shared meaning.

This confirms these bytes are caller-owned inputs in the tested calls, not a universal parameter
schema. It does not resolve their per-title meanings or stream grammar. The analysis uses only
derived counts, hashes, coverage, and ROM addresses; RAM-watch final values and source payloads are
omitted.

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

These results identify horizontal and vertical position inputs for selected drawing paths, not a
complete `$F027` grammar or a guarantee about all later calls. Galaxy Invaders reads its inherited
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
RAM, status, and hardware-effect signature. Across all completed calls, A returns zero, X/Y/S are
preserved, and C/Z are set, N is clear, and B/D/I/V are preserved.

| Observable | Contract |
| --- | --- |
| Entry A | Ignored; A returns zero. |
| Entry RAM `$00B` | Values `$00-$17` wait `24 - n` IRQs (`n` is the numeric entry value); `$18-$FF` take the ready path. |
| RAM `$00B` on return | Written as zero. |
| Registers | X, Y, and S are preserved. |
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
registers, counts, extents, and the checked read/write address sets match. The hash differences do
not localize a service-output mismatch; their cause remains unresolved. The bit-6 stride's hardware
meaning and physical-hardware behavior remain open. Controlled calls mask IRQs to isolate the
service; this does not imply that a replacement should suppress the IRQs that occur during
authentic calls.

### Rectangle-fill contract: `$F02D`

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
| Master clocks | `120 + columns * (58 + 18 * rows)`. |
| Registers | A and S preserved; X and Y return zero. |
| Status | C and N clear, Z set; I, D, and V preserved. |
| Scratch | `$018` is the raw plane/column byte and finishes one column past the last start; `$019` caches Y, `$01A` caches A, and `$0A8` receives the plane-1 selector. |
| Other hardware | Scroll, AY, mapper, and UART state are preserved. |

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

### Stage 3: extend reachable paths

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

### Stage 4: probe narrow hypotheses

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

