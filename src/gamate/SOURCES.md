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
- Contributor: SundownerSport.
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
- Used for: choosing a representative olive-green profile for the optional `Gamate LCD` palette.
- Method and limitations: the Nintendo Life/Neo Fuji game images show software on physical Gamate
  screens, but several were made by scanning a paused LCD and have severe green-channel clipping.
  The Ashens video thumbnail provides a less saturated view of an unlit physical screen under room
  lighting. The representative profile therefore follows that screen's olive hue while using the
  gameplay images only to check shade ordering and contrast. It is a visual estimate, not calibrated
  colorimetry: camera white balance, exposure, ambient light, contrast-wheel position, aging, and the
  two reported Gamate panel types all prevent a single photograph-derived palette from being exact.
- Local visual feedback: the user requested a murkier transparent/background shade because the
  existing Gamate LCD palette appeared too yellow. This is a subjective presentation preference, not
  a new hardware measurement.
- Implementation impact: the optional `Gamate LCD` palette uses RGB565 `8C6A/6348/3208/1906`; its
  background is a darker, more yellow-olive panel tone, while progressively driven shades shift
  toward a slightly lighter navy ink. The background's blue channel was raised by one RGB565 step in
  response to the visual feedback above. Additional palettes beyond `Gamate LCD` are presentation
  options included for fun, not representations of the handheld's display. Palette selection only
  maps completed two-bit pixels to output colors and does not change emulated LCD state or timing.

### Contrast presentation control

- Physical context: the real-hardware appearance references above show that screen contrast varies
  with the handheld's contrast wheel, viewing conditions, panel type, and age, but they do not provide
  calibrated shade measurements across wheel positions.
- Implementation impact: the frontend exposes 16 contrast levels from 1 through 16, defaulting to
  8. Level 1 retains one eighth of each ink's distance from the background, and level 8 reproduces
  each selected palette exactly. Levels above 8 move the ink channels toward the selected dark
  endpoint with channel clipping at the format limits. The remaining ink weights for levels 9-16
  are 14/16, 12/16, 10/16, 8/16, 6/16, 4/16, 3/16, and 0, leaving level 15 more visible before
  level 16 reaches the endpoint. The background entry itself is copied byte-for-byte at every level.
  L3 and R3 adjust one level per press and request that the frontend update the visible core-option
  value.
- Limitations: these steps are a useful presentation model, not measurements of the physical wheel's
  electrical or optical response. Contrast affects only frontend color mapping and is excluded from
  machine save states. The D-pad remains available, while the left analog stick maps through a dead
  zone to the same four controller direction bits as an accessibility/default-control convenience.

### Estimated LCD persistence filter

- Physical basis: the Nintendo Life hardware account above reports severe motion blur and at least
  two panel types, while the Museum of Obsolete Media describes the original screen as a green,
  non-backlit monochrome LCD. Neither source provides measured rise/fall response times.
- Local observation: an interactive 1,338-frame Brick Blaster trace captured raw completed pixels,
  VRAM, LCD phase, and cumulative VRAM writes. During gameplay the 120-pixel paddle region at
  `x=64–87, y=132–141` was deliberately absent for one full frame on a repeating nine-frame cadence.
  The same 36 VRAM bytes were explicitly zeroed and restored on the adjacent frames. This establishes
  the redraw artifact being masked; it does not measure the physical panel response. The trace is a
  gitignored local diagnostic trace and contains no ROM or BIOS bytes.
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
  and is accepted only after the same value persists longer than the selected one-, two-, or
  three-frame suppression window. If the prior darker value returns first, the pending lightening is
  discarded. Filtering occurs before optional LCD ghosting, adds no full-frame/audio delay, and
  remains off by default.
- Tradeoff: genuine lightening changes are delayed by the selected number of frames, and moving dark
  objects may leave a corresponding trail. The option is therefore named `Redraw Flicker Reduction` and described as a
  compatibility filter, not hardware emulation. Its presentation-only history is excluded from
  machine save state and reinitialized after reset, content load, successful state load, or an
  on/off option change.
- Validation: purpose-written firmware verifies all three suppression windows, cancellation when the
  dark shade returns, acceptance of sustained lightening immediately after the selected window,
  immediate darkening, and raw behavior after disabling the option.

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

### Libretro Netpacket interface and RetroArch validation

- Upstream interface: <https://github.com/libretro/libretro-common/blob/master/include/libretro.h>
- Vendored interface used by the build: `shared/libretro/libretro.h`,
  `RETRO_ENVIRONMENT_SET_NETPACKET_INTERFACE` and `retro_netpacket_callback`.
- Used for: the two-player session lifecycle, one-peer admission callback, reliable ordered packet
  sending, protocol-version negotiation, legal send-call contexts, and the frontend-managed
  disabling of time manipulation while a packet-based multiplayer session is active.
