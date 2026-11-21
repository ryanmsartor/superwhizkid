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
ensures that the 16-bit state at `$0012-$0013` is nonzero, advances a feedback shift register, and
returns the new `$0012` low byte in A. `$0014` is scratch feedback overwritten by each call. This
name is based on measured state transitions and cartridge use, not on another emulator's label.

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

### First resolved contract: `$F030`

The pseudorandom service is the first slot with a sufficiently narrow observed contract. The table
combines 362 completed calls across both BIOS revisions; the two revisions have identical results.

| Property | Observed contract |
| --- | --- |
| State | Advances the nonzero 16-bit feedback state at `$012-$013`; `$014` is scratch feedback. |
| Return | A receives the new `$012` value. |
| Preserved registers | X, Y, and S in 362/362 calls. |
| Status | I, D, and V are preserved. N and Z describe returned A. Controlled probes establish that carry receives the pre-advance state’s bit 15, independent of incoming carry. |
| Uninterrupted timing | 174 master clocks (87 CPU cycles) for a nonzero state; 188 master clocks when the zero-state repair path is taken. |
| Hardware effects | None in the 360 uninterrupted calls: no VRAM, LCD, AY, mapper, UART, scroll, or hardware-address-state change. |
| Interrupt interaction | Two calls included a periodic IRQ, took 910 master clocks, and include callback-attributed effects. |

Observed input A values cover much of the byte range and do not correlate with the returned value,
but this is not yet a controlled proof that A is ignored. A controlled state matrix varied incoming
carry and selected states on either side of bit 15. Incoming carry had no effect; returned carry
always matched the state’s original bit 15. A zero state took 14 extra master clocks and produced the
same derived result as starting from `$FFFF`, establishing that the repair path seeds `$FFFF` before
the ordinary advance.

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

### Third resolved contract: `$F033`

`$F033` is observed only in Tornado in the current corpus. Controlled register, status, and output
inspection under both BIOS revisions resolves it as a six-digit packed-BCD formatter when entered
under the commercial calling convention with decimal mode clear.

| Property | Controlled contract |
| --- | --- |
| Input | A, X, and Y contain six packed-BCD digits in display order: A high/low, X high/low, then Y high/low. |
| Output buffer | Writes `$06` at `$0A1`, followed by six character bytes at `$0A2-$0A7`. Valid BCD nibbles become ASCII digits. Leading zeroes become spaces, except that an all-zero value retains the final `0`. |
| Return | A becomes `$06`; X and S are preserved; Y becomes the zero-based index of the first displayed digit, with `$05` returned for both a single final digit and the all-zero value. |
| Status | Carry is set. N and Z describe returned A (`$06`). I and V are preserved; D remains clear. |
| Timing | Depends on leading-space count: 222, 266, 310, 354, 398, or 422 master clocks for first displayed positions 0 through 5; all-zero input also takes 422. |
| Inherited RAM | No non-stack RAM is read before write. The service writes exactly `$0A1-$0A7` outside its stack use. |
| Hardware effects | None: VRAM/LCD state, scroll, AY selection/registers, mapper state, UART, IRQ, and NMI remain unchanged. |

For example, packed input A/X/Y = `$12/$34/$56` produces a six-character `123456` field, while
`$00/$00/$01` produces five spaces followed by `1`. Tornado immediately supplies the buffer at
`$0A1` to service `$F012`, consistent with `$06` being the field length consumed by the downstream
display helper.

The contract is limited to valid packed-BCD input with D clear. Values containing nibbles `$A-$F`
produced non-decimal character or suppression behavior, and setting D changed the routine's internal
arithmetic. Neither condition was observed in commercial calls, so replacement firmware should not
promise those undocumented behaviors. The output examples are derived interface behavior; no BIOS
instruction listing or firmware data table was retained.

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

