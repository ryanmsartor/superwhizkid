# Gamate implementation sources

This file records the sources used while developing superwhizkid, what each source was
used for, and important disagreements between them. It should be updated whenever a new source
influences implementation or tests.

The code in this directory is an original C implementation for this project. Emulator source is
used to cross-check observations and identify test cases, not as source text to translate. Primary
hardware measurements take precedence where they conflict with an emulator.

## Primary hardware research

### Kevtris, “Gamate Inside”, version 1.00 (2016-06-29)

- Archived URL: <https://web.archive.org/web/20160630141941id_/http://blog.kevtris.org/blogfiles/Gamate%20Inside.txt>
- Original URL (currently unavailable): <http://blog.kevtris.org/blogfiles/Gamate%20Inside.txt>
- Status: reverse-engineering notes based on measurements of physical Gamate hardware.
- Used for:
  - 4.433 MHz crystal, 2.2165 MHz CPU, and the NCR 65CX02 being NMOS-6502-like rather than a 65C02.
  - Physical address map, open-bus behavior, active-low controller layout, and BIOS mirroring.
  - Cartridge unlock protocol (`BIT CORPORATION`, nine writes per byte, `$47` reply), ROM validation
    sequence, common banking, and the distinct 4-in-1 banking scheme.
  - LCD register bit definitions, VRAM addressing, 256×256 backing bitmap, 160×150 viewport,
    200-line Y wrapping, both window modes, and the measured 72,900-crystal-clock frame.
  - AY-compatible register behavior, channel routing, and measured divider behavior.
  - The IRQ interval of 32,768 crystal clocks and UART loading, shifting, receiver-byte/NMI, and
    idle-line behavior.
- Implementation impact: this is the principal behavioral specification for `gamate.c` and its
  tests. It caused corrections to the initial PSG read and IRQ-period implementations.

## Independent implementations used for comparison

### xrip/gamate

- URL: <https://github.com/xrip/gamate>
- Relevant files: `src/main.cpp`, `src/gamate/vdp.cpp`.
- License: repository source identifies its license in-file; `vdp.cpp` is BSD-3-Clause and its
  README says its video renderer was derived from MAME.
- Used for: checking ROM bank write addresses, input bit order, AY clock/routing choices, and how a
  small standalone emulator schedules the periodic IRQ.
- Not used as an independent authority for LCD behavior because its README and source explicitly
  say that portion came from MAME/Kevtris.
- Known limitation: its protection handling is abbreviated and it does not model all bus behavior.

### emulators.org Gamate machine

- Documentation: <https://emulators.org/emulator/gamate/>
- Machine source: <https://emulators.org/emulator/gamate/src/gamate-machine.js>
- Status: independently authored JavaScript machine around the public-domain Fake6502 core, though
  its header says its hardware values were verified against MAME.
- Used for: comparing the clean machine/CPU boundary, side-effect-free debugger reads, basic LCD
  address-port behavior, active-low input, and instruction-driven IRQ accumulation.
- Not treated as authoritative for protection, banking, special Y-window behavior, or exact timing;
  those pieces are absent or simplified.

## Libretro integration references

### SundownerSport, pull request #1

- PR: <https://github.com/ryanmsartor/superwhizkid/pull/1>
- Contributor: SundownerSport (`kilchisblue@aol.com` in the contributed commits).
- Original contributed commits and scope:
  - `fb2fef2`, optional tap latch.
  - `d3ce03d`, optional LCD ghosting filter.
  - `3519f6f`, UMC/Bit BIOS preference.
  - `000eb1c`, optional authentic-BIOS intro skip.
- Authorship: SundownerSport authored the initial implementations of those four options. Ry authored
  the later palette/option integration, moved the tap latch out of machine state, added and expanded
  the synthetic integration tests, made state-load and intro-timeout behavior failure-safe, selected
  the documented option lifecycle/UI wording, and performed the follow-up source documentation.
- Contributor-reported tap observation: the PR description reports that some games defer `$4400`
  polling around the BIOS `$F009` wait (24 IRQs is approximately 0.177 seconds at the measured Gamate
  clock). On Brick Blaster's title screen, SundownerSport reports 76/76 six-frame Start taps accepted
  with the latch enabled versus 17/76 without it. The PR does not include the automation script or a
  fuller trial protocol, so these counts are recorded as contributor-reported rather than independently
  reproduced measurements. Our purpose-written integration firmware independently verifies the latch
  mechanism, missed-between-read case, consumption, option-off behavior, and save/load boundaries.
