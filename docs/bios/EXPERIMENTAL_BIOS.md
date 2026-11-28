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

For unattended frame-by-frame comparison, `gamatebioscompare` runs UMC, Bit, and SWK together and
reports transitions in video/VRAM equality plus summary counts for RAM, CPU, hardware registers, and
activity. It excludes audio samples, AY oscillator/envelope phase, sample phase, and absolute master
clock from equality. The built-in `controller_cycle` is the standard schedule used by the BIOS-call
campaign tool.

```sh
build/gamatebioscompare path/to/cartridge.bin \
  path/to/gamate_bios_umc.bin path/to/gamate_bios_bit.bin \
  build/gamate_bios_swk.bin no_input 1000

build/gamatebioscompare path/to/cartridge.bin \
  path/to/gamate_bios_umc.bin path/to/gamate_bios_bit.bin \
  build/gamate_bios_swk.bin controller_cycle 1000
```

Hashes are derived diagnostics, not retained content. Strict RAM/CPU equality is intentionally
reported but is not by itself a compatibility verdict: independently authored firmware uses
different resident code and scratch, and even the authentic revisions can occupy different
instruction boundaries while presenting the same settled frame.

`gamatebioscalls` reports completed-service clocks relative to `$6020`, entry wait/timer state, and
the final two boot IRQ boundaries. Setting `GAMATEBIOSCALLS_BOOT_IRQ_REPORT` to a temporary output
path additionally writes the full boot IRQ index, master clock, and interrupted PC sequence. This
report contains derived execution metadata only and should remain under temporary storage.

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

### Brick Blaster three-BIOS frame matrix

A 1,000-post-entry-frame matrix compared standalone Brick Blaster under UMC, Bit, and SWK with no
input and with the standard controller-cycle schedule. Audio samples and audio/AY phase were
excluded. All six runs completed without a trap.

Before the boot-IRQ correction, SWK differed at frame 1 because its blank startup presentation does
not retain the authentic image. Video reconverged at frame 2 and VRAM at frame 5. A later one-frame
video boundary began at frame 109, VRAM diverged at 111, and continuing video divergence began at
113. Absolute-call tracing showed that boot/IRQ phase moved the progressive `$F00C` clear across
frame boundaries; its data contract was not responsible.

The corrected reset tail masks the final clear, finishes initialization with the timer request
pending, and releases that IRQ at a calibrated boundary. It restores the cartridge-visible counters
after the handler and preserves all runtime-service addresses. For Brick Blaster, SWK now matches
UMC entry exactly at master clock `30,239,372`; both accept their final boot IRQ at `30,239,154` and
enter 218 clocks later. Bit enters 58 clocks later with the same 218-clock IRQ phase. SWK accepts 913
boot IRQs versus 917 authentic because four requests coalesce while masked; those historical counts
are diagnostic-only, and the exposed counters are restored to the measured entry values.

After correction, both campaigns retain the known frame-1 startup difference, reconverge at frames
2/5, and remain matched through frame 112. Their first later video and VRAM difference is frame 113,
where the known `$F018` timing deficit begins. No-input video/VRAM matches rise from 110/106 to
111/108 frames. Controller-cycle matches rise from 442/443 to 445/450; it still reconverges at frame
401 and shows later progressive-update boundaries. UMC and Bit can also differ on such boundaries,
so a transient hash mismatch is not automatically a functional failure.

An absolute-call-timing follow-up found the same pre-frame-109 service order under all three BIOSes.
Measured from `$6020`, the first `$F00C` starts at exactly 16,814 clocks in each run. The initial
boundary is therefore inherited boot/IRQ phase, not an early cartridge call. Current-source SWK
enters 11,118 master clocks before UMC with its IRQ timer 2,526 clocks past the last accepted
interrupt; UMC enters only 218 clocks after its last accepted interrupt. A rejected fixed
11,118-clock endpoint delay matched UMC's absolute entry clock but left the IRQ timer at 13,644
clocks and did not remove the frame-109 boundary. The improved diagnostic placed SWK's last
boot IRQ at `$F187`, the `INY` in the second `clear_plane` loop, 2,526 clocks before entry. Both
authentic revisions accept their last boot IRQ at `$F2FA`, only 218 clocks before entry. These PCs
identify boundaries in independently executed firmware; no authentic instructions were retained or
reconstructed. This evidence led to the pending-IRQ release design described above.