The next analysis step is a structural sweep of authentic streams across calls and titles: count rows
and runs, sum horizontal spans, identify repeated stream/base triples, and flag wrapping or out-of-range
selectors. That can distinguish the ordinary contract from caller-specific edge cases before a
clean-room replacement is designed.

### Fixed-height tile-run contract: `$F012`

The combined completed-call corpus contains 39,034 balanced `$F012` calls, evenly split between BIOS
revisions. Of those, 36,518 do not span an IRQ or NMI. Every non-interrupted call fits the same
register and write-shape contract; interrupt-spanning calls are excluded because their summaries mix
callback work into the service effects.

Controlled authentic-call probes identify a compact descriptor stream in cartridge ROM. RAM
`$000-$001` points to its first byte, the number of horizontal tiles. One tile selector follows for
each column. Every selector produces eight row bytes at `(X >> 3, Y)` through
`(X >> 3, Y + 7)`, after which the service advances one byte column. The starting X and Y registers
therefore specify pixel coordinates, while entry A is ignored. The cached LCD-control byte at `$015`
supplies the selected plane and other retained mode bits.

Every tile unconditionally overwrites eight complete destination bytes. A controlled selector whose
output was all zero still issued eight VRAM writes: an initially zero destination showed no changed
bytes, while `$FF` and `$5A` fills were replaced. There is no transparent-row path.

| Observable | Contract |
| --- | --- |
| Descriptor | One width byte followed by `width` tile selectors. |
| Graphics lookup | Each selector reads one run of eight cartridge bytes; identical selectors read identical runs. Output row `r` is a fixed single-byte function of graphics byte `r`. The selector-to-address transform remains unresolved. |
| VRAM writes | `8 * width`. |
| LCD writes | `11 * width`. |
| Registers | A and Y return the tile width; X returns the final selector multiplied by eight; S is restored. |
| Status | N clears and Z sets; I and D are preserved. Carry and V depend on intermediate address arithmetic. |
| Scratch | `$018-$01B` are service scratch. Stack bytes `$1F9-$1FB` are also written during the call. |
| Other hardware | Scroll, AY, mapper, and UART state are preserved. |

Width perturbation expands both the selector stream and destination coverage exactly one column at a
time. Selector perturbation changes only the corresponding eight-byte graphics lookup and output.
X perturbation moves the byte column at each eight-pixel boundary, Y perturbation moves the first row,
and A perturbation has no effect. Both BIOS revisions produced identical ordered ROM coverage,
timing, VRAM coverage, output, registers, and LCD activity in the controlled comparison.

Observed authentic widths range from 1 through 18 tiles.

Controlled mapping study (one authentic Mighty Boxer call, width 15, 6,714 cycles, both BIOS
revisions): address-only tracing shows the width byte, then one selector read per tile, with each tile
then reading a contiguous run of eight cartridge bytes. Tiles that share a selector read the same run
and produce identical output. For eight-read tiles, output row `r` is a consistent single-byte function
of graphics byte `r`: 96 rows covering 16 distinct graphics values had no conflicting outputs. The
mapping is not a copy or bit reversal, and it is not reproduced here. Two tile forms issue extra
reads. In 16-read tiles, each row reads a dummy address (the data address minus `$100`) before the data
read. The wrap row of a 9-read tile reads `$00` before `$100`. Perturbing the dummy reads changed neither
VRAM nor cycles. Interpreting them as 6502 page-crossing dummy accesses is an inference.

Timing follows the descriptor, not graphics content. In this call, the only perturbations that changed
the cycle count were descriptor bytes: the width byte produced a +100,036-cycle delta, and selector
bytes produced deltas between −30 and +16 cycles. Changing any of the 130 unique graphics or dummy-read bytes left the
cycle count unchanged. The VRAM maps and cycle deltas were identical for both BIOS revisions.

Open questions are the selector-to-address transform (a simple linear offset does not fit the observed
selector/run pairs), the origin of the 16-entry mapping, and an exact cycle formula. Per-selector cycle
cost has not been isolated. None of these is needed to validate the resolved drawing and calling
convention above.

