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

The manual release workflow also builds this image with cc65 and validates the synthetic prototype
suite before placing `gamate_bios_swk.bin` at `bios/gamate_bios_swk.bin` in the release archive.

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

### Three-BIOS differential sessions

`tools/gamate-three-bios-diff.py` launches three isolated RetroArch instances: UMC, Bit, and SWK.
Each instance receives the same core, cartridge, window geometry, and keyboard input, but an isolated
system directory containing only its selected BIOS. The session and screenshots, save states, replay
files, logs, and manifest remain under `/tmp`.

```sh
tools/gamate-three-bios-diff.py \
  path/to/Cube-Up.bin \
  --core build/superwhizkid_libretro.so \
  --bios-dir path/to/local/gamate \
  --swk-bios build-bios/gamate_bios_swk.bin \
  --record-replay
```

The terminal interface broadcasts arrows, Enter/Escape, and Z/X/A/S/Q/W to all three instances.
Uppercase `P` pauses, `U` resumes, `V` frame-advances, `R` resets, `T` screenshots, `S` captures a
paused save-state/screenshot snapshot, and `Q` quits. Snapshot report hashes and pairwise divergence
flags are written to `snapshots.tsv`; state and screenshot files remain in the per-BIOS session
directories. Interactive window broadcast requires `xdotool`.

An existing Replay-Event `.bsv` file can be replayed in all three instances without key broadcasting:

```sh
tools/gamate-three-bios-diff.py \
  path/to/Cube-Up.bin \
  --core build/superwhizkid_libretro.so \
  --bios-dir path/to/local/gamate \
  --swk-bios build-bios/gamate_bios_swk.bin \
  --replay path/to/inputs.bsv
```

Both modes retain the ROM and BIOS inputs read-only. No commercial payload, BIOS bytes, or ROM bytes
are copied into the repository or included in the report. Replay and snapshot files are diagnostic
artifacts under the local temporary session only.

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
  establishes a known silent AY state, initializes LCD state and enters at `$6020`. Controlled
  accepted-cartridge boots under both authentic BIOS revisions produced no AY-register writes and
  reached entry with every AY register zero. SWK nevertheless writes zero to all 16 registers as a
  compatibility policy rather than relying on an assumed physical or emulated power-on state. This
  is deliberately defensive initialization, not a claim that the authentic firmware performs the
  writes. The reset path includes measured timing padding; it does not reproduce the authentic
  startup graphics or exact hardware, LCD, AY, or audio-sample phase.
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
  Link reception and BRK-specific behavior have initial implementations with the synthetic
  validation boundaries recorded below. Live link traffic, nested or reentrant callbacks, and
  physical timing remain unvalidated. Standalone Cube Up has received a successful interactive SWK
  run, but this is not a complete playthrough or a hardware-validation result.

### Remaining implementation deficiencies

This table tracks unresolved replacement-firmware work separately from the timing tracker. Status
is "Open", "In progress", "Partially complete", or "Cleared".