- Contributor-reported display observation: the PR identifies Brick Blaster clearing and redrawing
  its paddle and ball as the motivation for its persistence filter. Our later 1,338-frame trace
  independently confirmed the paddle's one-frame erase/restore cadence and exact VRAM changes, as
  documented below. Neither the PR nor our trace measures physical-panel response time, so the
  coefficients remain explicitly estimated.
- BIOS options: the PR introduced filename priority and accelerated execution of the real BIOS until
  protected cartridge entry, rather than fabricating post-BIOS state. Follow-up tests independently
  validate both BIOS choices, fallback, `$6020` success, reset/reload lifecycle, and timeout recovery.
- External-source use: PR #1 cites behavioral observations and the existing superwhizkid interfaces;
  it does not cite MAME or another emulator as source for these option implementations. The project
  does not treat the PR's visual coefficients or tap counts as hardware specifications.

### Real-hardware LCD appearance references

- Nintendo Life, “Meet The Gamate, The Handheld Which Tried To Take On The Game Boy And Failed”:
  <https://www.nintendolife.com/news/2014/02/feature_meet_the_gamate_the_handheld_which_tried_to_take_on_the_game_boy_and_failed>
- Neo Fuji Gamate game archive: <https://fuji.12bit.club/gamate/games/>
- Ashens, “Gamate Review”: <https://www.youtube.com/watch?v=aIsKG-pewM0>
- Museum of Obsolete Media, “Bit Corporation Gamate (1990–1995)”:
  <https://obsoletemedia.org/gamate/>
- Used for: choosing a representative olive-green profile for the optional `Accurate` palette and
  confirming that neutral greyscale is not representative of the original non-backlit panel.
- Method and limitations: the Nintendo Life/Neo Fuji game images show software on physical Gamate
  screens, but several were made by scanning a paused LCD and have severe green-channel clipping.
  The Ashens video thumbnail provides a less saturated view of an unlit physical screen under room
  lighting. The representative profile therefore follows that screen's olive hue while using the
  gameplay images only to check shade ordering and contrast. It is a visual estimate, not calibrated
  colorimetry: camera white balance, exposure, ambient light, contrast-wheel position, aging, and the
  two reported Gamate panel types all prevent a single photograph-derived palette from being exact.
- Implementation impact: the libretro frontend offers `Original`, `Accurate`, and `Greyscale`.
  `Original` preserves the core's initial uncited visual approximation exactly. `Accurate` uses
  RGB565 `9CEC/6B89/3A08/10C4`; its darker driven shades shift from the panel's olive background
  toward the cool, low-saturation ink appearance visible in the hardware references. `Greyscale`
  uses neutral 255/170/85/0 levels. Palette selection only
  maps completed two-bit pixels to output colors and does not change emulated LCD state or timing.

### Estimated LCD persistence filter

- Physical basis: the Nintendo Life hardware account above reports severe motion blur and at least
  two panel types, while the Museum of Obsolete Media describes the original screen as a green,
  non-backlit monochrome LCD. Neither source provides measured rise/fall response times.
- Local observation: an interactive 1,338-frame Brick Blaster trace captured raw completed pixels,
  VRAM, LCD phase, and cumulative VRAM writes. During gameplay the 120-pixel paddle region at
  `x=64–87, y=132–141` was deliberately absent for one full frame on a repeating nine-frame cadence.
  The same 36 VRAM bytes were explicitly zeroed and restored on the adjacent frames. This establishes
  the redraw artifact being masked; it does not measure the physical panel response. The trace is a
  local diagnostic at `/tmp/swk-brick-blaster.trace` and contains no ROM or BIOS bytes.
- Model: once per 60.8093 Hz output frame, an internal level moves toward the completed two-bit pixel
  target using `next = level + (target - level) * rate / 256`; a sub-unit final step snaps exactly to
  the target. Darkening rates for levels 1–5 are `256, 248, 240, 224, 208`, leaving respectively
  `0%, 3.125%, 6.25%, 12.5%, 18.75%` of the prior difference per frame. Fading rates are
  `224, 192, 160, 128, 96`, leaving `12.5%, 25%, 37.5%, 50%, 62.5%`.
- Limitations: these coefficients were selected by SundownerSport as useful visual strengths, not
  obtained from a photodiode, high-speed camera, service specification, or calibrated recording.
  The option is therefore presented as `LCD Ghosting`, with its description explicitly identifying
  the response as estimated; its levels describe strength, not milliseconds. It affects only
  frontend RGB output, is excluded from machine save state, snaps
  after a successful state load, and can be disabled for exact raw LCD-controller output.
- Validation: synthetic integration tests check raw-output identity, both response directions,
  convergence to both endpoints, increasing one-frame retention, all five levels, and interpolation
  through every selectable palette.

### Redraw flicker-reduction filter