By the second `$F00C`, SWK has accumulated an additional 2,752-clock lead. The first live `$F009`
accounts for 2,432 clocks of the baseline difference, while later waits differ by 32-50 clocks each.
Before the `$F018` timing pass, the first Brick Blaster call took 119,224 SWK clocks versus 152,264
authentic. The retained emitted-column and opaque-pointer corrections raise SWK to 151,676 clocks,
leaving a 588-clock live residual across five interrupts. The first later video mismatch moves from
frame 113 to 116; VRAM still differs at 113 because SWK enters the call 742 clocks early from the
then-current live `$F009` phase. `$F027` is not implicated. The next bounded targets are the remaining
`$F018` fixed/row and zero-count cases plus broader callback/first-wait phase. Longer play,
alternative schedules, physical hardware, and user-perceived playability remain untested here.

A subsequent live `$F009` pass found that its isolated threshold/ready-path contract was already
correct, while the enabled IRQ callback wrapper placed steady Brick Blaster waits 32-48 clocks
short. One NOP after the `$6026` callback moves those waits to within 0-16 clocks; a second NOP
returns them to the former phase and was rejected. The accumulated lead at the second `$F00C` entry
falls from 448 to 160 clocks. Because `$F018` spans five IRQs, its live duration changes slightly to
151,652 clocks and its entry lead falls from 742 to 426 clocks. Frame-level no-input boundaries and
match counts are unchanged; controller-cycle VRAM matches improve from 480 to 482. This is an
emulator-observed compatibility alignment, not a physical IRQ-wrapper timing measurement.

Measuring uninterrupted and live clears then resolves the wrapper cost directly. Each enabled
callback IRQ adds 308 master clocks to either authentic `$F00C` call, independent of whether the
call spans seven or eight IRQs; SWK previously added 270. Extending the enabled wrapper by the
missing 38 clocks makes both Brick Blaster clears exact and also aligns all ten observed `$F009`
returns. SWK and UMC consequently enter the first `$F018` on the same master clock and IRQ phase.
With IRQs masked, that call remains 422 clocks short (`150,302` versus `150,724`), so the first
remaining difference is now the known `$F018` fixed/row and edge-term timing rather than boot,
`$F009`, `$F00C`, or interrupt overhead. These are emulator measurements of authentic firmware,
not physical hardware timing.

The next `$F018` pass decomposed that 422-clock difference using derived stream statistics rather
than cartridge payload: Brick Blaster's call has 18 rows, 83 pairs (21 transparent), 125 opaque
columns, and tile-index groups concentrated in groups 0, 1, 6, and 7. The old residual is exactly
`+78 fixed - 14*rows + 4*transparent - 4*group6 - 8*group7 = -422`. A plane-aware row adjustment
now supplies the missing 14 master clocks per logical row, matching the authentic 260-clock row
term. Keeping earlier service addresses fixed places the enlarged `$F018` body at a different
internal page alignment, adding another 44 clocks in this long call. The live call rises from
151,842 to 152,138 clocks versus 152,264 authentic, leaving 126 clocks. Output, earlier service
timing, and entry phase remain unchanged; the remaining fixed, transparent-pair, high-group, and
code-page terms must be corrected together rather than treated as an unexplained delay.