| # | Deficiency | Status | Current boundary |
| --- | --- | --- | --- |
| 1 | UART receive NMI and `$6023` cartridge callback dispatch | Cleared | Emulator-level implementation reads `$4800` into RAM `$00A`, dispatches direct or absolute-JMP `$6023` stubs only when `$000C` is enabled, restores A, retains callback-modified X/Y, resumes partial receive-byte state, and covers all 64 P combinations and 256 S values in synthetic tests. Live Netpacket timing, callback reentrancy, nested IRQ/NMI and physical timing remain outside the emulator-level completion claim. |
| 2 | BRK-specific cartridge and interrupt behavior | Cleared | An independently authored private-cartridge BRK callback matrix passed 65,536 cases under UMC, Bit, and SWK: 64 P combinations, 256 S values, callback enabled/disabled, and direct/absolute-JMP `$6026` forms. It confirms the IRQ vector, padding-byte PC, B/I/U stack status, RTI register restoration, callback dispatch, and no hardware-IRQ diagnostic count. Physical BRK/timer side effects and arbitrary callback reentrancy remain outside this completion boundary. |
| 3 | Interrupt interference and callback aliasing | Open | Arbitrary callback modifications to RAM, LCD, streams, scratch, stack, nested IRQ/NMI, and callback register state are not fully modeled. |
| 4 | Decimal-mode and aliased-source edges | Open | D-set behavior and mutable I/O/scratch/stack aliases are outside most validated ordinary contracts. |
| 5 | Save/reset/NMI and in-flight UART lifecycle | Cleared | Exact-size save/load and failed-load immutability are tested; reset clears transmit/receive UART state and pending NMI. SWK synthetic coverage saves and restores partial receive bytes and execution paused inside a direct `$6023` callback, then completes the callback without replay or lost receive state. Live Netpacket queue/timing, arbitrary nested callback delivery, and physical UART edge timing remain outside this emulator-level boundary. |
| 6 | Authentic startup presentation and boot phase | Open | Reset performs protection, identity and checksum checks and enters `$6020`, but does not reproduce the authentic startup graphics or hardware/audio/frame phase. |
| 7 | Broad cartridge and interactive hardware validation | In progress | A 62-cartridge SWK sweep now reaches and runs every title for 100 post-entry frames without an unsupported-service trap. Fifty-nine produce nonblank output. Famous 7, One Million Whys, and QuizFighter execute normally but retain SWK's blank startup VRAM; their authentic runs retain a shared nonblank BIOS presentation. Longer input-driven runs, 4-in-1 selection, network link, save-state campaigns, and complete playthroughs remain incomplete. |

The chosen low-RAM initialization is a compatibility policy, not measured physical power-on state.
The AY clear follows the same policy: it guarantees the observed silent, all-zero cartridge-entry
register state, but exact oscillator, noise, envelope, and audio-sample phase is intentionally part
of the unresolved boot-phase work. No observed BIOS service generates sound; AY changes during
interrupt-spanning service calls can come from the cartridge's `$6026` callback rather than the
service itself.
The common entry contract and its uncertainty are described in
[`BIOS_REPLACEMENT_ANALYSIS.md`](BIOS_REPLACEMENT_ANALYSIS.md); service contracts and their
limitations are in [`BIOS_RUNTIME_API_ANALYSIS.md`](BIOS_RUNTIME_API_ANALYSIS.md).

## Validation and limits

The synthetic test suite checks successful and rejected boots, reset, dirty initial VRAM, IRQ
counter carry, callback gating and register preservation, unsupported traps, IRQ waits, and the
eighteen service implementations. It exercises all 256 glyph selectors with both incoming overflow
states, a 256-tile run, wrapped stacks and streams, both LCD-clear modes, and rectangle boundaries for both planes. The newer cases cover:

- Entry from deliberately nonzero AY registers, verifying that SWK writes every AY register once
  and reaches the documented all-zero silent state rather than relying on emulator reset contents.
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

