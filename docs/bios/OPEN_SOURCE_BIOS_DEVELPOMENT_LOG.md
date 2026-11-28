# Open-source BIOS development log

This log contains the project’s development history, provenance, validation, design decisions, and limitations for the independently authored experimental Gamate BIOS. It is not an authenticity or hardware-accuracy claim; consult the individual findings for their stated evidence and limits.

## Entry-state research informing the replacement

### Authentic BIOS cartridge-entry state sweep

- Local origin: the 62-cartridge user-supplied collection under both recognized BIOS revisions,
  read in place.
  Captures were taken at `$6020` before the first cartridge instruction; local state artifacts remain
  gitignored and are not distributed.
- Method: an independently authored harness captured serialized machine state and readable fields.
  Paired BIOS RAM and VRAM hashes matched for all cases. No captured image was retained in the
  repository.
- Result and use: all 124 cases reached entry. Paired BIOS captures had identical RAM/VRAM hashes.
  The established entry fields and checksum relationship are described in
  [`BIOS_REPLACEMENT_ANALYSIS.md`](BIOS_REPLACEMENT_ANALYSIS.md) and its conclusion
  that copied state templates are not a clean-room replacement strategy.
- Limits: this is one instruction boundary in the current emulator, not a physical bus trace.
  Invariance does not prove necessity, and the sweep does not establish post-entry service use.

### Cartridge-entry VRAM perturbation sweep

- Local origin and method: the 62-cartridge/two-BIOS matrix was run for 1,000 post-entry frames
  with authentic, zero, `$FF`, alternating-plane, or address-derived entry VRAM; authentic and zero
  were also tested with all buttons held. Diagnostic read-before-write instrumentation and
  tests live in a separate diagnostic build. The production `gamate.c` object remained byte-identical
  to its pre-instrumentation version. `tools/gamate-vram-perturbation-summary.py` excludes expected
  VRAM/video changes from execution comparisons; focused tests verify CPU/display read-before-write
  accounting and that display blanking does not count a VRAM fetch.
- Result and use: authentic and cleared VRAM matched execution and final video in all 124 cases for
  both input conditions, as did their final VRAM hashes. The 124-case no-input comparison was
  repeated after an earlier recorded count of eight final-VRAM differences could not be reproduced;
  its temporary reports are no longer available to resolve the discrepancy.
  Across the five no-input patterns, nonzero patterns changed execution for
  seven titles and final video for two; every case had a different final VRAM hash under at least one
  pattern. The replacement policy is clear VRAM, not arbitrary VRAM; details and limits are in
  [`BIOS_REPLACEMENT_ANALYSIS.md`](BIOS_REPLACEMENT_ANALYSIS.md).
- Limits and legal boundary: deterministic 1,000-frame emulator paths are not exhaustive or physical
  power-on evidence. Reports retain derived counts and hashes, not ROM, BIOS, VRAM, or framebuffer
  payloads.

### Cartridge-entry RAM perturbation and access sweep

- Local origin and method: the 62-cartridge/two-BIOS, 1,000-frame corpus was tested with full-RAM,
  block, chunk, byte, four-byte, inverse, semantic-field, and phase perturbations. The semantic
  low-RAM policy was checked with no input and all buttons held (`GAMATEBOOT_PRESS="1:255:1000"`).
  Filtered watches traced selected RAM accesses without retaining RAM values. Diagnostic-only hooks
  classify reads before each byte's first write by cartridge, BIOS, or other instruction without
  recording RAM values. Additional pre-entry watches of `$00CE` and `$021F` covered all 124
  BIOS/cartridge cases; 1,000-frame no-input watches compared authentic and individually zeroed
  bytes in the 14 titles affected by either perturbation. Derived access counts, first-access
  metadata, and write-delta categories were compared across BIOSes without retaining access values.
  A further local experiment constructed all WRAM using `$FF` fill and the named low-RAM fields.
  Both input schedules were tested across the corpus, with and without a separate `$021=$00`
  initialization, and with that initialization combined with cleared entry VRAM. A pre-entry
  `$021` watch covered all 124 cases. In Galaxy Invaders and Mars Voyage, restoring each 64-byte
  block, each chunk in `$020-$03F`, and each byte in `$020-$03F` isolated the policy discrepancy;
  both revisions and both input schedules were used for each control.
  A pre-entry `$022` watch covered all 124 cases; authentic and zero-byte entry runs were compared
  for 1,000 frames across the full corpus under no input and all buttons held. Focused Galaxy
  Invaders watches established the first post-entry access without retaining RAM values.
  Focused tests cover owner attribution and read/write accounting; production objects remained
  unchanged.
  `tools/gamate-ram-perturbation-summary.py` compares aggregate observations while excluding only
  the expected final-RAM hash change.
- Result and use: clearing all WRAM or filling it with `$FF` changed 114 and 110 of 124 executions.
  The semantic low-RAM policy using the protection reply, reset-fill sentinels, callback flag, IRQ
  counter, and nonzero `$F030` state matched all 124 cases with no input and all 124 with all buttons
  held. Every case had cartridge reads before first writes; their union covered all 1,024 physical
  bytes. BIOS reads before first post-entry writes occurred in 118 cases. Controller-input tracking
  added no addresses to either owner union. The full block, byte, phase, and access results are in
  [`BIOS_ENTRY_RAM_ANALYSIS.md`](BIOS_ENTRY_RAM_ANALYSIS.md); replacement
  implications are summarized in
  [`BIOS_REPLACEMENT_ANALYSIS.md`](BIOS_REPLACEMENT_ANALYSIS.md).
  Clearing `$00CE` changed 26 cases, with final-video and final-VRAM differences in two and three
  cases; clearing `$021F` changed four cases without final-video or final-VRAM differences. Their
  pre-entry watches found no CPU access in any case: both retain the emulator's `$FF` reset fill.
  All 13 `$00CE`-sensitive titles first read that value in cartridge IRQ context; writes preserved or
  decremented it. Zeroing prevented writes in 12 titles, while Money Maze continued decrementing.
  At `$021F`, Mighty Boxer read but did not write in the baseline window; zeroing led to
  IRQ-context writes. Witty Apee also read the inherited value before writing it. Both BIOSes
  produced identical derived watch results in these selected cases. This supports preserving the
  emulator's reset-fill policy for compatibility, not attributing it to the BIOS.
  The all-WRAM constructed policy differed from authentic entry in four cases for each input
  schedule until `$021` was initialized to zero. The BIOS last wrote zero there in every pre-entry
  watch; restoring only this byte removed the discrepancy in the two affected titles. With `$021`
  constructed as zero, the policy and the combined clear-VRAM policy matched measured execution,
  final video, and final VRAM hashes across all 124 cases in both input schedules. Expected whole-RAM
  hash differences were excluded. All other machine state and the resident BIOS remained authentic.
  No pre-entry writes to `$022` were observed; it retains the emulator's `$FF` reset fill. Clearing
  it changed Galaxy Invaders' execution, final video, and final VRAM under both BIOSes and both
  input schedules. Cartridge code read the inherited byte before writing it. This supports retaining
  the modeled fill in the candidate policy, not a physical power-on claim.
- Limits and legal boundary: the experiments establish tested sufficiency, not exhaustive gameplay
  coverage or physical RAM behavior. The software meanings of these bytes and later input-driven
  use remain unknown. Local BIOS/ROM inputs remained read-only; no memory images, firmware-derived
  initialization template, or payload data were published. Reports retain derived counts and
  hashes, not ROM, BIOS, VRAM, framebuffer, or RAM payloads; no BIOS or cartridge code was copied.

## Post-entry BIOS runtime API analysis

#### Experimental replacement: standalone Cube Up

- Local origin and method: the standalone Cube Up cartridge and both legally obtained BIOS
  revisions were executed read-only in this project's emulator. Four 1,000-frame post-entry
  campaigns used no input, repeated long Start, menu actions, and the controller cycle defined
  in the runtime API analysis. The trace retained only derived service targets, call counts and
  frame numbers; it did not retain cartridge or firmware bytes. Both revisions produced the
  same service-call summaries: the unattended path reached `$F024`, `$F00C`, `$F01E`, `$F000`
  and `$F009`; the three input campaigns additionally reached `$F012` and `$F02A`. No link NMI
  was exercised. A separate 20-frame RAM-watch experiment classified writes to `$00B` without
  retaining values: during the first observed wait, BIOS-owned IRQ-context writes incremented
  that byte under both revisions; the CPU's read-modify-write sequence also wrote back its prior
  value. These observations identify an initial prototype target, not all Cube Up paths or a
  physical-hardware guarantee.
- Fixed-bitmap geometry: an address-only LCD-port probe of Cube Up's first `$F000` call under each
  revision observed the same four columns at row `$8C` in each plane, with eight data writes per
  column. Only control/address writes and data-write counts were reported, not bitmap bytes.
  Both revisions completed the uninterrupted service in 3,278 master clocks. The experimental
  firmware uses that layout with a purpose-written blank placeholder and matches the measured total
  duration; its pixels do not reproduce the authentic BIOS asset, and its intermediate event timing
  remains unclaimed. The observation is emulator-based and does not establish the bitmap's meaning
  or physical display appearance.
- Rectangle boundary correction: an independently controlled `$F02A` call with a zero-derived
  256-column count, one row, X=`$F8` and Y=`$FE` was run under both BIOSes. Address-only VRAM
  coverage showed all 32 columns of that row in each plane, rather than plane 0 alone. The
  corresponding LCD address trace showed all 256 selectors incrementing as a raw eight-bit
  counter. The firmware therefore retains that selector instead of clipping it; the runtime
  analysis now limits its plane-0 description to ordinary nonzero widths. Other zero-count
  combinations and physical hardware remain unverified. No source bytes were retained.
- Wait/callback correction: a purpose-written synthetic cartridge supplied an IRQ callback that
  changed A, X, and Y during a one-IRQ `$F009` wait. Both authentic BIOSes and the experimental
  firmware returned the callback-modified X, restored Y, and returned A zero. This disproves an
  unconditional X-preservation claim for arbitrary callbacks; the prior corpus preservation result
  remains valid for the observed callers. The synthetic cartridge contained no commercial payload,
  and the result is an emulator observation rather than a physical measurement.