The following high-group pass corrected tile-index bit 7 without adding dispatch overhead. Carry is
known set on that path, so a taken branch replaces one NOP and supplies the missing odd CPU cycle in
each plane. Groups 4-7 consequently gain four master clocks per logical pair while groups 0-3 are
unchanged. Brick Blaster's masked `$F018` becomes 150,802 clocks versus 150,724 authentic, and the
live call becomes 152,342 versus 152,264: a 78-clock residual in both cases. For this stream the
`+4 * 83` pair surplus now cancels the remaining group-6/7 deficits exactly, isolating the residual
to the fixed-call term. Groups 5/6 remain four clocks short and group 7 eight clocks short in minimal
cases; pair and fixed terms remain deliberately open rather than being hidden by a game-specific pad.

A fixed-cost tail-call rewrite reduced the residual to 64 clocks but moved the following `$F01B`
body and was rejected. Inlining `$F018`'s row-delimiter handler instead recovers one byte without
moving its calibrated column loop; a carry-guaranteed three-cycle branch preserves the exact row
term. Minimal opaque groups 0-6 now have no pair or group residual, group 7 is four clocks short,
and transparent pairs remain four clocks long. Brick Blaster remains 78 clocks long because its
`+84` transparent-pair term, `-128` group-7 term, and `+44` long-path page contribution sum to
`-44`, leaving the `+78` fixed term at a net `+34` before that page contribution. The rewrite is
retained for its broader coefficient improvement, not as a title-specific timing adjustment.

Moving the two-instruction pass epilogue beside the pass entry then permits a direct terminator
branch, removing two CPU cycles per plane without moving following services. Rebalancing the
first-plane row delay restores the exact row coefficient after the one-byte hot-loop shift. The
result removes the former long-path page term and reduces Brick Blaster's masked/live residual from
78 to 26 clocks: 150,750 versus 150,724 masked and 152,290 versus 152,264 live. Its remaining formula
is `+70 fixed + 4*21 transparent - 4*32 group7 = +26`. Rows, opaque pairs, groups 0-6 and emitted
columns are exact in the measured cases. This is still a general stream-shape result, not a
Brick-specific delay.

The repeated 1,000-frame matrix retains 110/114 no-input VRAM/video matches. Controller-cycle
matching becomes 720 VRAM and 719 video frames; one fewer later reconverged frame is accepted in
exchange for the smaller causal service-timing error. Audio phase remains excluded.

The next edge-term pass removes the remaining transparent and group-7 errors. Transparent pairs
skip a redundant `$1A` store but retain five setup cycles, saving exactly one CPU cycle per plane;
unreachable padding keeps the opaque loop fixed. After pointer selection, X is dead, so the group-7
path classifies the saved tile and takes one extra cycle without disturbing A or Y. Brick Blaster's
masked/live `$F018` becomes 150,794/152,334 clocks versus 150,724/152,264 authentic: an isolated
70-clock fixed residual. Minimal groups 0-2 and 5-7 are exact relative to that fixed term; group 3
is four clocks short and group 4 four clocks long. These non-Brick edges remain open. The repeated
matrix retains 110/114 no-input VRAM/video matches and records 720/720 controller-cycle matches.

A fixed-term phase check then varied row count independently: empty streams and streams with one
through four rows all retain the same +70-clock result. Transparent and opaque one-pair cases in
tile groups 0, 1, 6, and 7 also retain +70, confirming that this residual is independent of tested
row and pair/group shape. A direct two-pass dispatch experiment shortened the masked residual to
+44 clocks but shifted Brick Blaster's interrupt boundary and increased the live open-call residual
to +336 clocks; it was reverted. The remaining work is therefore a 35-CPU-cycle fixed correction
with unchanged observable timing phase, not a simple aggregate-duration pad or dispatch shortcut.

An upstream-boundary audit checked every completed service before the first Brick Blaster `$F018`.
It found two genuine SWK-only differences earlier in the attract sequence:

- live `$F00F` takes the same 15,930 clocks and writes the same 775 VRAM bytes, but UMC/Bit issue
  838 LCD-port writes while SWK issues 829 in a different traversal order;