A separate 100-post-entry-frame SWK sweep covered all 62 catalogued cartridges. Every case reached
`$6020`, ran without an unsupported-service trap, and retained balanced BIOS calls. Fifty-nine cases
produced nonblank output. Famous 7, One Million Whys, and QuizFighter remained blank while continuing
to execute cartridge code, periodic callbacks, `$F024`, and `$F003`. Equivalent UMC BIOS runs were
nonblank from the first post-entry frame and converged on the same video and VRAM hashes across all
three titles. Their observed cartridge paths did not call a BIOS drawing service during the window;
they retain the BIOS startup presentation instead. SWK therefore needs independently authored
startup artwork for visual compatibility with this software family. This result does not authorize
copying authentic startup pixels and does not establish behavior beyond the 100-frame no-input
window.

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
| Reset and `$6020` entry | Partial frame match | SWK uses repeated measured 24-IRQ and a 20-IRQ wait at reset. For Brick Blaster it enters `$6020` on frame floor 414, matching UMC/Bit; exact master clocks differ (`30,228,402` versus `30,239,372/30,239,430`) and frame/LCD/AY/audio phase remains unmeasured broadly. |
| Periodic IRQ wrapper and `$6026` callback | Partial | Counter carry and callback gating are synthetic-tested, and callback register effects are bounded, but arbitrary callback work, IRQ timing inside services, stack bus timing, and full frame-level phase are not established. |
| Link NMI and `$6023` callback | Exact synthetic timing; live timing unknown | Disabled, direct, and absolute-JMP synthetic forms measured 66, 110, and 116 master clocks respectively at instruction-granular emulator boundaries. The 64 P/256 S matrix and partial receive-state save/load pass. Live UART/Netpacket edges, callback reentrancy, nested IRQ/NMI and physical timing remain open. |
| `$F000` fixed bitmap placeholder | Partially complete | The independently authored blank placeholder matches the measured geometry, write counts, final LCD address, return state, and 3,278-master-clock uninterrupted duration. It does not reproduce the authentic asset or claim matching intermediate event timing and interrupt interference. |
| `$F003` eight-IRQ wait | Exact service contract | A shared 256-value matrix verifies the 42-clock ready path, `8-n` IRQ count, return state, and lack of direct LCD/VRAM effects. IRQ phase and callback elapsed time are external to the service; callback-modified X is intentionally observable. |
| `$F006` counter wait | Exact service contract | The same matrix verifies the 42-clock ready path, `5-n` IRQ count, return state, and lack of direct LCD/VRAM effects. IRQ phase and callback elapsed time remain caller/hardware context. |
| `$F009` long wait | Exact service contract | The shared matrix verifies the 42-clock ready path, `24-n` IRQ count, return state, and lack of direct LCD/VRAM effects. A one-IRQ callback test confirms the same callback-modified-X behavior as `$F003/$F006`. |
| `$F00C` two-plane clear | Exact uninterrupted timing | The ordered 12,800 VRAM writes, 13,602 LCD writes, registers, scratch, LCD state, and 247,326-master-clock duration match the controlled contract for both ordinary and row-increment control modes. Mid-call mutation, interrupt interference, and exact event timestamps remain open. |
| `$F00F` fixed plane-0 clear | Exact for the controlled contract | Synthetic 775-byte VRAM, 829-LCD-write and 15,930-master-clock results agree; broader interrupt and side-effectful contexts are not claimed. NOP padding is an implementation convenience, not a timing substitute. |
| `$F012` glyph run | Partial, improved | The fixed-row unroll, direct selector transfer, reduced stack bookkeeping, and combined C/V decision tree preserve the measured output, flags, and stack footprint. Across all 256 one-tile selectors and both tested entry-V states, SWK is now 68-130 clocks over the authentic selector/page formula; the original worst residual was 274. Exact class-dependent timing remains open. |
| `$F015` two-plane bitmap copy | Partial, improved | Geometry, LCD writes and tail-JMP behavior are implemented. Keeping status on the stack lets `$01A` count columns while preserving `$01C`, eliminating both width stack round trips. Reordering the saved status/X values avoids a temporary status copy, and the entry X value is consumed directly when deriving the destination column. An out-of-line 256-row pointer path removes the ordinary per-column taken branch. Tested nonzero-height shapes now take `322 + c*(184 + 76*r)` versus the authentic `234 + c*(132 + 76*r)`; zero height uses `r=256` and a 176-clock column term. The per-byte cost is exact; the ordinary remaining difference is 88 fixed clocks plus 52 per column. A source-pointer carry still adds eight clocks beyond the authentic indexed-read page penalty. |
| `$F018` pair stream | Partial | Corpus and synthetic non-timing properties match. Direct replacement measurements were 3,848 versus 4,172 authentic-model clocks for a mixed three-pair stream and 958 versus 876 for a transparent-only stream. Exact replacement timing across the broader caller matrix remains open. |
| `$F01B` row-run stream | Partial | Synthetic ordinary and escape/wrap cases matched UMC timing. Later callbacks, RAM aliases, decimal mode and interrupt-handler work are not fully timing-equivalent. |
| `$F01E` tile-literal stream | Partial | A sampled context was 314,334 prototype clocks versus 324,556 authentic clocks; exact timing and broad stream variation remain open. |
| `$F021` row-literal stream | Exact for the executable contract | All 406 authentic corpus calls and 450 synthetic cases matched the independent model, including exact uninterrupted timing and handler-time-only additions in 280 interrupt-spanning replays. Mutable aliases, nonterminating streams and handlers that alter service inputs remain outside the contract. |
| `$F024` record initialization | Exact for the controlled contract | Synthetic D-clear record initialization matches 824 master clocks, 32 `$FF` writes, register/status and no peripheral effects. D-set, later callback and interrupt-spanning contexts remain open. |
| `$F027` sprite/background compositor | Object formulas nearly exact; redraw timing open | Objects now use a separate selector-timed lookup: callable selector groups 0-3 are exact and groups 4-7 are two master clocks fast. Large-object shift residuals are 0/+4 and small-object residuals 0/-2; the controlled combined case is +2. In the 141-call Cube Up replay, both object-only shapes (three large and eight small) are now exact. The common four-small-redraw/four-small-object shape remains +2,224 clocks, isolating the next timing work to 556 clocks per small redraw in that context. Pixel output, VRAM/LCD/ROM access sequences, registers and peripherals match; stack write footprint differs. |
| `$F02A` plane-0 rectangle fill | Exact uninterrupted timing | Ordinary and zero-derived synthetic shapes now follow `126 + 58*c + 18*c*r`; tested 1×1, 2×3, 256×1, and 1×256 cases take 202, 350, 19,582, and 4,792 master clocks. Interrupt-spanning timing remains open. |
| `$F02D` plane-1 rectangle fill | Partial | Ordinary synthetic timing is `120 + 58*c + 18*c*r`, and direct 256×1/1×256 checks measure 19,574/4,784 clocks. The 1×1 authentic residual remains 196 versus 194 in the replacement, and broad IRQ combinations remain open. |
| `$F030` pseudorandom step | Exact uninterrupted timing | The state transition and register/status contract match the exhaustive checker; SWK now takes 174 master clocks for every tested nonzero input and 188 for zero, matching both authentic revisions. Interrupt interleaving, reentrancy, and callback-alias timing remain open. |
| `$F033` digit formatter | Exact for the documented formula | The full 526,848-call matrix matched the authentic formula. Y=`$FF` adds 2 NOP (8 master clocks); ordinary Y adds 3 NOP (12 master clocks). Stack-slot and interrupt-spanning timing remain outside the ordinary matrix. |