`tools/gamate-bios-f012-summary.py` derives width from LCD/VRAM write counts and validates the write
shape, return registers, status invariants, stack balance, and unrelated hardware effects. It reports
zero mismatches across all 36,518 available non-interrupted calls.

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

### IRQ-gated counter wait: `$F006`

The validated report contains 6,326 `$F006` calls; 192 uninterrupted calls are evenly split between
the two BIOS revisions and span 15 titles. Every uninterrupted call takes 42 master clocks and has
the same register, RAM-write, status, and hardware-effect shape.

| Observable | Contract |
| --- | --- |
| Entry A | Ignored; A returns zero. |
| Entry RAM `$00B` | Values 0–4 wait; 5 and tested higher values return immediately. |
| RAM `$00B` on return | Written as zero. |
| Registers | X, Y, and S are preserved. |
| Status | C and Z set; N clear; I, D, and V preserved. |
| Ready-path timing | 42 master clocks when the wait condition is already satisfied. |
| Other hardware | No VRAM, LCD, AY, mapper, or UART effects in uninterrupted calls. |

Controlled probes in Dinosaur Park and Witty Apee, under both BIOS revisions, set `$00B` to each
value from 0 through 5. The calls took respectively 5, 4, 3, 2, 1, and 0 periodic IRQs before
returning; `$00B=5` returned immediately. Values `$7E`, `$7F`, `$80`, `$81`, and `$FF` also took the
42-clock ready path. With IRQs masked and `$00B=0`, the probe did not observe a clean return within
its 10-million-instruction bound. These results imply that the IRQ path advances or otherwise
satisfies a `$00B` threshold; the intermediate counter values were not traced directly, so the exact
incrementing mechanism remains an inference.

The corpus supports the 42-clock ready-path contract across 15 titles, while the controlled wait
matrix covers two titles. Wait-path timing varies with the IRQ phase and callback work and is not
specified by a fixed cycle equation. RAM and peripheral changes during an IRQ-spanning call must not
be attributed to `$F006` without separating callback effects.

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

### Parallel mapper audit

After the completed-call tracer is stable, instrument executed `$8000-$9FFF` accesses over the full
catalog before considering lazy 4-in-1 mapper activation. This is independent of BIOS-helper
semantics but can remove a cartridge fingerprint if ordinary software never triggers a false
activation.

The original implementation checklist remains:

1. Pair each direct entry with its matching BIOS-to-cartridge return while allowing nested IRQ/NMI
   callback crossings. (Implemented and validated across the unattended matrix.)
2. Record input/output CPU registers and flags for each completed helper call. (Implemented.)
3. Record summarized RAM, VRAM, LCD, AY, UART, mapper, and timing changes without retaining
   copyrighted buffers. (Implemented.)
4. Cluster call signatures across titles to identify shared calling conventions. (Aggregate and
   selected-slot reporting implemented; semantic classification remains.)
5. Exercise gameplay states, Start/button paths, and link traffic; compare newly reached slots and
   call signatures against the unattended baseline. (Deterministic controller campaign implemented;
   adaptive gameplay and link/NMI coverage remain.)
6. Use purpose-written cartridges to probe individual slots only after their observed inputs and
   effects suggest safe, narrow hypotheses.

These stages should continue to report behavioral facts rather than publish BIOS instruction
listings or translated source.

## Limits

The baseline is execution evidence for the paths reached in 1,000 unattended frames. It cannot prove
that an unobserved input combination, later level, failure path, or link session uses no additional
entry behavior. The tracer sees boundary crossings, not internal calls made entirely within BIOS
space. IRQ return destinations are deliberately retained in the raw local report but are not treated
as BIOS API entries.

The combined report remains outside the repository and contains filenames and derived addresses but
no firmware or cartridge payload. Source provenance and use are recorded in
[`../src/gamate/SOURCES.md`](../src/gamate/SOURCES.md).