- Motivation: increasing estimated LCD persistence can reduce Brick Blaster's one-frame erase, but
  eliminating it that way would require retaining nearly all lightening changes and would create long
  trails during ordinary movement. This separate compatibility filter targets the redraw pattern
  rather than claiming to model panel physics.
- Trace replay: the retained 1,338-frame Brick Blaster trace contains 76,673 raw pixel transitions.
  Replaying it through this filter produced 63,636 transitions and held 32,567 candidate lightening
  transitions across 236 frames. It caught every inspected full-paddle erase, including the 120-pixel
  events at frames 1123, 1132, 1141, and 1150. These counts describe this captured play session, not
  the entire software library.
- Model: a darker or equal two-bit shade is accepted immediately. A lighter shade becomes pending
  and is accepted only if the same value appears on the following frame. If the prior darker value
  returns, the pending lightening is discarded. Filtering occurs before optional LCD ghosting, adds
  no full-frame/audio delay, and remains off by default.
- Tradeoff: genuine lightening changes appear one frame late, and moving dark objects may leave a
  one-frame trail. The option is therefore named `Redraw Flicker Reduction` and described as a
  compatibility filter, not hardware emulation. Its presentation-only history is excluded from
  machine save state and reinitialized after reset, content load, successful state load, or an
  on/off option change.
- Validation: purpose-written firmware verifies one-frame suppression, cancellation when the dark
  shade returns, acceptance of sustained lightening on its second frame, immediate darkening, and
  raw behavior after disabling the option.

### BIOS selection option lifecycle

- The preferred UMC/Bit filename is consulted only when content is loaded. If it is absent, the other
  recognized BIOS is tried. Core reset resets the already loaded machine and deliberately retains its
  current BIOS; changing the option takes effect after content reload.
- The option description states this lifecycle explicitly. Purpose-written identifiable BIOS files
  test both preferences, missing-file fallback, preference changes across reset/reload, and invalid
  values without including commercial firmware bytes.

### BIOS intro-skip deadline and fallback

- Local measurements with both 32 KiB Brick Blaster and the 512 KiB Golden Pyramid reached `$6020`
  after 30,239,372 master clocks using the validated UMC/NCR BIOS and 30,239,430 clocks using the BIT
  BIOS: approximately 6.82 emulated seconds at the measured 4.433 MHz oscillator. The matching
  smallest/large-cartridge results are consistent with the earlier representative-size boot matrix.
  The skip deadline is therefore expressed in hardware time as 53,196,000 master clocks (12 seconds),
  about 1.76 times these observed boot paths, rather than as a host-dependent or CPU-instruction
  iteration count.
- Intro skipping succeeds only after cartridge protection has passed and the CPU is exactly at the
  documented `$6020` entry. If the deadline expires, the machine is reset to a clean ordinary BIOS
  boot and the frontend receives a timeout message; execution never continues from the arbitrary
  point at which accelerated boot stopped.
- The purpose-written protection-handshake BIOS tests successful skip on load and reset. A separate
  synthetic BIOS that never reaches `$6020` tests the deadline, message, and clean-reset fallback on
  both paths. These tests contain no commercial firmware or cartridge bytes.

### Official libretro core-info example and RetroArch core installation guide

- Core-info template: <https://github.com/libretro/libretro-core-info/blob/master/00_example_libretro.info>
- Installation guide: <https://docs.libretro.com/guides/download-cores/>
- Used for: declaring save-state support and its `serialized` feature level in
  `superwhizkid_libretro.info`, and confirming that an out-of-tree core may load without its info file
  while RetroArch leaves metadata-gated features such as save states disabled.
- Implementation impact: the shipped info file now advertises the serialization backend already
  implemented by the core. This metadata affects frontend feature discovery only; it does not
  supply or replace serialization behavior.

## Build and release infrastructure references

### spruceUI Cores-spruce

- Repository: <https://github.com/spruceUI/Cores-spruce>
- Reference workflow: <https://github.com/spruceUI/Cores-spruce/blob/main/.github/workflows/build-armnes.yml>
- Toolchain definitions: <https://github.com/spruceUI/Cores-spruce/blob/main/Dockerfile.arm64>,
  <https://github.com/spruceUI/Cores-spruce/blob/main/Dockerfile.armhf>,
  <https://github.com/spruceUI/Cores-spruce/blob/main/toolchain-arm64.cmake>, and
  <https://github.com/spruceUI/Cores-spruce/blob/main/toolchain-armhf.cmake>.
- Used for: the supported ARM64/ARMHF hardware classes, public `toolchains` release assets,
  Cortex-A53 and Cortex-A7 compiler targets, A30/Miyoo ARMHF glibc 2.23 ceiling, Ubuntu Focal ARM64
  glibc 2.31 ceiling, and spruce package layout (`cores/`, `cores64/`, and core-info metadata).