- Prototype inputs and implementation: the published physical-hardware notes by Kevtris cited
  above supplied the protection identifier and serial protocol; the project-authored
  [`ROM_HEADER_ANALYSIS.md`](../ROM_HEADER_ANALYSIS.md), boot and replacement analyses
  supplied the exact header field, checksum range, entry boundary and candidate RAM policy; and
  [`BIOS_RUNTIME_API_ANALYSIS.md`](BIOS_RUNTIME_API_ANALYSIS.md) supplied the observed
  service and callback effects. The new firmware was written independently from those behavioral
  requirements. Its exact short protocol/header identifiers are functional format constants, not
  firmware instructions or assets. No BIOS instruction sequence, original bitmap, commercial
  cartridge program, or other payload was copied or embedded. The optional assembler/linker are
  `ca65` and `ld65` from the [cc65 project](https://cc65.github.io/); no cc65 runtime library is
  linked into the image.
- Prototype validation: the purpose-written synthetic-cartridge suite checks reset, successful and
  rejected boot, IRQ counter carry and gated callbacks, unsupported traps, wait behavior, the
  implemented services, both LCD-clear modes, all 256 glyph selectors with two incoming overflow
  states, 256-tile, 256-column and 256-row boundaries, and wrapped stacks and streams. A separate
  CLI check confirmed explicit unsupported-service, invalid-identity and invalid-checksum errors
  with synthetic cartridges. Both authentic BIOSes and the prototype were then run against standalone
  Cube Up for each of the four 1,000-frame input campaigns above. Per-slot call counts matched;
  final video and VRAM hashes matched in all three input-driven campaigns but differed in the
  unattended run. Audio hashes, boot/IRQ phases, frame timing, PCs and RAM hashes differed.
  Controlled masked first-call probes of `$F01E`, `$F012`, and `$F02A` matched output registers,
  VRAM hashes/addresses and LCD/VRAM write counts against both BIOSes, but not elapsed clocks.
  Only derived metadata and hashes were compared; local commercial inputs remained read-only.
  This does not establish physical behavior, full gameplay, link NMI, interrupt interference,
  arbitrary inputs or compatibility with other cartridges. See
  [`EXPERIMENTAL_BIOS.md`](EXPERIMENTAL_BIOS.md) for build instructions and scope.

- Broad SWK startup-retention sweep: all 62 catalogued local cartridges were run read-only for 100
  post-entry frames under the independently authored firmware. All reached `$6020` and ran without
  an unsupported-service trap; 59 produced nonblank output. Famous 7, One Million Whys, and
  QuizFighter continued through periodic callbacks and calls to `$F024` and `$F003` but retained
  blank VRAM. Controlled UMC BIOS runs were nonblank from frame one and produced the same later video
  and VRAM hashes across those three titles, without a post-entry BIOS drawing-service call in the
  observed paths. This establishes a dependency on retained BIOS startup presentation, not on a
  newly discovered callable service. Implementation impact: a broadly compatible SWK boot needs
  independently authored nonblank startup artwork; authentic pixels must not be extracted or copied.
  The sweep used no input and does not establish longer gameplay, link behavior, or physical timing.

#### Corpus and general method

- SWK startup timing, before the final-IRQ correction: the reset routine performed 36 repeated
  24-IRQ waits plus one 20-IRQ wait before clearing VRAM and continuing initialization to `$6020`.
  Brick Blaster measured entry at `30,228,254` master clocks in a clean rebuild, while UMC measured
  `30,239,372` and Bit `30,239,430`. All three were in frame floor `414` of the 72,900-clock frame
  model; SWK was 11,118 master clocks earlier than UMC. The later absolute-call-timing follow-up
  below records the retained correction.
  This is emulator-observed timing work, not a physical-hardware measurement or claim of exact
  frame-aligned BIOS timing. Logo rendering and image assets are not retained as part of this result.

- SWK AY initialization policy: controlled Brick Blaster boots under each authentic BIOS revision
  reached `$6020` with all 16 AY registers zero and no diagnostic AY-register writes during boot.
  Existing controlled runtime-service probes also found no intrinsic AY effects when cartridge
  interrupt callbacks were excluded; aggregate AY changes during interrupt-spanning calls may be
  callback effects. The independent firmware retains its explicit 16-register clear to establish a
  known silent state without relying on assumed physical or emulator power-on contents. This is a
  defensive compatibility policy, not a reproduction of authentic firmware activity. It does not
  resolve the differing oscillator, noise, envelope, or audio-sample phase at entry. Inputs remained
  read-only, and the result retained only counters and register-state summaries, not BIOS or
  cartridge bytes. The observation is emulator-derived and does not establish physical AY reset or
  bus behavior. The synthetic prototype suite now dirties all AY registers before boot and verifies
  that SWK writes each register exactly once and reaches the all-zero entry state, so the policy does
  not depend on the emulator's normal reset initialization.

- Current SWK `$F027` gameplay replay: the established Cube Up input schedule (Start at frame 90,
  then four A pulses) produced 141 `$F027` calls under both SWK and UMC. The diagnostic replay masks
  IRQs for each captured entry and compares SWK with the independently derived authentic contract.
  All SWK calls matched pixels, VRAM reads/writes, ordered LCD activity, cartridge reads, returned
  registers/status, and unrelated peripheral state. The 139 common four-small-redraw/four-small-
  object calls were 3,936 master clocks above the contract; one three-large-object call was 3,424
  above; one eight-small-object call was 894 below. Final modeled scratch values matched, while the
  stack write footprint did not. These current residuals supersede the earlier 33,000–68,000-clock
  gameplay gap after intervening compositor optimizations. Their opposing signs show that a global
  padding correction is unsafe; shift- and shape-specific execution needs further work. Inputs were
  read-only and no BIOS, cartridge, RAM, VRAM, or graphics bytes were retained. This is emulator
  replay evidence, not physical timing.

- SWK `$F027` isolated shift matrix: the synthetic prototype test now checks both object-size arms
  at every horizontal shift 0-7 against the authentic timing formulas while retaining the established
  output and state checks. Large-object residuals are `-394,-318,-254,-190,-126,-62,+2,+66`
  master clocks; small-object residuals are `+428,+296,+152,+8,-136,-280,-424,-568`. The large and
  small redraw samples remain exact. This confirms that the four authentic operation families are
  parameterized spectra and that the formerly sampled shifts 6 and 3 happened to be the near-zero
  points. Implementation impact: correct the fixed and shift-dependent work within each object arm;
  do not add a global compositor delay. The matrix uses only purpose-written cartridge data and is
  an emulator timing result, not physical measurement.

- SWK `$F027` first large-object timing correction: the large shifter now processes two rows per
  loop, reducing its slope by the measured 32 CPU cycles per horizontal shift, with separate fixed
  timing for zero and nonzero shifts. The isolated matrix now measures zero residual at shift 0 and
  +4 master clocks at shifts 1-7, replacing the former -394 through +66 range without changing
  pixels or modeled state. Moving the code changed instruction-page placement in later paths: the
  untouched small-object residuals are now `+466,+320,+176,+32,-112,-256,-400,-544`, and the 141-call
  Cube Up replay shapes are +4,088, +3,728, and +288 clocks. This confirms that isolated arm timing
  and mixed selector/source-alignment timing must both be rechecked after each layout change. No
  commercial payload was retained; the local inputs remained read-only.

- SWK `$F027` small-object specialization: the generic variable-height source traversal was replaced
  for eight-pixel objects by a dedicated two-plane, eight-row loader that writes the same source and
  spill buffers. Measured fixed and per-shift timing blocks then bring the isolated small-object
  residual to zero at shift 0 and -2 master clocks at shifts 1-7; the prior post-layout range was
  +466 through -544. The accompanying layout movement leaves isolated large objects +16/+20 clocks,
  while the 141-call Cube Up replay now measures 0 for its three-large-object shape, +336 for its
  eight-small-object shape, and +2,224 for the common four-small-redraw/four-small-object shape.
  Pixels, ordered VRAM/LCD/cartridge accesses, registers, status and unrelated peripherals still
  match. The remaining replay gaps therefore target selector/source alignment, mixed redraw work,
  and the already identified stack footprint rather than object shift slopes. The synthetic loader
  contains no commercial data; local BIOS and cartridge inputs remained read-only.

- SWK `$F027` selector-group matrix and rejected local correction: a purpose-written small-object
  matrix now exercises every callable object selector `$00-$FE` against the authentic
  `T(selector)` formula (`$FF` disables an object record). Residuals are
  constant within each 32-selector group and measure `0,-48,-92,-140,-180,-228,-272,-320` master
  clocks from group 0 through 7. The differences decompose through selector bits, but adding those
  costs directly to `point_tile` exposed compensating traversal overhead: the controlled large and
  small redraw samples became 2,640 and 1,108 clocks slow. Because code placement also changes 6502
  branch page-crossing costs, that local correction was reverted. Implementation impact: rewrite or
  retime tile selection and redraw traversal as one measured unit, then rerun object and mixed-call
  matrices. The test uses synthetic cartridge data; local commercial inputs remained read-only and
  no payload bytes were retained.

- SWK `$F027` object-only selector correction: object source loading now uses an out-of-line
  `point_object_tile` path while redraw retains its existing jointly calibrated lookup/traversal.
  Selector timing is applied once across the two graphics planes. After rebalancing the existing
  object fixed-cost blocks, groups 0-3 match the authentic `T(selector)` formula and groups 4-7 are
  two master clocks fast; the controlled combined case is +2 clocks. In the 141-call Cube Up replay,
  the three-large-object and eight-small-object shapes are both exact. The common shape remains
  +2,224 clocks with four small redraws, identifying a 556-clock-per-redraw residual in that context.
  Functional outputs and ordered accesses continue to match. This retained split avoids changing
  redraw behavior while establishing that the next timing target is redraw traversal/selector
  interaction. Inputs remained read-only and no payload bytes were retained.

- SWK `$F027` small-redraw selector matrix: a purpose-written isolated redraw forces all four
  captured 2x2 map cells to each selector `$00-$FF` in turn. Against the authentic formula, group
  residuals are `+556,+364,+188,-4,-164,-356,-532,-724` master clocks. Their deltas are exactly four
  copies of the previously measured selector-group deficit. The current Cube Up common shape uses
  four such group-0 redraws, accounting exactly for its +2,224-clock residual. Implementation impact:
  create a dedicated small-redraw traversal using the timed selector lookup, then remove its measured
  278-CPU-cycle fixed excess; do not alter the shared lookup or large-redraw path independently.
  Pixels are still compared for every matrix case. The test is synthetic and no commercial payload
  or local input bytes were retained.

- SWK `$F027` dedicated small-redraw implementation: records 8-15 now dispatch to an out-of-line
  `SMALL_REDRAW` segment, keeping the established page-aligned compositor addresses stable. The path
  captures its fixed 2x2 selectors directly in column-major order, uses the object-timed selector
  lookup, and terminates each two-tile column from descriptor-index parity instead of a RAM counter.
  A measured 33-CPU-cycle fixed block completes the timing balance. The 256-selector matrix is exact
  for groups 0-3 and eight master clocks fast for groups 4-7; isolated small redraw is two clocks
  fast in the established mixed-selector case. Most importantly, all 141 calls in the Cube Up input
  campaign now match the authentic uninterrupted timing formula: the common four-small-redraw/four-
  small-object shape and the three-large/eight-small object-only shapes all report zero residual.
  Pixels, ordered accesses, registers and peripheral state also match. The replay still reports only
  the known stack write-footprint mismatch. Inputs remained read-only and no payload was retained.

- Brick Blaster three-BIOS per-frame matrix, before the final-IRQ correction: the project-authored
  `gamatebioscompare` diagnostic ran
  the local standalone cartridge for 1,000 post-entry frames under both authentic BIOS revisions and
  SWK, with no input and the standard controller-cycle schedule. It compared derived framebuffer,
  VRAM and RAM hashes, CPU/register state, hardware registers, and cumulative activity while
  explicitly excluding audio samples, AY oscillator/envelope phase, sample phase, and absolute
  master clock. All inputs remained read-only and no payload, pixels, RAM, or VRAM were retained.
  SWK's known blank-startup difference appears at frame 1; video matches at frame 2 and VRAM at frame
  5. The first later video boundary is frame 109 versus authentic frame 110 during `$F00C`; VRAM
  diverges at 111 and continuing no-input video divergence begins at 113. No-input SWK matches an
  authentic video/VRAM hash on 110/106 frames; controller-cycle matches on 442/443 and reconverges at
  frame 401 before later transient differences. This is emulator evidence, not physical validation.
  Implementation impact: investigate entry phase and the pre-frame-109 service/IRQ/LCD schedule;
  do not attribute the first divergence to `$F027` or audio phase.

- Brick Blaster absolute-call-timing follow-up: `gamatebioscalls` was extended to report call-entry
  and return clocks relative to `$6020`, plus the entry wait counter, IRQ timer and pending-IRQ state.
  The UMC, Bit and SWK runs use the same service order through frame 109, and their first `$F00C`
  begins exactly 16,814 clocks after `$6020`. Current-source SWK reaches `$6020` 11,118 master clocks
  before UMC with 2,526 IRQ-timer clocks elapsed, versus 218 for UMC. A controlled fixed-delay
  experiment reached UMC's exact absolute entry clock but left 13,644 IRQ-timer clocks elapsed and
  did not move the frame-109 comparison boundary; the delay was reverted. SWK's final boot IRQ
  interrupts `$F187`
  (the `INY` in its second `clear_plane`) 2,526 clocks before entry. Both authentic revisions report
  `$F2FA` and an age of 218 clocks. The authentic address is treated only as an observed execution
  boundary; no authentic instruction or listing was retained or reconstructed. By the second
  `$F00C`, SWK leads by an
  additional 2,752 clocks: the first live `$F009` contributes 2,432 and later waits differ by 32-50
  clocks. The following `$F018` takes 119,224 SWK clocks versus 152,264 under either authentic BIOS.
  These are local emulator observations using read-only BIOS/cartridge inputs; reports remained in
  temporary storage and retained no payload.
  Implementation impact: the retained reset tail masks IRQs before the final clear, completes
  initialization with a request pending, releases it at a calibrated boundary, and restores the
  cartridge-visible counters afterward. Boot-only synchronization was moved after the runtime
  services so their addresses and page-cross timing remain unchanged. Brick Blaster SWK now matches
  UMC at entry clock `30,239,372`, final-IRQ clock `30,239,154`, and IRQ age 218; Bit enters 58 clocks
  later with the same age. SWK accepts 913 boot IRQs versus 917 authentic because requests coalesce
  while masked; this diagnostic history is not machine state. The corrected 1,000-frame no-input
  matrix first differs again at frame 113 and matches video/VRAM on 111/108 frames, up from 110/106.
  Controller-cycle matching rises from 442/443 to 445/450. The next targets are live `$F009`/IRQ
  wrapper timing and `$F018`.

- `$F018` replacement timing alignment: the executable contract and purpose-written synthetic
  streams above were used to separate fixed, row, pair, tile-group and emitted-column terms. The
  retained independent implementation adds a tail-resident 46-CPU-cycle column delay, uses the
  opaque contract's required X=`$FF` result to avoid unnecessary X preservation, and uses a
  specialized pointer helper with bit-weighted high-tile delays. The in-place opaque setup was
  relocated with unreachable fill so `$F01B` and every later runtime service retain their prior
  addresses. No authentic instructions or payload bytes were consulted or reconstructed.
  Minimal ordinary cases now place the emitted-column term at the authentic 330 master clocks;
  opaque-pair and tile-group residuals are a few clocks. The first live Brick Blaster `$F018` call
  improves from 119,224 to 151,676 clocks versus 152,264 authentic, across 2,000 VRAM writes and five
  interrupts. Its first later video mismatch moves from frame 113 to 116. In the 1,000-frame matrix,
  no-input video/VRAM matching becomes 114/108 frames and controller-cycle matching becomes 483/480.
  All 1,373 boundary, 1,280 open-bus and 131,072 flag cases retain matching LCD/ROM sequences,
  registers, RAM and peripheral behavior. Large timing residuals remain for some zero-count and
  page-cross combinations, and fixed/row and interrupt-spanning timing are not yet exact. The
  62-cartridge 100-frame SWK smoke sweep remains 59 normal passes plus the same three known
  blank-output titles. Reports stayed in temporary storage; BIOS and cartridge inputs were read-only.

- Live `$F009`/IRQ-wrapper alignment: absolute completed-call reports compared Brick Blaster's first
  ten waits under UMC and SWK after the boot-phase correction. The isolated `$F009` contract was
  unchanged; its ready path remains 42 clocks and the existing 256-counter matrix still passes.
  Before the wrapper change, steady live waits were 32-48 clocks short. A single NOP after an enabled
  `$6026` callback reduces them to 0-16 clocks short; a two-NOP experiment returned to the earlier
  phase and was reverted. Unreachable reset fill was shortened by one byte so `$F003` and every later
  runtime service retain their established addresses. The first Brick Blaster wait enters with
  `$00B=8` and remains 128 clocks short; no caller-specific delay was invented. The second `$F00C`
  entry lead falls from 448 to 160 clocks, and the following `$F018` entry lead falls from 742 to 426.
  The IRQ-spanning `$F018` duration changes from 151,676 to 151,652 clocks versus 152,264 authentic.
  Both 1,000-frame matrices retain their first video/VRAM boundaries; controller-cycle VRAM matching
  rises from 480 to 482 frames. All project tests pass, and the 62-cartridge 100-frame SWK sweep
  remains 59 normal passes plus the same three known blank-output titles. This is local emulator
  evidence, not physical IRQ timing; reports stayed temporary and inputs were read-only.

- Enabled IRQ-wrapper cost follow-up: comparing each live `$F00C` duration against its independently
  established 247,326-master-clock uninterrupted contract shows that UMC adds exactly 308 master
  clocks per enabled `$6026` callback IRQ: 2,464 clocks for eight IRQs and 2,156 for seven. SWK
  added 270 per IRQ. Supplying the missing 38 clocks in the shared enabled wrapper makes both Brick
  Blaster clears and all ten observed `$F009` waits exact, so SWK and UMC enter the first `$F018` at
  master clock 8,114,978 with IRQ phase 20,608. An I-masked authentic-occurrence replay isolates the
  remaining `$F018` deficit at 422 clocks: SWK takes 150,302 versus UMC's 150,724 while producing the
  same LCD/VRAM result. This is local emulator evidence from independently executed firmware, not a
  physical timing measurement. Diagnostic reports remained temporary and no firmware or cartridge
  payload was retained.

- The corresponding three-BIOS 1,000-frame comparison excludes audio samples, AY/audio phase and
  master-clock equality. SWK retains its startup-only frame-1 difference, then first differs again
  in VRAM at frame 114 and video at frame 116. It matches UMC on 109/114 no-input VRAM/video frames
  and 720/720 controller-cycle frames; the latter improves from the preceding 482/483 result due to
  later reconvergence. The remaining boundary begins during the isolated 422-clock `$F018` timing
  deficit, so this matrix guides the next implementation pass rather than establishing complete
  equivalence.

- A 100-post-entry-frame SWK smoke sweep after the IRQ-wrapper change covered all 62 local
  cartridges: 59 completed normally, while Famous 7, One Million Whys and QuizFighter retained
  their already documented blank-output failures. This was a short regression screen, not the
  standard 1,000-frame compatibility window.

- `$F018` row-term follow-up: the executable model's derived counters report 18 rows, 83 pairs,
  21 transparent pairs, 125 opaque columns, no stream crossing/wrap, and tile-group counts
  `21,11,0,0,0,0,19,32` for Brick Blaster's first call. No stream or graphics payload was retained.
  Minimal synthetic cases show the pre-change timing residual as `+78` fixed, `-14` per logical
  row, `+4` per transparent pair, `-4` for each group-6 pair and `-8` for each group-7 pair; these
  terms reproduce the measured `-422` exactly. A plane-aware delay supplies seven CPU cycles across
  the two passes, correcting the authentic 260-master-clock row term. Preserving all earlier service
  addresses changes internal `$F018` page alignment and contributes another 44 clocks in this long
  path. The live call becomes 152,138 clocks versus 152,264 UMC while retaining identical output
  and five IRQs, leaving 126 clocks across the fixed, transparent-pair, high-group, and code-page
  terms. The repeated 1,000-frame matrix retains 109/114 no-input VRAM/video matches and records
  719/719 controller-cycle matches. A repeated 62-cartridge, 100-frame smoke sweep remains 59 normal
  passes plus the same three known blank-output titles. This is emulator-derived timing evidence,
  not a physical measurement.

- `$F018` bit-7 timing follow-up: minimal group cases showed that the row-aligned implementation's
  bit-7 pointer path was one CPU cycle short per plane. Because its preceding ASL guarantees carry
  set, replacing one NOP with an always-taken branch adds that single cycle without changing flags,
  registers, RAM, or port activity. Groups 4-7 gain four master clocks per logical pair; groups 0-3
  are unchanged. Brick Blaster's masked call changes from 150,598 to 150,802 clocks versus 150,724
  UMC, and its live call changes from 152,138 to 152,342 versus 152,264. The identical +78 residual
  isolates the next Brick target to fixed-call timing: this stream's +332-clock pair surplus cancels
  its remaining -332-clock groups 6/7 deficit. Groups 5/6 remain four clocks short and group 7 eight
  clocks short in minimal tests, so this does not claim complete `$F018` timing alignment. The
  repeated 1,000-frame comparison records 110/114 no-input VRAM/video matches and 721/720
  controller-cycle VRAM/video matches. Audio samples, AY/audio phase, and master-clock equality
  remain excluded from that frame-hash comparison.

- `$F018` fixed-cost/layout follow-up: an internal two-pass tail transition reduced Brick Blaster's
  residual from 78 to 64 clocks, but expanded the terminator dispatch and moved later BIOS code; it
  was rejected rather than risk an adjacent service regression. A retained alternative inlines the
  `$FF` row-delimiter handler and uses carry known set by `CMP #$FF` for a three-cycle row delay.
  This keeps the 260-master-clock row term and the calibrated column loop while reducing code size.
  Minimal opaque groups 0-6 now have zero pair/group residual, group 7 is four clocks short, and
  transparent pairs remain four clocks long. Brick Blaster stays 78 clocks long: 21 transparent
  pairs contribute +84, 32 group-7 pairs contribute -128, and the long path retains a +44 internal
  page-layout term in addition to the +78 fixed term. The unchanged title residual is therefore not
  evidence of no improvement to the general timing formula.

- `$F018` direct-terminator follow-up: relocating the two-byte pass epilogue beside the pass entry
  replaces the branch-over/absolute-jump terminator path with a direct conditional branch. One
  additional first-plane NOP restores the exact row term after the resulting one-byte hot-loop
  shift, while unreachable fill keeps following service placement fixed. Minimal cases now measure
  a +70-clock fixed term, exact opaque-pair timing for groups 0-6, +4 clocks per transparent pair,
  and -4 clocks for group 7. Brick Blaster's derived shape therefore predicts and measures +26:
  `70 + 4*21 - 4*32`. Its masked call is 150,750 versus 150,724 UMC and its five-IRQ live call is
  152,290 versus 152,264. Output, registers, RAM, port sequence, IRQ count, and entry phase match.
  The repeated 1,000-frame matrix retains 110/114 no-input VRAM/video matches and records 720/719
  controller-cycle VRAM/video matches. That hash count is a compatibility observation, not the
  optimization target; the service-level timing residual is reduced from 78 to 26 clocks.

- `$F018` transparent/group-7 follow-up: the transparent path already has tile zero in `$1A`, so it
  skips the generic helper's redundant store while a NOP plus carry-known branch retains five setup
  cycles. This removes one CPU cycle per plane and makes the transparent-pair term exact. In the
  opaque helper, X is dead after pointer selection; comparing the saved tile in X gives group 7 one
  extra cycle while preserving A and the stream index. Brick Blaster's masked/live calls become
  150,794/152,334 clocks versus 150,724/152,264 UMC, isolating a +70-clock fixed residual. Minimal
  groups 0-2 and 5-7 match relative to that fixed term; group 3 remains four clocks short and group
  4 four clocks long. The repeated matrix retains 110/114 no-input VRAM/video matches and records
  720/720 controller-cycle matches. No cartridge payload was retained.

- `$F018` fixed-term phase check: purpose-written ordinary streams with zero, one, two, three, and
  four rows, plus transparent and opaque single-pair cases across groups 0, 1, 6, and 7, all retain
  the same 70-master-clock residual. The term is therefore independent of row count and pair/group
  shape in the tested domain. A withdrawn direct two-pass dispatch experiment reduced the masked
  fixed residual from 70 to 44 clocks, but its changed instruction sequence moved Brick Blaster's
  live interrupt boundary and increased the open-call residual to 336 clocks. Output, registers, RAM,
  ports, and ROM-read order still matched, demonstrating that a duration-only reduction is
  insufficient: the eventual 35-CPU-cycle fixed correction must preserve interrupt-visible phase.
  The prototype was reverted; no firmware code or cartridge payload was retained.

- `$F018` intra-column phase check: LCD-port timestamps show UMC spaces the eight tile bytes by 32
  master clocks, while SWK's existing exact 330-clock column batches 92 clocks after its eighth
  write. Redistributing that delay exactly matches UMC's byte spacing and leaves the standalone
  column term unchanged, but changes instruction boundaries and grows the Brick Blaster open-call
  residual to 114 clocks after code relocation. The 1,000-frame no-input matrix retained 114 matching
  video frames but fell from 110 to 109 matching VRAM frames. That byte-delay redistribution was
  therefore also reverted: successful framelock work must solve early-call phase, per-write spacing,
  terminal fixed cost, and IRQ placement as one coupled change.

- `$F018` aggregate instruction-count check: on the retained first Brick Blaster call, UMC executes
  23,852 instructions in 152,264 master clocks and SWK executes 24,910 in 152,334 clocks; both take
  the same five IRQs. The difference is therefore not 35 extra instructions that can be deleted in
  isolation: the independent SWK implementation uses 1,058 more instructions with a different
  average cycle distribution. A purpose-written empty-stream probe isolates the fixed wrapper at
  the documented 144-master-clock contract versus 214 in the current replacement. The next
  implementation target is the two-pass wrapper: remove 35 CPU cycles without changing per-row,
  per-pair, per-column, stack, or IRQ-visible behavior. No authentic opcode sequence or firmware
  bytes were used for replacement design.

- Brick Blaster pre-`$F018` boundary audit: completed-call reports and local state snapshots were
  compared under UMC, Bit and SWK through frame 115. Bit and UMC agree exactly on all pre-`$F018`
  service entry/return clocks and live ordered LCD timings. SWK's `$F00F` retains the authentic
  15,930-clock duration and 775 VRAM writes but uses 829 LCD writes versus the authentic 838;
  its traversal order is column-major rather than the authentic row/column register pattern.
  SWK's first `$F012` takes 4,046 clocks versus 3,668 authentic (+378), with identical ordered LCD
  events and output. The immediately following `$F009` is 516,282 authentic versus 515,914 SWK
  (-368) because the later entry changes how much timer waiting remains. By the second `$F00C`,
  both versions enter at 7,865,412 clocks and return at 8,114,894; the first `$F018` enters both at
  8,114,978, at frame clock 8,950, LCD clock 40/row 55, IRQ clock 20,608 with no pending IRQ, and
  identical CPU registers and `$F018` inputs. VRAM, framebuffer and LCD accumulation also match.
  The whole machine is not identical: nonstack scratch, stack history and AY envelope counter differ
  at this point, while AY registers and sample phase match. This proves the live `$F018` video/control
  call itself is not contaminated by earlier timing drift, but also establishes upstream SWK audio
  and scratch mismatches that must be resolved separately before claiming whole-machine identity.
  No commercial payload bytes or authentic implementation code were retained or copied.

- `$F018` exact-byte-spacing experiment: the retained column's 92-master-clock tail delay was
  redistributed as five CPU cycles between each adjacent pair of tile-byte writes, using
  independently authored flag-neutral carry/branch sequences. The resulting first-column LCD
  timestamps advanced from 22-clock write spacing to the authentic 32-clock spacing, while a
  purpose-written single-column case retained a 330-clock column term. An out-of-line tail shim
  using a three-cycle A read and fixed JMP boundaries removed the direct column-size slope, but
  code relocation introduced a shape-stable +4 clock residual for opaque tile groups 0, 1 and 2.
  Brick Blaster's 11 group-0 logical pairs contributed 44 clocks, producing a 114-clock live
  residual. Inverting the bit-5 branch did not recover this cost; relocation made the live residual
  +438 clocks. The experiment was reverted. It establishes that exact inter-byte spacing and exact
  aggregate column duration are necessary but insufficient: low-group pointer selection and code
  placement must be redesigned together with the delay transfer.

- `$F018` compact scratch-RMW experiment: `INC $01B` supplies five CPU cycles in two bytes and was
  tested between each adjacent tile-byte write. The live repeat count is held in X, so changing the
  scratch count between bytes did not alter final output, and the service later overwrites `$01B`
  to zero. This eliminated the large inline code expansion, but it added seven undocumented RAM
  read/write-modify-write actions per column and changed N/Z during the loop. It also moved the
  group helper by nine bytes; the one-column synthetic residual was +126, the two-column residual
  +146, and Brick Blaster's live call +2,622 clocks. The variant was reverted. A compact timing
  primitive must also preserve observed bus accesses and helper placement; final RAM equality alone
  is insufficient.

- `$F018` address-neutral primitive trial: `TXA; BNE +0` provides five CPU cycles only after each
  opaque byte write because X is the nonzero repeat count; A is immediately overwritten by the next
  source load, so the result is not callback-visible between bytes. Replacing the batched column tail
  with JMP boundaries and replacing the 21-cycle bit-7 NOP run by `LDX #$04; DEX/BNE` preserved the
  nominal cycles and shrank the tail regions. It nevertheless moved `f018_repeat` by -5 bytes,
  the following services by +16 bytes, the column delay by +16, the opaque helper entry by +13 and
  the point helper by +13. One/two-column cases acquired +74 rather than the retained +70, and the
  live Brick Blaster residual became -4,170 clocks. A fixed-size low-group branch experiment
  additionally changed pointer reads. The trial was reverted. The target is now explicit: every
  external service address and both opaque helper entries must remain at their verified positions
  until the 35-cycle distribution can be moved without relocating the group-sensitive path.

- `$F018` direct-wrapper phase trial: replacing the two pass `JSR` entries with `JMP` and carrying
  the second-pass transition in `f018_end` saves 13 CPU cycles, reducing the masked empty/ordinary
  residual from +70 to +44 clocks while preserving all model outputs. The second pass then begins
  26 master clocks earlier; in Brick Blaster that changes which service instructions coincide with
  the five live IRQs and produces a +336-clock call residual, with VRAM frame matches falling from
  110 to 109. The trial was reverted. This establishes that wrapper savings alone are not phase
  neutral: compensating delay must be placed before the affected IRQ boundaries, while the remaining
  22 CPU-cycle reduction is still required for the masked contract.

- `$F018` compensated-wrapper trial: the direct two-pass wrapper was paired with an out-of-line
  28-master-clock delay at the second-pass transition, intended to cancel its 26-clock early start
  without moving inline helper addresses. It moved the helper entry by nine bytes and brought IRQ
  1, 3, 4 and 5 near authentic clock offsets, but at IRQ 5 SWK had completed only 2,279 LCD writes
  versus 2,289 authentic. The call still required 152,640 clocks (+376). This proves the extra live
  duration is not removable by restoring timer alignment alone: the direct wrapper executes less
  progress before the same interrupt boundary. An inline 26-clock compensation moved more code and
  performed worse. The variants were reverted; future wrapper work must match both IRQ clock and
  write count at every boundary.

- `$F018` direct-return follow-up: the independently authored second-pass tail-call experiment
  reduced the isolated residual from 70 to 62 master clocks, but the added first-pass conditional
  shifted that pass terminator by two CPU cycles. In the live Brick Blaster call this changed
  interrupt-visible alignment enough to raise the residual from 70 to 354 clocks and reduced the
  1,000-frame VRAM match count from 110 to 109. This rejects optimizations that change the first
  terminator even when the measured final branch appears irrelevant to the call's data result.
  The candidate was reverted.

- `$F018` IRQ-progress decomposition: a diagnostic overlay counted emitted LCD writes at each
  accepted IRQ in Brick Blaster's first live call. UMC accepts its five IRQs at 12,160, 44,928,
  77,702, 110,470, and 143,240 master clocks with 258, 641, 1,295, 1,769, and 2,289 writes
  complete; SWK accepts them 2, 4, 2, 10, and 12 clocks later. SWK is at 257 writes at IRQ 1,
  641 at IRQ 2, 1,291 at IRQ 3, 1,766 at IRQ 4, and 2,287 at IRQ 5. Equal 32-clock spacing between
  the eight bytes of a column can be achieved independently, but calibrating that replacement to
  the 330-clock column total produces a 44-clock live code-layout term, while the empty-stream
  +70-clock wrapper term remains. This separates three requirements: wrapper duration, byte-write
  phase, and IRQ progress. A final solution must satisfy all three together; per-write spacing
  alone, even with an exact column coefficient, is not sufficient. The independent aggregate test
  remains 23,852 versus 24,910 instructions, 152,264 versus 152,334 clocks, and five IRQs for
  UMC and SWK respectively. No replacement logic from this phase was retained.

- `$F018` explicit-plane ground-up redesign: a replacement controller encoded plane selection in
  X and used one low bit as an "opaque pair seen" marker, removing the two pass-subroutine calls and
  their PHA/PLA marker from the parser. It passed the synthetic prototype and 1,373 boundary cases
  on every non-timing property, and row, transparent-pair and 330-clock emitted-column terms could
  be calibrated independently. However, opaque pairs required saving/restoring the encoded X state
  around the count loop; that cost 17 CPU cycles per physical plane pass. An opaque pair therefore
  retained a 34-clock residual, and Brick Blaster's 62 opaque pairs contributed 2,096 extra clocks
  relative to the isolated +70 fixed wrapper term. The redesign was reverted. It establishes that
  replacing the pass wrapper is insufficient unless X/count/opaque state can remain resident without
  a per-opaque push/restore cycle, so the next retained attempt must solve the opaque-loop state
  ownership as part of the controller redesign rather than after it.

- `$F018` retained byte-loop/direct-pass step (local experiment against UMC as a black box): an
  independently written 16-cycle per-byte column loop replaced the unrolled writes and batched
  column delay, matching the measured 32-master-clock write spacing and preserving the 330-clock
  column total. The pass `JSR` entries became fall-through/`JMP` entries; unexecuted padding keeps
  following services and opaque helper entries fixed. Compatibility policy, not hardware
  measurement: UMC rewrites `$00` with its unchanged value between passes. That write is
  invisible to software, so SWK omits it, and `gamatebiosf018` now requires only that `$00` keeps
  its value. Keeping the write produced +38 rather than +26 clocks.
  The retained version changes the fixed term from +70 to +26 clocks. Brick Blaster's first call
  takes 150,750/152,290 masked/live clocks versus 150,724/152,264 UMC. All five IRQ handlers take
  308 clocks, handler-entry offsets are within 2 clocks, and the masked/live residuals are equal.
  The boundary, open-bus and flag selftests and the gameplay corpus match on all non-timing
  properties. The 1,000-frame no-input matrix remains at 114 video and 110 VRAM matches. Only
  derived clocks, PCs and LCD-write counts were recorded. Temporary probes lived under `/tmp` and
  were deleted. No authentic code was disassembled or copied. The remaining +26 is pass
  bookkeeping (85 versus 72 CPU cycles on the empty path), not a row, pair or column coefficient.

- `$F018` post-return liveness audit (local experiment; UMC as a black box): each local cartridge
  ran for 1,000 frames under the no-input and controller-cycle schedules, covering up to 24
  `$F018` returns per title. At each return, one of A, X, Y, N, Z, C, V, `$00`, `$01` or `$18`-`$1B`
  was inverted in a machine copy. That copy and an unperturbed copy each ran 60 frames, and their
  frame hashes and serialized states were compared. Across 77 returns in 23 titles, A and N/Z/C
  never mattered. `$18`-`$1B` only persisted unread. X, Y and V changed later state in GP Race, and
  V also in Vindicators. `$01` changed video in Mighty Tank and Vindicators, and `$00` changed
  Mighty Tank state.
  Effect: compatibility policy, not hardware behavior. SWK omits the authentic A=`$FE`/N/Z/C
  result and does not reproduce final scratch values. `gamatebiosf018` and the prototype test
  check X, Y, V/D/I and the pointer only. The fixed residual falls from +26 to +18 clocks.
  Limitations: finite horizon (60 frames), two input schedules, at most 24 calls per title, and
  untested game paths. Only derived pass/fail counts were retained; the temporary probe lived under
  `/tmp` and was deleted.

- `$F018` page-as-marker trial and downstream call comparison (local experiments; UMC as a black
  box): using the saved stream page as the first pass marker failed every functional check,
  because the marker also indexes the plane base pointers and must be 0 or 2. Counted
  alternatives (JSR return-address markers, pre-pushed second markers, fall-through reordering
  and zero-page index offsets) each retain 81 empty-path cycles. A 200-frame `gamatebioscalls`
  comparison of Brick Blaster then showed the next live disagreements to be larger:
  `$F01B` -4,840 clocks, `$F015` +360 to +512 and `$F027` +1,316 to +2,958, with `$F003` waits
  realigning frames. Only derived entry and duration clocks were recorded. Temporary reports were
  deleted.

- `$F01B` timing correction and liveness audit (local experiments; UMC as a black box): synthetic
  `gamatebiosrowstream` cases decomposed the SWK residual into +8 fixed and -48 clocks per row and
  per pair. A 60-frame post-return perturbation audit covered 51 returns in 22 titles under the
  no-input and controller-cycle schedules. Only X was consumed (Dino Ball); A, Y, N/Z/C/V, `$00`,
  `$01` and `$1A` were not.
  Effect: the redundant epilogue was removed, and a shared 24-cycle delay now runs once per row and
  per pair. Instruction reordering aligns all 1,247 LCD writes of Brick Blaster's first call with
  UMC. The full contract, including A/status, is retained.
  Validation: Brick Blaster's masked/live replay, all 18 corpus calls in the no-input 1,000-frame
  campaign, and 1,200 randomized synthetic cases matched with zero clock delta. The cases cover
  escapes, zero counts, crossings/wraps in RAM and ROM, `$015` classes, D-set entry and random
  registers.
  Model limitation: 56 empty-stream cases entered with N or V set differ from the model's status
  formula identically under UMC and SWK.
  Only derived timings and pass/fail counts were recorded. Temporary probes lived under `/tmp`
  and were deleted. No authentic code was disassembled or copied.

- `$F015` liveness audit, indexing discovery and redesign (local experiments; UMC as a black box):
  the post-return perturbation method used for `$F018` covered 969 returns in 47 titles under the
  no-input and controller-cycle schedules (60-frame horizon, at most 24 sampled returns per title).
  A and N/Z/C never mattered. `$00`, `$01`, `$18`, `$19`, `$1A`, `$1B`, `$1C`, `$1E` and `$1F` only
  persisted unread. X, Y and V changed later state in a few titles, and `$1D` was read by Kill
  Shot. Effect: SWK omits A, N/Z/C, the final source pointer and the scratch values as a
  compatibility policy, not a hardware claim.
  A derived-input probe then read the real pointers and dimensions of Brick Blaster's first four
  `$F015` calls. The first (10 columns by 16 rows, planes at `$AD40` and `$ADE0`) takes 256 clocks
  longer under UMC than the resolved formula predicts, and a running source index reproduces
  exactly 128 indexed page crossings (none from a per-column pointer update). That also explains
  the documented Y result. This supersedes the earlier pointer-carry surcharge model. SWK now runs Y
  continuously over each plane, advances the page byte at wraps, and uses cycle pads for the
  authentic per-column and fixed terms.
  Validation: seven synthetic shapes (1x1, 1x8 to 4x8, crossing and 256-row) match the formula
  `234 + c*(132 + 76*r) + 2*crossings + 8*wraps`. Brick Blaster's four calls are exact. Across 8,018
  corpus calls (300 frames per title, both BIOSes), 7,890 had comparable inputs. Their LCD/VRAM
  write counts, X, Y, scroll and VRAM-address outputs matched in 7,889. Of 7,245 uninterrupted
  calls, 7,210 matched in duration and 35 were exactly 16 clocks long (one 8-clock wrap per plane).
  Limitations: wrap cost is not reproduced, the horizon is finite and no game path outside the
  corpus was tested. Only derived timings, pointers and pass/fail counts were recorded. Temporary
  probes lived under `/tmp` and were deleted. No authentic code was disassembled or copied.

- Local origin: all 62 user-supplied cartridges and both recognized BIOS revisions, read in place.
  The unattended baseline covers 1,000 post-entry frames; deterministic controller schedules add
  input-driven coverage. Local trace reports are not distributed.
- Method: the independently authored `gamatebioscalls` and `gamatebiosprobe` diagnostics observe
  executed boundary transitions, pair service calls, and summarize registers, status, timing, memory
  effects, and peripheral state. Controlled probes use authentic call contexts, named state
  perturbations, address-only ROM traces, or XOR changes to a private in-memory ROM copy. Diagnostic
  instrumentation is isolated from production builds; output contains derived metadata, not payloads.
- Result: all 124 unattended cartridge/BIOS runs completed. Across the corpus, calls targeted the
  same 18-slot table at `$F000-$F033`; every title used at least one slot, and direct-call event
  multisets and paired register signatures matched across BIOS revisions. The paired sweep completed
  326,334 calls, with 82 legitimately open at the observation boundary. Controller campaigns
  expanded observed call-site and ABI signatures without exposing a new slot; they did not exercise
  link-driven `$6023` NMI callbacks.
- Resolved and bounded contracts are documented in
  [`BIOS_RUNTIME_API_ANALYSIS.md`](BIOS_RUNTIME_API_ANALYSIS.md): `$F000`
  fixed bitmap writer; `$F003`, `$F006`, and `$F009` IRQ waits; `$F00C` two-plane VRAM zero-writer
  with an LCD-control input; `$F00F` plane-0 rectangle clear; `$F012` selector-driven eight-row tile
  writer; `$F015` two-plane rectangle copy; `$F018` tile-row blitter; `$F01B` plane-0 row-run
  stream service; `$F01E` two-plane tile-literal stream service; `$F021` plane-0
  row-literal stream service; `$F024` strided RAM initializer; `$F02A`
  rectangle fill; `$F02D` plane-1 fill; `$F030` pseudorandom service; and `$F033` packed-BCD formatter.
  `$F027` remains caller-dependent.
- Used for: the replacement-ABI contracts, the 1,000-frame sweep default, and the conclusion that
  replacement firmware needs resident service and interrupt code, not just an entry-state jump.
  The linked analysis owns per-service matrices, exact behavior, and probe-specific caveats.
- Limits: this is behavioral evidence from the project-authored emulator, not physical measurement.
  Finite corpus and input schedules do not establish exhaustive reachability; link/NMI paths and some
  caller-specific or timing details remain open. No external emulator implementation was consulted
  or copied.

#### Runtime-ABI documentation decisions

- The runtime API document is the canonical implementation contract. It leads with callback rules,
  service inputs and outputs, formulas, side effects, and interruption behavior. This source record
  owns corpus counts, experiment provenance, confidence limits, and the rationale for interpreting
  an observation as a contract.
- Service names are deliberately effect-based. Labels such as “rectangle fill,” “row-run stream,”
  and “strided RAM initializer” describe controlled inputs and outputs; they are not recovered
  authentic symbols. Broader labels were withdrawn when later perturbations established a narrower
  operation, and unresolved services retain qualified names.
- The 1,000-post-entry-frame default was selected because the earlier 300-frame window missed
  `$F030` and `$F033`. It is a coverage policy, not an ABI timing requirement.
- Uninterrupted timing formulas exclude IRQ/NMI work because authentic services generally leave
  interrupts enabled. Aggregate calls that span callbacks can include cartridge RAM, VRAM, and
  peripheral effects; those effects are not attributed to the service without controlled replay.
- Exact event offsets are emulator instruction-boundary observations unless a section explicitly
  identifies physical measurement. They are useful for interference models but are not claims about
  physical bus-cycle edges.
- The two authentic revisions have the same observed public slot surface and controlled contracts,
  despite internal relocation and a 58-master-clock boot-entry phase difference. The replacement
  specification therefore describes their shared cartridge-facing ABI and documents a difference
  only where a probe observes one.

#### `$F030` pseudorandom transition and interrupts

- Source and method: local user-supplied BIOS revisions and cartridges read in place; the existing
  1,000-frame no-input, long-Start, menu-action, and controller-cycle reports identify 27 caller
  cartridge images (26 titles, including both Bomb Blaster dumps). At the first call context in
  each image under both BIOSes, the independently authored
  diagnostic `gamatebiosf030` exercised every 16-bit state with two entry profiles, plus every
  A/X/Y/scratch byte and all 64 status combinations at seven selected states. It checked the
  independently formulated state, scratch, register, flag, RAM-access, stack-access, timing,
  ROM-read, and hardware-effect hypotheses. All 7,489,152 calls matched (138,688 per context).
  Seven states across every stack pointer passed another 1,792 calls per revision in one context.
  Those tests use a synthetic return after reaching the authentic context.
- Result: for `s = input ? input : $FFFF`, `next = ((s << 1) | (bit(s,4) XOR bit(s,10))) & $FFFF`,
  `$014 = bit(s,10) << 7`, A receives the low byte, and carry receives bit 15 of `s`. X/Y/S and
  I/D/V are preserved; N/Z describe A. The zero-input path costs 188 master clocks, other inputs
  174. The SWK implementation is regression-tested against both timing branches as well as the
  transition and returned state. Input `$8000` produces zero, correcting the earlier nonzero-output
  assertion. Enumerating
  the derived transition yields cycles of lengths 7, 85, 85, 85, 595, 595, and 595, with maximum
  transient 10; the observed entry seed lies on a 595-state cycle.
- Interrupt method and result: address watches on seven synthetic inputs under both BIOSes
  identified state/scratch access ordering. Purpose-written callbacks installed only in private
  in-memory cartridge copies demonstrated a half-updated state, scratch-byte interference, and a
  late state mutation leaving A different from `$012`. A callback reentering `$F030` also produced
  a different result from two sequential calls. All four scenarios matched across 27 caller
  contexts and both BIOSes using direct IRQ and NMI injection (432 interrupted calls); NMI
  injection did not simulate UART link traffic. An exploratory single-context matrix tested IRQ
  injection at each modeled instruction boundary for three states and one `$80` write to each of
  `$012-$014` (288 calls per BIOS); derived signatures matched between revisions and varied with
  injection point. An independently derived event model then matched 813,240 IRQ/NMI tests covering
  eight synthetic states, five byte values, three targets, every modeled instruction boundary,
  all 27 caller contexts, and both BIOSes. The tests covered both D/V-clear and D/V-set entry;
  the latter NMI profile also set I. A second matrix matched all 20,818,944 cases covering those
  states, targets, available boundaries, contexts, BIOSes, IRQ/NMI, and all 256 callback byte values,
  with entry P=`$20`. A separate 27,108-case matrix matched a model of one uninterrupted reentrant
  `$F030` call inside the callback, across eight input states, every outer boundary, IRQ/NMI, all
  27 contexts, and both BIOSes; entry P was `$20`. This does not establish arbitrary nested
  interrupts, callback programs, or link-driven NMI delivery. Read-only BIOS/ROM files were not
  modified; no firmware, commercial cartridge, or graphics payload was retained or published, and
  no external emulator code was consulted or copied. Results are emulator observations, not
  physical measurement.

#### `$F033` formatter follow-up

- Source and status: both local, user-supplied BIOS revisions and the local cartridge collection,
  read in place. The existing 1,000-frame unattended and input-driven call reports expose 16
  cartridge images calling `$F033`, not only the unattended caller. These are execution observations
  in the project-authored emulator, not physical measurements.
- Initial method and result: an independently authored diagnostic supplied synthetic A/X/Y and
  status values after an authentic Tornado call. Both BIOSes produced two trailing spaces when
  input Y was `$FF`; with A=X=`$00`, all six positions were spaces and returned carry was clear.
  Synthetic invalid BCD nibbles otherwise produced character values `$30 + nibble` in the
  examined cases. A 20,000-call mixed-input check under one BIOS agreed with this candidate model
  for D clear and set, including timing and status. The existing four completed-call campaigns
  contain 2,470 uninterrupted calls, all matching the derived register, status, and timing model;
  42 interrupt-spanning calls were excluded. Campaigns overlap and contain no observed invalid
  nibbles, Y=`$FF`, or D-set inputs. Output bytes are not retained in those reports.
- Corpus matrix: `gamatebiosf033` exercised 526,848 synthetic inputs per first-call context across
  16 images and both BIOSes, yielding 16,859,136 matches. Each context tested every A/X pair with
  Y=`$00`, `$99`, and `$FF`; 65,536 mixed 24-bit values; both D states; and every status byte and
  stack pointer on five representative inputs. Registers, output, timing, and unrelated hardware
  effects were checked in every call; those representative cases also checked RAM and stack access
  maps. Tornado used the no-input campaign and the other 15 the existing long-Start schedule.
- Full-domain verification: all 16,777,216 A/X/Y inputs matched the uninterrupted model under each
  BIOS and both D profiles, totaling 67,108,864 calls. D-clear runs used Tornado's no-input
  context; D-set runs used Brick Blaster's long-Start context. Entry I was set. Output, registers,
  status, timing, and peripheral effects were checked; the exhaustive run does not replace the
  separate context, stack, status, and interrupt matrices.
- Interrupt method and result: diagnostic address watches for five synthetic inputs at the first
  caller context of all 16 images and both BIOSes yielded identical output-buffer access sequences
  and emulator clock offsets. Independently written single-byte IRQ and NMI callbacks, installed
  only in a private in-memory cartridge copy, matched the derived event model in 1,257,984 cases
  across all contexts and both BIOSes: seven inputs, six chosen write values, each output-buffer
  byte, every outer instruction boundary, and entry P=`$20`. Another 1,257,984 cases passed with D/V
  set; that NMI profile also set I. A further 209,664 cases matched one uninterrupted reentrant
  `$F033` call per callback, with the callback saving and restoring X/Y, using seven outer and inner
  inputs, all outer boundaries, both interrupt types, every context, and entry P=`$20`. Direct NMI
  injection did not test UART link delivery; arbitrary callback programs, nested interrupts, and
  physical timing remain unverified. The private ROM/BIOS inputs were not modified or copied into
  the repository; no external emulator source or BIOS code was consulted or copied.
- Callback register observation: a purpose-written callback replaced A, X, and Y before returning
  from an interrupt injected at a synthetic cartridge boundary. In all 32 caller/BIOS contexts,
  enabled IRQ callbacks left the replacement X but not A or Y; enabled NMI callbacks left
  replacement X and Y but not A. P, PC, and S were restored in both cases. Disabling callbacks
  preserved all tested registers. The expanded 38,912-case matrix covered all 256 stack pointers,
  both callback-enable states, and all N/V/D/I/Z/C flag combinations for NMI or I-clear combinations
  for IRQ in all 32 contexts. This corrects the former claim that both wrappers always restore the
  entire interrupted context. NMI was injected directly, not delivered by UART; arbitrary
  callbacks, stack modification, and nested interrupts were not tested.
- Register-interference result: with synthetic A/X/Y=`$12/$78/$9A`, a callback changing X/Y to
  `$34/$56` and an NMI at emulator offset 182 caused `$F033` to access synthetic RAM `$0F8`,
  transforming its test value `$30` to `$20`. Adjacent NMI boundaries and matched IRQ controls did
  not change `$0F8`. All 192 probes across both interrupt types, three boundaries, and all 32
  caller/BIOS contexts matched, showing that callback register clobber can invalidate the
  uninterrupted output-buffer write set. These are emulator observations with direct NMI injection,
  not physical measurements or a link-traffic test. Leaving the targeted RAM byte unchanged instead
  yielded carry clear in 10 of 32 contexts and set in 22; the inherited byte values were not retained.
- A follow-up used the same synthetic formatter input with NMI callback Y=`$5E`, S=`$F0`,
  stack-page `$100=$30`, and zero-page `$000=$31`. At offset 182, `$100` became `$20` and Y returned
  `$5F`, while `$000` was preserved; adjacent-boundary and IRQ controls did not change `$100`.
  The expanded 384 tests of both target addresses, three boundaries, IRQ/NMI, and all 32
  caller/BIOS contexts matched. The test did not overwrite a live return address, and it does not
  establish physical or link-driven behavior.
- A synthetic NMI callback at offset 182 set Y to each value `$06-$FF` and initialized
  `$0A2 + Y` to `$30` before returning. All 8,000 tests across the 32 caller/BIOS contexts left
  that target `$20` and returned `(Y + 1) & $FF`; targets span `$0A8-$1A1`. Callback-side
  initialization was necessary to isolate the formatter from preceding wrapper effects, and live
  stack entries were avoided. This is direct emulator NMI injection, not physical or UART testing.
- Earlier IRQ/NMI callback register changes were tested at offsets 0, 68, and 86 using the same
  synthetic input, with callback X=`$34` and Y=`$56` or `$5E`. The X-derived character pair was
  respectively `34`, `38`, or `78`; NMI also replaced the Y-derived pair with the callback Y
  digits, while IRQ retained the original pair. All 384 tests across both callback-Y values, both
  interrupt types, three boundaries, and all 32 contexts matched. These are selected examples,
  not an exhaustive model of register clobber or physical/link-driven timing.

#### `$F003` IRQ-gated counter wait

- Method and result: `gamatebiosprobe` set RAM `$00B` in its private machine copy and invoked the
  first authentic `$F003` call of each of the 30 titles observed using the service, under both BIOS
  revisions. All 256 byte values were tested in every context (15,360 probes, zero mismatches).
  Values `$00-$07` waited exactly `8 - n` IRQs; `$08-$FF` returned in 42 master clocks without an
  IRQ, wrote only `$00B`, and had no peripheral effects. Every probe zeroed the counter, returned
  A=`$00` with X/Y/S preserved, and returned status `(P_in & $7C) | $03`. Probe outputs retained
  only derived access counts and result summaries; ROM and BIOS inputs remained read-only and no
  payload was retained.
- Use and limits: this closes the full input-byte threshold map across every observed `$F003`
  caller's first call context; later call contexts were not individually perturbed. Wait-path
  elapsed clocks still include title-specific IRQ callback work, whose intermediate counter updates
  were not traced. This is local emulator evidence, not a physical timing measurement; no external
  emulator source was used or copied.

#### `$F006` IRQ-gated counter wait

- Method and result: `gamatebiosprobe` set RAM `$00B` in its private machine copy and invoked the
  first authentic `$F006` call of each of the 26 titles observed using the service, under both BIOS
  revisions. All 256 byte values were tested in every context (13,312 probes, zero mismatches).
  Values `$00-$04` waited exactly `5 - n` IRQs; `$05-$FF` returned in 42 master clocks without an
  IRQ, wrote only `$00B`, and had no peripheral effects. Every probe zeroed the counter, returned
  A=`$00` with X/Y/S preserved, and returned status `(P_in & $7C) | $03`. Probe outputs retained
  only derived access counts and result summaries; ROM and BIOS inputs remained read-only and no
  payload was retained.
- Use and limits: this closes the full input-byte threshold map across every observed `$F006`
  caller's first call context; later call contexts were not individually perturbed. Earlier
  IRQ-masked probes in Dinosaur Park and Witty Apee did not complete within a 10-million-instruction
  bound at `$00B=0`; callback counter updates remain untraced. This is local emulator evidence, not
  a physical timing measurement; no external emulator source was used or copied.

#### `$F009` IRQ-gated counter wait

- Method and result: `gamatebiosprobe` set RAM `$00B` in its private machine copy and invoked the
  first authentic `$F009` call of each of the 41 titles observed using the service, under both BIOS
  revisions. All 256 byte values were tested in every context (20,992 probes, zero mismatches).
  Values `$00-$17` waited exactly `24 - n` IRQs; `$18-$FF` returned in 42 master clocks without an
  IRQ, wrote only `$00B`, and had no peripheral effects. Every probe zeroed the counter, returned
  A=`$00` with X/Y/S preserved, set C/Z, cleared N, and preserved B/D/I/V. Probe outputs retained
  only derived access counts and result summaries; ROM and BIOS inputs remained read-only and no
  payload was retained.
- Use and limits: this closes the full input-byte threshold map across every observed `$F009`
  caller's first call context; later call contexts were not individually perturbed. Wait-path
  elapsed clocks still include title-specific IRQ callback work, whose intermediate counter updates
  were not traced. This is local emulator evidence, not a physical timing measurement; no external
  emulator source was used or copied.

#### `$F012` cross-corpus graphics lookup

- Source and status: the local user-supplied collection of 39 titles with observed `$F012` calls and
  both authentic BIOS revisions, read in place. These are execution observations from the
  independently authored emulator, not physical-hardware measurements.
- Method: `gamatebiosprobe` captured address-only cartridge-ROM read sequences and derived VRAM
  output for the first authentic `$F012` occurrence in each title under both BIOSes (78 contexts).
  One graphics-data byte per context was XOR-perturbed only in the probe's private in-memory ROM
  copy; baseline and perturbed traces, timing, VRAM coverage, and output were compared. No payload
  values or lookup table were written to reports or repository files. A separate RAM matrix toggled
  each individual bit of `$000` and `$001` at service entry in the four one-tile contexts (64 probes).
- Result: all 78 contexts parsed. The 794 tile runs yielded 6,352 source/output row observations;
  89 distinct graphics input-byte values had one consistent output globally and within each title.
  Repeated selectors used identical eight-byte address runs in observed repeats. The per-call
  `base + 8 * selector` address formula fit 14 of 74 descriptor-bearing contexts, with single-tile
  contexts unable to discriminate the formula. Four one-tile contexts from two titles exposed no
  width/selector reads in the cartridge-ROM trace; each showed eight contiguous graphics reads and
  the return dummy read. RAM `$000` and `$001` were each read twice in baseline contexts. All 78
  graphics-byte perturbations changed only their associated output byte or bytes as predicted by the
  observed mapping, without changing address trace, timing, or VRAM coverage. Baseline address
  traces, timing, VRAM output and coverage, registers, and LCD activity matched between BIOS
  revisions in all 39 title pairs.
- RAM-perturbation result: all 64 single-bit probes returned without writing the toggled byte.
  Every `$000` bit toggle changed the ROM-read trace, output, timing, write shape, returned registers,
  and VRAM coverage. For `$001`, 20 of 32 toggles changed the trace, 20 changed output, 20 changed
  timing, 20 changed returned registers, and 16 changed VRAM coverage and write shape. None of the
  32 individual bit toggles produced the corresponding uniform shift in the first eight cartridge
  reads expected of a direct 16-bit linear ROM pointer. For `$001` XOR `$01`, one title pair differed
  between BIOSes only in read sequence and timing; output VRAM, returned registers, and write counts
  matched. Baseline and toggled byte values each matched across the pair, and duplicate probes
  reproduced the difference.
- Limits and legal boundary: only one authentic first-call context per title and one graphics byte
  per context were tested; 89 of 256 possible graphics input-byte values were observed. The RAM
  matrix toggled one bit at a time in only the four contexts that expose the one-tile path; combined
  changes and later call contexts were not tested. The control-data source, any indirect or
  table-mediated relationship to `$000-$001`, selector-to-address transform, and exact timing remain
  unresolved. RAM byte values were not retained. BIOS and cartridge files remained read-only;
  perturbations were confined to private in-memory copies. No BIOS/cartridge payload or code was
  copied or published, and no external emulator source code was consulted or copied.

#### `$F012` executable model and synthetic matrix

- **Source and status:** the local user-supplied collection (39 `$F012` caller titles, plus the two
  titles used for synthetic runs) and both authentic BIOS revisions, read in place. These are
  execution observations from the independently authored emulator, not physical-hardware
  measurements.
- **Method:** the diagnostic-only `tests/gamatebiosf012.c` (CMake target `gamatebiosf012`, linked
  only against the diagnostics library) contains an independently written executable model of the
  observed contract. It compares the model with every call, checking:
  - cartridge read addresses, including dummy and trailing reads;
  - VRAM write count, address, plane, and value;
  - A/X/Y/S and status flags;
  - exact master-clock duration.

  Corpus mode ran every caller under both BIOSes for the standard 1,000-frame window (78 runs,
  parallel processes). Synthetic mode boots once to the first authentic call, snapshots that state,
  installs RAM descriptors, and replays batched cases:
  - all single selectors × 10 entry-flag sets;
  - all 65,536 selector pairs;
  - 740 width, column-wrap, and page cases;
  - width 0;
  - all 256 `$015` values;
  - decimal entry.

  The transform test XORs one glyph byte at a time in the private in-memory ROM copy and restores
  it. Synthetic mode ran on two titles under both BIOSes (293,780 calls). `gamatebiosprobe` also
  gained an address-only CPU-address line for its ROM-read sequence. Private per-call feature
  reports contained only derived counts, positions, flags, and clocks, and remained outside the
  repository.
- **Result:**
  - All 36,518 uninterrupted corpus calls (35,564 ROM descriptors and 954 RAM descriptors) and all
    synthetic categories matched with zero failures, including timing and C/V.
  - Both BIOS revisions produced identical results, including identical decimal-mode deviations.
  - The resolved contract (pointer descriptor, selector-to-glyph-address classes, identity copy,
    column-counter plane selection, width-0 behavior, flag rules, and exact clock formula) is in
    `docs/bios/BIOS_RUNTIME_API_ANALYSIS.md`.
- **Corrections:** this supersedes several earlier `$F012` conclusions in this file and in the
  analysis document:
  - The mapping is an identity copy; the earlier "not a copy" conclusion was wrong.
  - There is no separate lookup table.
  - `$015` does not supply the plane.
  - The one-tile contexts without visible descriptors use RAM descriptors.
  - `$000-$001` is a direct descriptor pointer.
- **Limits and legal boundary:**
  - Selector classes `$00-$1F` and `$21-$2E` never occur in the corpus; their costs and flags rest
    on synthetic cases.
  - Interrupt-spanning calls (2,516) and D-set entry are excluded from the contract.
  - The model was derived from observed addresses, counts, timing, and equality comparisons. No
    BIOS instructions were reconstructed.
  - The glyph font is cartridge-supplied; no glyph, BIOS, or cartridge bytes were retained or
    published.
  - BIOS and ROM files remained read-only.
  - No external emulator source was consulted or copied.

#### `$F018` cross-corpus command-stream structure

The first-call findings below are extended by the executable model entry that follows; their stated
open questions are addressed there for the tested memory domains.

- Source and status: the local user-supplied collection of 15 titles with observed `$F018` calls and
  both authentic BIOS revisions, read in place. These are execution observations from the
  independently authored emulator, not physical-hardware measurements.
- Method: `gamatebiosprobe` captured address-only ROM-read sequences for the first authentic call in
  each title under both BIOSes, with I set to isolate the service (30 contexts). A local in-memory
  parser interpreted cartridge bytes beginning at the first traced read using the previously
  established row/run grammar; it reported only aggregate counts, write-formula results, and
  BIOS-pair comparisons.
- Result: the grammar parsed in all 30 contexts, and
  `16 * sum(run_length for nonzero tile indices)` matched VRAM writes in every context. Across the 15
  paired title streams there were 249 rows, 1,244 run pairs, 257 transparent pairs, 2,791 active
  byte columns, and 5,238 total byte columns. Each stream contained 6-24 rows and 19-194 run pairs;
  row spans were 11-32 columns, with no row selector above 31 or zero run length. Read traces,
  stream structure, timing, returned registers, write counts, and VRAM hashes matched between BIOS
  revisions in all 15 title pairs.
- Limits and legal boundary: only the first authentic call in each observed title was parsed. This
  sweep did not resolve the precise interleaving of control and graphics reads, repeated
  stream/base triples, or later invocation contexts. BIOS and cartridge files remained read-only;
  no ROM/BIOS bytes, stream contents, or graphics payload were written to reports, files, or the
  repository. No BIOS or cartridge code was copied, and no external emulator source code was
  consulted or copied.

#### `$F018` synthetic replay follow-up

- Source and status: both authentic BIOS revisions and the local user-supplied cartridges, read in
  place and executed by this project's independently authored emulator. These are emulator
  observations, not physical measurements.
- Method: `tests/gamatebiostilestream.c`, linked only to the diagnostic library, replays entry snapshots
  against an independently authored behavioral model. It compares ordered cartridge read addresses
  including dummy reads, LCD port writes, register/status results, non-stack RAM write coverage
  and values, selected peripheral effects and clocks.
  `corpus` runs the four standard 1,000-frame campaigns on all 62 cartridges and both BIOSes;
  three initial no-input probes were included without rerunning them. Every entry was replayed
  with interrupts masked and with its authentic interrupt state. After address-order checking was
  added, the observed caller/campaign combinations were replayed for that new comparison.
  `selftest`, `selftest-open`, and `selftest-flags` generate purpose-written streams and tile
  patterns, including in the private in-memory ROM copy, which is restored afterwards. The
  previously documented Visual6502 and Bruce Clark decimal references informed the decimal-mode
  model, which was checked against both BIOS revisions.
- Result: 326 authentic calls in 25 titles (163 per BIOS) matched the masked model, including
  elapsed clocks; 246 unmasked replays spanned interrupts, and all 326 matched port and return
  state. The largest observed masked stack depth was six bytes below entry S. In two title
  contexts, each BIOS passed 1,373 boundary cases, including zero-page sources, 1,280
  status-byte/interrupt-mask cases, and
  131,072 pairs of transparent counts across binary and decimal mode. Another 1,200 seeded,
  varied streams matched each BIOS in Tornado. The resulting stream, memory, return, status and
  timing specification is recorded in
  [`BIOS_RUNTIME_API_ANALYSIS.md`](BIOS_RUNTIME_API_ANALYSIS.md).
- Corrections: the broader campaigns found more callers than the earlier first-call analysis.
  Zero opaque count means 256 tiles; zero transparent count advances by zero. The LCD control
  write forces bit 6, while columns beyond 127 can change the plane-select bit.
- Limits and boundary: this is finite execution evidence using the emulator's CPU, bus and LCD
  models, not a physical measurement. The model rejects side-effectful I/O and mutable scratch or
  stack aliases as stream or tile sources. Nonterminating streams and asynchronous handlers that
  interfere with the service are not specified by these tests. Synthetic patterns were placed only
  in RAM or the checker's private ROM copy. Reports contain derived results, not BIOS, cartridge,
  or graphics payloads. BIOS and ROM files remained read-only; no BIOS instructions, cartridge
  code, or external emulator implementation were copied or reconstructed.

#### `$F027` sprite and background compositor

- `$F027` follow-up: IRQ-masked `occurrence=1` probes covered 14 additional titles under both BIOS
  revisions; both authentic occurrences in Incantational Couple were also compared. A controlled
  matrix toggled `$006-$009` and zeroed `$023/$028/$030` in five varied titles and both Incantational
  calls. The fields were read-before-write and unwritten in these contexts, but their effects varied
  by caller and occurrence; at that stage they did not justify inventing a shared format. Address
  watches found `$006/$007` read twice as often as `$008/$009`, with equal
  counts within each pair; `$023/$028/$030` were read 1/2/2 times. This is consistent with two
  caller-supplied 16-bit cartridge-address words, but not a common data grammar. A separate
  IRQ-masked Galaxy Invaders first-call probe varied `$021` among `$00`, `$40`, and `$FF` under
  both revisions. The service read it once without writing it; output VRAM hashes and coverage
  differed, and `$FF` also changed timing. Follow-up local experiments varied `$021` across ten
  values in the first call of all 25 observed callers and seven further second-call contexts,
  under both BIOSes with IRQs masked. Each of the 640 probes read the byte once without writing
  it; 12 contexts changed coverage and 20 did not. All 256 byte values were then tested in the
  first Galaxy Invaders and Time Warrior calls under both revisions (1,024 probes). In those
  contexts coverage decomposed into fixed writes plus a 96-byte, two-plane region shifted by the
  high five bits with row wrapping; timing increased by 1,516 master clocks per low-bit offset.
  Eleven of the 12 sensitive contexts initially had the same address-set decomposition for the ten
  tested values; combined-position probes later resolved the twelfth by separating overlapping
  writes. Zero-filled Time Warrior probes at 13 selected values compared pixel rows in memory and
  found a corresponding right shift by the low three bits; `$FF`-filled probes showed
  background-dependent output. No pixel,
  ROM, or BIOS payload was retained. The result supports a horizontal-position input in selected
  paths, not a universal grammar or blend rule; no external implementation was consulted or
  copied. An analogous ten-value matrix for `$022` covered the same 32 contexts and both BIOSes
  (640 probes); all read it once without writing it, with coverage sensitive in the same 12
  contexts. All 256 values were tested in the first Galaxy Invaders and Time Warrior calls under
  both revisions (1,024 probes): coverage was a fixed set plus a 96-byte region translated by
  `$022` rows modulo 256, with unchanged timing. A combined four-by-four `$021/$022` matrix across
  the 12 sensitive contexts and both revisions (384 probes) matched horizontal/vertical translation
  and the previously bounded horizontal timing increment, including the formerly ambiguous Mars
  Voyage coverage. In-memory, zero-filled Time Warrior output comparisons for six horizontal and
  eleven vertical values checked both planes and all 16 rows under both BIOSes (4,224 row/plane
  comparisons), excluding positions overlapping fixed writes. Subsequent no-input, I-masked
  uniform-background experiments compared written VRAM bytes in memory. In Time Warrior's first
  call, 16 shifted and wrapping positions, 14 fills, and both BIOSes (448 probes) matched an output
  equation derived from the all-zero and all-one fills. Applying that equation to the same 32 caller contexts at
  inherited positions, 12 fills, and both revisions (768 probes) yielded 28,906 mismatches in
  139,296 byte comparisons; coverage stayed fixed, but timing could vary. Six-fill localization
  with the Bit BIOS identified ten failing contexts, including Box Forum, where output bits changed
  despite identical corresponding input bits and timing varied by background. These observations
  reject a universal per-bit rule and background-independent timing; uniform fills do not locate
  the dependence.
  Those initial findings did not establish a shared format or blend rule; the record-model follow-up
  below resolves those questions in the tested binary-mode cases. No pixel, ROM,
  or BIOS payload was retained; no external implementation was consulted or copied. Exact results,
  call scopes, and limits are in
  [`BIOS_RUNTIME_API_ANALYSIS.md`](BIOS_RUNTIME_API_ANALYSIS.md).
- Record-model follow-up: the two local, legally obtained BIOS revisions were executed in this
  project's emulator after booting Time Warrior from the read-only local collection. Diagnostic
  probes initialized the 128-byte table, varied fields and backgrounds, and recorded only access
  counts, addresses, hashes, and synthetic-output comparisons. Opt-in probe views report VRAM
  read-before-write coverage and LCD register ordering without exposing pixel data. They revealed
  the off-screen plane-0 tile map,
  explaining the previously observed cross-bit dependence. Isolated records identified separate
  redraw and object-draw gates, coordinates and graphics selectors, two record sizes, and the
  two-plane transparent object blend. Controlled overlaps established the tested pass and priority
  order. The diagnostic-only `tests/gamatebiosf027.c` independently models these observations:
  per BIOS, 1,024 isolated object cases, 1,024 redraw cases, 256 overlapping multi-record cases,
  and 2,052 selector and sentinel cases matched final VRAM and access counts. The latter two sets
  also checked the complete ordered LCD writes and cartridge read addresses, varied cached LCD
  control and entry flags, page-crossing dummy reads, and an additive timing formula. The checker
  replayed 496 cartridge/BIOS/campaign combinations with the four established 1,000-frame input
  schedules: 28,516 of 28,518 service entries matched final VRAM, access counts, registers and
  exact timing with IRQs masked; the same Mini Golf long-Start call under each revision had a modeled
  graphics tile range ending in the BIOS window at `$E0E7` and was skipped rather than modeling
  BIOS-sourced data. The ordered LCD writes and cartridge read addresses, allowed RAM-write
  region and final `$300-$307` record cache, and selected peripheral and final LCD states were
  subsequently checked across the same runs with zero mismatches among the same 28,516 eligible
  calls. Inputs included purpose-written RAM
  graphics and temporary synthetic overlays in the emulator's private cartridge copy, never the
  source files. Controlled decimal-mode comparisons did not follow the binary-mode model.
  These are emulator observations, not physical measurements or a complete ABI: individual bus-event
  timing, then-unmodeled scratch values, decimal mode, side-effectful source aliases and interrupt
  interference remain open. No BIOS instructions or external emulator implementations were copied
  or reconstructed; no commercial BIOS, ROM or graphics payload is retained or published.
- Scratch and timing follow-up: the two local, legally obtained BIOS revisions and the read-only
  local cartridge collection were executed in this project's emulator. The diagnostic checker
  tracks scratch bytes written during each authentic `$F027` call, then cartridge-code accesses
  before an intervening write until the next entry or campaign end. Synthetic tests exercise owner
  attribution, write-before-read exclusion, indexed-store dummy reads and read-modify-writes.
  Four 1,000-frame campaigns across 62 cartridges and both BIOSes observed 28,482 returns, 19,846
  interrupt-spanning returns and 34 in-flight calls. Among 24 initially flagged combinations,
  targeted replays identified indexed-store dummy reads and one Tough Guy controller-cycle
  window per BIOS with real read-modify-writes of `$39C-$3A3`. Last-writer and return-state
  comparisons attribute these eight bytes to non-interrupt BIOS writes and match them to the
  final large object's plane-1 output buffer. Synthetic object tests under both BIOSes establish
  the pre-draw and post-compositing buffer layout for both object sizes; combined cases validate
  overlapping sprites. A fresh replay of all 496 cartridge/BIOS/campaign combinations checked
  modeled sprite-buffer slots in 28,516 eligible calls with zero mismatches; the same two
  BIOS-window source cases were skipped in that cartridge-only run. Sixteen disabled-table
  cases per BIOS found no writes to `$000-$003`,
  `$018-$01F` or `$308-$3C7`. The caller scan cannot rule out other paths, consumers outside the
  cartridge window, nested-interrupt attribution issues, or reads after a later `$F027` entry.
  An opt-in LCD diagnostic records only register numbers and instruction-boundary clock offsets.
  Isolated first-event checks cover all 16 records and both operation kinds under each BIOS;
  they do not establish later event timing or physical bus timing. Isolated RAM-watch probes
  counted one `$015` read per redraw, three per large sprite and two per small sprite. Synthetic
  tests changed `$015` after the first sprite-control write and observed the changed value in
  later control writes for both sprite sizes and BIOSes, establishing a live dependency without
  assuming exact read timing. RAM watches counted repeated graphics-base reads in isolated
  redraw and sprite cases; changing the plane-0 base after the first synthetic redraw write
  changed the next tile's source under both BIOSes. This establishes another live dependency,
  not its complete read schedule. Enabling a second synthetic sprite record after the first
  sprite's first VRAM write caused both BIOSes to draw that later sprite. Paired first-call probes in
  Tough Guy, Time Warrior and Box Forum compared forced binary-mode I-clear and I-set statuses
  with IRQ phase reset at entry: one or two IRQs occurred with I clear, with unchanged final VRAM
  hashes and write counts but changed whole-RAM hashes. Pipemania and GP Race were no-IRQ controls.
  Both revisions were tested. These few emulator runs do not establish general interrupt behavior.
- BIOS-window source follow-up: the same Mini Golf long-Start call previously excluded from the
  cartridge-only replay was compared under both BIOSes using their bytes privately in memory as
  graphics data. All 65 calls per revision matched the model. Replacing BIOS-window graphics data
  with zero only in a separate in-memory model changed the excluded call's predicted final VRAM
  under each revision. This establishes a source-content sensitivity in the tested emulator
  context, not a requirement to distribute or reproduce firmware data. The local BIOS and ROM
  files remained read-only; no BIOS instructions, cartridge code, graphics payloads or external
  emulator implementations were copied, reconstructed, retained or published. These are emulator
  observations, not physical measurements; decimal mode, side-effectful source aliases, full
  interrupt interference and intermediate scratch state remain open.
- Scratch-state follow-up: both local, legally obtained BIOS revisions were executed in this
  project's emulator using the read-only local collection and purpose-written synthetic graphics.
  Isolated redraws with varied map selectors established a column-major selector cache at
  `$308` for each record size. An independent final-state model now includes this cache, both
  sprite buffers, and `$000-$003` and `$018-$01F`. It compares values and write sets throughout
  those ranges, including varied initial values and preserved slots, across 256 overlapping-record
  and 2,052 selector/sentinel cases per BIOS; 1,024 isolated redraw cases per BIOS also checked
  selector ordering. All passed. The model assumes uninterrupted binary-mode execution, stable
  inputs and safe sources; it does not predict intermediate scratch values or access times,
  live mutation, or arbitrary interrupt effects. These are emulator experiments, not physical
  measurements. Sixteen disabled-table cases per BIOS also confirmed writes to all eight
  `$300-$307` record-cache bytes. In four 1,000-frame campaigns across 62 cartridges and both
  BIOSes, all 28,518 masked replays matched the expanded model without skips or failures. The
  two Mini Golf calls using BIOS-window graphics were included by private in-memory reads, and
  each differed from an alternate zero-source model. Local BIOS and cartridge files were read
  only; no firmware instructions, commercial data or external implementation were copied into
  the project, reconstructed, retained or published. The derived contract and
  further qualifications are in
  [`BIOS_RUNTIME_API_ANALYSIS.md`](BIOS_RUNTIME_API_ANALYSIS.md).
- Stack-write follow-up: in the same emulator and with both local BIOS revisions, 516 synthetic
  cases per BIOS checked stack write-set coverage without recording stack contents. They varied
  operation size, record, position, selector and four entry stack pointers, including wrap. An
  empty call wrote nine consecutive stack slots; isolated redraws wrote 11, large objects 13 and
  small objects 18. The maximum applicable span also matched 256 multi-record cases per BIOS.
  Across 62 cartridges and both BIOSes, all 7,196 masked replays in the 1,000-frame no-input
  campaign matched that span without skips or failures. These are uninterrupted emulator
  observations, not physical measurements, a stack-content model
  or a prediction of interrupt-handler writes. Inputs were read only, synthetic graphics were
  purpose-written, and no commercial data or BIOS instructions were retained or copied.
- Paired interrupt follow-up: the same read-only collection and both legally obtained BIOS
  revisions were executed in this project's emulator for four 1,000-frame campaigns across
  62 cartridges. At each binary-mode entry, two machine copies preserved the authentic IRQ
  phase; one retained its original interrupt state and the other suppressed IRQs. All 28,518
  pairs returned without decimal or pending-NMI skips or timeouts. Of these, 19,880 original
  copies took IRQs and none took NMIs; every IRQ-spanning pair matched in final VRAM, modeled
  scratch, tracked inputs, registers apart from I, LCD register state and mapper registers, but
  differed in other non-stack RAM. The 8,638 quiet pairs matched in those categories and elapsed
  clocks. A synthetic self-test exercises the categories and stack/I exclusions. These comparisons
  do not cover full AY/UART state, event-by-event timing, injected NMIs, arbitrary interrupt
  handlers or physical hardware, and do not prove general interrupt safety. No commercial data
  or BIOS instructions were copied, reconstructed, retained or published.

#### `$F00C` two-plane VRAM zero-writer

- Executable-model follow-up: both authentic BIOS revisions and the local user-supplied
  cartridges were read in place and executed by this project's independently authored emulator.
  `tests/gamatebiosf00c.c`, linked only to the diagnostic library, checks the ordered LCD writes,
  scratch writes and reads, returned registers/status, cartridge-read addresses, peripheral state,
  and elapsed clocks. The four standard 1,000-frame campaigns covered all 62 cartridges: 1,596
  calls in 61 titles (798 per BIOS) matched the masked model, and 1,572 original-interrupt-state
  replays spanned an IRQ or NMI. Two previously completed no-input jobs were included without
  rerunning them. A further no-input pass checked every modeled LCD event time and final VRAM
  byte for 258 calls (129 per BIOS). Both BIOSes also passed 768 synthetic cases covering all
  cached-control bytes, all entry-status bytes at both output signs, varied entry registers,
  stack positions, VRAM fills and scroll positions. IRQ delivery was suppressed for the
  status-byte cases; authentic interrupt-state replays were checked separately.
  Controlled host mutations between the two `$015` reads and during the second plane's `$01E`
  reads confirmed those live dependencies. Paired first-call snapshots across 61 titles found
  four BIOS-pair RAM differences, each in a pre-existing, unchanged stack byte, with no new RAM
  differences caused by the calls. In the emulator, the previously observed bit-6 sparse writes
  follow from the LCD's row-increment mode applied to the unchanged BIOS port sequence.
  The contract and instruction-boundary timing are in
  [`BIOS_RUNTIME_API_ANALYSIS.md`](BIOS_RUNTIME_API_ANALYSIS.md).
  The SWK implementation now uses a direct Y-register LCD row write and relies on the final row
  comparison's carry, matching the 247,326-master-clock uninterrupted duration without padding;
  its synthetic test locks that duration in both contiguous and row-increment control modes.
  These are emulator observations, not physical measurements or claims of exact hardware bus
  edges. Synthetic mutations changed only in-memory state, and reports retained derived counts
  and equality results rather than firmware, cartridge, graphics, or stack payload. The input
  files remained read-only; no BIOS instructions, cartridge code, or external emulator
  implementation were copied or reconstructed.

- `$F00C` follow-up: `occurrence=1` probes covered 57 titles under both BIOSes with I set and VRAM
  seeded to `$FF`; controlled `$015` bit patterns and A/X/Y changes were tested in Baseball, and the
  bit-6 path was checked in Dino Ball and Volcano Panic. Time Warrior's authentic second occurrence
  was separately probed under both revisions. The tests found a 12,800-write two-plane zeroing path,
  LCD-control effects from `$015`, and a distinct sparse write pattern when bit 6 is set. Four
  paired whole-RAM hashes differed despite matching checked access sets and all other reported
  outputs; the follow-up above localizes these to pre-existing stack differences. The probe streamed
  VRAM values into aggregate counters and retained no payload. A Baseball matrix of combined `$015`
  values `$00`, `$3F`, `$40`, `$41`, `$50`, `$60`, `$C0`, `$7F`, and `$FF` was expanded to a full
  `$00-$FF` sweep under both BIOSes at the first Baseball occurrence (512 probes). All values
  matched the additive LCD-flag mapping,
  returned-A rule, status/register signature, timing, write counts, final pointer, and the expected
  contiguous or sparse VRAM coverage; there were no cross-revision non-hash field or coverage
  mismatches. Whole-RAM hashes also matched in this matrix. The follow-up above explains the
  bit-6 stride in the emulator; physical-hardware behavior remains open. The probe retained only
  derived results, not VRAM or ROM payload. Exact scopes and limits are in
  [`BIOS_RUNTIME_API_ANALYSIS.md`](BIOS_RUNTIME_API_ANALYSIS.md).

#### `$F01B` ROM-driven graphics writer

The two historical entries below are superseded in part by the
[executable model entry](#f01b-row-run-stream-executable-model) that follows. They are retained as a
record of probes that were actually run. Their LCD-flag "mapping", "sparse addressing mode", and
unresolved stream format are corrected there.

- `$F01B` follow-up: the no-input 1,000-frame inventory records 18 entries per BIOS across 12
  titles. The paired balanced-call corpus contains 34 calls, 17 per BIOS across 12 titles, with 32
  calls spanning IRQs. Every paired report row matches across BIOS revisions; all calls return
  A=`$FE`, X=`$00`, and preserve S. I-masked probes in Baseball, Brick Blaster, Magic Jigsaw,
  Witty Apee, and later Devil Castle/Witty Apee calls covered input-register, RAM, `$015`,
  VRAM-pointer, LCD, and scroll perturbations. The ordinary-mode coverage was `$1CC0-$1FDF` in
  those contexts, but a broader matrix found a Cosmic Fighter exception. The observed status
  relation is `P_out = (P_in & $0C) | $23`; `$000-$001` format, ROM-source mapping, return-Y rule,
  and timing model remain unresolved. ROM traces retained addresses only; no values or payload
  were retained. An exhaustive 256-value `$015` matrix in Baseball under both BIOSes produced 512
  probes with no mismatch. The same mapping was tested at the first authentic call in every title
  where `$F01B` was observed in four existing 1,000-frame campaigns over 62 titles and both BIOSes:
  22 titles and 11,264 probes. All probes returned. Within each title and addressing mode, checked
  return/status, timing, write counts, VRAM hashes and geometry, and ROM-read coverage stayed
  constant across the byte sweep. Paired BIOS outputs agreed, excluding whole-RAM hashes. The LCD
  mapping `(bit 0->$20, bit 4->$08, bit 5->$04, bit 6->$02, bit 7->$10)` held in every context.
  Bits 1-3 had no effect on checked return, timing, ROM-read, or VRAM results; bits 0, 4, 5, and 7
  changed only flags; bit 6 selected the same 56 `$20`-spaced addresses in all 22 contexts.
  Twenty-one ordinary-mode contexts cover 800 addresses at `$1CC0-$1FDF`; Cosmic Fighter covers
  820 at `$1CC0-$1FF3`. In Cosmic Fighter the ordinary-mode call took 42,708 master clocks and
  issued 1,060 VRAM and 1,137 LCD writes; bit-6 mode retained those counts and timing with sparse
  coverage. The probe now shares the frame-relative input schedule helper with `gamatebioscalls`;
  its 4-in-1 call source/frame matched the existing controller-cycle report. Both diagnostic
  targets built successfully; production behavior and performance are unaffected. Selected
  single-byte XORs in a private ROM copy yielded no output change at some offsets and localized or
  broad changes at others, sometimes with expanded writes and timing. This did not resolve the
  source format; no BIOS/ROM or graphics payload was retained. These are local emulator
  observations, not physical measurements. The matrix covers only the first authentic call per
  observed title; later invocation contexts and the physical meaning of the sparse mode remain
  open. The detailed methods, results, and limits are in
  [`BIOS_RUNTIME_API_ANALYSIS.md`](BIOS_RUNTIME_API_ANALYSIS.md).
- `$000-$001` follow-up: the four existing 1,000-frame campaign reports selected the first
  authentic `$F01B` call in all 22 observed titles, under both BIOS revisions. A baseline and
  separate XOR-1 perturbations to `$000` and `$001`, with I set and the campaign's frame-relative
  input schedule, produced 132 probes; all returned. Both bytes were read before write in all 44
  contexts; `$000` was never written and `$001` was written in 32. Each perturbation changed the
  address-only ROM-read sequence, timing, VRAM hash/coverage, and VRAM/LCD write counts in all 44
  contexts. Returned registers changed in all `$000` probes and 8 `$001` probes. Clock deltas
  ranged from -57,012 to +30,255,628 for `$000` and -29,002 to +601,432 for `$001`. Baselines
  matched across all 22 paired BIOS contexts in checked output signatures, captured sequence
  prefixes, and total read counts; whole-RAM hashes and per-address RAM-write summaries were
  excluded. `$001` perturbations matched in 22 pairs, while `$000` comparisons matched in 20.
  Two `$000` traces exceeded the diagnostic capture limit, so full sequence equality is not
  claimed for them. These observations establish sensitivity, not a pointer or descriptor format;
  the two `$000-$001` roles and source-address transformation remain unresolved. The independently
  authored, diagnostic-only probe retained only derived results and address-only traces; no
  payload or external emulator code was copied. This is emulator evidence, not physical
  measurement.

#### `$F01B` row-run stream executable model

- **Source and status:**
  - Inputs: the local user-supplied collection (all 62 cartridges, 23 of which call `$F01B` in the
    standard campaigns) and both authentic BIOS revisions, read in place.
  - Status: execution observations from the independently authored emulator, not physical
    measurements.
  - Supporting reference: published NMOS 6502 decimal-mode arithmetic, used only to write the model
    of D-set row addition. The model was checked against the emulator's own CPU and was not taken
    from any emulator implementation.
- **Method:**
  - Exploratory probes:
    - `gamatebiosprobe` XORs to `$000-$001` shifted the first cartridge read by exactly the
      perturbed pointer bits.
    - Address-run analysis found sequential reads with indexed-indirect page-crossing dummy reads.
    - Synthetic RAM streams with exact LCD port logs exposed the row/escape/value/count grammar.
  - Instrumentation: a diagnostic-only LCD port-write callback (`gamateSetLcdDiagnosticWatch`,
    compiled only with `GAMATE_ACCESS_DIAGNOSTICS`) was added to expose the register/value
    sequence written to `$5000-$5007`.
  - Checker: `tests/gamatebiosrowstream.c` (CMake target `gamatebiosrowstream`, service `F01B`;
    linked only against the diagnostics library) is an independently written model. It checks:
    - the complete LCD port-write sequence;
    - A/X/Y/S and the status register;
    - RAM writes below `$400` outside the stack page;
    - AY writes, bank writes, UART clocks, and UART transmit state;
    - exact master-clock duration.
  - Corpus mode: the four standard 1,000-frame input campaigns were run over all 62 cartridges and
    both BIOSes, 496 runs on three parallel workers. At each `$F01B` entry, the checker:
    - replays an I-masked copy;
    - replays a second copy with the authentic interrupt state.
  - Synthetic mode: the checker snapshots one authentic entry and replays batched streams:
    - in RAM;
    - in the checker's private in-memory ROM copy, restored after each case;
    - with random entry registers and flags;
    - with all classes of `$015` bits;
    - with D set.
- **Result:**
  - Authentic corpus: all 242 calls, 121 per BIOS in 23 titles, matched the model with zero
    mismatches, including exact timing. All streams were in cartridge ROM and used up to 2 Y wraps.
  - Unmasked replays: all calls entered with I clear, and 224 spanned interrupts. Every call
    produced the same port sequence, registers, and status, with only handler time added (at most
    1,628 master clocks).
  - Synthetic cases: all 960 cases matched under both BIOS revisions. These included up to 4 Y
    wraps, every `$015` bit class, and D-set entry.
  - Stack use: the service uses at most 2 stack bytes below entry S. This was measured after the
    checker was generalized; see the `$F021` entry below.
  - Both BIOS revisions behaved identically.
  - The full contract is in
    [`BIOS_RUNTIME_API_ANALYSIS.md`](BIOS_RUNTIME_API_ANALYSIS.md). It covers:
    - the `$000-$001` stream pointer;
    - the fixed 25-row clear;
    - the absolute-row/escape/run grammar;
    - the verbatim `$015` register-1 write;
    - the returns and the `$001`/`$01A` side effects;
    - the status formula with V;
    - the exact clock formula.
- **Corrections:**
  - The earlier `$015` "flag mapping" was the probe's re-encoding of the emulator's decoded
    register-1 fields; the BIOS writes the byte verbatim. Bits 1-3 had no effect only because the
    emulator does not model them.
  - Bit-6 "sparse mode" is the LCD increment-by-row setting applied to an unchanged port
    sequence.
  - Cosmic Fighter's larger extent comes from stream content.
  - The `$000-$001` perturbation results follow from pointer redirection.
  - The earlier status relation is the V-clear case.
  - The earlier 22-title count missed Incantational Couple.
- **Limits and legal boundary:**
  - Timing is the emulator's validated CPU/bus model.
  - D-set behavior is outside all authentic calls and inherits the emulator's decimal arithmetic.
  - Non-terminating streams are not part of the contract.
  - The model was derived from port logs, address patterns, counts, timing, and equality
    comparisons. No BIOS instructions were reconstructed.
  - Only derived counts and purpose-written synthetic streams were retained. No BIOS, ROM, stream,
    RAM, or VRAM payload was retained or published.
  - BIOS and ROM files remained read-only.
  - No external emulator source was consulted or copied.
  - The diagnostic callback compiles out of production builds and is not part of save states.

#### `$F01E` two-plane graphics service

- Clean-room executable-model follow-up: both authentic BIOS revisions and the local
  user-supplied cartridges were read in place and executed by this project's independently
  authored emulator; this is not physical-hardware evidence. `tests/gamatebiostilestream.c`,
  linked only to the diagnostic library, replays entry snapshots and compares ordered cartridge
  addresses, LCD port writes, register/status results, non-stack RAM coverage and values, selected
  peripheral state, and elapsed clocks. The model extends the earlier `$F018` tile-stream model,
  using purpose-written single-selector streams and patterned writable tile sources. The
  previously documented Visual6502 and Bruce Clark decimal references inform its decimal
  arithmetic. Both BIOS revisions were checked against synthetic boundary and status cases,
  transparent-column arithmetic cases, and seeded varied streams. No BIOS instructions,
  cartridge code, or external emulator implementation were copied or reconstructed. Input files
  remained read-only; synthetic ROM modifications use only a private in-memory copy, and reports
  retain derived results rather than firmware, cartridge, or graphics payload.
  The four standard 1,000-frame campaigns covered 62 cartridges and both BIOSes: all 808 calls
  in 25 titles (404 per BIOS) matched the masked model, and 736 original-interrupt-state replays
  spanned interrupts. Four already completed no-input jobs were included without rerunning them.
  Each BIOS passed 838 boundary cases, 1,280 entry-status cases, 1,024 transparent-column cases,
  and 1,200 varied streams in Time Warrior. Paired first-call snapshots in Famous 7, Time Warrior,
  Volcano Panic, and Robin Hood localized the earlier whole-RAM differences to one or two
  pre-existing, unchanged stack bytes; no new RAM differences appeared during the calls.
  The resulting rules and limitations are recorded in
  [`BIOS_RUNTIME_API_ANALYSIS.md`](BIOS_RUNTIME_API_ANALYSIS.md).

- `$F01E` follow-up: the paired balanced-call corpus contains 216 calls across 18 titles. First-call
  probes in Famous 7, Time Warrior, Volcano Panic, and Robin Hood were compared under both BIOSes
  with I set; register changes, RAM XOR perturbations, and address-only ROM coverage were used to
  bound the caller inputs. In Time Warrior, `$000-$001` select a ROM-side control/descriptor region,
  `$006-$007` and `$008-$009` feed separate plane outputs, and `$015` affects LCD flags. VRAM output
  was streamed to per-plane hashes without retaining payload. The stream grammar, return-Y rule,
  status behavior, and BIOS-specific whole-RAM hash differences were unresolved at that stage.
  The follow-up above addresses them for the tested domains. Exact results and limits are in
  [`BIOS_RUNTIME_API_ANALYSIS.md`](BIOS_RUNTIME_API_ANALYSIS.md).
- `$F01E` cross-corpus source-byte matrix: the four existing 1,000-frame campaign reports supplied
  first authentic call contexts for 24 titles under both BIOS revisions. I-masked baselines and
  separate XOR-1 perturbations to `$006-$009` produced 240 probes; all returned. Each target byte
  was read before write in all 48 contexts and never written. `$006` and `$007` changed only the
  plane-0 output hash in 46 contexts each; `$008` and `$009` changed only plane 1 in 46 contexts
  each. Coverage, write counts, and return registers
  remained unchanged. Address-sequence summaries changed in 46 contexts per byte. Timing changed
  for `$006` in 42 contexts and `$008` in 40, by -8 to +36 and -10 to +36 master clocks,
  respectively; `$007` and `$009` did not change timing. Checked baseline and perturbation
  signatures, per-plane hashes/coverage, access maps, and captured address-sequence summaries
  matched across all 24 paired BIOS contexts, excluding whole-RAM hashes and changed-byte
  summaries. Two sequence comparisons per byte included a trace over the capture limit; full
  equality is not claimed for those traces. VRAM values were consumed directly into per-plane
  hashes and counts and were not retained. This extends the two-plane input mapping beyond Time
  Warrior but does not establish a general pointer formula or stream grammar.
  The evidence is from the independently authored emulator, not physical hardware; no external
  emulator code was consulted or copied.
- `$F01E` control-byte follow-up: the same 24 first-call contexts were probed under both BIOSes
  with I set, using baselines and separate XOR-1 perturbations to `$000` and `$001` (144 probes;
  all returned). Each byte was read before write and rewritten in all 48 contexts. Both
  perturbations changed ROM-address-sequence summaries and timing in all contexts. `$000` changed
  both plane hashes and coverage in 44 contexts, write counts in 32, and returned registers in 48;
  `$001` changed both plane hashes, coverage, and write counts in all 48, and returned registers in
  28. Clock deltas were -453,616 to +1,084 for `$000` and -412,844 to +47,848,702 for `$001`.
  Baseline checked outputs matched across all 24 BIOS pairs; perturbed `$000` comparisons matched
  in 24, and `$001` in 23. Whole-RAM hashes and changed-byte summaries were excluded. Two `$000`
  and 14 `$001` address-sequence comparisons involved traces over the diagnostic capture limit.
  The tests establish input sensitivity and service rewrites, not pointer or descriptor semantics;
  no payload was retained. The source is the project-authored diagnostic run over the existing
  1,000-frame campaign contexts and read-only local BIOS/ROM inputs, not physical measurement.

#### `$F021` ROM-backed graphics service

The historical entry below is superseded in part by the
[executable model entry](#f021-row-literal-stream-executable-model) that follows. Its LCD-flag and
sparse-mode interpretations, and its unresolved format, are corrected there.

- `$F021` follow-up: the paired balanced-call corpus contains 92 calls, 46 per BIOS across 14
  titles; 66 calls span IRQs. Every call returns A=`$FE`, X=`$00`, and preserves S, while Y varies.
  Paired summaries match except for one IRQ-spanning Volcano Panic call from `$AF24` at frame 350,
  whose changed-RAM count is 12 versus 13; the difference is not attributed to the service.
  First-call probes in Volcano Panic and Famous 7 were isolated with I set and compared under both
  BIOSes. Entry registers, VRAM pointers, and scroll values were perturbed; `$000-$001` received
  bitwise XOR perturbations; `$015` was tested with controlled values. Address-only ROM traces and
  derived VRAM/LCD write summaries were compared without retaining payload. The probes write plane
  0 only, preserve `$000-$001` and `$015`, and show that the former affects ROM-read/output behavior
  while the latter affects LCD flags and selects a sparse addressing mode when bit 6 is set. The
  `$000-$001` format, ROM address transformation, selector mapping, return-Y rule, status contract,
  and exact timing remain unresolved. The direct probes' access maps and reported summaries match
  across BIOSes, but their whole-RAM hashes differ for unresolved reasons. Methods, measurements,
  and limits are in
  [`BIOS_RUNTIME_API_ANALYSIS.md`](BIOS_RUNTIME_API_ANALYSIS.md).

#### `$F021` row-literal stream executable model

- **Source and status:**
  - Inputs: the local user-supplied collection (all 62 cartridges, 19 of which call `$F021` in the
    standard campaigns) and both authentic BIOS revisions, read in place.
  - Status: execution observations from the independently authored emulator, not physical
    measurements.
- **Method:**
  - Hypothesis: the `$F01B` contract and the shared signature of both services suggested a sibling
    grammar.
  - Exploration: synthetic RAM streams with exact LCD port logs, captured through the diagnostic-only
    LCD write callback, showed the same preamble and row/escape/terminator handling. They also
    showed that data bytes are written once rather than decoded as value/count pairs.
  - Checker: the `$F01B` checker was generalized into `tests/gamatebiosrowstream.c` (CMake target
    `gamatebiosrowstream`, diagnostics library only). Service argument `F021` selects the
    literal-byte model.
  - Additional measurement: the checker now records stack depth below entry S for both services.
  - Validation runs:
    - the standard four-campaign, 1,000-frame corpus over all 62 cartridges and both BIOSes, using
      I-masked and authentic-interrupt replays;
    - batched synthetic streams in RAM and in the private in-memory ROM copy, with random entry
      registers, `$015` values, and D-set entry.
- **Result:**
  - Authentic corpus: all 406 calls, 203 per BIOS in 19 titles, matched with zero mismatches,
    including exact timing.
  - Unmasked replays: all calls entered with I clear, and 280 spanned interrupts. Every call
    produced the same port sequence, registers, and status, with only handler time added.
  - Synthetic cases: all 450 cases matched under both BIOSes.
  - Stack use: both row-stream services use at most 2 stack bytes below entry S.
  - The contract, including the literal timing term of 66 master clocks per byte and the absence of
    an `$01A` write, is in
    [`BIOS_RUNTIME_API_ANALYSIS.md`](BIOS_RUNTIME_API_ANALYSIS.md).
- **Corrections:**
  - The earlier `$015` flag mapping and the bit-6 "sparse addressing" interpretation were probe
    artifacts. `$015` is written verbatim to LCD register 1.
  - The `$000-$001` XOR results follow from pointer redirection.
  - The earlier inventory missed five caller titles.
- **Limits and legal boundary:**
  - Timing reflects the emulator's validated CPU and bus model.
  - D-set behavior lies outside all authentic calls.
  - Non-terminating streams are not part of the contract.
  - The model was derived from port logs, counts, timing, and equality comparisons. No BIOS
    instructions were reconstructed.
  - No BIOS, ROM, stream, RAM, or VRAM payload was retained or published.
  - BIOS and ROM files remained read-only.
  - No external emulator source was consulted or copied.

#### Shared clean-room boundary

- Clean-room boundary: BIOS and cartridge files remained read-only. No BIOS/ROM bytes, listings,
  decoded graphics, RAM/VRAM payloads, or ROM-derived mapping tables were retained or published;
  only behavioral descriptions and derived addresses, counts, and comparisons were used.

## Frontend integration and SWK validation

- Replacement BIOS discovery (project design decision, user-directed): the independent firmware's
  public filename and optional build output are `gamate_bios_swk.bin`. The libretro adapter retains
  UMC priority by default, tries the other authentic BIOS before SWK, and permits explicit SWK
  priority with UMC then Bit fallback. Invalid values use UMC priority; reset retains the loaded
  BIOS and content reload applies preference changes. Each candidate must be exactly 4 KiB.
  Loading SWK produces an experimental-firmware notice, not an assertion of authenticity.
  Purpose-written `gamatecoretest` firmware validates modern/legacy option values, all selection
  orders, SWK-only fallback, missing/incorrect-size rejection, reset/reload and intro skipping;
  no commercial BIOS or cartridge bytes are used or copied. This is frontend discovery policy,
  not hardware evidence or validation of interactive Cube Up gameplay. Unsupported prototype
  service/NMI traps remain firmware halts rather than frontend error notifications.
- SWK interactive Cube Up check (local experiment, RetroArch plus emulator diagnostics): a user
  session under SWK reached the setup screen with plausible audio and video but could not start
  play. A temporary, uncommitted harness then logged only per-frame controller-read counts, poll
  PCs and end-of-frame PCs; it retained no ROM or BIOS bytes. Under UMC and SWK, the setup screen
  polled the controller at the same cadence, about every 21-22 frames. A 12-frame scripted Up
  press was missed only because SWK's poll fell one frame earlier, so it was not an input fault.
  Holding each button for 30 frames gave identical poll-site responses under both BIOSes except A.
  Under UMC, A starts play. Under SWK, A halts in the `$F036` unsupported-service trap within a few
  frames. `gamatebioscalls` under UMC shows that starting play calls `$F030`, `$F00C`, `$F018`,
  `$F01B`, `$F012`, `$F033`, `$F024` and `$F027`, then `$F02A`, `$F02D` and `$F003`. Seven of
  these slots are unimplemented in SWK. The earlier four 1,000-frame campaigns never entered
  gameplay, so their equal per-slot counts covered only the title and setup flow.
- SWK gameplay services (independently authored firmware; local emulator experiments):
  - **Implementation basis.** `$F003`, `$F018`, `$F01B`, `$F027`, `$F02D`, `$F030` and `$F033`
    were written from the executable contracts in `docs/bios/BIOS_RUNTIME_API_ANALYSIS.md`. Those
    contracts derive from observed register, memory, VRAM and peripheral behavior. No authentic BIOS
    instruction sequence, table or graphic was read, translated or reproduced. MAME was not
    consulted for this step. `$F006`, `$F00F`, and `$F015` were added later from their independently
    derived contracts in the same document.
  - **Iteration-informed implementation process.** Synthetic tests and differential runs were used
    as feedback, not as a substitute for the written contracts. The implementation was never
    generated or repaired by reading or reconstructing authentic BIOS instructions:
    - `$F006` initially retained an unnecessary ready-path zero/load instruction sequence. Timing
      measurements showed 50 instead of 42 master clocks; removing that sequence yielded the
      complete threshold and exact ready-path contract.
    - `$F00F` began with a per-column/per-row LCD address setup that wrote outside the intended
      rectangle and gave an incorrect timing profile. The implementation was reduced to the
      measured row/31-column geometry and exact 15,930-clock result. Its final code uses neutral
      NOP padding for the single ready path only as an implementation convenience; the observable
      contract, not the padding, is the design authority, and a loop or equivalent output can
      replace it later if it produces identical peripheral and timing behavior.
    - `$F015` initially used `$02-$03` as copy-loop scratch. Synthetic mismatch checks showed that
      this aliases two caller source-pointer fields and corrupts plane-1 setup. The implementation
      moved its pointer scratch to `$01E-$01F`, preserved `$000-$003`, and retained entry X for
      the normal JSR return. Its first geometry pass also had to be corrected for source data
      advancement and plane switching; the synthetic suite now checks 1×8, 2×8, 3×8, 4×8 and
      page-crossing shapes. Timing remains deliberately provisional.
    - `$F021` was initially implemented as a single row/literal loop and an initial synthetic stream
      initially used `$FE` in the wrong grammar position. The test was corrected: `$FE` terminates
      only as a row offset or following an escape, while `$FE` as literal data is written. Synthetic
      RAM/ROM streams and the independent row-stream checker confirmed port, register, status, RAM
      and peripheral behavior. Exact timing remains deliberately separate from this initial
      implementation.
    - `$F018` tile-address handling was initially considered with PHP/CLD/ADC/PLP-style arithmetic.
      Emulator observations showed that preserving the stored P/B-bit convention requires a
      different implementation. The final `point_tile` routine uses independent high-byte increment
      logic and does not restore P through PLP, so I/D/V/B and unrelated entry flags are not
      accidentally rewritten.
    - `$F027` was likewise driven by synthetic reference models of the executable record and VRAM
      contract, then checked against the independent corpus. No sprite, background, glyph or
      cartridge image was copied into the firmware. `$F000` remains a deliberately blank
      independently authored placeholder.

    Temporary diagnostic copies and synthetic ROM inputs never exposed commercial BIOS or cartridge
    payloads in source files or retained reports. Reports retained derived counts, hashes, addresses
    and register state. Discarded implementation attempts were not promoted to research conclusions
    unless a measured behavior demonstrated them. These development iterations reduce the likelihood
    of accidental reconstruction: they explain why tests, observed boundaries and public contracts
    control the implementation, rather than authentic instruction text or an external emulator
    implementation.
  - **Synthetic tests.** New cases in `tests/gamatebiosprototypetest.c` compare each service against
    purpose-written reference models of the same contracts. The models use synthetic data only.
    Mutation checks showed the suite fails when the `$F027` mask or the `$F01B` row base is
    broken.
  - **Corpus comparisons.** The existing `gamatebiosf027`, `gamatebiosf018` and `gamatebiosf01b`
    checkers ran against SWK on locally supplied, read-only Cube Up gameplay. They matched every
    non-timing property:
    - `$F027`: 141 of 141 calls.
    - `$F018`: the corpus, plus 1,373 selftest, 1,280 open-bus and 131,072 flag cases.
    - `$F01B`: the corpus, plus 772 synthetic cases.

    Temporary, uncommitted copies of the `$F030` and `$F033` checkers, with timing and stack-slot
    checks removed, matched 138,688 and 526,848 calls respectively.
  - **Differential runs.** `gamatebioscalls`, `gamateboot` and a temporary per-frame hash harness
    compared UMC, Bit and SWK:
    - Over 1,000 frames with gameplay, all three made identical per-slot calls and had equal
      frame-1,000 video hashes.
    - Over 1,600 frames, all three showed the same 31 settled gameplay screens. SWK produced more
      transient redraw frames: 339 against 225.
    - Over 8,000 frames, SWK called only implemented slots.
  - **Input-phase sensitivity.** The 8,000-frame run diverged at frame 1,506. Delaying the UMC
    run's later scripted inputs by one frame reproduced SWK's divergent call frame. This identifies
    input-phase sensitivity to service timing rather than a data mismatch.
  - **`$F006` Bao Qing Tian validation.** After implementing `$F006` from the existing
    byte-domain contract, a 1,000-frame run of Bao Qing Tian completed 261 `$F006` calls under SWK,
    matching UMC and Bit's call counts. The synthetic suite tested all 256 entry-counter values,
    representative D/I/V/P variants, the exact 42-master-clock ready path, and the wait count
    (`5 - n` IRQs for `$00-$04`). UMC and Bit agreed on all 261 observed call contexts in the
    trace fields compared here. SWK did not enter an unsupported trap. Frame-1,000 VRAM hashes
    matched UMC and Bit; audio hashes differed, as they did in prior BIOS comparisons.
  - **`$F00F` fixed rectangle validation.** After implementing `$F00F` from the resolved
    775-byte plane-0 contract, the synthetic suite matched the 25-row by 31-column coverage,
    exact 15,930-master-clock timing, 775 VRAM writes, 829 LCD writes, final `$1FFE` pointer,
    plane-0 and row-increment state, scratch values, and status/register contract across
    representative LCD-control and P values.
    Baseball, a 1,000-frame title that uses only `$F00F` beyond the implemented slots, called
    `$F00F` once under UMC, Bit and SWK. UMC and Bit each took 15,930 master clocks; SWK matched
    timing and completed the call without a trap. Whole-run timing and RAM differences shifted
    the entry frame in SWK and produced a different frame-1,000 video hash; no physical or
    complete-gameplay result is claimed.
  - **Initial `$F015` implementation validation.** Synthetic 1×8, 2×8, 3×8, 4×8 and page-crossing
    two-plane copies match the resolved copy geometry, two source pointers, final byte, width/height
    return values, scratch roles and unrelated hardware model. Tennis, which contains a tail-JMP
    entry form for `$F015`, completed 73 `$F015` calls under each of UMC, Bit and SWK in a
    1,000-frame run, with frame-1,000 video identical across all three. Initial timing remains
    slower than the authentic baseline; exact page-cross and timing formulas are not claimed by
    this implementation.
  - **Initial `$F021` implementation validation.** A synthetic RAM stream and ROM stream exercised
    two row commands, multiple literal bytes, `$FF` escape handling, and `$FE` termination. The
    independent `gamatebiosrowstream` model matched VRAM/LCD port, returned-register, status, RAM,
    and unrelated-peripheral behavior; only the deliberately unpadded timing differed. Volcano
    Panic completed 25 `$F021` calls under UMC, Bit and SWK in a 1,000-frame run. All three call
    counts matched, while the SWK frame-1,000 video diverged; this run does not establish
    frame-identical gameplay or exact `$F021` timing.
  - **Initial NMI and `$6023` implementation validation.** The independent firmware now installs an
    NMI vector wrapper that reads `$4800` and stores the completed byte at RAM `$00A`, dispatches
    `$6023` through `$000C`, restores A, and uses RTI to restore P/PC/S. Synthetic callback tests
    used two received bytes (`$A5` and `$3C`) with callback enable on and off. With dispatch enabled,
    a purpose-written `$6023` code sequence incremented RAM `$08`; disabled dispatch left it unchanged.
    Entry A/X/Y/P/PC/S and the received byte were checked. The test now covers direct callbacks,
    absolute-JMP `$6023` stubs, callback clobber of X/Y/A, all 64 C/Z/I/D/V/N status combinations,
    and all 256 stack pointers. Partial receive-byte state survives save/load and completion of the
    byte requests NMI. Measured synthetic elapsed clocks are 66 (disabled), 110 (direct callback),
    and 116 (absolute-JMP stub) master clocks. These values are implementation-observation timings,
    not physical hardware timing. The test used direct NMI injection and purpose-written cartridge
    bytes, not live UART traffic or commercial images. Live Netpacket timing, callback reentrancy,
    nested IRQ/NMI, and physical byte-edge timing remain open.
  - **Reset and state-load lifecycle validation.** Save/load now requires an exact
    `gamateStateSize` buffer; undersized and oversized buffers are rejected. A corrupt or
    wrong-size load leaves machine state unchanged. The core test verifies transmit progress,
    MSB-first partial receive bytes, pending receive-complete NMI, pending IRQ/NMI serialization,
    and reset clearing of transmit/receive state. The libretro frontend test also rejects exact
    and short/long state sizes and clears transient link/presentation state only after successful
    machine-state load. The SWK prototype pauses inside a purpose-written direct `$6023` callback,
    saves state, mutates registers/RAM, restores state, and completes the callback; this verifies
    in-flight emulator callback resume. It does not establish live Netpacket queue timing,
    arbitrary nested callback mutation, or physical UART edge timing.
  - **`$F024` timing padding validation.** The documented strided-record initializer takes 824
    master clocks in D-clear synthetic calls. The independent firmware measured 780 before a focused
    NOP block was added; 11 6502 `NOP` instructions add 44 master clocks under this emulator and
    close the controlled timing gap. The synthetic test now asserts 824 clocks, 32 `$FF` writes,
    and the documented register/status effect. D-set arithmetic and interrupt-spanning timing remain
    outside this result.
  - **`$F033` timing padding validation.** The independent formatter measured 210/410/270/358
    clocks for focused ordinary, zero, Y=`$FF`, and blank cases. Two NOP instructions in the
    Y=`$FF` suffix path and three NOP instructions in ordinary Y formatting add 8 and 12 master
    clocks respectively. The full 526,848-call matrix then matched the documented formula for
    every input, including first-nibble branches and Y=`$FF`. Interrupt-spanning and callback
    interference remain outside the matrix.
  - **`$F012` replacement row-loop optimization.** The resolved contract always copies exactly
    eight glyph rows per tile. Replacing the decrement/branch row loop with eight explicit indexed
    reads and LCD writes removes 86 master clocks per tile while retaining the existing selector,
    flag, stack-depth, page-crossing and output behavior. The purpose-written 256-selector matrix
    now compares every one-tile duration with the authentic formula under both tested entry-V
    states: SWK remains 102-188 clocks high, down from a worst residual of 274. This is a bounded
    implementation improvement, not an accuracy claim for the remaining selector-class overhead.
    Moving the compositor into its own
    page-aligned linker segment prevents future earlier-service size changes from silently changing
    its branch timing; after alignment its controlled redraw paths and empty baseline remain exact,
    while large/small object paths are two/eight clocks high. These decisions use only the existing
    behavioral formulas and synthetic data; no authentic implementation code or payload was
    consulted or copied.
  - **`$F012` selector and stack follow-up.** The selector is now transferred to X before its
    descriptor-index push instead of being recovered through TSX and a stack-page load. After the
    glyph copy, `$018` temporarily retains the return X value while the stack-balanced width check
    runs; the documented final width write still replaces it before return, including low-stack
    wrap cases. A single range tree now derives C and V instead of two overlapping comparison
    chains, retaining entry V for selectors `$00-$20` and the three measured carry ranges. The
    exhaustive selector/entry-V matrix continues to match output registers, flags, stack balance,
    LCD writes and glyph bytes. Its one-tile timing residual is now 68-130 master clocks, reduced
    from 102-188 at the start of this follow-up and 188-274 before the row-loop optimization.
    Evidence and clean-room limits are the same synthetic behavioral comparisons documented above;
    no firmware implementation text was consulted.
  - **`$F015` replacement timing decomposition.** Six purpose-written synthetic shapes compared
    the independently authored firmware with the resolved authentic formula: 1x8, 1x1, 2x8,
    3x8, 4x8, and a 3x8 case whose second-plane source begins at `$60FE`. With no source-pointer
    carry, SWK follows `390 + columns * (248 + 76 * rows)` master clocks, compared with the
    authentic `234 + columns * (132 + 76 * rows)`. Replacing the destination's load/add/store with
    the equivalent eight-bit `INC`, and reusing the completed row index for source-pointer addition,
    reduces 24 master clocks per column. Testing the completed row index directly for the 256-row
    case removes four more per column without changing that wrap path. Combining the second-plane
    pointer stores and removing a redundant post-copy rewrite of `$000-$001` removes another 36
    fixed clocks while leaving the documented final pointers unchanged. Finally, deriving returned
    Y from the final second-plane source-low delta replaces a repeated-add multiplication and saves
    `20 * columns - 8` clocks. The resulting SWK formula is
    `362 + columns * (200 + 76 * rows)`. The identical 76-clock byte-row coefficient now isolates
    the remaining mismatch to 128 clocks of fixed setup and 68 clocks per column. In the `$60FE`
    case, the authentic indexed-read crossings account for 12 clocks while SWK adds another eight
    when its explicit source-pointer update carries. Implementation impact: retain the row-copy
    loop, but replace the stack/setup and per-column pointer/address bookkeeping; do not compensate
    with global padding. These are emulator observations using synthetic data, not physical timing.
    No BIOS or cartridge instructions or payload bytes were retained or copied.
  - **`$F015` pointer-update follow-up.** On every ordinary nonzero-height column, Y already holds
    the row count when it is tested against zero and remains in A across the taken branch. Removing
    the immediately repeated `TYA` preserves the 256-row path, source-pointer arithmetic, carry
    behavior, scratch state, ordered reads and output while saving four master clocks per plane and
    column. The entry status now remains on the stack during both passes, allowing `$01A` to serve
    as the destructive column counter while `$01C` remains intact. This removes both width
    push/restore round trips. Reordering the saved status and X entries lets the return restore X
    before normalizing status, eliminating the temporary `$01A` status write and reload. Deriving
    the destination column while the entry X value is still in A also removes a later `TXA`. The
    seven-shape synthetic matrix now asserts
    `322 + columns * (184 + 76 * rows) + 2 * crossing_reads + 8 * pointer_carries` for the tested
    nonzero heights, reducing the no-carry mismatch to 88 fixed clocks plus 52 per column. The
    ordinary pointer branch was inverted and its 256-row case moved out of line, saving four master
    clocks per two-plane column without changing the carry surcharge. The exact 76-clock byte-row
    term is retained. A new zero-height case verifies the derived 256-row behavior and its
    `322 + columns * (176 + 76 * 256)` timing term. The explicit pointer-carry surcharge remains
    unresolved. These are independently authored dataflow optimizations based on the existing
    behavioral model; no authentic implementation code was consulted.
  - **Remaining-service timing ranking.** A local replacement-firmware run measured the existing
    purpose-written `$F018` mixed stream at 3,848 master clocks against 4,172 from the independently
    derived authentic timing formula, and its transparent-only stream at 958 against 876. These
    roughly 8-9% residuals are smaller than `$F015`'s tested 1x1 residual, so `$F015` remains the
    least accurate measured service after the preceding change. This ranking uses synthetic
    emulator observations, not physical timing, and does not claim that the two `$F018` shapes bound
    every stream. No authentic implementation code or payload bytes were consulted or retained.
  - **Replacement wait-service consolidation.** The independently authored `$F003`, `$F006`, and
    `$F009` loops were checked through one synthetic matrix covering all 256 entry values, multiple
    preserved-status combinations, and thresholds 8, 5, and 24. Removing redundant `SEC`
    instructions from `$F003/$F009` reduced their ready paths from 46 to the measured 42 master
    clocks; the successful `CMP` already supplies carry, while `LDA #$00` supplies the returned Z/N
    state. Every service waited for exactly `threshold - n` IRQs below threshold, zeroed `$00B`,
    preserved Y/S, and made no direct LCD or VRAM writes. A purpose-written one-IRQ callback for
    each slot confirmed that callback-modified X remains observable while A and Y follow the wrapper
    and service contracts. Callback execution time and IRQ phase are deliberately not attributed to
    the wait loop. This validation used only synthetic cartridge code and the previously documented
    execution contracts; no commercial implementation code or payload was consulted or copied.
  - **Rectangle-fill timing correction.** `$F02A/$F02D` were probed with a removed final
    `LDA $1A`; clean 2×3, 256×1, 1×256 and 1×1 shapes were four master clocks high in both
    routines. Removing the load moved 1×1 from 202/196 to 200/194 and the shortest neutral 6502
    instruction could not supply the required two-master-clock correction in isolation, so the
    removal was retained only after the 1×1 and 1×256 real-call probe. A direct synthetic rerun
    then resolved the 256-wrap values. The plane-0 entry now forces absolute addressing for its
    initial `$01A` scratch write, adding the single CPU cycle required without changing observable
    state. `$F02A` measures 202 for 1×1, 350 for 2×3, 19,582 for 256×1, and 4,792 for 1×256;
    `$F02D` measured 19,574 and 4,784. Plane 0 follows `126 + 58*c + 18*c*r`; plane 1 follows
    the same expanded form with an additive 118 in 256-wrap shapes. These are retained as exact
    SWK synthetic shape assertions. Ordinary plane-1 timing uses base 120 in the authentic contract
    but remains two master clocks short in SWK; broad interrupt timing remains open. `$F030` branch NOP padding raised
    a zero-state synthetic mismatch to 220 clocks, so it was reverted. Later NOP/branch attempts
    measured 190 and 238 for the zero state while the documented target remained 188, so they were
    also reverted. `$F033` ordinary/Y=`$FF`
    padding blocks produced branch-dependent mismatches and were reverted. `$F027` shape/delta
    instrumentation found 139 of 141 scheduled Cube Up calls with 4 background and 4 sprite records
    at a 30,588-clock shortfall, plus 0/3 and 0/8 classes with 62,544 and 53,684 shortfalls. No
    global or shape-only NOP constant can be inferred from that corpus distribution. Temporary
    diagnostics and all speculative code changes were removed; these attempts are recorded as
    negative evidence, not implementation facts.
  - **`$F027` replacement timing correction.** A later contract-driven rewrite supersedes the
    provisional timing result above. Direct gate checks make each disabled redraw record use the
    measured 44-clock stride; the object pass advances X directly instead of rewinding and
    re-advancing each copied record. Tile pointers are resolved once per eight source rows, LCD
    columns and rows are carried across their natural loops, and the small-object shift kernel uses
    eight explicit absolute-RMW rows. These changes retain the documented ordered graphics and LCD
    accesses while removing repeated address arithmetic. Size-specific neutral delay loops close
    the remaining fixed redraw and large-object differences; they are consequences of the measured
    operation formula, not reconstructed firmware code. The synthetic suite now asserts exact
    clocks for the 6,326-clock empty table, isolated large and small redraws, isolated large and
    small current objects, selector/page penalties, horizontal-shift costs, and a combined
    four-operation case. Output, access-count, scratch, and return-state assertions continue to
    pass. The inputs and graphics are purpose-written synthetic data. No authentic BIOS instruction,
    table, graphic, or cartridge payload was consulted or copied.
  - **Interactive follow-up.** A RetroArch session loaded the rebuilt core with explicit SWK
    priority and ran the standalone Cube Up cartridge for 12 minutes 36 seconds. Gameplay began
    successfully, and the user-observed gameplay, score digits and piece behavior appeared correct
    with no reported freeze or halt. This is a qualitative, emulator-only session; it was not a
    complete playthrough and no intermediate frames or state were retained.
  - **Limitations.** `$F027` now matches the controlled uninterrupted binary-mode timing formula,
    but the earlier Cube Up corpus timing comparison has not yet been rerun against this rewrite.
    Stack contents, decimal-mode entry, interrupt-visible event timing, and side-effectful graphics
    sources are not fully modeled. No physical hardware was used. Reports, harnesses and traces
    stayed under temporary storage and retained no ROM or BIOS bytes.