- Implementation impact: each local `$4C00-$4FFF` UART clock samples the current transmit-data line
  through an original machine callback. The libretro adapter batches those bits with their sender
  master-clock offsets, sends them reliably to the sole peer, and replays received edges against the
  receiver's emulated clock. This preserves separate byte-complete NMI edges instead of injecting a
  whole network packet instantaneously. Connection, queues, clock alignment, and callback pointers
  are transport state outside Gamate save states; reset, unload, and successful state load discard
  queued remote edges. No other emulator implementation supplied this transport code.
- Synthetic validation: the machine test wires one Gamate directly to another and checks the sampled
  bit stream, receiver shift state, byte NMI, and diagnostic counts. The libretro test registers a
  mock Netpacket frontend, rejects a third player, transmits `$A5` through eight real emulated UART
  accesses, receives `$3C`, and replays a timed two-byte `$A5/$5A` packet while verifying that both
  byte NMIs execute independently.
- Cartridge/library validation: the current 1,000-frame, two-BIOS sweep passes 124/124. World Cup
  Soccer and Mighty Boxer exercise the UART unattended, producing 1,136 and 768 transmit clocks
  respectively under either BIOS. World Cup Soccer remains the real-content transport test. Two
  headless RetroArch 1.20.0 instances connected on
  localhost with the current protocol and ran World Cup Soccer for several accelerated emulated
  minutes without connection, protocol-size, or core errors.
- Limitation: the transport is validated locally and preserves sender-relative edge spacing, but a
  complete two-player match across separate physical devices has not yet been played. Internet
  latency is necessarily added before a received frame of edges can be replayed, so game-level
  timeout behavior still needs interactive cross-device validation.

## Build and release infrastructure references

### spruceUI Cores-spruce

- Repository: <https://github.com/spruceUI/Cores-spruce>
- Primary reference workflow: <https://github.com/spruceUI/Cores-spruce/blob/main/.github/workflows/build-superwhizkid.yml>
- Aggregate packaging reference: <https://github.com/spruceUI/Cores-spruce/blob/main/.github/workflows/build-all.yml>
- Toolchain definitions: <https://github.com/spruceUI/Cores-spruce/blob/main/Dockerfile.arm64>,
  <https://github.com/spruceUI/Cores-spruce/blob/main/Dockerfile.armhf>,
  <https://github.com/spruceUI/Cores-spruce/blob/main/toolchain-arm64.cmake>, and
  <https://github.com/spruceUI/Cores-spruce/blob/main/toolchain-armhf.cmake>.
- Used for: the supported ARM64/ARMHF hardware classes, public `toolchains` release assets,
  Cortex-A53 and Cortex-A7 compiler targets, A30/Miyoo ARMHF glibc 2.23 ceiling, Ubuntu Focal ARM64
  glibc 2.31 ceiling, the current spruce package layout (`cores/`, `cores64/`, and `info/`), and
  GitHub Actions error annotations for architecture or missing-output failures. The aggregate workflow
  was used to confirm that its merger accepts those three directories at archive root.
- Implementation impact: the manual release workflow consumes the same public Docker images, checks
  both output architectures and glibc symbol ceilings, packages both cores and metadata in the same
  directory structure, and reports validation failures with explicit `::error::` messages. The GPL
  license remains an additional top-level archive file. Tag replacement and the draft-release lifecycle
  are independently written for this repository's explicit release-tag input.

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

### Audio Output core option

- Source basis: Kevtris's measured Gamate channel routing recorded above. No additional emulator
  implementation was used for this option.
- Implementation impact: `Stereo` remains the default and preserves the emulated PSG output exactly:
  channel A is routed left, B right, and C to both. `Mono` is a libretro presentation-layer option
  that averages the completed left and right samples with equal weights and sends that value to both
  output channels. Averaging rather than summing preserves headroom and prevents the downmix itself
  from clipping. It does not alter PSG state, filtering state, timing, or save states, and is intended
  as the basis for a later built-in-speaker reproduction mode.
- Validation: the purpose-written libretro firmware enables channel A alone. Integration tests verify
  that the default and explicit `Stereo` output have unequal left/right samples, while changing the
  option live to `Mono` produces identical samples on both output channels.

### Recorded built-in-speaker references

- The Retro Future, “Retro Handheld Extravaganza! - Bit Corp Gamate”:
  <https://www.youtube.com/watch?v=29MwdKRClD8>, especially Cube Up at approximately 9:14–10:25.
- ReplayRetro, “Bit Corp Gamate Review”:
  <https://www.youtube.com/watch?v=WdlJGff256g>, especially Cube Up at approximately 12:27–15:00,
  Devil Castle at 15:00–17:58, and Pharaoh Revenger at 17:58–20:03.
- Used for: cross-checking the audible bandwidth and mono, mid-focused character of multiple games
  played through physical Gamate units. Spectral inspection of the marked windows supported retaining
  useful game harmonics through the upper midrange while rolling off the frequency extremes.
- Limitations: these are room recordings made through unknown camera microphones and processing.
  They contain speech, handling noise, room response, microphone coloration, lossy encoding, and
  possibly automatic gain control. The recordings consistently sound overloaded enough to support
  modeling speaker distortion as a real characteristic, but they cannot establish its calibrated
  transfer curve, absolute gain, or unit-to-unit variation. The option description therefore identifies
  the response as estimated, while the concise user-facing value is `Built-in Speaker`.
