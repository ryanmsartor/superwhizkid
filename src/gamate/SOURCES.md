# Gamate implementation sources

This file records the sources used while developing superwhizkid, what each source was
used for, and important disagreements between them. It should be updated whenever a new source
influences implementation or tests.

External hardware, implementation, frontend, build, audio, compatibility, and CPU references are
followed by local cartridge and BIOS analysis, emulator validation, and performance experiments.
Local hardware-behavior results are emulator observations unless identified as physical measurements.

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
- Additional tuning: the core's own gitignored trace of a user-played Brick Blaster session showed
  that 41 non-background pixels in the title-selection cursor region (raw LCD coordinates x=44-51,
  y=104-111) were repeatedly absent for one or two frames between longer drawn runs. This evidence
  was used only to choose the filter's one- and two-frame lightening-confirmation limits. It did
  not change Gamate machine timing or LCD-controller output. No external emulator implementation
  was consulted for this filter.
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
  A separate Ubuntu-host step installs cc65, builds the 4 KiB independently authored
  `gamate_bios_swk.bin` from `gamate_bios.s`, runs the synthetic prototype target, and verifies the
  exact 4,096-byte output size before copying it into `output/bios/`. The spruce archive includes
  `bios/gamate_bios_swk.bin` alongside `cores/`, `cores64/`, and `info/`. This packages a documented
  synthetic/incomplete replacement experiment, not an authentic, commercial, or substitute firmware
  image.

- Three-instance interactive differential method: [tools/gamate-three-bios-diff.py](../../tools/gamate-three-bios-diff.py)
  isolates UMC, Bit, and SWK in separate RetroArch system/config/state/screenshot directories.
  The selected authentic or experimental BIOS is linked from its local read-only source rather than
  copied or embedded. The launcher records ROM/core/BIOS SHA-256 values and per-instance priorities
  in a temporary manifest, broadcasts keyboard inputs and control commands, optionally records or
  replays RetroArch `.bsv` input, and writes save-state/screenshot snapshots derived from the three
  processes. It does not retain ROM or BIOS bytes in reports, and replay/snapshot artifacts stay under
  `/tmp`. `xdotool` is required for interactive window broadcast; replay-only use does not broadcast
  input. This tool is an interactive diagnosis aid, not proof of physical timing or identical frame
  alignment.

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
  as the basis for the built-in-speaker reproduction mode described below.
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
- Sanitizer follow-up: a local undefined-behavior sanitizer run identified a pre-existing signed
  left shift in the decimal-SBC wrap calculation; it was replaced with an explicit eight-bit
  conversion and the exhaustive decimal test's independent reference calculation was updated to
  express the same defined wrap. Normal and debug suites retain the same exhaustive results.

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

## Local cartridge analysis and tools (files not distributed)

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

## Local BIOS analysis (files not distributed)

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
  The experimental firmware uses that layout with a purpose-written blank placeholder; its pixels
  do not reproduce the authentic BIOS asset. The observation is emulator-based and does not
  establish the bitmap's meaning or physical display appearance.
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
  [`ROM_HEADER_ANALYSIS.md`](../../docs/ROM_HEADER_ANALYSIS.md), boot and replacement analyses
  supplied the exact header field, checksum range, entry boundary and candidate RAM policy; and
  [`BIOS_RUNTIME_API_ANALYSIS.md`](../../docs/BIOS_RUNTIME_API_ANALYSIS.md) supplied the observed
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
  [`EXPERIMENTAL_BIOS.md`](../../docs/EXPERIMENTAL_BIOS.md) for build instructions and scope.

#### Corpus and general method