The prototype has intentionally not padded most services except `$F00F`'s controlled ready path.
NOP padding is not itself proof of exact timing and can shift IRQ or NMI boundaries if applied
without a measured formula and call-context review.

### Withdrawn timing experiments

The following experiments were measured, but were reverted because they did not close their
targeted timing equations or could not be defended across the tested shape classes:

- **`$F02A` and `$F02D` redundant-A-load removal:** a controlled probe removed a final
  `LDA $1A` from the shared fill routine. Clean baseline measurements in 2×3, 256×1, 1×256,
  and 1×1 shapes were four master clocks high for both planes (for example `$F02A` $354/$19586/
  $4796/$206 versus formulas $350/$19582/$4792/$202). Removing the load moved the sampled values
  in the wrong direction: the shortest 1×1 path became 200/194 instead of 202/196 in the
  diagnostic matrix. One NOP added 4 master clocks back and still left the 1×1 shapes two clocks
  high. The change was reverted; the shortest neutral 6502 instruction is also 2 CPU cycles/4
  master clocks in this emulator, so no one-instruction neutral correction exists for the observed
  two-clock residual. Neither fill service is upgraded from partial timing status.
- **`$F024` initial NOP count:** an earlier 22-NOP experiment for the strided-record initializer
  overshot the target. Corrected 11-NOP padding then measured exactly 824 master clocks and was
  retained; it is not listed here as withdrawn.