- Implementation impact: the speaker option begins with the documented equal Mono downmix, applies
  an approximately 300 Hz first-order high-pass, then two approximately 6 kHz first-order low-pass
  stages. It then applies 1.625× drive into a continuous soft knee beginning slightly below half scale,
  progressively compressing loud peaks without hard-clipping discontinuities. This conservative curve
  represents the audible overload without claiming measurement-level precision. Filter history is
  presentation state outside save states and is cleared on mode changes, reset, content load/unload,
  and successful state load.
- Validation: the libretro integration test verifies that this third value is advertised, remains
  dual-mono, differs from the unfiltered Mono samples, switches live, and retains Stereo as default.

## Compatibility reference

### Cartridge corpus inventory

- MAME software list: <https://github.com/mamedev/mame/blob/954def46685cd0276671138fbd032036b1a771fb/hash/gamate.xml>
- Neo Fuji historical catalogue: <https://fuji.12bit.club/gamate/?p=games>
- Accessed: 2026-11-19; local-match inventory refreshed 2026-11-20.
- Used for: establishing the known dumped-image baseline, preserving reported and rumored undumped
  cartridge identities separately, reconciling historical naming/serial disagreements, and matching
  the local collection by full-file SHA-1.
- Result: all 62 local cartridge images match the pinned MAME entries exactly, covering all 62
  catalogued dumps. Neo Fuji is treated as historical catalogue
  evidence rather than dump verification because its page explicitly combines owned material,
  auctions, other websites, and rumors.
- Full inventory and methodology: [`../../docs/ROM_CORPUS.md`](../../docs/ROM_CORPUS.md).

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
  indirect-JMP page-wrap read now has an explicit regression test. Indirect-JMP and both zero-page
  indirect modes explicitly sequence the pointer's low-byte read before its high-byte read, retaining
  NMOS page and zero-page wrapping. Previously, unspecified C operand evaluation order could reverse
  these accesses; focused tests check wrapping and observable open-bus order. This is the documented
  NMOS baseline, not a new physical NCR measurement. All documented official NMOS instruction/
  addressing classes are represented; undocumented opcodes, an externally asserted warm reset, and
  NCR-specific deviations remain outside this evidence-backed audit.

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

### Commercial cartridge header and boot-ABI analysis

- Local origin: all 62 user-supplied cartridge images and both recognized BIOS revisions, read in
  place without retaining ROM or firmware bytes.
- Method: `tools/gamate-rom-header-report.py` reports structural metadata and
  `tools/gamate-rom-layout-report.py` reports trailing fill, uniform banks, aligned duplication, and
  whole-image half mirroring. Focused comparisons additionally examined embedded multicart headers
  and the two Bomb Blaster variants. BIOS code was inspected to identify the cartridge callback
  dispatchers and the alternate UMC `$6002` format-marker check.
- Used for: establishing the commercial header layout, checksum range, `$6020` start stub, `$6023`
  NMI callback, `$6026` IRQ callback, multicart component layout, and corpus padding conventions.
- Result: all 62 checksums validate; all start and IRQ slots are absolute jumps; four games use an
  explicit no-op NMI callback; and the 4-in-1 contains four independently valid component headers.
  No dump is a systematic half mirror. Four 512 KiB games have terminal uniform banks, while Hot
  Hero has the corpus's only repeated nonuniform regions at 16/32 KiB granularity. Bytes
  `$6003-$6004` and `$601E-$601F` remain unexplained and are not assigned semantics.
- Header and boot-ABI findings: [`../../docs/ROM_HEADER_ANALYSIS.md`](../../docs/ROM_HEADER_ANALYSIS.md).
- Capacity, padding, and repetition findings:
  [`../../docs/ROM_LAYOUT_ANALYSIS.md`](../../docs/ROM_LAYOUT_ANALYSIS.md).

### Commercial software-library comparison

- Local origin: the fixed first 16 KiB bank from all 58 user-supplied cartridge images, read in
  place without retaining ROM bytes.
- Method: `tools/gamate-rom-shared-report.py` indexes overlapping high-information 32-byte windows,
  excludes ubiquitous matches, and merges windows unique to both images into exact spans. Header
  callback tuples from `tools/gamate-rom-header-report.py` provide an independent ABI comparison.
- Used for: identifying candidate shared engines, libraries, assets, and build lineages to prioritize
  for later disassembly.
- Result: strong distinct-title reuse candidates include Brick Blaster/Enchanted Bricks, Mighty
  Tank/Vindicators, Treasure Hunter/Witty Apee, and a dense network among later large-ROM games.
  Matches remain classified as code-or-data until disassembly and reachability analysis establish
  their role.
- Full findings and limitations:
  [`../../docs/SOFTWARE_LIBRARY_ANALYSIS.md`](../../docs/SOFTWARE_LIBRARY_ANALYSIS.md).