- Implementation impact: the manual release workflow consumes the same public Docker images, checks
  both output architectures and glibc symbol ceilings, and packages both cores together. Its tag and
  draft-release lifecycle is independently written for this repository's explicit release-tag input.

### GitHub Actions and GitHub CLI documentation

- Manual-workflow documentation: <https://docs.github.com/en/actions/how-tos/manage-workflow-runs/manually-run-a-workflow>
- Workflow syntax: <https://docs.github.com/en/actions/reference/workflows-and-actions/workflow-syntax>
- Release creation: <https://cli.github.com/manual/gh_release_create>
- Release deletion: <https://cli.github.com/manual/gh_release_delete>
- Used for: `workflow_dispatch` inputs, the `contents: write` token permission, verified existing
  tags, draft release creation, and non-interactive replacement of an existing release. The action
  deliberately has no automatic event trigger.

## Audio specification and comparison

### General Instrument AY-3-8910/8912/8913 data manual

- Scan: <https://map.grauw.nl/resources/sound/generalinstrument_ay-3-8910.pdf>
- Status: original chip-manufacturer data manual.
- Used for: register layout and masks, tone/noise/mixer signal paths, amplitude/envelope controls,
  period widths, envelope shape bits, reset state, and the relationship between the input clock and
  the tone, noise, and envelope dividers.

### Ayumi

- URL: <https://github.com/true-grue/ayumi>
- License: MIT.
- Status: independent high-accuracy AY-3-8910/YM2149 implementation.
- Used for: comparison of counter organization, mixer gating, envelope restart/hold/alternate
  semantics, sample-rate conversion, and DC-removal strategy. No Ayumi source was copied.

### emu2149

- Example source: <https://github.com/emestee/bk-emulator/blob/master/emu2149.c>
- Status: Mitsutaka Okazaki's established AY/YM implementation; its source lists the AY data sheet,
  MSX documentation, and earlier emulators as references.
- Used for: cross-checking the AY rather than YM logarithmic 16-level amplitude curve and zero-period
  handling. The compact level table in `gamate.c` is an independently scaled integer expression of
  the documented AY curve, not imported code.

Gamate-specific clocking and channel routing continue to come from Kevtris's physical measurements:
the PSG runs at 4.433 MHz / 4; channel A goes left, B right, and C to both sides.

## Compatibility reference

### MAME Gamate implementation

- Machine: <https://github.com/mamedev/mame/blob/master/src/mame/bitcorp/gamate.cpp>
- LCD: <https://github.com/mamedev/mame/blob/master/src/mame/bitcorp/gamate_v.cpp>
- Cartridge: <https://github.com/mamedev/mame/tree/master/src/devices/bus/gamate>
- License: GPL-2.0-or-later for the machine/LCD files; BSD-3-Clause for the cartridge-device files.
- Used for: identifying currently recognized BIOS variants, known cartridge sizes/types, and
  compatibility-oriented edge cases to investigate against primary notes or tests.
- Not used as the sole specification. In particular, its `$5800/$5A00` card-presence behavior
  differs from Kevtris's measured open-bus/`xx11` description; superwhizkid follows the primary
  hardware notes and records the disagreement here.

## CPU specification and validation

### MOS Technology, MCS6500 Microcomputer Family Programming Manual

- Archive: <https://archive.org/details/mos_microcomputers_programming_manual>
- Scanned original: <https://www.bitsavers.org/components/mosTechnology/6500-50A_MCS6500pgmManJan76.pdf>
- Status: original manufacturer programming manual.
- Used for: official NMOS 6502 instruction semantics, addressing modes, stack behavior, interrupt
  vectors, status flags, documented cycle counts, level-sensitive IRQ masking, and completion of the
  current instruction before interrupt service.

### MOS Technology, MCS6500 Microcomputer Family Hardware Manual

- Scan: <http://archive.6502.org/books/mcs6500_family_hardware_manual.pdf>
- Status: original manufacturer hardware manual; Appendix A gives the address/data-bus activity for
  each instruction cycle.
- Used for: the NMOS read-modify-write sequence, which reads the operand, writes its original value,
  then writes the modified value. This differs from later CMOS parts and is observable with
  access-triggered Gamate registers.
- Implementation impact: memory ASL/ROL/LSR/ROR/DEC/INC now perform both NMOS writes. A focused test
  targets `$4C00`, where the operand read and both writes must produce three UART clocks. The NCR
  65CX02 has not been logic-analyzer-traced for this sequence, so this is explicitly an NMOS
  baseline derived from the chip's otherwise measured NMOS-like behavior.
