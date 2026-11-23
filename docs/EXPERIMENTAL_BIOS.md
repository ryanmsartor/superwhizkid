# Experimental Gamate BIOS prototype

This independently authored, 4 KiB resident firmware is an **incomplete experiment**, initially
targeting the standalone Cube Up cartridge. It is not an authentic BIOS, a drop-in replacement, or
a claim of hardware accuracy. The libretro core recognizes it as `gamate_bios_swk.bin`; UMC remains
the default BIOS priority, with SWK used as a fallback when neither authentic BIOS is available.

The source implements published protocol and behavioral observations, not the original BIOS
instructions, startup graphics, or cartridge program data. The protection identifier and cartridge
header text are exact format/protocol checks, not a copied firmware or game image. Do not copy
commercial BIOS or cartridge files into the repository or rename this prototype as an authentic
BIOS.

## Building and running

The optional build requires `ca65` and `ld65` from [cc65](https://cc65.github.io/). Default builds
do not require them and do not build or install this firmware.

```sh
cmake -S . -B build-bios -DSUPERWHIZKID_EXPERIMENTAL_BIOS=ON
cmake --build build-bios --parallel --target superwhizkid_libretro gamateboot gamatebiosprototypetest
ctest --test-dir build-bios --output-on-failure -R '^gamate-bios-prototype$'
GAMATEBOOT_EXPERIMENTAL_BIOS=1 build-bios/gamateboot \
  build-bios/gamate_bios_swk.bin path/to/cartridge.bin 1000
```

Replace the cartridge argument with a legally obtained local image. `GAMATEBOOT_EXPERIMENTAL_BIOS=1`
enables diagnostic trap reporting in `gamateboot`; it must only be used with this experimental
image. Protection, identity and checksum failures and unsupported service/NMI traps then print an
error and return a nonzero exit status. Without this option, `gamateboot` does not recognize these
prototype-specific trap addresses. The image is also usable explicitly through the library's
`gamateLoad` BIOS argument, but unsupported calls will halt in the firmware rather than produce a
frontend notification.

For interactive testing in RetroArch, place the generated `gamate_bios_swk.bin` in the frontend's
system directory and load standalone Cube Up with the newly built core. Select **BIOS Priority:
SWK** and reload content to prefer the replacement without changing either authentic BIOS file.
UMC priority tries UMC, Bit, then SWK; Bit priority tries Bit, UMC, then SWK; SWK priority tries SWK,
UMC, then Bit. Missing or incorrectly sized files are skipped; each BIOS must be exactly 4 KiB.
Invalid option values use UMC priority. Reset retains the BIOS loaded with the content.

The frontend displays an experimental-BIOS notice when SWK is loaded, including through fallback.
It does not detect the prototype's unsupported-service traps, which can leave the game halted.
This integration enables testing; it is not a compatibility claim.

An earlier interactive check reached only Cube Up's title and setup screens. Starting a game halted
in the `$F036` unsupported trap because seven gameplay slots were missing. Those slots are now
implemented, and automated runs reach and continue gameplay. A follow-up RetroArch session started
play successfully and continued for 12 minutes 36 seconds; user-observed gameplay, score digits and
piece behavior appeared good.

## Implemented subset

- Reset performs the documented serial protection exchange, checks the exact cartridge identity
  field and the `$7000-$7FFF` checksum, then constructs the tested RAM policy, clears VRAM,
  initializes AY/LCD state and enters at `$6020`. It does not show the authentic startup sequence
  or reproduce its duration and phase.
- The periodic IRQ handler increments the `$00B` wait counter and the big-endian `$00E-$011`
  counter, calling `$6026` when `$00C` is nonzero. Its register and callback behavior is tested
  with a synthetic cartridge; broader interrupt interference remains unverified.
- The NMI wrapper reads the completed UART receive byte from `$4800` into RAM `$00A`, then
  conditionally calls `$6023` when `$000C` is enabled. Synthetic tests cover enabled/disabled
  dispatch and two received-byte values. Callback register clobber, nested interrupts, live
  Netpacket traffic and physical timing remain unvalidated.
- The eighteen Cube Up and general service slots observed in title and gameplay runs have initial
  implementations:
  - `$F000`: fixed bitmap destination.
  - `$F003`: eight-IRQ wait.
  - `$F006`: five-IRQ-threshold counter wait.
  - `$F009`: IRQ wait.
  - `$F00C`: two-plane clear.
  - `$F00F`: fixed plane-0 rectangle clear.
  - `$F012`: glyph run.
  - `$F015`: two-plane bitmap copy.
  - `$F018`: pair-tile stream.
  - `$F01B`: row-run stream.
  - `$F01E`: two-pass tile stream.
  - `$F021`: plane-0 literal row stream.
  - `$F024`: record initialization.
  - `$F027`: object-record compositor.
  - `$F02A`: plane-0 rectangle fill.
  - `$F02D`: plane-1 rectangle fill.
  - `$F030`: random-number step.
  - `$F033`: decimal digit formatting.

  `$F000` writes a deliberately **blank, independently authored placeholder** rather than the
  authentic asset. The other services implement their documented ordinary data effects, not their
  exact instruction or event timings. Their scratch use, stack layout and arbitrary interrupt
  interactions are not claimed to be equivalent. Decimal-mode calls, side-effectful or aliased
  sources, and unvisited game paths are not generally validated. `$F027` clears decimal mode
  internally and does not model decimal-mode entry.
- All other slots enter the explicit `$F036` unsupported trap.
  Link NMI is implemented independently below. Protection, identity,
  and checksum failures enter distinct traps at `$F039`, `$F03C`, and `$F03F`, respectively.
  Link reception and BRK-specific behavior are not implemented. Other games are not validated.
  Standalone Cube Up has received a successful interactive SWK run, but this is not a complete
  playthrough or a hardware-validation result.

### Remaining implementation deficiencies

This table tracks unresolved replacement-firmware work separately from the timing tracker. Status
is "Open", "In progress", "Partially complete", or "Cleared".

| # | Deficiency | Status | Current boundary |
| --- | --- | --- | --- |
| 1 | UART receive NMI and `$6023` cartridge callback dispatch | Cleared | Emulator-level implementation reads `$4800` into RAM `$00A`, dispatches direct or absolute-JMP `$6023` stubs only when `$000C` is enabled, restores A, retains callback-modified X/Y, resumes partial receive-byte state, and covers all 64 P combinations and 256 S values in synthetic tests. Live Netpacket timing, callback reentrancy, nested IRQ/NMI and physical timing remain outside the emulator-level completion claim. |
| 2 | BRK-specific cartridge and interrupt behavior | Open | The wrapper and synthetic IRQ path do not establish BRK-specific stack, vector, or callback behavior. |
| 3 | Interrupt interference and callback aliasing | Open | Arbitrary callback modifications to RAM, LCD, streams, scratch, stack, nested IRQ/NMI, and callback register state are not fully modeled. |
| 4 | Decimal-mode and aliased-source edges | Open | D-set behavior and mutable I/O/scratch/stack aliases are outside most validated ordinary contracts. |
| 5 | Save/reset/NMI and in-flight UART lifecycle | Open | Core UART state is serialized, but successful SWK state load/reset paths with pending NMI, `$6023` callback, and partial receive bytes need firmware-specific validation. |
| 6 | Authentic startup presentation and boot phase | Open | Reset performs protection, identity and checksum checks and enters `$6020`, but does not reproduce the authentic startup graphics or hardware/audio/frame phase. |
| 7 | Broad cartridge and interactive hardware validation | Open | Cube Up is strongly exercised, while broad 4-in-1, network-link, save-state and complete-playthrough coverage remains incomplete. |

The chosen low-RAM initialization is a compatibility policy, not measured physical power-on state.
The common entry contract and its uncertainty are described in
[`BIOS_REPLACEMENT_ANALYSIS.md`](BIOS_REPLACEMENT_ANALYSIS.md); service contracts and their
limitations are in [`BIOS_RUNTIME_API_ANALYSIS.md`](BIOS_RUNTIME_API_ANALYSIS.md).

## Validation and limits

The synthetic test suite checks successful and rejected boots, reset, dirty initial VRAM, IRQ
counter carry, callback gating and register preservation, unsupported traps, IRQ waits, and the
eighteen service implementations. It exercises all 256 glyph selectors with both incoming overflow
states, a 256-tile run, wrapped stacks and streams, both LCD-clear modes, and rectangle boundaries for both planes. The newer cases cover:

- The short wait and the five-IRQ-threshold counter wait.
- The fixed plane-0 `$F00F` 775-byte rectangle, with exact timing and scratch/register behavior.
- `$F015` two-plane bitmap copies in 1×8, 2×8, 3×8, 4×8 and source-page-crossing shapes.
- `$F021` literal row streams, escapes, terminating `$FE`, row clearing and register/status behavior.
- The random step.
- Digit formatting.
- Opaque and transparent pair streams.
- Row runs.
- `$F027`: an all-terminator table, plus large-redraw, small-redraw-plus-object and large-object
  cases. These are compared against reference models derived from the documented contract.

Mutation checks confirmed that removing the compositor mask, or changing the row-run base, fails
the suite.
The synthetic cartridge and firmware are purpose-written and contain no commercial images.

Four 1,000-frame post-entry runs of standalone Cube Up were compared with the same runs under each
authentic BIOS: no input, long Start, menu actions, and the controller cycle defined in the runtime
analysis. The prototype reached the same per-slot call counts in every campaign, with no detected
unsupported trap or NMI. Frame numbers, execution timing, PCs, RAM and audio hashes differ. In the
three input-driven runs, the frame-1,000 video and final VRAM hashes matched both authentic BIOSes;
the unattended title-screen hashes differed. Hash equality at one endpoint does not imply matching
intermediate frames, audio, timing, complete game state, or successful interactive play. None of
these campaigns started a game, so they exercised only the title and setup flow.

For the first observed `$F01E`, `$F012`, and `$F02A` calls, controlled IRQ-masked probes used
the same VRAM fill and compared output registers/status, final VRAM hash and address, and LCD/VRAM
write counts against both BIOS revisions. Those properties matched, while their respective
prototype/authentic durations were 314,334/324,556, 3,218/2,300, and 1,532/1,540 master clocks.
The probe covered one call context per service, not every possible call.

The gameplay services were then compared against the existing authentic-call checkers and
contract models. The table below distinguishes checked service and callback timing from
non-timing behavior.

### Runtime callback and timing tracker

This table tracks emulator-observation timing status. "Exact" means a synthetic or controlled
probe matched the documented/authentic value in the listed scope. "Partial" means ordinary
geometry or register behavior is established but timing still varies, spans callbacks, or lacks
broad coverage. "Unknown" means exact elapsed clocks or all interrupt/callback contexts are not
measured or modeled. These labels are not claims of physical Gamate timing.

| Area | Timing status | Remaining issue |
| --- | --- | --- |
| Reset and `$6020` entry | Unknown | SWK startup code, entry phase, frame/LCD/AY/audio phase, and boot duration are independently authored and not matched to UMC/Bit. UMC and Bit differ by 58 master clocks at `$6020`; SWK phase remains unmeasured broadly. |
| Periodic IRQ wrapper and `$6026` callback | Partial | Counter carry and callback gating are synthetic-tested, and callback register effects are bounded, but arbitrary callback work, IRQ timing inside services, stack bus timing, and full frame-level phase are not established. |
| Link NMI and `$6023` callback | Exact synthetic timing; live timing unknown | Disabled, direct, and absolute-JMP synthetic forms measured 66, 110, and 116 master clocks respectively at instruction-granular emulator boundaries. The 64 P/256 S matrix and partial receive-state save/load pass. Live UART/Netpacket edges, callback reentrancy, nested IRQ/NMI and physical timing remain open. |
| `$F000` fixed bitmap placeholder | Unknown | The blank placeholder has no authentic timing claim. It is intentionally not modeled as the authentic asset. |
| `$F003` eight-IRQ wait | Exact ready path; partial wait path | 42-master-clock ready path and `8-n` IRQ wait values are tested across the byte domain. Interrupt/callback handler elapsed clocks are separate and remain untraced. |
| `$F006` counter wait | Exact ready path; partial wait path | 42-master-clock ready path and `5-n` periodic-IRQ wait values are exact in synthetic probes. Callback work between IRQs is not modeled. |
| `$F009` long wait | Exact ready path; partial wait path | 42-master-clock ready path and `24-n` periodic-IRQ wait values are exact. Callback-modified X and callback elapsed time remain context-dependent. |
| `$F00C` two-plane clear | Partial | One controlled context and ordinary VRAM/LCD effects are compared; exact broad timing formulas and callback-interference timing remain open. |
| `$F00F` fixed plane-0 clear | Exact for the controlled contract | Synthetic 775-byte VRAM, 829-LCD-write and 15,930-master-clock results agree; broader interrupt and side-effectful contexts are not claimed. NOP padding is an implementation convenience, not a timing substitute. |
| `$F012` glyph run | Partial | A sampled prototype/authentic context was 3,218 versus 2,300 master clocks. Selector, address and write behavior are resolved, but exact implementation timing remains unresolved. |
| `$F015` two-plane bitmap copy | Partial | Geometry, LCD writes, page crossing and tail-JMP behavior are implemented and tested, but exact page-cross formulas and authentic elapsed clocks are not matched. |
| `$F018` pair stream | Partial | Corpus and synthetic non-timing properties match. Timing differs; complete caller, alias and interrupt-spanning timing remain open. |
| `$F01B` row-run stream | Partial | Synthetic ordinary and escape/wrap cases matched UMC timing. Later callbacks, RAM aliases, decimal mode and interrupt-handler work are not fully timing-equivalent. |
| `$F01E` tile-literal stream | Partial | A sampled context was 314,334 prototype clocks versus 324,556 authentic clocks; exact timing and broad stream variation remain open. |
| `$F021` row-literal stream | Partial | Port, register, status, RAM and peripheral behavior match the independent model. Initial implementation timing differs, and exact formula/page-cross behavior has not yet been fitted. |
| `$F024` record initialization | Unknown | Ordinary RAM-write behavior is implemented, but exact timing across all strided-record contexts is not established by the prototype tests. |
| `$F027` sprite/background compositor | Poor timing match | Cube Up corpus calls averaged about 88,900 master clocks versus 54,800 under UMC; a 33,000–68,000-clock disadvantage appeared in the corpus. Pixel and access behavior is resolved. |
| `$F02A` plane-0 rectangle fill | Partial | A single controlled prototype/authentic probe measured 1,532 versus 1,540 master clocks. The 8-clock difference and broader dimension/IRQ combinations remain open. |
| `$F02D` plane-1 rectangle fill | Partial | Geometry and ordinary output are implemented, but exact timing across dimensions and callback boundaries is not fully matched. |
| `$F030` pseudorandom step | Unknown exact timing | The timing-removed checker matched 138,688 calls. Exact implementation timing and interrupt-interleaving remain unvalidated. |
| `$F033` digit formatter | Unknown exact timing | The timing-removed checker matched 526,848 calls. Exact implementation timing and stack-slot behavior remain unvalidated. |

The prototype has intentionally not padded most services except `$F00F`'s controlled ready path.
NOP padding is not itself proof of exact timing and can shift IRQ or NMI boundaries if applied
without a measured formula and call-context review.

| Service | Corpus or input set | Result (non-timing properties) |
| --- | --- | --- |
| `$F027` | 141 Cube Up gameplay calls | All matched: pixels, VRAM access sequence, registers, RAM, peripherals, LCD sequence and cartridge reads |
| `$F018` | Gameplay corpus; 1,373 selftest, 1,280 open-bus and 131,072 flag cases | All matched |
| `$F01B` | Gameplay corpus; 772 synthetic cases covering offsets, flags, escapes and wraps | All matched |
| `$F033` | 526,848 input combinations | All matched |
| `$F030` | 138,688 calls | All matched |

The `$F030` and `$F033` comparisons used temporary copies of the checkers with timing and
stack-slot checks removed.

Timing differs from the authentic BIOSes:

- `$F027` averages about 88,900 master clocks per gameplay call, against 54,800 under UMC (about
  1.6×). In the corpus it is 33,000–68,000 clocks slower per call.
- `$F018` timing also differs.
- `$F01B` matched UMC timing in the synthetic cases.

A 1,000-frame run used this schedule: Start at frame 90 for 20 frames, then A at frames 300, 450,
600 and 750 for 30 frames each. Gameplay began near frame 467. UMC, Bit and SWK made the same
per-slot calls, with 389 completed calls each and no trap. Their frame-1,000 video hashes were
identical; their audio hashes differ, as they also do between UMC and Bit.

A 1,600-frame per-frame video comparison with movement inputs produced the same sequence of 31
settled gameplay screens under all three BIOSes. SWK showed more transient, partially redrawn
frames: 339, against 225 under UMC. These come from its slower `$F027` and are visible as
additional mid-redraw flicker.

An 8,000-frame varied-input run under SWK called only implemented slots. It completed 2,343 calls,
against 2,345 under UMC. The call sequences diverged at frame 1,506, when UMC first called the
game's `$6685` routine and SWK made the same call 29 frames later. Every completed call entered
within one frame of its UMC counterpart until that point. Replaying the UMC run with the scripted
inputs after frame 800 delayed by one frame reproduced SWK's `$6685` call at frame 1,535.
Advancing them by one frame moved the call to frame 1,502. The game is therefore sensitive to the
frame on which input is sampled. The divergence is attributed to SWK's different service timing,
not to a service data mismatch. Scripted runs longer than a few hundred gameplay frames cannot
expect identical game state.

These are emulator observations of one standalone cartridge, not physical measurements. No complete
playthrough, other cartridges, 4-in-1 selection, save-state continuity, link/network behavior,
arbitrary callbacks, or exhaustive runtime ABI coverage is established.