- `$F012` takes 3,668 clocks in UMC/Bit and 4,046 in SWK, a 378-clock delay.

The following first `$F009` is 368 clocks shorter in SWK because it reaches the IRQ gate later.
Subsequent waits reconverge: the second `$F00C` and first `$F018` both enter at the identical
master clock, frame clock, LCD clock/row, IRQ clock and pending state. CPU registers, VRAM,
framebuffer, LCD accumulation, AY registers, scroll, bank, and every documented `$F018` input also
match at that boundary. Nonstack scratch, stack history and AY envelope phase are not identical,
so the complete machine state remains contaminated for audio and later scratch-sensitive behavior.
Those differences do not change the documented `$F018` call in this context: its +70 masked/live
duration remains an isolated service discrepancy after the video/control boundary has reconverged.

LCD-port timestamps add a second constraint: authentic UMC spacing is 32 master clocks between
successive bytes in a repeated tile column, whereas SWK retains the same 330-clock column total by
placing a 92-clock delay after the eighth write. Exact redistribution matches the byte timestamps,
but relocation makes the fixed residual +114 clocks and leaves the 1,000-frame no-input matrix at
114 video matches and 109 VRAM matches, down from 110 VRAM matches. That experiment was also
reverted. Early-call phase, per-byte spacing, terminal fixed cost and interrupt placement must
therefore be corrected together for framelock.

IRQ-progress instrumentation now separates those terms more directly. In the first live Brick
Blaster call, UMC accepts IRQs 1-5 at 12,160, 44,928, 77,702, 110,470, and 143,240 clocks with 258,
641, 1,295, 1,769, and 2,289 LCD writes complete. SWK accepts them 2, 4, 2, 10, and 12 clocks later,
at 257, 641, 1,291, 1,766, and 2,287 writes. A replacement column loop can match UMC's 32-clock
byte spacing and be calibrated to the same 330-clock column total, but this produces a 44-clock
live code-layout residual; it does not remove the empty-stream wrapper's +70 clocks. A status-safe
direct second-pass return likewise improves the masked residual to +62 while worsening the live
call to +354 clocks and reducing 1,000-frame VRAM matches to 109. Both experiments were reverted.
The remaining implementation must therefore redesign the fixed two-pass wrapper while preserving
the five IRQ offsets and their LCD-progress positions, not merely adjust the tile loop or terminal
duration.

A later exact-spacing experiment redistributed the old 92-master-clock column-tail delay as five
CPU cycles between adjacent tile bytes. It achieved authentic 32-clock byte spacing and, with a
separate tail shim, removed the per-column duration slope in one- and two-column cases. The
implementation nevertheless gained four clocks for opaque groups 0-2. Brick Blaster's 11 group-0
logical pairs therefore added 44 clocks to the retained +70 fixed residual. Inverting the bit-5
branch changed helper placement enough to worsen the live call to +438 clocks. Both variants were
reverted. The old batched tail delay and low-group pointer path are coupled: transferring exact
inter-byte phase must occur without moving the group-sensitive helper boundary.

Compensating that wrapper's 26-clock early second pass with an out-of-line 28-clock transition
delay restored several IRQ clock offsets, but did not restore work progress: at IRQ 5 SWK had
2,279 LCD writes versus 2,289 authentic, and the live call still took 152,640 clocks (+376).
Inline compensation moved more code and was worse. The direct wrapper was reverted. Future work
must match both interrupt clock and completed LCD writes, not timer phase alone.

A compact `INC $01B` variant then supplied five cycles in two bytes between adjacent tile writes.
Although final output and `$01B` still matched because X held the live repeat count, it added seven
RAM read-modify-write accesses per column, changed status during each gap, moved the helper by nine
bytes, and worsened Brick Blaster to +2,622 clocks. It was rejected; final-RAM equality is not enough
when timing, scratch accesses, and flags are part of the service contract.

