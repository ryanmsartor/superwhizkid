# BIOS replacement and boot-skipping analysis

This document evaluates what the authentic-BIOS entry-state sweep establishes about accelerated
boot, independently authored replacement firmware, and a true BIOS-free boot path. It records
derived behavior and aggregate state properties without publishing RAM/VRAM images, firmware bytes,
startup graphics, or a state snapshot that could substitute for either BIOS.

## Entry-state sweep

The sweep captured every combination of 58 locally available cartridges and both recognized BIOS
revisions. Each capture occurred immediately after the BIOS set CPU PC to `$6020` and before the
cartridge executed its first instruction.

All 116 combinations reached the boundary. The important result is that successful boot produces an
almost entirely cartridge-independent state:

- Paired UMC/NCR and Bit Corporation captures have byte-identical 1 KiB RAM for every cartridge.
- All 116 captures have byte-identical 16 KiB VRAM.
- Within either BIOS set, 1,020 of 1,024 RAM bytes are invariant across all 58 cartridges.
- The only cartridge-dependent RAM bytes are `$00E0-$00E1` and `$00E5-$00E6`. Both pairs contain the
  cartridge's stored 16-bit header checksum; this relationship holds for all 58 images.
- There are consequently 57 distinct RAM images. The 4-in-1 and standalone Cube Up share the sole
  duplicate because their relevant component/checksum is shared.
- Mapper type and ROM size vary as expected, but the selected bank and first-bank registers are zero
  at entry for every image.
- CPU registers, protection state, LCD configuration, VRAM address/plane, scroll values, UART state,
  AY registers, interrupt-pending state, and BIOS callback-enable state are invariant across the
  cartridge set and equal between BIOS revisions.

These are aggregate and semantic findings. The invariant RAM and VRAM contents themselves are kept
out of the repository because they include firmware-derived working data and startup presentation.

## Common cartridge-entry contract

The following public facts are sufficient to describe the observed successful-entry boundary:

| State | Observed entry value or relationship |
| --- | --- |
| CPU PC | `$6020`, before its instruction executes |
| CPU A / X / Y | `$FF` / `$28` / `$FF` |
| CPU stack pointer | `$FF` |
| CPU status | `$A1`; IRQ polling is enabled |
| Callback flag | RAM `$000C` is `$01`, so `$6023/$6026` dispatch is enabled |
| Cartridge checksum work fields | RAM `$00E0-$00E1` and `$00E5-$00E6` both equal the stored header checksum |
| Protection | Passed, with no failed or pending reply state |
| Mapper banks | Upper and first-bank selections are zero |
| Controller | All active-low inputs released |
| UART | No in-flight transmit or receive byte |
| AY registers | All zero; no channel is producing a tone |
| LCD data position | Address `$18CE`, plane 1, row-increment mode |
| LCD presentation | X scroll `$00`, Y scroll `$5F`, window and plane swap off, display running and unblanked |
| IRQ/NMI | Neither pending; periodic-IRQ phase is 218 master clocks |

The table is an observed compatibility boundary, not a claim that every field must have exactly that
value for every game. Perturbation tests are required to distinguish required inputs from harmless
startup residue.

## Revision-dependent phase

The BIOS revisions produce the same RAM, VRAM, CPU registers, hardware configuration, and cartridge
validation result, but arrive at different oscillator phases:

| Entry measurement | UMC/NCR BIOS | Bit Corporation BIOS |
| --- | ---: | ---: |
| Executed instructions | 5,645,786 | 5,645,788 |
| CPU cycles | 15,119,693 | 15,119,722 |
| Master clocks | 30,239,372 | 30,239,430 |
| Frame phase | 58,772 | 58,830 |
| LCD row-event / clock | 362 / 128 | 363 / 24 |

The 58-master-clock difference also leaves different AY oscillator/noise/envelope counters and audio
sample phase even though all AY registers are zero. It does not change IRQ phase at entry.

Both phases boot the complete cartridge set. Exact phase therefore does not appear to be a general
compatibility requirement, but it remains observable: progressive LCD and audio hashes can differ,
and Mini Golf and Money Maze previously demonstrated BIOS-dependent sampling at fixed diagnostic
frame numbers. A replacement that claims to reproduce a particular BIOS should preserve its phase;
a compatibility-oriented replacement may choose a canonical phase but must not call it identical to
either authentic boot.

## What the sweep makes feasible

### Accelerated authentic boot

The current intro-skip design remains the lowest-risk accuracy path: run the selected authentic BIOS
without frontend presentation until it naturally reaches `$6020`. This preserves validation,
firmware working state, resident interrupt code, display/audio phase, and unknown side effects. The
sweep strengthens the safety of using `$6020` as the stop condition but does not justify replacing
execution with a copied state template.

### Independently authored replacement firmware

A clean-room replacement now appears technically feasible. It can implement the documented
protection, header/checksum validation, entry point, callback ABI, and machine initialization from
behavioral requirements rather than reproduce authentic instruction sequences.