- Complete-corpus extension: the same independently authored fixed-bank comparison was rerun after
  four locally dumped, MAME-hash-matched cartridges completed the 62-image catalogue. No new pair
  reached the original 500-window summary threshold. Mighty Boxer shares a consistently placed
  288-byte block with members of the existing later-game library network; Kiki Inland's strongest
  match is a 257-byte same-offset post-header span with Box Forum; Fortress of Fierceness has only
  smaller fragmented candidates; and Incantational Couple has no unique exact span under this
  method. Used to add the bounded extension analysis and next classification targets to
  `docs/SOFTWARE_LIBRARY_ANALYSIS.md`.
- Extension limitations: selectively shared-window counts depend on the complete owner set and are
  similarity scores rather than byte totals. Exact spans may be code or data. Header-adjacent or
  consistently placed matches suggest a shared startup/library package but do not establish an
  engine family until disassembly and dynamic reachability confirm their role.

### ROM graphics exploration tool

- The local `rc2tools` project's `rc2t render-graphics` command was consulted as a presentation and
  command-line workflow reference for `tools/gamate_rom_render_graphics.py`. It informed the use of
  full-ROM images with bank sections, address labels, visually separated groups, range selection,
  and nearest-neighbor scaling. Its implementation and Game Boy graphics decoder were not copied or
  translated. Superwhizkid's decoder was independently authored around the Gamate plane behavior
  documented by Kevtris, and exposes both planar and row-interleaved ROM-storage hypotheses because
  no source currently establishes either one as the commercial cartridge convention.
- Kevtris's “Gamate Inside” measurements were used to assign bit 7 to the leftmost pixel and combine
  plane 0 with weight one and plane 1 with weight two. The measurements describe framebuffer access,
  not ROM graphics storage; the tool and `docs/ROM_GRAPHICS_ANALYSIS.md` preserve that limitation.

### Authentic BIOS revision comparison

- Local origin: the recognized UMC/NCR and Bit Corporation 4 KiB BIOS images, identified by the
  hashes in `docs/ROM_CORPUS.md`, were read in place. No firmware bytes, listings, or patches were
  retained in the repository.
- Method: byte equality, contiguous changed ranges, page density, exact matching-block relocation,
  vector/control-flow inspection, and controlled execution of the same cartridge under both images.
- Used for: `docs/BIOS_DIFFERENCE_ANALYSIS.md`, including the 79.05% byte identity, localization of
  nearly all differences to `$FC2F-$FFFF`, reset/vector comparison, relocated validation and
  interrupt routines, the BIT revision's revised failure paths, and the opaque UMC-only regions.
- Validation: both images reach `$6020` with identical initialized RAM, VRAM, scrolling, LCD control,
  mapper, and IRQ phase in the controlled comparison. The BIT path takes two additional instructions
  and 29 CPU cycles, preserving the previously documented 58-master-clock entry-phase difference.
- Legal boundary: only derived addresses, sizes, hashes, behavioral descriptions, and aggregate
  statistics were committed. Raw comparison output, firmware bytes, full disassembly, embedded
  graphics, and opaque data were deliberately excluded.
- The behavioral results were restated in `docs/BIOS_BOOT_BEHAVIOR.md` as normalized pseudocode.
  The document retains externally relevant stages, conditions, state effects, entry/callback
  addresses, and known revision branches, but deliberately omits instruction order, register
  allocation, private temporary layouts, delay/drawing implementations, and reconstructable data.

### BIOS behavioral-documentation boundary