- SWK startup timing: the reset routine now performs 36 repeated 24-IRQ waits plus one 20-IRQ wait
  before clearing VRAM and continuing initialization to `$6020`. Brick Blaster measured entry at
  `30,228,402` master clocks, while UMC measured `30,239,372` and Bit `30,239,430`. All three are
  in frame floor `414` of the 72,900-clock frame model, within `109,970` master clocks of UMC.
  This is emulator-observed timing work, not a physical-hardware measurement or claim of exact
  frame-aligned BIOS timing. Logo rendering and image assets are not retained as part of this result.

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
  174. Input `$8000` produces zero, correcting the earlier nonzero-output assertion. Enumerating
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
    `docs/BIOS_RUNTIME_API_ANALYSIS.md`.
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
  [`../../docs/BIOS_RUNTIME_API_ANALYSIS.md`](../../docs/BIOS_RUNTIME_API_ANALYSIS.md).
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
  [`../../docs/BIOS_RUNTIME_API_ANALYSIS.md`](../../docs/BIOS_RUNTIME_API_ANALYSIS.md).
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
  [`../../docs/BIOS_RUNTIME_API_ANALYSIS.md`](../../docs/BIOS_RUNTIME_API_ANALYSIS.md).
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
  [`../../docs/BIOS_RUNTIME_API_ANALYSIS.md`](../../docs/BIOS_RUNTIME_API_ANALYSIS.md).
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
  [`../../docs/BIOS_RUNTIME_API_ANALYSIS.md`](../../docs/BIOS_RUNTIME_API_ANALYSIS.md).

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
  [`../../docs/BIOS_RUNTIME_API_ANALYSIS.md`](../../docs/BIOS_RUNTIME_API_ANALYSIS.md).
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
    [`../../docs/BIOS_RUNTIME_API_ANALYSIS.md`](../../docs/BIOS_RUNTIME_API_ANALYSIS.md). It covers:
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
  [`../../docs/BIOS_RUNTIME_API_ANALYSIS.md`](../../docs/BIOS_RUNTIME_API_ANALYSIS.md).

- `$F01E` follow-up: the paired balanced-call corpus contains 216 calls across 18 titles. First-call
  probes in Famous 7, Time Warrior, Volcano Panic, and Robin Hood were compared under both BIOSes
  with I set; register changes, RAM XOR perturbations, and address-only ROM coverage were used to
  bound the caller inputs. In Time Warrior, `$000-$001` select a ROM-side control/descriptor region,
  `$006-$007` and `$008-$009` feed separate plane outputs, and `$015` affects LCD flags. VRAM output
  was streamed to per-plane hashes without retaining payload. The stream grammar, return-Y rule,
  status behavior, and BIOS-specific whole-RAM hash differences were unresolved at that stage.
  The follow-up above addresses them for the tested domains. Exact results and limits are in
  [`../../docs/BIOS_RUNTIME_API_ANALYSIS.md`](../../docs/BIOS_RUNTIME_API_ANALYSIS.md).
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
  [`../../docs/BIOS_RUNTIME_API_ANALYSIS.md`](../../docs/BIOS_RUNTIME_API_ANALYSIS.md).

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
    [`../../docs/BIOS_RUNTIME_API_ANALYSIS.md`](../../docs/BIOS_RUNTIME_API_ANALYSIS.md).
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

## Local emulator validation (files not distributed)

### Boot and cartridge identification

The following user-supplied files were read in place for the original boot validation. They are not
copied into the repository, build output, tests, or releases.

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

### Audio validation

- Audio validation over the same representative set observed PSG register writes and nonzero output
  without clipping. `gamatetest` separately verifies tone timing/sample count, noise-LFSR advance,
  envelope restart/advance, register masks, A-left isolation, B-right isolation, and equal C routing
  to both sides. `gamateboot` records deterministic per-frame audio hashes without storing samples.

### Progressive LCD validation

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