It does not need 58 cartridge-specific boot recipes. Cartridge-dependent entry RAM is limited to the
header checksum, which the replacement can calculate or copy from the public header fields. Mapper
selection remains a core/cartridge responsibility rather than firmware content.

The replacement must remain resident after boot. Commercial software relies on BIOS interrupt
wrappers that update BIOS counters, receive link data, and conditionally call cartridge callbacks at
`$6023` and `$6026`. Several cartridges deliberately preserve or modify the callback flag at `$000C`.
A replacement that merely initializes state and jumps to `$6020` but provides no compatible
post-entry interrupt code is incomplete.

### True BIOS-free high-level boot

A high-level skip inside the emulator is possible in principle but has a larger semantic surface. It
would need to perform or explicitly replace all of the following:

1. Put cartridge protection into the successfully unlocked state.
2. Validate or define policy for the identity field and checksum.
3. Initialize CPU, RAM, mapper, LCD, AY, UART, interrupt, and timing state.
4. Establish the `$000C` callback contract and BIOS-maintained counters.
5. Provide functional NMI/IRQ dispatch after cartridge entry.
6. Handle any cartridge calls to other BIOS helpers.
7. Choose and document a display/audio phase policy.

Items 5 and 6 prevent a simple `PC = $6020` shortcut. Without firmware mapped at `$F000-$FFFF`, the
CPU vectors and any direct cartridge-to-BIOS calls need an independently implemented replacement
boundary. Options include a small clean-room resident firmware or explicit high-level vector/helper
interception. A resident replacement is easier to reason about with ordinary 6502 execution and less
likely to create special CPU paths.

## Entry-VRAM perturbation result

The exact authentic startup image is not required by the tested commercial corpus. A controlled
experiment replaced only the 16 KiB VRAM contents at `$6020`, preserving CPU, RAM, LCD state and
phase, mapper, interrupts, audio, peripherals, and every other entry field.

Five entry policies were tested for 1,000 frames across all 58 cartridges and both BIOS revisions:
authentic contents, all zeroes, all `$FF`, alternating plane-oriented bytes, and a deterministic
address-derived pattern. The authentic and cleared runs were also repeated with the standard broad
controller-input schedule.

| Comparison | Cases | Execution differences | Final-video differences | Final-VRAM differences |
| --- | ---: | ---: | ---: | ---: |
| Authentic versus cleared, unattended | 116 | 0 | 0 | 0 |
| Authentic versus cleared, automated input | 116 | 0 | 0 | 0 |
| All five patterns, unattended | 116 | 14 (7 titles under both BIOSes) | 4 (2 titles under both BIOSes) | 116 |

“Execution” compares final CPU/RAM/mapper/LCD-address state, audio and peripheral activity, controller
behavior, and relevant counters while deliberately excluding video and VRAM hashes. Under both
unattended and automated-input coverage, clearing VRAM was exactly equivalent to inheriting the
authentic startup image: every compared execution field, frame-1/60/1000 video and audio observation,
final framebuffer hash, and final VRAM hash matched.

The broader perturbations establish an important limit. Sixteen titles in the unattended run made
CPU VRAM reads before writing every byte they read; input-driven coverage found fifteen such titles.
Some read thousands of bytes, and Insect War read 12,800 distinct bytes before writing them. Arbitrary
garbage is therefore unsafe: nonzero patterns changed execution in Dinosaur Park, Galaxy Invaders,
Kill Shot, Mars Voyage, The Golden Pyramid, Tornado, and Witty Apee, while Galaxy Invaders and Tornado
also retained a different final displayed frame.

The supported replacement policy is consequently **clear VRAM**, not “VRAM contents do not matter.”
The authentic logo/image bytes may be omitted without copying or reconstructing them. This conclusion
applies to the current catalog and exercised paths; later gameplay or unknown cartridges could still
impose additional initialized-state requirements, but the exact authentic image is no longer a
blocker for clean-room replacement work.

## Entry-RAM perturbation result

WRAM does not admit the same simple policy as VRAM. A second controlled experiment replaced entry
RAM at `$6020` while preserving CPU and hardware state. It ran all 58 cartridges under both BIOS
revisions for 1,000 frames and compared final video, audio, CPU, mapper, peripheral, VRAM, and
activity state while excluding only the deliberately changed final RAM hash.

| Entry-RAM policy | Cases | Changed executions | Changed final video | Changed final VRAM |
| --- | ---: | ---: | ---: | ---: |
| All zeroes | 116 | 106 | 30 | 37 |
| All `$FF` | 116 | 102 | 32 | 31 |
| Zeroes plus authentic callback/checksum fields | 116 | 104 | 16 | 23 |
| `$FF` plus authentic callback/checksum fields | 116 | 102 | 32 | 31 |
| Zeroes plus callback/checksum fields and authentic stack page | 116 | 104 | 16 | 23 |