- Also used for: indexed-address bus sequencing. Zero-page indexed modes perform their preliminary
  base-address read; page-crossing indexed loads read once from the uncorrected page; indexed stores
  and indexed read-modify-write instructions always perform that preliminary read. Tests cross from
  `$4FFF` to `$5000` and verify that the otherwise-discarded `$4F00` read clocks the UART before the
  corrected LCD-space access. Dedicated tests also cover the separately decoded LDY/STY indexed
  forms, including the zero-page preliminary read's open-bus result. As above, this is the documented
  NMOS baseline pending an NCR trace.
- Also used for: branch bus sequencing. A taken branch reads the fall-through PC; a page-crossing
  branch then reads the target low byte with the old page before fetching from the corrected target.
  Tests verify the cycle count, target, and open-bus value for taken page-crossing and untaken cases.
- Also used for: the discarded next-opcode fetch made by two-cycle implied and accumulator
  instructions. Focused NOP and accumulator-ASL tests verify that PC is unchanged by the discarded
  fetch while its byte becomes the machine's open-bus value.
- Also used for: stack and control-flow bus order. PHP/PHA, PLP/PLA, RTI, and RTS now include their
  discarded program/stack reads; JSR fetches its target high byte after pushing the return address;
  BRK reads its padding byte; and hardware IRQ/NMI perform their two replaced opcode reads. Tests
  verify PHA's next-opcode open-bus value and RTS's final return-address read as well as stack/PC
  results. Exact interleaving with peripheral clocks still requires a per-cycle CPU architecture.
- Also used for: completing the official NMOS bus-sequence audit. Power-on reset now performs two
  discarded PC reads, three non-writing stack reads/decrements, and two vector reads; a focused test
  verifies the resulting PC, S, cycle count, untouched stack memory, and open bus. The existing NMOS
  indirect-JMP page-wrap read now has an explicit regression test. All documented official NMOS
  instruction/addressing classes are represented; undocumented opcodes, an externally asserted warm
  reset, and NCR-specific deviations remain outside this evidence-backed audit.

### Masswerk 6502 instruction set

- URL: <https://www.masswerk.at/6502/6502_instruction_set.html>
- Status: independently compiled opcode/addressing/cycle reference based on MOS documentation.
- Used for: cross-checking the official opcode matrix, instruction lengths, base cycles, and
  page-crossing penalties in `cpu.c`.

### Visual6502 decimal-mode research

- URL: <https://www.nesdev.org/wiki/Visual6502wiki/6502DecimalMode>
- Status: transistor-level analysis and exhaustive tests of NMOS 6502 decimal arithmetic.
- Used for: identifying the NMOS decimal datapath and its distinction from later CMOS parts. This
  motivated exhaustive testing rather than treating invalid BCD digits as ordinary binary-adjusted
  arithmetic. No Visual6502 implementation code is used.

### Bruce Clark decimal-mode test, as maintained by Klaus Dormann

- URL: <https://github.com/Klaus2m5/6502_65C02_functional_tests/blob/master/6502_decimal_test.a65>
- Status: public-domain exhaustive test whose predicted results are calculated with 6502
  instructions; its comments identify Bruce Clark as the author.
- Used for: independently specifying all accumulator/carry combinations for NMOS decimal `ADC` and
  `SBC`, including invalid BCD digits, and the NMOS points at which N, V, and Z are observed.
- Implementation impact: replaced a valid-BCD-oriented ADC shortcut that mishandled invalid digits
  and computed ADC N/V before the low-nibble carry. `gamatetest` now checks 262,144 decimal
  operations across every accumulator, operand, carry, and both arithmetic instructions.

### Visual6502 interrupt-timing research

- URL: <https://www.nesdev.org/wiki/Visual6502wiki/6502_Timing_of_Interrupt_Handling>
- Status: transistor-level simulations and observations of the NMOS 6502 interrupt pipeline; the
  page itself labels some collision/branch material as work in progress.
- Used for: the established I-flag polling distinction where CLI, SEI, and PLP use the old mask for
  the following IRQ boundary, while RTI's restored mask is effective immediately. The uncertain
  collision and branch claims were not implemented.
- Implementation impact: `GamateCpu` now saves an explicit IRQ-mask polling latch, state format
  version 2 includes it, and focused tests cover pending IRQs across CLI, SEI, PLP, and RTI.

### Klaus Dormann 6502 functional tests

- URL: <https://github.com/Klaus2m5/6502_65C02_functional_tests>
- License: tests repository license applies to the external test programs; no test ROM is bundled.
- Used for: external conformance validation of all official opcodes, addressing modes, flags,
  branches, stack operations, and valid-BCD decimal arithmetic. `gamate-6502test` links the
  production `cpu.c` to a separate flat 64 KiB bus and runs the repository's unmodified
  `6502_functional_test.bin` (SHA-1 `55c9ab5b137c8ced3c666bfdb55c8285782baad9`). The image is
  downloaded to `/tmp` for testing and is not distributed by this project.
