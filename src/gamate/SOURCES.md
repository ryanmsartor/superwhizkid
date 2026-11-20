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
- Used for: choosing a representative olive-green profile for the optional `Gamate LCD` palette and
  confirming that neutral greyscale is not representative of the original non-backlit panel.
- Method and limitations: the Nintendo Life/Neo Fuji game images show software on physical Gamate
  screens, but several were made by scanning a paused LCD and have severe green-channel clipping.
  The Ashens video thumbnail provides a less saturated view of an unlit physical screen under room
  lighting. The representative profile therefore follows that screen's olive hue while using the
  gameplay images only to check shade ordering and contrast. It is a visual estimate, not calibrated
  colorimetry: camera white balance, exposure, ambient light, contrast-wheel position, aging, and the
  two reported Gamate panel types all prevent a single photograph-derived palette from being exact.
- Implementation impact: the libretro frontend offers `Original`, `Gamate LCD`, and `Greyscale`.
  `Original` preserves the core's initial uncited visual approximation exactly. `Gamate LCD` uses
  RGB565 `8C69/6348/3208/1906`; its background is a darker, more yellow-olive panel tone, while
  progressively driven shades shift toward a slightly lighter navy ink. The adjustment remains a
  visual interpretation of the same uncalibrated hardware references rather than a new measurement;
  the legacy internal option value remains `accurate` so existing frontend configurations continue
  to work. `Greyscale` uses neutral 255/170/85/0 levels. Palette selection only
  maps completed two-bit pixels to output colors and does not change emulated LCD state or timing.

### Contrast presentation control

- Physical context: the real-hardware appearance references above show that screen contrast varies
  with the handheld's contrast wheel, viewing conditions, panel type, and age, but they do not provide
  calibrated shade measurements across wheel positions.
- Implementation impact: the frontend exposes 16 contrast levels from 0 through 15, defaulting to 8.
  Level 8 reproduces each selected palette exactly. Level 0 collapses all three driven shades into
  the palette's unchanged background; levels above 8 extend each RGB565 channel's distance from that
  background with channel clipping at the format limits. The background entry itself is copied
  byte-for-byte at every level. L3 and R3 adjust one level per press and request that the frontend
  update the visible core-option value.
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
- Cartridge/library validation: the current 1,000-frame, two-BIOS sweep passes 116/116. World Cup
  Soccer is the only unattended title to exercise the UART, producing 1,136 transmit clocks under
  either BIOS;
  this made it the real-content transport test. Two headless RetroArch 1.20.0 instances connected on
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
- Accessed: 2026-11-19.
- Used for: establishing the known dumped-image baseline, preserving reported and rumored undumped
  cartridge identities separately, reconciling historical naming/serial disagreements, and matching
  the local collection by full-file SHA-1.
- Result: all 58 local cartridge images match current MAME entries exactly, covering 58 of its 62
  catalogued dumps. The four catalogued images absent locally are Mighty Boxer, Kiki Inland,
  Fortress of Fierceness, and Incantational Couple. Neo Fuji is treated as historical catalogue
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

### Commercial cartridge header and boot-ABI analysis

- Local origin: all 58 user-supplied cartridge images and both recognized BIOS revisions, read in
  place without retaining ROM or firmware bytes.
- Method: `tools/gamate-rom-header-report.py` reports structural metadata and
  `tools/gamate-rom-layout-report.py` reports trailing fill, uniform banks, aligned duplication, and
  whole-image half mirroring. Focused comparisons additionally examined embedded multicart headers
  and the two Bomb Blaster variants. BIOS code was inspected to identify the cartridge callback
  dispatchers and the alternate UMC `$6002` format-marker check.
- Used for: establishing the commercial header layout, checksum range, `$6020` start stub, `$6023`
  NMI callback, `$6026` IRQ callback, multicart component layout, and corpus padding conventions.
- Result: all 58 checksums validate; all start and IRQ slots are absolute jumps; four games use an
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

- Local origin: all 58 user-supplied cartridges were paired with both recognized authentic BIOS
  revisions. Captures were taken immediately after the BIOS set PC to `$6020` and before the first
  cartridge instruction executed. The resulting 116 state/RAM artifacts remain gitignored and are
  not distributed.
- Method: a temporary capture harness used Superwhizkid's independently authored serialization plus
  readable field diagnostics. RAM and the serialized VRAM regions were compared byte-for-byte;
  scalar state, timing, buffer hashes, and SHA-256 artifact identities were indexed locally.