The known fields in these rows are `$000C`, `$00E0-$00E1`, and `$00E5-$00E6`. Retaining them is
clearly insufficient. This does not mean every other authentic byte must be copied; it means the
replacement still lacks a semantic description of additional BIOS-produced working state.

Clearing one physical 256-byte page at a time localized the dependency:

| Cleared physical RAM page | Changed executions | Changed final video | Changed final VRAM | Affected titles |
| --- | ---: | ---: | ---: | ---: |
| `$000-$0FF` | 106 | 24 | 29 | 53 under both BIOSes |
| `$100-$1FF` (stack page) | 2 | 0 | 0 | Time Warrior under both BIOSes |
| `$200-$2FF` | 36 | 8 | 12 | 18 under both BIOSes |
| `$300-$3FF` | 10 | 0 | 0 | 5 under both BIOSes |

The follow-up 64-byte sweep localized these dependencies further. `$000-$03F` accounts for all 106
zero-page execution differences, while the other zero-page quarters affect smaller title subsets.
Only `$100-$13F` affected a stack-page case (Time Warrior); `$140-$1FF` had no observed effect.
Pages 2 and 3 contain multiple title-specific regions. The complete block/title matrix is in
[`BIOS_ENTRY_RAM_ANALYSIS.md`](BIOS_ENTRY_RAM_ANALYSIS.md).

Post-entry access tracking found at least one cartridge read-before-write in every case, and the
union across the corpus covered all 1,024 physical RAM bytes. BIOS code read inherited bytes before
their first post-entry write in 110 of 116 cases. Repeating authentic-state tracking with the broad
automated-input schedule added no new union addresses and all 116 cases completed, although neither
run proves exhaustive gameplay coverage.

The current supported conclusion is therefore: **preserve authentic WRAM when using an authentic
BIOS, and do not implement BIOS skipping with a blanket fill.** A clean-room replacement should
derive and initialize named functional fields, then repeat these perturbations as those fields are
understood. The experiment rules out the proposed minimal callback/checksum-only policy without
turning the invariant authentic RAM image into a permissible replacement template.

## What should not be copied

The sweep is useful as a behavioral oracle, not as replacement payload data. The project should not:

- Embed the invariant authentic RAM image as an initialization template.
- Embed or reconstruct the authentic VRAM/logo image.
- Ship captured `entry.state` or `entry.ram` artifacts.
- Translate the authentic BIOS instruction sequence into source-like replacement code.
- Treat opaque firmware data or incidental working bytes as required without perturbation evidence.

An independently authored implementation should initialize named functional state, calculate
cartridge-derived values, and generate any presentation assets independently. This preserves a clear
line between compatibility behavior and firmware expression.

## Validation required before removing the BIOS requirement

The following work remains necessary:

1. Determine register, flag, RAM, and hardware semantics for the 18 executed service-table slots
   identified in [`BIOS_RUNTIME_API_ANALYSIS.md`](BIOS_RUNTIME_API_ANALYSIS.md).
2. Extend runtime coverage beyond the unattended 1,000-frame baseline with representative gameplay
   and link activity.
3. Continue decomposing the now-confirmed RAM dependencies into named functional groups; page-level
   perturbation found broad zero-page use and title-specific dependencies in both upper pages.
4. ~~Test a clean RAM policy while preserving only documented callback/checksum state.~~ Complete:
   blanket fills and callback/checksum-only initialization are insufficient across the corpus.
5. ~~Test clearing or independently drawing VRAM instead of inheriting authentic startup graphics.~~
   Complete: cleared VRAM matches authentic entry exactly across unattended and automated-input
   matrices; arbitrary nonzero contents are unsafe.
6. Compare both authentic entry phases with a canonical replacement phase across the complete corpus.
7. Verify reset, save-state, NMI, IRQ, UART, and frontend intro-skip behavior under replacement boot.
8. Keep replacement-firmware tests purpose-written and free of authentic byte sequences or assets.

Until post-entry BIOS calls and state dependencies are mapped, authentic BIOS support should remain
the default and a replacement should be experimental rather than described as drop-in equivalent.

## Evidence and limits

The exact local captures are gitignored and intentionally not distributable. Public conclusions were
derived from equality comparisons, field relationships, hashes, controlled execution, and the
project's independently authored save-state representation. Hash equality was supplemented by direct
byte comparison for RAM and serialized VRAM regions.

This analysis establishes the state produced by the current Superwhizkid model at one instruction
boundary. It is not a physical hardware trace, does not prove that every invariant byte matters, and
does not establish exact within-instruction bus timing. The publication boundary follows
[`BIOS_BOOT_BEHAVIOR.md`](BIOS_BOOT_BEHAVIOR.md) and
[`BIOS_DIFFERENCE_ANALYSIS.md`](BIOS_DIFFERENCE_ANALYSIS.md).