- Result: passed at the documented `$3469` success trap after 30,646,193 instructions. The first run
  failed in decimal `SBC`; that result led to correcting the nibble borrow/adjust implementation.

## Local compatibility validation (files not distributed)

The following user-supplied files were read in place solely for boot validation. They are not copied
into the repository, build output, tests, or releases.

- UMC/NCR BIOS, SHA-1 `ea449dc607601f9a68d855ad6ab53800d2e99297`.
- BIT BIOS, SHA-1 `4e9dfbfe916ca485530ef4221593ab68738e2217`.
- Representative cartridges at 32, 64, 128, 256, and 512 KiB, plus the 128 KiB 4-in-1 cartridge.
  These were used to exercise the BIOS protection handshake, header text and checksum validation,
  logo delay, and final jump to the documented cartridge entry at `$6020`.
- Result: both BIOS revisions accepted every representative cartridge and reached `$6020`.
  `gamateboot` provides the reproducible diagnostic without containing any copyrighted bytes.
- The known 4-in-1 image has SHA-1 `4dd20632eb31023e18241290eca7d5a5acb1fa84` and CRC32
  `10e81571`. The runtime uses the whole-image CRC32 plus the exact 128 KiB size to select its unique
  mapper; ordinary cartridges continue to use size only to distinguish plain from commonly banked
  hardware. This fingerprint came from the locally validated image, not an emulator source.
- Post-entry validation ran 300 frames for Bomb Blaster under both BIOS revisions and representative
  64, 128, 256, and 512 KiB cartridges plus the 4-in-1. The diagnostic observed changing framebuffer
  hashes, nonblank output, IRQ service, LCD/VRAM traffic, and expected bank changes, with no unknown
  opcodes or persistent self-loop traps. Frame hashes are computed from four-shade pixel indices and
  contain no ROM or BIOS data.
- Audio validation over the same representative set observed PSG register writes and nonzero output
  without clipping. `gamatetest` separately verifies tone timing/sample count, noise-LFSR advance,
  envelope restart/advance, register masks, A-left isolation, B-right isolation, and equal C routing
  to both sides. `gamateboot` records deterministic per-frame audio hashes without storing samples.
- LCD execution now follows the measured 450 row-latch events per frame: 150 visible rows, three
  162-master-clock refresh passes per row, totaling 72,900 clocks. Plane 0 is sampled once and plane
  1 twice (reversed by plane-swap), reproducing the physical four-level accumulation described by
  Kevtris. A regression test changes VRAM between passes and verifies that completed earlier passes
  are not retroactively redrawn. The partial accumulator, row event, clock phase, and completed image
  are included in save states.
- The progressive renderer was rerun for 300 post-entry Bomb Blaster frames. Early hashes changed
  from the former frame-end snapshot, as expected; the settled frame-300 hash remained `e324a485`.
  No local MAME executable or hardware capture was available for pixel-by-pixel external comparison,
  so those hashes are regression baselines rather than claims of independent visual equivalence.
- An interactive RetroArch run of Bomb Blaster confirmed correct panel orientation, working audio,
  Start, and all four directions. The initial isolated frontend configuration did not produce an
  observable response from its implicit X/Z face-button bindings. Kevtris's physical controller
  readout was rechecked to distinguish frontend configuration from emulated wiring: `$4400` bits 4
  and 5 are active-low A and B, matching the core; xrip independently uses the same bit order. A
  temporary live trace then confirmed both A and B press/release transitions reached those exact
  bits, and Bomb Blaster began accepting bomb input. The trace was removed after diagnosis. The
  libretro core polls once immediately before each emulated frame (at 60.8093 Hz), bounding its own
  input-sampling delay to one frame; remaining perceived sluggishness has not yet been separated
  from Bomb Blaster's game logic or display behavior.