- Result: 116/116 combinations reached entry. Every UMC/BIT pair has identical RAM and VRAM. Across
  cartridges, only RAM `$00E0-$00E1` and `$00E5-$00E6` vary; both pairs exactly match that
  cartridge's stored 16-bit header checksum in all 58 cases. All other RAM bytes and all VRAM bytes
  are invariant. CPU registers and hardware configuration are common, while the BIOS revisions
  retain their documented 29-CPU-cycle/58-master-clock display and audio phase difference.
- Used for: `docs/BIOS_REPLACEMENT_ANALYSIS.md`, including the conclusion that one behavioral boot
  contract can cover the corpus, the warning that authentic RAM/VRAM templates must not be shipped,
  and the identification of resident interrupt/helper behavior as the principal blocker to a true
  BIOS-free jump to `$6020`.
- Limitations: captures reflect the current emulator at one instruction boundary, not a physical bus
  trace. Invariance does not prove necessity, and hashes were not used as a substitute for the direct
  RAM/VRAM comparisons. The sweep does not establish post-entry helper usage.

### Cartridge-entry VRAM perturbation sweep

- Local origin: all 58 user-supplied cartridges were run for 1,000 post-entry frames under both BIOS
  revisions after replacing only entry VRAM with authentic, zero, `$FF`, alternating-plane, or
  deterministic address-derived contents. Authentic and zero were repeated with the standard broad
  controller schedule. Reports remain outside the repository and contain derived hashes/counters but
  no ROM, BIOS, VRAM image, or framebuffer payload.
- Method: independently authored diagnostics restart VRAM coverage at `$6020`, count CPU and active
  LCD reads before a byte's first cartridge write, and retain bitmaps only in nonserialized diagnostic
  state. The instrumentation is compiled into a separate diagnostic library used by test tools; the
  normal library and libretro core contain neither its storage nor its per-access checks. A release
  build's normal `gamate.c` object was byte-identical to the pre-instrumentation object. `gamateboot`
  reports final machine hashes and behavioral counters;
  `tools/gamate-vram-perturbation-summary.py` excludes expected VRAM/video differences when comparing
  execution state. Purpose-written unit tests verify CPU/display read-before-write accounting and
  ensure display blanking does not count a VRAM fetch.
- Result: authentic versus cleared VRAM produced zero execution, final-video, or final-VRAM differences
  in all 116 unattended cases and all 116 automated-input cases. Across all five unattended patterns,
  arbitrary nonzero contents changed execution for seven titles under both BIOSes and changed the
  final displayed frame for two; all 116 final VRAM images varied because cartridges need not rewrite
  the whole 16 KiB backing store. Sixteen unattended titles performed CPU reads before writes, with
  Insect War reading 12,800 distinct bytes before their first write.
- Used for: `docs/BIOS_REPLACEMENT_ANALYSIS.md` and the clean-room policy that replacement startup may
  clear VRAM instead of inheriting or reconstructing authentic startup graphics. It does not justify
  uninitialized or arbitrary VRAM.
- Limitations: 1,000-frame unattended and deterministic-input paths are broad but not exhaustive;
  unknown cartridges or later gameplay may have additional requirements. The experiment evaluates
  behavior in the current emulator, not power-on electrical VRAM state on physical hardware.

### Cartridge-entry RAM perturbation and access sweep

- Local origin: all 58 user-supplied cartridges were run for 1,000 post-entry frames under both
  authentic BIOS revisions. The experiment compared authentic RAM with all-zero, all-`$FF`,
  callback/checksum-preserving, stack-preserving, and individual 256-byte-page-clearing policies.
  Authentic-state access tracking was repeated with the standard broad controller schedule. Reports
  remain outside the repository and contain derived hashes, counters, and address sets, not ROM,
  BIOS, or RAM payloads.
- Method: independently authored, diagnostic-only hooks restart physical 1 KiB RAM coverage at
  `$6020`, mark writes, and classify reads before the first post-entry write by the PC of the current
  cartridge, BIOS, or other instruction. `gamateboot` applies entry perturbations after authentic
  boot and reports owner-specific counts and address sets. The reusable
  `tools/gamate-ram-perturbation-summary.py` compares final CPU, video, audio, VRAM, mapper,
  peripheral, and activity observations while excluding only the expected final-RAM hash change.
  Purpose-written unit tests cover owner classification and read/write accounting. The hooks share
  the separate diagnostic library used by the VRAM experiment; the production `gamate.c` object
  remained byte-identical to the pre-instrumentation release object.
