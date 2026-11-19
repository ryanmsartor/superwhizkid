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

The table establishes entry use, not helper semantics. Names such as drawing, delay, formatting, or
input must not be assigned until register, RAM, and hardware effects are measured independently.

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

The tracer establishes the service surface. Determining behavior requires additional layers:

1. Pair each direct entry with its matching BIOS-to-cartridge return while allowing nested IRQ/NMI
   callback crossings.
2. Record input/output CPU registers and flags for each completed helper call.
3. Record summarized RAM, VRAM, LCD, AY, UART, mapper, and timing changes without retaining
   copyrighted buffers.
4. Cluster call signatures across titles to identify shared calling conventions.
5. Exercise gameplay states, Start/button paths, and link traffic; compare newly reached slots and
   call signatures against the unattended baseline.
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