- [17 U.S.C. § 102](https://www.law.cornell.edu/uscode/text/17/102), particularly § 102(b), was used
  for the distinction between functional processes/methods of operation and program expression.
- U.S. Copyright Office,
  [Circular 61: Copyright Registration of Computer Programs](https://www.copyright.gov/circs/circ61.pdf),
  was used to corroborate that algorithms, functions, logic, formatting, and system design are
  functional aspects, while copyright can protect program expression.
- [17 U.S.C. § 1201(f)](https://www.law.cornell.edu/uscode/text/17/1201) was consulted for its
  conditional treatment of reverse engineering and sharing information necessary for
  interoperability of an independently created program.
- These legal sources affected documentation scope only. They were not used to claim categorical
  legality or to broaden the project's access, copying, or distribution permissions. The resulting
  policy is to publish normalized behavioral facts and exclude firmware bytes, patches, full
  disassembly, and source-like reconstruction.

### Cartridge use of the BIOS callback flag

- Local origin: all 58 user-supplied cartridge images were executed for 1,000 post-entry frames under
  each recognized BIOS revision with no input. Inputs remained read-only and no ROM, BIOS, or trace
  bytes were retained in the repository.
- Method: a temporary instrumented Superwhizkid build outside the repository recorded executed CPU
  reads and writes whose effective RAM address maps to physical cell `$000C`. A preliminary raw ROM
  pattern scan found 2,946 apparent instruction patterns in 53 titles and was rejected as evidence
  because graphics/data and false instruction boundaries dominated it.
- Result: Bao Qing Tian, Basketball, Dinosaur Park, Riddle of the Ancient Tomb, Robin Hood, and The
  Golden Pyramid accessed the flag during the observed window. All six clear or preserve/restore it
  during cartridge startup RAM initialization. Basketball later changes `$01` to `$00`, then to
  `$07` from bank-9 code. Both BIOS revisions produced the same transition counts.
- Used for: the callback-flag section in `docs/BIOS_BOOT_BEHAVIOR.md` and the conclusion that the
  interrupt wrappers interpret zero as disabled and any nonzero value as enabled. No additional
  semantics are assigned to Basketball's `$07` without further code analysis.
- Limitation: no access in 1,000 unattended frames does not establish non-use in later, gameplay, or
  input-dependent paths. The execution scan establishes observed accesses, not complete static
  reachability.

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
  [`../../docs/BIOS_REPLACEMENT_ANALYSIS.md`](../../docs/BIOS_REPLACEMENT_ANALYSIS.md) and its conclusion
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
  [`../../docs/BIOS_REPLACEMENT_ANALYSIS.md`](../../docs/BIOS_REPLACEMENT_ANALYSIS.md).
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
  [`../../docs/BIOS_ENTRY_RAM_ANALYSIS.md`](../../docs/BIOS_ENTRY_RAM_ANALYSIS.md); replacement
  implications are summarized in
  [`../../docs/BIOS_REPLACEMENT_ANALYSIS.md`](../../docs/BIOS_REPLACEMENT_ANALYSIS.md).
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

### Post-entry BIOS runtime API analysis

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
  [`../../docs/BIOS_RUNTIME_API_ANALYSIS.md`](../../docs/BIOS_RUNTIME_API_ANALYSIS.md): `$F000`
  fixed bitmap writer; `$F003`, `$F006`, and `$F009` IRQ waits; `$F00C` two-plane VRAM zero-writer
  with an LCD-control input; `$F00F` plane-0 rectangle clear; `$F012` selector-driven eight-row tile
  writer; `$F015` two-plane rectangle copy; `$F018` tile-row blitter; `$F01B` partially bounded
  ROM-driven plane-0 graphics writer; `$F01E` partially bounded two-plane graphics service; `$F021`
  partially bounded ROM-backed plane-0 graphics service; `$F024` strided RAM initializer; `$F02A`
  plane-0 fill; `$F02D` plane-1 fill; `$F030` pseudorandom service; and `$F033` packed-BCD formatter.
  `$F027` remains caller-dependent; `$F00C`'s bit-6 addressing mode, `$F01B`'s source-data format,
  `$F01E`'s stream format, and `$F021`'s source-data format remain unresolved.
- Used for: the replacement-ABI contracts, the 1,000-frame sweep default, and the conclusion that
  replacement firmware needs resident service and interrupt code, not just an entry-state jump.
  The linked analysis owns per-service matrices, exact behavior, and probe-specific caveats.
- Limits: this is behavioral evidence from the project-authored emulator, not physical measurement.
  Finite corpus and input schedules do not establish exhaustive reachability; link/NMI paths and some
  caller-specific or timing details remain open. No external emulator implementation was consulted
  or copied.
- `$F027` follow-up: IRQ-masked `occurrence=1` probes covered 14 additional titles under both BIOS
  revisions; both authentic occurrences in Incantational Couple were also compared. A controlled
  matrix toggled `$006-$009` and zeroed `$023/$028/$030` in five varied titles and both Incantational
  calls. The fields were read-before-write and unwritten in these contexts, but their effects varied
  by caller and occurrence; this supports retaining the caller-dependent label rather than inventing
  a shared format. Address watches found `$006/$007` read twice as often as `$008/$009`, with equal
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
  Findings do not establish the remaining stream grammar or a general blend rule. No pixel, ROM,
  or BIOS payload was retained; no external implementation was consulted or copied. Exact results,
  call scopes, and limits are in
  [`../../docs/BIOS_RUNTIME_API_ANALYSIS.md`](../../docs/BIOS_RUNTIME_API_ANALYSIS.md).
- `$F00C` follow-up: `occurrence=1` probes covered 57 titles under both BIOSes with I set and VRAM
  seeded to `$FF`; controlled `$015` bit patterns and A/X/Y changes were tested in Baseball, and the
  bit-6 path was checked in Dino Ball and Volcano Panic. Time Warrior's authentic second occurrence
  was separately probed under both revisions. The tests found a 12,800-write two-plane zeroing path,
  LCD-control effects from `$015`, and a distinct sparse write pattern when bit 6 is set. Four
  paired whole-RAM hashes differed despite matching checked access sets and all other reported
  outputs; their cause is unresolved. The probe streamed VRAM values into aggregate counters and
  retained no payload. A Baseball matrix of combined `$015` values `$00`, `$3F`, `$40`, `$41`,
  `$50`, `$60`, `$C0`, `$7F`, and `$FF` was expanded to a full `$00-$FF` sweep under both BIOSes
  at the first Baseball occurrence (512 probes). All values matched the additive LCD-flag mapping,
  returned-A rule, status/register signature, timing, write counts, final pointer, and the expected
  contiguous or sparse VRAM coverage; there were no cross-revision non-hash field or coverage
  mismatches. Whole-RAM hashes also matched in this matrix. The bit-6 stride's hardware meaning
  and its behavior across every caller context remain open. The probe retained only derived
  results, not VRAM or ROM payload. Exact scopes and limits are in
  [`../../docs/BIOS_RUNTIME_API_ANALYSIS.md`](../../docs/BIOS_RUNTIME_API_ANALYSIS.md).
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
  [`../../docs/BIOS_RUNTIME_API_ANALYSIS.md`](../../docs/BIOS_RUNTIME_API_ANALYSIS.md).
- `$F01E` follow-up: the paired balanced-call corpus contains 216 calls across 18 titles. First-call
  probes in Famous 7, Time Warrior, Volcano Panic, and Robin Hood were compared under both BIOSes
  with I set; register changes, RAM XOR perturbations, and address-only ROM coverage were used to
  bound the caller inputs. In Time Warrior, `$000-$001` select a ROM-side control/descriptor region,
  `$006-$007` and `$008-$009` feed separate plane outputs, and `$015` affects LCD flags. VRAM output
  was streamed to per-plane hashes without retaining payload. The stream grammar, return-Y rule,
  status behavior, and BIOS-specific whole-RAM hash differences remain unresolved. Exact results
  and limits are in
  [`../../docs/BIOS_RUNTIME_API_ANALYSIS.md`](../../docs/BIOS_RUNTIME_API_ANALYSIS.md).
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
  [`../../docs/BIOS_RUNTIME_API_ANALYSIS.md`](../../docs/BIOS_RUNTIME_API_ANALYSIS.md).
- Clean-room boundary: BIOS and cartridge files remained read-only. No BIOS/ROM bytes, listings,
  decoded graphics, RAM/VRAM payloads, or ROM-derived mapping tables were retained or published;
  only behavioral descriptions and derived addresses, counts, and comparisons were used.

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
- Current post-entry validation runs every locally available cartridge for 1,000 frames under both
  BIOS revisions. The diagnostic observes changing framebuffer hashes, nonblank output, IRQ service,
  LCD/VRAM traffic, controller reads, and expected bank changes, with no unknown opcodes or persistent
  self-loop traps. Frame hashes are computed from four-shade pixel indices and contain no ROM or BIOS
  data. See `docs/COMPATIBILITY_SWEEP_ANALYSIS.md`.
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
- `tools/gamate-sweep.sh` and equivalent direct read-only invocations covered all 62 locally supplied
  cartridges under both validated BIOS revisions for 1,000 frames after `$6020`: 124/124 combinations
  booted and produced nonblank video with no unsupported opcode or persistent self-loop diagnosis.
  Cartridge sizes covered 16, 32, 64, 128,
  256, and 512 KiB, including the fingerprinted 4-in-1 mapper. The report remains outside the
  repository and contains local filenames, hashes, and counters but no input bytes. All titles
  performed controller reads in this window. World Cup Soccer and Mighty Boxer exercised the UART;
  the latter produced 768 clocks under either BIOS. See `docs/COMPATIBILITY_SWEEP_ANALYSIS.md`. This
  is broad automated smoke coverage, not a claim that every title is playable or hardware-accurate.
- Tornado was the sole title without audible output during the unattended 1,000-frame sweep. It was
  investigated with per-register AY/channel and controller-read diagnostics. At its title state
  it deliberately rewrites registers 0–13 in sequence, leaving periods and volumes zero and mixer
  register 7 at `$3F`; all three synthesized channels are consequently inactive. A targeted trace
  found that the game polls controls sparsely (25 reads in 400 frames), so a six-frame Start pulse
  could be missed. A 20-frame Start pulse at frame 120 was observed once, entered gameplay under both
  BIOS revisions, and yielded
  audio in 270/400 frames with all three channels active and a peak of 30,558. Tornado's initial
  silence is therefore title-state behavior, not evidence of an AY synthesis fault.
- Further Adventures of Hannibal The Cat first produced unattended audio at frame 357 and was audible
  for 432 or 430 of 1,000 frames, depending on BIOS entry phase. A targeted 30-frame Start pulse at
  frame 120 entered gameplay under both BIOS revisions and produced audio on all three channels; an
  A-only pulse did not leave the title flow.
- Famous 7, One Million Whys, and QuizFighter first produced audio at frame 277 under both BIOS
  revisions and were respectively audible for 724, 513, and 521 frames in the current 1,000-frame
  UMC/NCR run. This strongly suggests a shared title sequence or engine timing (an inference from
  matching behavior, not a source-derived claim). Famous 7 and QuizFighter do not begin controller
  polling until frame 564; a 30-frame Start pulse at frame 600 was observed and changed their
  subsequent execution. One Million Whys polls from frame 2 and observed the same pulse, but remained
  in its current flow. The delayed observations are title sequencing, not evidence of an audio or
  input fault.
- The two BIOS-dependent final-video cases, Mini Golf and Money Maze, were isolated to inherited
  entry phase rather than memory, mapper, or rendering nondeterminism. At `$6020`, both BIOSes leave
  identical RAM and VRAM hashes, scroll/LCD control, and IRQ phase. The BIT BIOS arrives 29 CPU
  cycles (58 master clocks) later, moving the progressive LCD from row-event/clock `362/128` to
  `363/24` and shifting frame and AY oscillator/sample phase. A diagnostic-only experiment reset
  clock, LCD, IRQ, and AY phase at entry while preserving CPU, memory, registers, and cartridge
  state. Under that normalization, each game produced identical video/audio hashes, PCs, and
  activity counters across BIOSes through the targeted 300-frame experiment. That shorter result is
  retained because it describes the phase-normalization experiment rather than general catalog
  coverage. The ordinary differing hashes therefore sample slightly different execution instants
  because the diagnostic numbers frames from cartridge entry;
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
  applies its restored mask immediately. The current 58-cartridge/two-BIOS/1,000-frame matrix is
  116/116. This validates compatibility and the tested CPU boundary cases, but not
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
- Redraw-flicker reduction was tuned from the core's own gitignored diagnostic trace, captured during
  a user-played Brick Blaster session; no external emulator implementation was consulted for this
  filter. In the title-selection cursor region (raw LCD coordinates x=44-51, y=104-111), its 41
  non-background pixels were repeatedly absent for one or two frames between longer drawn runs.
  This evidence was used only to choose the optional presentation filter's one- and two-frame
  lightening-confirmation limits. It did not change Gamate machine timing or LCD-controller output.
- The performance pass used local GNU `gprof` sampling of a 2,000-frame Brick Blaster run, not code
  from another emulator. It identified `gamateRunClocks` as roughly 75% of sampled runtime before
  optimization. A 5,000-frame run dropped from 6.32 to 4.72 seconds on the development system after
  batching master-clock work to the next observable AY/audio/IRQ/LCD/frame event. Before/after
  1,000-frame `gamateboot` reports for Brick Blaster, Tornado, Money Maze, and Mini Golf were compared
  byte-for-byte; their video/audio hashes, machine diagnostics, and reported state remained exact.
  These measurements justified only event batching, an avoidable-ROM-modulo guard, and a direct
  raw-palette frontend path when both presentation filters are disabled; no timing was omitted.
- A second profile after link integration still placed clock-event processing first. AY tone, noise,
  and envelope periods are now derived once on reset/load and their relevant register writes instead
  of being rebuilt on every instruction's clock advance. They remain derived runtime values outside
  the state format. This reduced the same 5,000-frame Brick Blaster benchmark from about 4.80 to 4.69
  seconds with exact output. CMake's official `CheckIPOSupported` module documentation
  (<https://cmake.org/cmake/help/latest/module/CheckIPOSupported.html>) was used while evaluating
  release IPO/LTO. Once the event-free fast path was present, otherwise identical local GCC builds
  measured about 4.29 seconds both with and without IPO. It was therefore not enabled in the final
  build, avoiding extra release-toolchain risk for no demonstrated gain. No source-level emulator
  implementation was consulted.
- The same profile motivated an event-free fast path for the overwhelmingly common case where one
  CPU instruction crosses no AY, audio-sample, IRQ, LCD-row, or frame boundary. It advances exactly
  the same counters once and falls back to the ordered event loop at every boundary. Together with
  cached AY periods, the current release build reduced the 5,000-frame Brick Blaster benchmark from
  about 4.80 to 4.29 seconds (approximately 10.6%). The batched-versus-single-clock equivalence test,
  four byte-identical 1,000-frame game reports, and the complete 116-case/two-BIOS 1,000-frame sweep
  all passed after this second pass. An undefined-behavior sanitizer run also identified a pre-existing
  signed left shift in the decimal-SBC wrap calculation; it was replaced with an explicit eight-bit
  conversion and the exhaustive decimal test's independent reference calculation was updated to
  express the same defined wrap. Normal and debug suites retain the same exhaustive results.
- A follow-up local `gprof` profile of the production machine core used 5,000 unattended frames
  including boot in Brick Blaster and World Cup Soccer with the UMC BIOS. `gamateRunClocks`
  accounted for approximately 61% and 55% of sampled time, respectively; sampling and compiler
  inlining limit the attribution. The event loop now bounds the step by other device events before
  calculating the next sample deadline, and divides only if that step can reach the sample. Event
  processing order and serialized state remain unchanged. Batched-versus-single-clock tests cover
  simultaneous device events, pending NMI, in-flight UART, sample phases, and sample rates from 1
  through the maximum 32-bit value. Sixteen 1,000-post-entry-frame comparisons across eight titles,
  both BIOSes, the 32/64/128/256/512 KiB size classes, and the 4-in-1 matched full-frame
  video/audio hashes, activity counts, PC, and serialized-state hashes. Baseline and optimized
  1,000-frame reports also matched byte-for-byte across the 62-cartridge/two-BIOS sweep; both
  completed 124/124 cases. After correctness checks, seven alternating uninstrumented
  5,000-post-entry-frame benchmark pairs with no input and the UMC BIOS measured
  median times of 4.2326 versus 4.2012 seconds for Brick Blaster and 4.3087 versus 4.2623 for World
  Cup Soccer, improvements of 0.74% and 1.08%. These small gains are host-specific and do not
  establish ARM performance. This work used local experiments and no external emulator source.
- A further local performance experiment grouped pixels by source byte within each existing LCD
  row-latch event, without changing the three-pass timing or per-pixel diagnostic read accounting.
  The existing physical LCD measurements above specify the row and plane behavior; no external
  emulator implementation was consulted or copied. Synthetic progressive-row tests compare against
  the independent snapshot renderer for all 256 horizontal scroll values in nine combinations of
  row, vertical scrolling, window, plane swap, and blanking, including byte and row wrapping. They
  also check diagnostic read counts and read-before-write coverage for unaligned scrolling. Sixteen
  1,000-post-entry-frame comparisons across eight titles, both BIOSes, the 32/64/128/256/512 KiB
  size classes, and the 4-in-1 matched full-frame video/audio hashes, activity counts, PC, and
  serialized-state hashes. Baseline and optimized 1,000-frame sweep reports matched byte-for-byte
  across all 62 titles and both BIOSes, with 124/124 cases passing in each. Three titles under both
  BIOSes also produced identical 1,000-frame reports with VRAM coverage diagnostics enabled.
  LCD-focused tests passed with undefined-behavior sanitization; an `-O1` full-unit sanitizer run
  failed the indirect-JMP/open-bus assertion on both the original and modified cores without a
  sanitizer diagnostic. The later CPU bus-order correction described above resolves this
  compiler-dependent ordering issue. The standard release tests passed. After correctness checks,
  seven alternating uninstrumented 5,000-post-entry-frame benchmark pairs with no input and the UMC
  BIOS measured median times of
  4.1855 versus 3.8319 seconds for Brick Blaster and 4.2678 versus 3.9250 seconds for World Cup
  Soccer, improvements of 8.45% and 8.03%. These local measurements do not establish ARM
  performance or additional physical accuracy.
- A local `gprof` profile of 5,000 unattended post-entry Brick Blaster frames with the UMC BIOS
  attributed approximately 53.6% of sampled time to `gamateRunClocks`, 24.3% to `gamateCpuStep`, and
  14.7% to `gamateRead`; sampling and inlining limit attribution. The read dispatcher now checks
  cartridge and BIOS ranges before the lower-address peripheral ranges, retaining RAM first and
  preserving protection, mapper, diagnostic, open-bus, and peripheral effects. Focused synthetic
  tests cover the affected region boundaries, and the CPU pointer-read ordering correction above
  fixes a separate compiler-dependent bus-order issue. Sixteen 1,000-post-entry-frame comparisons
  across eight titles, both BIOSes, the 32/64/128/256/512 KiB size classes, and the 4-in-1 matched
  video and audio hashes accumulated for every frame, activity counts, PC, and final serialized-state
  hashes. Baseline and candidate 1,000-frame sweep reports matched byte-for-byte across all 62
  titles and both BIOSes, with 124/124 cases passing in each; three titles with both BIOSes also
  matched with RAM and VRAM coverage enabled. The standard tests, the external 6502 functional test,
  and the full `-O1` undefined-behavior sanitizer unit run passed. After correctness checks, seven
  alternating uninstrumented 5,000-post-entry-frame benchmark pairs with no input and the UMC BIOS
  measured median times of 3.8327 versus 3.6208 seconds for Brick Blaster and 3.9225 versus 3.6934
  for World Cup Soccer, improvements of 5.53% and 5.84%. The local experiments used read-only
  user-supplied BIOS and cartridges without retaining their payloads, and no external emulator
  source was consulted or copied. These results do not establish ARM performance, exhaustive
  compatibility, or new physical hardware accuracy.
- A frontend presentation pass used the purpose-written libretro integration workload, which cycles
  raw output, every ghosting level, every redraw-flicker level, live option transitions, palettes,
  and contrast states. The filtered renderer now has separate flicker-only and ghosting-only loops;
  the raw path seeds filter history once when a filter is enabled instead of rewriting two history
  buffers every frame. Two interleaved ten-run samples measured 14.678/14.628 seconds before and
  14.527/14.585 seconds after on the development system, about a 0.7% improvement for the complete
  mixed emulation-and-presentation test rather than a display-only claim. A conditional test hash
  covered all 487 delivered frames and matched exactly at `6C6C28E5` before and after. No external
  emulator implementation or source code informed these changes.

## Still needed

- CPU validation traces or tests specifically for the NCR 65CX02, especially decimal mode and the
  reported bit-manipulation instructions.
- Direct test ROMs or logic-analyzer traces for the within-sequence IRQ/BRK clearing clock and LCD
  mid-frame writes.
- Identification data for the 4-in-1 cartridge that does not depend only on a filename/database.