- Result: full zero and `$FF` replacement changed 106 and 102 of 116 executions respectively.
  Preserving only `$000C`, `$00E0-$00E1`, and `$00E5-$00E6` still changed 104; additionally retaining
  the authentic stack page did not reduce that count. Clearing pages `$000-$0FF`, `$100-$1FF`,
  `$200-$2FF`, and `$300-$3FF` individually changed 106, 2, 36, and 10 cases respectively. Final
  video changed in 24 zero-page cases and 8 page-2 cases. Every case performed cartridge
  read-before-write activity, their union covered all 1,024 physical bytes, and BIOS reads occurred
  in 110 cases. Automated input added no address to either union.
- Quarter-page extension: each of the sixteen 64-byte physical blocks was independently cleared in
  another 1,856 unattended runs. `$000-$03F` accounted for all 106 zero-page cases. `$100-$13F`
  affected only Time Warrior, while `$140-$1FF` affected none. The four page-2 blocks affected 26,
  18, 6, and 2 cases; the four page-3 blocks affected 2, 2, 7, and 4. All but one title-level result
  was paired across BIOSes. Dino Bibo differed only under UMC for `$380-$3BF`, limited to one active
  AY-A sample and its audio hash at the observation boundary.
- Sixteen-byte extension: the four highest-priority blocks (`$000-$03F`, `$0C0-$0FF`,
  `$200-$23F`, and `$240-$27F`) were divided into sixteen chunks and exercised in another 1,856
  runs. All completed. `$000-$00F` affected 106 cases and `$010-$01F` affected 102; `$020-$02F`
  and `$030-$03F` affected 8 and 4. `$0C0-$0CF` and `$0D0-$0DF` affected 18 and 22, while
  `$0E0-$0FF` affected none. The page-2 chunks affected 2, 12, 12, 6, 4, 10, 4, and 0 cases in
  ascending address order. For each parent block, the union of chunk-level changed cases exactly
  matched the parent set, with no parent-only interaction case at this resolution.
- Byte/four-byte extension: all 32 bytes at `$000-$01F` and eight aligned groups at `$0C0-$0DF`
  were independently cleared in another 4,640 passing runs. `$000C` alone affected the same 106
  cases as its sixteen-byte parent; `$0011` alone affected the same 102 cases as its parent.
  `$004-$009` and `$012-$013` produced smaller title-specific changes. Only `$0CC-$0CF`,
  `$0D0-$0D3`, `$0D8-$0DB`, and `$0DC-$0DF` affected execution among the four-byte groups. Child
  unions exactly matched all four parent sets. The summary tool's final-state normalization uses a
  word-boundary match for `RAM=` so final `VRAM=` remains part of the execution signature; separate
  final-video and final-VRAM columns provide narrower presentation/storage views.
- `$000E-$0011` trace: a diagnostic-only address watch recorded physical address, instruction PC,
  owner, access direction and values, nearby `$0010-$0013`, post-entry frame, master clocks,
  IRQ/NMI counts, and explicit IRQ/NMI/BRK context. Both BIOS revisions increment `$0011` on every
  periodic IRQ. Inspection of the locally executed handler shows the four-byte carry loop continuing
  toward `$000E`; a focused overflow trace observed `$00000394` advance through the
  `$000003FF->$00000400` boundary. A cartridge-owner-only full sweep found `$0011` accesses in
  fourteen titles at matching sites under both BIOSes. Reports remain local and contain events, not
  firmware or cartridge bytes.
- Inverse low-RAM sweep: clearing `$000-$01F` changed 106 cases; preserving `$000C` reduced that to
  102, preserving `$0011` alone left 106, and preserving both reduced it to ten. Preserving
  `$0004-$000A`, `$000C`, and `$000E-$0013` produced zero execution, final-video, or final-VRAM
  differences across all 116 unattended and 116 broad-input cases. `$000A` was added after the
  input sweep isolated a two-BIOS Treasure Hunter difference to that byte. This establishes tested
  sufficiency; the following writer/consumer trace supplied the semantic interpretation.
- Pre-entry writer and focused consumer traces: a diagnostic watch enabled before `$6020` recorded
  final BIOS writers for `$004-$00A` and `$012-$013` under both revisions. `$004` is the shift
  accumulator for the accepted `$47` protection reply; `$005-$009` receive no BIOS writes and retain
  `$FF` reset fill; `$00A` receives an explicit final `$FF` write. `$012-$013` are repeatedly advanced
  together by the routine behind service `$F030`; static instruction/transition analysis identifies
  a nonzero-seeded 16-bit feedback shift register that returns its low byte. Focused cartridge traces
  linked the preserved fields to Hannibal, Fortune 'n Luck, Myth of Asamia, Treasure Hunter, Hot
  Hero, and Magic Jigsaw access sites. Used for the named low-RAM contract in
  `docs/BIOS_BOOT_BEHAVIOR.md`, the `$F030` description in `docs/BIOS_RUNTIME_API_ANALYSIS.md`, and
  the clean-room initialization conclusions in `docs/BIOS_ENTRY_RAM_ANALYSIS.md`.