- Interactive GP Race testing initially reported “save states unsupported.” Temporary traces proved
  RetroArch rejected the operation before calling any serialization entry point, while a direct
  query of the built core returned its 89,568-byte fixed state size. The cause was the isolated test
  configuration not loading this out-of-tree core's metadata. The test configuration now points at
  the repository info directory and treats `build/` as its core directory, allowing RetroArch to
  index and associate the out-of-tree `.so` with `superwhizkid_libretro.info`. With that installation-
  equivalent layout, GP Race successfully saved and restored an 89,568-byte core payload (89,592
  bytes in RetroArch's state file). The shipped info file explicitly declares serialized-state
  support following the official libretro template. Temporary traces were removed. A bypass test
  was used only to isolate metadata discovery and is not required by the installed core.
- A subsequent installed-layout run cleanly unloaded GP Race and loaded Cosmic Fighter with a
  content-specific state path. Cosmic Fighter reached responsive gameplay, successfully saved and
  loaded state, reset through the libretro reset callback, and unloaded without frontend or core
  errors. This validates the observed frontend lifecycle; it is not evidence about original-hardware
  timing accuracy.
- `gamatecoretest` automates that frontend contract without external copyrighted data. It generates
  original minimal 4 KiB test firmware at runtime whose small, purpose-written 6502 program produces
  one AY tone, writes a visible LCD pixel, and mirrors `$4400` into RAM. The temporary file is named
  `gamate_bios_umc.bin` solely because that is one of the core's firmware-discovery names; its
  contents are not derived from, intended to reproduce, or usable as a replacement for the UMC
  firmware. The harness now generates identifiable UMC/Bit variants plus a second synthetic BIOS
  that sends the documented `BIT CORPORATION` protection sequence, reads the eight-bit reply, and
  jumps to `$6020`. These test option-controlled BIOS selection, missing-preference fallback, and
  intro skipping on load/reset without commercial bytes. The controllable test program also covers
  modern and legacy option registration, defaults and invalid values, live updates, all palettes,
  every ghosting level, palette/ghosting interaction, redraw-flicker filtering, tap-latch behavior
  across state loads, and a failed state load during an LCD transition. The harness additionally
  verifies all eight active-low
  input mappings, video/audio callbacks, fixed state size, serialization restoration, reset, unload,
  and a second content load. This scaffolding was authored for superwhizkid; no emulator source or
  commercial firmware, BIOS, or game-ROM bytes were used to create it.
- `tools/gamate-sweep.sh` ran all 58 locally supplied cartridges under both validated BIOS revisions
  for 300 frames after `$6020`: 116/116 combinations booted and produced nonblank video with no
  unsupported opcode or persistent self-loop diagnosis. Cartridge sizes covered 16, 32, 64, 128,
  256, and 512 KiB, including the fingerprinted 4-in-1 mapper. The report remains outside the
  repository at `/tmp/superwhizkid-sweep.txt` and contains filenames, hashes, and counters but no input
  bytes. This is broad automated smoke coverage, not a claim that every title is playable or
  hardware-accurate.
- Two titles were silent during this unattended 300-frame window under both BIOSes: Further
  Adventures of Hannibal The Cat (four AY writes) and Tornado (9,310 AY writes). Tornado was
  investigated with new per-register AY/channel and controller-read diagnostics. At its title state
  it deliberately rewrites registers 0–13 in sequence, leaving periods and volumes zero and mixer
  register 7 at `$3F`; all three synthesized channels are consequently inactive. The game polls
  controls sparsely (25 reads in 400 frames), so a six-frame Start pulse could be missed. A 20-frame
  Start pulse at frame 120 was observed once, entered gameplay under both BIOS revisions, and yielded
  audio in 270/400 frames with all three channels active and a peak of 30,558. Tornado's initial
  silence is therefore title-state behavior, not evidence of an AY synthesis fault.
- Further Adventures of Hannibal The Cat was also a 300-frame-window false positive rather than a
  synthesis failure. Without input its first audible output occurs after that window: 44/400 frames
  and 344/700 frames were audible, initially at low amplitude through channel C before the longer
  attract/demo sequence used all channels. A 30-frame Start pulse at frame 120 was seen by 61 of the
  game's frequent controller reads, entered gameplay under both BIOS revisions, and produced audio
  in 281/400 frames with all three channels active. An A-only pulse did not leave the title flow.
- Famous 7, One Million Whys, and QuizFighter's identical 24/300 audio result was traced to the same
  exact first-audio frame, 277, under both BIOS revisions. This strongly suggests a shared title
  sequence or engine timing (an inference from matching behavior, not a source-derived claim). At
  700 unattended frames they were audible for 424, 276, and 351 frames respectively, with multiple
  AY channels active; at 1,000 frames the counts rose to 724, 576, and 651. Famous 7 and QuizFighter
  do not begin controller polling until frame 564; a 30-frame Start pulse at frame 600 was observed
  and changed their subsequent execution. One Million Whys polls from frame 2 and observed the same
  pulse, but remained in its current flow. These limited 300-frame results are therefore title
  sequencing, not evidence of an audio fault.
- The two BIOS-dependent final-video cases, Mini Golf and Money Maze, were isolated to inherited
  entry phase rather than memory, mapper, or rendering nondeterminism. At `$6020`, both BIOSes leave
  identical RAM and VRAM hashes, scroll/LCD control, and IRQ phase. The BIT BIOS arrives 29 CPU
  cycles (58 master clocks) later, moving the progressive LCD from row-event/clock `362/128` to
  `363/24` and shifting frame and AY oscillator/sample phase. A diagnostic-only experiment reset
  clock, LCD, IRQ, and AY phase at entry while preserving CPU, memory, registers, and cartridge
  state. Under that normalization, each game produced identical video/audio hashes, PCs, and
  activity counters across BIOSes through frame 300. The ordinary differing hashes therefore sample
  slightly different execution instants because the diagnostic numbers frames from cartridge entry;
  normal emulation continues to preserve each real BIOS path's timing. Audio hashes for other games
  commonly differ for the same phase reason.
- Focused IRQ tests now verify assertion at exactly 32,768 master clocks, persistence while masked,
  service only after the current instruction boundary, IRQ-vector and stack contents, I/B/U status
  handling, diagnostic counting, timer restart, pending-state serialization, BRK's distinct stacked
  B flag, and reset clearing. The MOS manual supplies the NMOS 6502 service semantics; Kevtris's
  physical Gamate notes supply the timer period and report that an IRQ or reset vector pull clears
  the timer/flag. The CPU is instruction-granular, so it currently performs that logical vector pull
  before advancing the seven service cycles. The exact physical clock within those cycles and
  whether a BRK vector pull has identical timer side effects still require a Gamate trace or focused
  hardware test; the tests explicitly preserve this distinction rather than claiming cycle accuracy.
- The NMOS interrupt-mask audit found and corrected delayed polling across CLI, SEI, and PLP; RTI
  applies its restored mask immediately. The full 58-cartridge/two-BIOS/300-frame matrix remained
  116/116 after this change. This validates compatibility and the tested CPU boundary cases, but not
  the NCR 65CX02's unmeasured interrupt-vector bus cycle.
- LCD stop/blank boundary tests now distinguish LCDcon E from N using Kevtris's measured register
  behavior. E freezes the internal 162-clock pass and row-event position because the physical pixel,
  row, frame, and inversion clocks stop; clearing E resumes that position. N continues refresh while
  emitting zero pixel data, so a plane pass completed before mid-line blanking remains accumulated.
- The special Y-scroll/window audit corrected two mappings against the same physical notes. Values
  `$C8-CF`, `$D8-DF`, `$E8-EF`, and `$F8-FF` now use the ordinary row-zero origin instead of
  repeating row zero on every LCD line. For `$D0-D7`, `$E0-E7`, and `$F0-F7`, the low three bits now
  remove leading rows from the fixed `$F8-FF` area in the measured order (F0 exposes eight rows, F1
  seven, through F7 one), then normal rows resume; the fixed portion ignores X scroll. LCDcon W's
  explicit 16-row `$D0-DF` window takes precedence for the top of the display even with a special Y
  value. Pixel-level tests cover these boundaries. The post-change 116-case sweep passed, and none
  of its frame-300 video/audio/PC lines changed, consistent with Kevtris's observation that known
  games may not use the special window behavior during ordinary execution.
- The UART now follows Kevtris's measured register behavior independently of any future frontend
  link transport. A write to `$4400-$47FF` loads the transmitter and immediately exposes bit 7.
  Every read or write at `$4C00-$4FFF` clocks one MSB-first transmit bit; after the eighth clock the
  data line remains low until another byte is loaded. The explicit `gamateUartReceiveBit` boundary
  supplies one external rising receiver clock and data bit, shifts MSB-first, latches the byte for
  `$4800-$4BFF`, and requests NMI after eight clocks. Kevtris supplies the Gamate-specific register,
  bit-order, clock, and NMI observations; the MOS manual supplies the CPU fact that NMI ignores I
  and vectors through `$FFFA`. Our instruction-granular CPU gives a pending NMI priority over IRQ at
  the next instruction boundary and does not apply the Gamate IRQ timer's IRQ-vector-clear side
  effect to NMI. Tests cover receive assembly, NMI stack/vector behavior and priority, reset, and
  serialization. State format version 4 includes in-flight transmit/receive state and a pending NMI.
  Standalone emulation still receives zero unless a caller injects link bits: peer transport,
  electrical timing, and exact NMI/IRQ collision timing are not yet modeled or claimed.

## Still needed

- CPU validation traces or tests specifically for the NCR 65CX02, especially decimal mode and the
  reported bit-manipulation instructions.
- Direct test ROMs or logic-analyzer traces for the within-sequence IRQ/BRK clearing clock and LCD
  mid-frame writes.
- Identification data for the 4-in-1 cartridge that does not depend only on a filename/database.