The next primitive trial used `TXA; BNE` between writes because the live repeat count guarantees X
is nonzero, giving five cycles while A is overwritten by the next source-byte load. A 21-cycle
bit-7 NOP delay was replaced by a cycle-equivalent X loop. Both are behaviorally plausible, but the
combined body moved the opaque helper by 13 bytes, its point helper by 13 bytes, and following
services by 16 bytes. The one- and two-column residual rose to +74, and the live Brick Blaster
residual became -4,170 clocks. This variant was reverted. The remaining redesign must keep external
service addresses and the two helper entries fixed while relocating internal cycle distribution.

A direct-wrapper phase trial removed the two pass `JSR` entries and placed the plane transition in
the terminator. It saved 13 CPU cycles and reduced the masked fixed residual to +44 clocks, with all
functional model properties intact. The second pass began 26 master clocks earlier, however,
shifting the five IRQ-visible boundaries and producing a +336-clock live residual. VRAM frame
matches fell from 110 to 109. The trial was reverted: the 26-clock saving must be compensated before
the affected IRQ boundaries while a separate 22 CPU-cycle reduction still closes the remaining
masked error.

An explicit-plane ground-up controller redesign then removed the two pass-subroutine calls and
encoded "opaque seen" alongside the plane selector. It remained behaviorally exact in the prototype
and synthetic boundary suite, and its row, transparent and 330-clock column terms could be
calibrated independently. Opaque output exposed the remaining architectural cost: preserving the
encoded selector around each count loop added 17 CPU cycles per plane, or 34 clocks per logical
pair. Brick Blaster's 62 opaque pairs consequently produced 2,096 extra clocks before group and
layout effects, so this redesign was reverted. The next attempt must redesign selector ownership
inside the opaque loop rather than optimize only the outer two-pass wrapper.

A retained redistribution step then addressed byte phase and wrapper overhead together, while
keeping every external service and both opaque helper entries at fixed addresses. The emitted
column replaces eight unrolled writes plus a batched 46-cycle delay with an eight-iteration
`LDA (zp),Y; STA; INY; DEX; BNE` loop. Its 16 CPU cycles per byte reproduce the authentic
32-master-clock write spacing, and the loop exactly preserves the 330-clock column total. The
freed bytes are unexecuted padding before the next service. The two pass subroutine calls became
`JMP` entries: the prologue falls into the first pass, and the terminator restores the stream
high byte before entering the plane-1 pass. The authentic firmware also rewrites `$00` with its
unchanged value between passes. That write is invisible to software, so SWK omits it as a
documented compatibility policy. The checker still requires `$00` to keep its value. Keeping the
write cost 12 clocks: an intermediate retained variant took +38 instead of +26 and moved one
1,000-frame VRAM match.

Every row, pair, group and column coefficient still matches. The empty-stream fixed term is now
170 clocks versus 144 authentic, a +26 residual rather than +70. Brick Blaster's first call is
150,750/152,290 masked/live clocks versus 150,724/152,264 authentic. Unlike earlier wrapper-only
trials, its masked and live residuals are equal. Each of the five IRQ handlers takes 308 clocks in
both BIOSes, and handler entry offsets lie within 2 clocks of UMC (12,160/44,928/77,700/110,468/
143,238 versus 12,160/44,928/77,702/110,470/143,240). The 1,373 boundary, 1,280 open-bus and
131,072 flag cases, along with the gameplay corpus, match on every non-timing property. The
no-input 1,000-frame matrix retains 114 video and 110 VRAM matches. The remaining 13 CPU cycles
are pass bookkeeping: the empty path spends 85 cycles versus 72 authentic.