- Semantic low-RAM validation: a diagnostic policy cleared `$000-$01F` and initialized only the
  named protection, sentinel, callback, IRQ-counter, and LFSR fields. It matched authentic-entry
  CPU, video, audio, VRAM, mapper, peripheral, controller, and activity observations in all 116
  unattended and all 116 broad-input 1,000-frame cases. Whole-RAM hashes were excluded because the
  policy intentionally clears unrelated boot residue. This validates behavioral construction of
  the common low-RAM contract; it does not establish that authentic counter or random phase must be
  hardcoded by replacement firmware.
- Counter/LFSR phase sweep: the same 116-case unattended and broad-input matrices tested counter
  states `$00000000`, `$0000FFFF`, `$0000FF94`, and `$00000494`, plus nonzero LFSR states `$0001`
  and `$FFFF`.
  All cases completed. Low-byte and full-wrap counter changes broadly shifted deterministic timing
  because the BIOS counter increment takes additional carry branches. `$00000494`, which preserves
  the low-byte carry schedule and avoids a carry into `$00F` during the window, matched every unattended
  case; input differed only for Kill Shot. A focused cartridge-owner trace found Kill Shot reading
  `$010` at PC `$8DCA`. LFSR differences were confined to Hot Hero and Magic Jigsaw without input,
  and Cosmic Fighter and Magic Jigsaw with input, paired across BIOS revisions. Used to recommend
  authentic `$00000394/$358A` compatibility phases while distinguishing named state from copied residue.
  Reports remain local and contain derived observations rather than firmware or cartridge payloads.
- Used for: `docs/BIOS_REPLACEMENT_ANALYSIS.md`, the detailed block/title matrix in
  `docs/BIOS_ENTRY_RAM_ANALYSIS.md`, rejection of blanket RAM initialization for BIOS skipping, and
  prioritization of semantic zero-page and upper-work-buffer analysis. Authentic BIOS execution
  continues to preserve its produced RAM unchanged.
- Limitations: an instruction-start PC attributes interrupt-entry stack writes to the interrupted
  instruction, although subsequent handler accesses are attributed to BIOS code; this does not
  weaken the read-before-write evidence central to the experiment. A read-modify-write counts its
  read before its write. Fixed 1,000-frame paths are not exhaustive, and a changed hash/counter does
  not by itself identify a field's meaning. The findings do not authorize copying invariant
  firmware-derived RAM bytes into replacement firmware.

### Post-entry BIOS runtime API trace

- Local origin: all 58 user-supplied cartridges were executed under both authentic BIOS revisions
  for 1,000 post-entry frames without input. The combined TSV report remains outside the repository
  and contains derived transition metadata but no ROM or BIOS payload.
- Method: the independently authored `gamatebioscalls` diagnostic classifies executed control-flow
  transitions across the cartridge/BIOS boundary. It separates hardware IRQ/NMI entry, direct calls
  and jumps, callback returns, BIOS callback dispatch, and interrupt/subroutine returns. Mapper bank,
  count, and first frame are retained. `tools/gamate-bios-call-sweep.sh` runs the full matrix.
- Result: 116/116 runs completed. Every cartridge directly called the stable 18-slot service table at
  `$F000-$F033`; no direct target outside it was observed. Per BIOS, 161,958 direct calls arose from
  1,031 distinct source/bank/target rows. The entire direct-event multiset matched between BIOS
  revisions. `$F030` and `$F033` first appeared at frame 408, demonstrating that the former 300-frame
  default was insufficient for this audit. All games received `$6026` IRQ callbacks; no `$6023` NMI
  callback occurred without link input.
- Used for: `docs/BIOS_RUNTIME_API_ANALYSIS.md`, the increase of standard sweep defaults to 1,000
  frames, and the conclusion that replacement firmware must implement the service table in addition
  to boot and interrupt dispatch.