### Frontend and synthetic integration validation

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
  firmware. The harness now generates identifiable UMC/Bit/SWK variants plus a second synthetic BIOS
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
    were written from the executable contracts in `docs/BIOS_RUNTIME_API_ANALYSIS.md`. Those
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
  - **Withdrawn timing experiments.** `$F02A/$F02D` were probed with a removed final
    `LDA $1A`; clean 2×3, 256×1, 1×256 and 1×1 shapes were four master clocks high in both
    routines. Removing the load moved 1×1 from 202/196 to 200/194 and the shortest neutral 6502
    instruction could not supply the required two-master-clock correction in isolation, so the
    removal was retained only after the 1×1 and 1×256 real-call probe. A direct synthetic rerun
    then resolved the 256-wrap values: `$F02A` measured 19,582 for 256×1 and 4,792 for 1×256;
    `$F02D` measured 19,574 and 4,784. Plane 0 follows `126 + 58*c + 18*c*r`; plane 1 follows
    the same expanded form with an additive 118 in 256-wrap shapes. These are retained as exact
    SWK synthetic shape assertions. Ordinary plane-1 timing uses base 120. 1×1 authentic residuals and broad interrupt
    timing remain open. `$F030` branch NOP padding raised
    a zero-state synthetic mismatch to 220 clocks, so it was reverted. Later NOP/branch attempts
    measured 190 and 238 for the zero state while the documented target remained 188, so they were
    also reverted. `$F033` ordinary/Y=`$FF`
    padding blocks produced branch-dependent mismatches and were reverted. `$F027` shape/delta
    instrumentation found 139 of 141 scheduled Cube Up calls with 4 background and 4 sprite records
    at a 30,588-clock shortfall, plus 0/3 and 0/8 classes with 62,544 and 53,684 shortfalls. No
    global or shape-only NOP constant can be inferred from that corpus distribution. Temporary
    diagnostics and all speculative code changes were removed; these attempts are recorded as
    negative evidence, not implementation facts.
  - **Interactive follow-up.** A RetroArch session loaded the rebuilt core with explicit SWK
    priority and ran the standalone Cube Up cartridge for 12 minutes 36 seconds. Gameplay began
    successfully, and the user-observed gameplay, score digits and piece behavior appeared correct
    with no reported freeze or halt. This is a qualitative, emulator-only session; it was not a
    complete playthrough and no intermediate frames or state were retained.
  - **Limitations.** Timing differs; `$F027` is about 1.6× slower on average. Stack layout and
    decimal-mode entry are not modeled. Only Cube Up was exercised, and no physical hardware was
    used. Reports, harnesses and traces stayed under `/tmp` and retained no ROM or BIOS bytes.

### Corpus sweep and title-specific validation

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

### CPU interrupt validation

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
  applies its restored mask immediately. The then-current 58-cartridge/two-BIOS/1,000-frame matrix was
  116/116. This validates compatibility and the tested CPU boundary cases, but not
  the NCR 65CX02's unmeasured interrupt-vector bus cycle.
- BRK cartridge-callback matrix: purpose-written BRK and `$6026` callback programs were installed
  only in private in-memory cartridge copies, never in local ROM/BIOS files. The
  `gamatebiosbrk` checker ran 65,536 cases for each of UMC, Bit, and the independently authored SWK
  BIOS: all 64 P combinations, all 256 S values, callback enabled and disabled, and direct plus
  absolute-JMP callback forms. A controlled `S=$FF` capture checked the BRK pushed status, low PC,
  and high PC; the matrix checked PC/stack restoration, entry P to post-RTI P, entry A/Y restoration,
  callback-modified X, `$0B` callback-counter behavior, and zero added hardware-IRQ/NMI diagnostics.
  This establishes emulator-level BRK vector and callback behavior against both authentic revisions
  and the replacement BIOS. It does not establish physical BRK/timer side effects, arbitrary callback
  reentrancy, callback-modified stack aliases, or nested interrupts. No authentic or commercial
  firmware instruction sequence or cartridge payload was copied or retained.

### LCD boundary validation

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

### UART validation

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

## Local performance experiments and validation

### Clock and AY scheduling

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
  cached AY periods, the release build at the time reduced the 5,000-frame Brick Blaster benchmark from
  about 4.80 to 4.29 seconds (approximately 10.6%). The batched-versus-single-clock equivalence test,
  four byte-identical 1,000-frame game reports, and the complete 116-case/two-BIOS 1,000-frame sweep
  all passed after this second pass.
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

### LCD row expansion

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

### CPU and bus dispatch

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

### Frontend presentation

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
- Native hardware identification for the 4-in-1 cartridge, independent of the current whole-image
  fingerprint policy.