- **`$F030` branch NOP padding:** state-dependent padding was probed in a first synthetic mismatch
  at input `$0000`; the measured SWK clock was 138 and the documented emulator-model clock was 188.
  Multiple zero/bit4 branches were tested, including 10- and 11-NOP blocks combined with taken
  branches. The first attempt measured 238/190 instead of 188, and further attempts could not
  isolate a 50/58-master-clock block without changing state or branch control. That experiment was
  reverted. A later control-flow-specific correction, described in the tracker above, matched the
  174/188-master-clock authentic formula across the exhaustive checker and is retained.
- **`$F033` initial separate branch padding:** an earlier ordinary/Y=`$FF` experiment used branch
  blocks that produced 428/372-clock mismatches and was reverted. A later narrower measurement
  proved that two NOP instructions (8 master clocks) are required for Y=`$FF` and three NOP
  instructions (12 master clocks) for ordinary Y formatting. That measured correction is retained
  and the full 526,848-call matrix now matches.
- **`$F027` global and shape-local padding:** shape/delta instrumentation was tested only to
  classify replay calls. In the 141-call scheduled Cube Up corpus, 139 calls had 4 background
  records and 4 sprite records, with predicted 53,630 and actual 84,218 master clocks; one call
  had 0/3 records at 76,304 predicted/138,848 actual; and one had 0/8 at 66,774 predicted/120,458
  actual. These gaps span 30,588–62,544 master clocks and are not a single safe NOP constant.
  Temporary diagnostics were removed and no padding or functional rewrite was retained.
- **`$F027` isolated `point_tile` correction:** the exhaustive callable-object selector matrix
  (`$00-$FE`; `$FF` disables the record) shows residuals
  by selector group `$00-$1F` through `$E0-$FF` of `0,-48,-92,-140,-180,-228,-272,-320`
  master clocks. Adding the corresponding bitwise timing directly to `point_tile` made its
  `T(selector)` term exact, but exposed 2,640 clocks of large-redraw and 1,108 clocks of small-redraw
  overhead previously hidden by the under-timed lookup. It also moved later code across instruction
  page boundaries. The change was reverted: tile selection and redraw traversal must be corrected
  together rather than trading one compensating error for another.

  A later retained implementation split object lookup into `point_object_tile` while leaving redraw
  lookup unchanged. This makes the object selector spectrum exact within two clocks without exposing
  the compensating redraw overhead. The rejected experiment remains relevant specifically to a
  shared lookup correction; redraw traversal still requires a unit-level rewrite or retiming.

No speculative fill, random, formatter, or compositor timing change remains in production code.
Retained timing corrections are limited to measured contracts with synthetic assertions; the
current status of each service, including exact `$F030` and `$F033` matrices, is recorded in the
tracker above.

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

- The earlier `$F027` implementation averaged about 88,900 master clocks per gameplay call against
  54,800 under UMC. After the retained compositor optimizations, a fresh IRQ-masked replay of the
  same 141-call Cube Up schedule found much smaller but shape-dependent residuals: 139 common calls
  were 3,936 clocks high, a three-large-object call was 3,424 high, and an eight-small-object call
  was 894 low. This supersedes the earlier 33,000–68,000-clock range; it does not yet establish
  exact timing for other shifts, cartridges, interrupt phases, or source aliases. After both object
  shift corrections, the same current mixed shapes measure +2,224, 0, and +336 clocks. The exact
  three-large-object result and near-exact isolated matrices leave selector/source alignment, mixed
  redraw paths, and stack footprint as the next bounded discrepancies.
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