- Limitations: this is unattended execution coverage, not complete reachability. Slot semantics,
  nested-call pairing, register conventions, memory/hardware effects, gameplay-only behavior, and
  link-driven NMI paths remain to be established. Single executed opcode values are used only for
  transition classification; no instruction listing or translated firmware code is retained.
- Follow-up method: the independently authored tracer can optionally pair service-table entries with
  their observed returns and write a separate local report containing input/output registers, flags,
  stack state, elapsed clocks, changed-memory counts/address extents, changed AY-register masks, and
  peripheral diagnostic deltas. It does not retain RAM, VRAM, ROM, or BIOS byte contents. Pairing
  uses the ordinary JSR return address or the cartridge's pre-existing stack return for the two
  observed tail-jump forms, while interrupt callbacks may occur inside the measured interval.
- Used for: the staged probing plan in `docs/BIOS_RUNTIME_API_ANALYSIS.md` and future clustering of
  behavioral call signatures. Derived clusters remain hypotheses until controlled probes establish
  helper semantics.
- Result: the 1,000-frame paired sweep completed 323,838 calls across the 116-case matrix, split
  evenly at 161,919 per BIOS. Another 39 calls per BIOS were open at the final frame boundary. All
  cases passed with no unmatched return or pairing-stack overflow. Per-slot counts and the complete
  cartridge/source/mapper/input-output-register signature multisets matched across BIOS revisions;
  timing and summarized effects could differ when the 58-master-clock entry-phase offset changed
  whether an interrupt fell inside a call. `tools/gamate-bios-call-summary.py` reproduces aggregate
  counts and effect categories from the local derived report.
- Hardware-state clustering extension: completed-call records now include entry/exit VRAM address,
  scroll, LCD mode flags, AY address, and mapper banks. These are emulator state observations, not
  copied firmware or cartridge bytes. A repeated 58-cartridge/two-BIOS/1,000-frame sweep completed
  116/116 cases. The summary tool reports register and status preservation, input-A timing groups,
  and frequent hardware-state transitions for a selected slot.
- `$F030` contract use: 362 paired calls across both BIOSes established exact X/Y/S and I/D/V
  preservation, N/Z derived from returned A, 174-master-clock uninterrupted timing, and no hardware
  state effects. Existing RAM transition analysis supplies the `$012-$014` LFSR state/scratch role.
  Carry is intentionally left unresolved because it was not preserved consistently and has not yet
  been isolated with a controlled state probe. Used for the first resolved service table in
  `docs/BIOS_RUNTIME_API_ANALYSIS.md`.
- `$F000` prioritization: sixteen uninterrupted calls each took 3,278 master clocks, issued 64 VRAM
  writes, ended at VRAM address `$128B`, preserved scroll/AY-selection/mapper state, and returned
  A/X/Y as `$38/$40/$20`. This evidence is used only to select it as the next narrow probe target;
  no visual or drawing semantic is assigned yet.
- Input-coverage method: `tools/gamate-bios-input-campaign.sh` applies independently authored,
  deterministic schedules for long Start, Select/Start plus A/B actions, and every individual control
  plus legal direction/button combinations. `tools/gamate-bios-call-coverage.py` compares derived
  boundary, call-site, register, and broad effect signatures while excluding frame/time/counter drift;
  local reports contain filenames and derived metadata but no cartridge or BIOS contents.
- Input-coverage result: the controller cycle passed all 116 cases and added 979 service call sites,
  5,817 detailed call-site signatures, and 4,027 normalized ABI signatures over the unattended
  baseline. Long Start added another 250/5,748/3,042 respectively over that union, and the subsequent
  menu/action schedule added 37/4,303/2,350. All schedules retained the same 18 service slots and only
  the `$6026` IRQ callback; ordinary controls cannot test link-driven `$6023` NMI behavior. These
  results are used to prevent unattended-only traces from being mistaken for a complete BIOS ABI.

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
- `tools/gamate-sweep.sh` ran all 58 locally supplied cartridges under both validated BIOS revisions
  for 1,000 frames after `$6020`: 116/116 combinations booted and produced nonblank video with no
  unsupported opcode or persistent self-loop diagnosis. Cartridge sizes covered 16, 32, 64, 128,
  256, and 512 KiB, including the fingerprinted 4-in-1 mapper. The report remains outside the
  repository and contains local filenames, hashes, and counters but no input bytes. All titles
  performed controller reads in this window. World Cup Soccer alone exercised the UART, with 1,136
  clocks under either BIOS. See `docs/COMPATIBILITY_SWEEP_ANALYSIS.md`. This is broad automated smoke
  coverage, not a claim that every title is playable or hardware-accurate.
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