The 1,000-frame matrix retains the startup-only frame-1 presentation difference and the first later
VRAM/video boundaries at frames 114/116. After the bit-7 correction, no-input matching is 110 VRAM
and 114 video frames. Under the controller-cycle schedule, video matches on 720 frames and VRAM on
721, a substantial increase
from the earlier 483/482 result because the corrected IRQ cost permits later reconvergence. This is
still a frame-hash compatibility result, not proof of complete functional equivalence; `$F018`
timing remains the next causal target.

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
| Reset and `$6020` entry | Exact Brick Blaster UMC clock/IRQ phase; broader phase partial | SWK uses repeated measured waits and an explicit pending-final-IRQ release. Brick Blaster SWK and UMC enter at master clock `30,239,372`, 218 clocks after their final boot IRQ; Bit enters at `30,239,430` with the same phase. Startup presentation, AY/audio phase, physical timing, and broad cartridge-specific timing remain unresolved. |
| Periodic IRQ wrapper and `$6026` callback | Exact for the observed enabled-callback cost | Counter carry and callback gating are synthetic-tested. The wrapper adds the measured 308 master clocks per enabled callback IRQ, making Brick Blaster's observed `$F009` and `$F00C` boundaries exact. Arbitrary callback work, physical timing, nested interrupts, stack bus timing, and other live phases remain open. |
| Link NMI and `$6023` callback | Exact synthetic timing; live timing unknown | Disabled, direct, and absolute-JMP synthetic forms measured 66, 110, and 116 master clocks respectively at instruction-granular emulator boundaries. The 64 P/256 S matrix and partial receive-state save/load pass. Live UART/Netpacket edges, callback reentrancy, nested IRQ/NMI and physical timing remain open. |
| `$F000` fixed bitmap placeholder | Partially complete | The independently authored blank placeholder matches the measured geometry, write counts, final LCD address, return state, and 3,278-master-clock uninterrupted duration. It does not reproduce the authentic asset or claim matching intermediate event timing and interrupt interference. |
| `$F003` eight-IRQ wait | Exact service contract | A shared 256-value matrix verifies the 42-clock ready path, `8-n` IRQ count, return state, and lack of direct LCD/VRAM effects. IRQ phase and callback elapsed time are external to the service; callback-modified X is intentionally observable. |
| `$F006` counter wait | Exact service contract | The same matrix verifies the 42-clock ready path, `5-n` IRQ count, return state, and lack of direct LCD/VRAM effects. IRQ phase and callback elapsed time remain caller/hardware context. |
| `$F009` long wait | Exact isolated contract and observed Brick Blaster schedule | The shared matrix verifies the 42-clock ready path, `24-n` IRQ count, return state, and lack of direct LCD/VRAM effects. A one-IRQ callback test confirms callback-modified X. With the measured enabled-wrapper cost, all ten observed Brick Blaster waits match UMC entry and return clocks. |
| `$F00C` two-plane clear | Exact uninterrupted and observed interrupt-spanning timing | The ordered 12,800 VRAM writes, 13,602 LCD writes, registers, scratch, LCD state, and 247,326-master-clock duration match the controlled contract for both ordinary and row-increment control modes. Brick Blaster's seven- and eight-IRQ calls also match UMC exactly. Mid-call mutation and exact individual event timestamps remain open. |
| `$F00F` fixed plane-0 clear | Exact for the controlled contract | Synthetic 775-byte VRAM, 829-LCD-write and 15,930-master-clock results agree; broader interrupt and side-effectful contexts are not claimed. NOP padding is an implementation convenience, not a timing substitute. |
| `$F012` glyph run | Partial, improved | The fixed-row unroll, direct selector transfer, reduced stack bookkeeping, and combined C/V decision tree preserve the measured output, flags, and stack footprint. Across all 256 one-tile selectors and both tested entry-V states, SWK is now 68-130 clocks over the authentic selector/page formula; the original worst residual was 274. Exact class-dependent timing remains open. |
| `$F015` two-plane bitmap copy | Partial, improved | Geometry, LCD writes and tail-JMP behavior are implemented. Keeping status on the stack lets `$01A` count columns while preserving `$01C`, eliminating both width stack round trips. Reordering the saved status/X values avoids a temporary status copy, and the entry X value is consumed directly when deriving the destination column. An out-of-line 256-row pointer path removes the ordinary per-column taken branch. Tested nonzero-height shapes now take `322 + c*(184 + 76*r)` versus the authentic `234 + c*(132 + 76*r)`; zero height uses `r=256` and a 176-clock column term. The per-byte cost is exact; the ordinary remaining difference is 88 fixed clocks plus 52 per column. A source-pointer carry still adds eight clocks beyond the authentic indexed-read page penalty. |
| `$F018` pair stream | Partial, substantially improved | Emitted columns, the 260-clock row term, transparent/opaque pairs, and the groups used by Brick Blaster now match. Emitted bytes use the authentic 32-clock spacing. Its first live call takes 152,290 clocks versus 152,264 authentic, leaving an isolated +26-clock fixed residual (down from +70) independent of tested row and pair/group shape. Duration-only dispatch trials changed IRQ phase and were rejected. Minimal group 3 is four clocks short and group 4 four clocks long. All synthetic boundary/open-bus/flag cases retain matching non-timing behavior. Zero opaque counts and page-cross combinations remain open. |
| `$F01B` row-run stream | Partial | Synthetic ordinary and escape/wrap cases matched UMC timing. Later callbacks, RAM aliases, decimal mode and interrupt-handler work are not fully timing-equivalent. |
| `$F01E` tile-literal stream | Partial | A sampled context was 314,334 prototype clocks versus 324,556 authentic clocks; exact timing and broad stream variation remain open. |
| `$F021` row-literal stream | Exact for the executable contract | All 406 authentic corpus calls and 450 synthetic cases matched the independent model, including exact uninterrupted timing and handler-time-only additions in 280 interrupt-spanning replays. Mutable aliases, nonterminating streams and handlers that alter service inputs remain outside the contract. |
| `$F024` record initialization | Exact for the controlled contract | Synthetic D-clear record initialization matches 824 master clocks, 32 `$FF` writes, register/status and no peripheral effects. D-set, later callback and interrupt-spanning contexts remain open. |
| `$F027` sprite/background compositor | Exact in the current 141-call gameplay replay | Objects use a separate selector-timed lookup. A dedicated out-of-line small-redraw path directly captures the fixed 2x2 map, uses descriptor-index parity instead of a RAM tile counter, and preserves the original compositor layout through a separate linker segment. Isolated small-redraw selector groups 0-3 are exact and groups 4-7 are eight clocks fast; isolated large redraw is +14 clocks, large objects are 0/+4, and small objects are 0/-2. All 141 Cube Up calls now match the authentic uninterrupted timing formula as well as pixels, VRAM/LCD/ROM access sequences, registers and peripherals. Stack write footprint remains different, and broader cartridges, interrupts, aliases and decimal mode remain open. |
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

- **`$F027` small-redraw selector matrix:** all 256 selectors were installed into all four captured
  map cells of an isolated small redraw. Residuals for selector groups 0-7 are
  `+556,+364,+188,-4,-164,-356,-532,-724` master clocks. The differences between groups equal four
  copies of the object selector deficit resolved above. Applying the timed lookup would therefore
  flatten every group to +556 clocks, after which the dedicated small-redraw traversal must remove
  278 CPU cycles of fixed overhead. This is the next implementation target; neither the shared
  lookup nor the currently exact sampled large-redraw path should be changed in isolation.

  The retained follow-up implements that boundary as a separate `SMALL_REDRAW` linker segment so
  existing compositor instruction addresses do not move. Direct fixed-size capture and parity-based
  tile termination remove the traversal excess; a measured 33-cycle fixed block restores the target.
  The resulting matrix is exact for groups 0-3 and eight clocks fast for groups 4-7, while every call
  in the current 141-call gameplay replay has exact uninterrupted timing.

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
