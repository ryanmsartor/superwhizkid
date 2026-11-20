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

The first baseline covered 58 cartridges under both BIOS revisions for 1,000 post-entry frames:

- 116/116 combinations completed without tracer overflow or execution failure.
- Every cartridge made at least one direct call into the BIOS.
- Every direct entry targeted one of 18 slots from `$F000` through `$F033`, spaced exactly three
  bytes apart.
- The complete direct-event multiset was identical across BIOS revisions: cartridge, source address,
  mapper bank, target slot, transition type, call count, and first-observed frame all matched.
- Per BIOS, the run observed 161,958 direct cartridge-to-BIOS calls from 1,031 distinct
  source/bank/target event rows.
- Direct entries were `JSR` except for tail jumps from Nightmare of Santa Claus to `$F027` and from
  Tennis to `$F015`. No indirect jump, `BRK`, fall-through, or target outside the service table was
  observed.
- All 58 titles received periodic cartridge IRQ callbacks at `$6026` during the window.
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
| `$F003` | 29 | 3,517 | 2 | `JSR` |
| `$F006` | 25 | 3,169 | 5 | `JSR` |
| `$F009` | 38 | 1,863 | 3 | `JSR` |
| `$F00C` | 57 | 124 | 1 | `JSR` |
| `$F00F` | 35 | 98 | 1 | `JSR` |
| `$F012` | 35 | 19,274 | 5 | `JSR` |
| `$F015` | 38 | 20,610 | 5 | `JSR`, tail `JMP` |
| `$F018` | 15 | 24 | 5 | `JSR` |
| `$F01B` | 12 | 17 | 5 | `JSR` |
| `$F01E` | 18 | 109 | 5 | `JSR` |
| `$F021` | 14 | 46 | 5 | `JSR` |
| `$F024` | 38 | 63 | 1 | `JSR` |
| `$F027` | 24 | 3,515 | 5 | `JSR`, tail `JMP` |
| `$F02A` | 21 | 4,707 | 1 | `JSR` |
| `$F02D` | 20 | 104,471 | 3 | `JSR` |
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
to `$6020`. A direct jump without the table would fail immediately in all 58 titles observed here.

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

The first full paired sweep completed all 116 cartridge/BIOS cases under a stricter validator that
fails a run on any unmatched BIOS return or pairing-stack overflow:

- 323,838 service calls completed within the observation windows: 161,919 per BIOS revision.
- Another 39 calls per BIOS began before the final frame boundary and remained legitimately open.
- Every slot's completed-call count matched exactly between BIOS revisions.
- For a given cartridge, source, slot, mapper state, and input/output register tuple, the complete
  signature multiset was identical between BIOS revisions.
- Phase-sensitive frames, elapsed clocks, interrupt inclusion, and resulting memory/peripheral
  summaries sometimes differed, as expected from the known 58-master-clock cartridge-entry offset.

The balanced register signatures strengthen the conclusion that both BIOS revisions expose the same
calling convention. They do not yet identify the meaning of any slot.

| Slot | Completed calls | Clock range | Calls changing VRAM | Calls changing AY registers | Calls spanning IRQ/NMI |
| --- | ---: | ---: | ---: | ---: | ---: |
| `$F02D` | 208,940 | 196–114,978 | 42,556 | 60 | 1,580 |
| `$F015` | 41,220 | 974–23,158 | 33,998 | 58 | 2,848 |
| `$F012` | 38,546 | 494–10,692 | 6,890 | 34 | 2,486 |
| `$F02A` | 9,414 | 202–114,436 | 8,822 | 8 | 394 |
| `$F003` | 7,008 | 42–268,662 | 0 | 1,748 | 6,948 |
| `$F027` | 7,026 | 6,326–309,980 | 2,898 | 414 | 4,660 |
| `$F006` | 6,326 | 42–168,820 | 0 | 1,338 | 6,134 |

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
| Status | I, D, and V preserved in 362/362 calls; N and Z describe returned A in 362/362 calls. Carry behavior remains unresolved. |
| Uninterrupted timing | 174 master clocks (87 CPU cycles). |
| Hardware effects | None in the 360 uninterrupted calls: no VRAM, LCD, AY, mapper, UART, scroll, or hardware-address-state change. |
| Interrupt interaction | Two calls included a periodic IRQ, took 910 master clocks, and include callback-attributed effects. |

Observed input A values cover much of the byte range and do not correlate with the returned value,
but this is not yet a controlled proof that A is ignored. Likewise, carry matched its entry value in
only 224 of 362 calls; its relationship to feedback state needs a purpose-written state probe before
replacement code can promise a carry result. Replacement tests should initially require the resolved
properties above and leave carry unspecified.

`$F000` is the next narrow candidate rather than one of the high-volume slots. Its sixteen observed
calls are uninterrupted and identical in duration at 3,278 master clocks. Each issues 64 VRAM writes,
ends at VRAM address `$128B`, and leaves scroll, AY selection, and mapper bank unchanged. It also
forces A/X/Y to `$38/$40/$20`. It sets the diagnostic plane/increment flags while retaining an
already-set window flag. This is consistent with a fixed drawing helper, but its visual purpose and
input contract remain unnamed pending controlled probes.

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

