# Gamate cartridge header and boot ABI

This is the first static-analysis report for the local Gamate cartridge corpus. It covers point 1
of [`ROM_STATIC_ANALYSIS.md`](ROM_STATIC_ANALYSIS.md): the cartridge format and boot ABI.

The analysis covers all 58 locally available cartridge images identified in
[`ROM_CORPUS.md`](ROM_CORPUS.md), including both Bomb Blaster variants and the 4-in-1. The four
MAME-catalogued dumps that are not locally available are outside the sample.

## Reproduction

Run the metadata analyzer against a directory containing legally obtained cartridge dumps:

```sh
python3 tools/gamate-rom-header-report.py /path/to/gamate/cartridges
```

The tool recognizes a cartridge by the documented copyright signature at offset `$0005`. It emits
only filenames, hashes, sizes, decoded header metadata, entry stubs, and header locations. It does
not copy ROM contents into the repository or a retained report.

## Header layout established by the corpus

CPU addresses below assume the first cartridge byte is mapped at `$6000`.

| ROM offset | CPU address | Length | Finding |
| ---: | ---: | ---: | --- |
| `$0000-$0001` | `$6000-$6001` | 2 | Little-endian 16-bit checksum of bytes `$1000-$1FFF`. |
| `$0002` | `$6002` | 1 | Format value `$01` in all 58 images. |
| `$0003-$0004` | `$6003-$6004` | 2 | Variable fields with no established meaning. |
| `$0005-$001D` | `$6005-$601D` | 25 | ASCII `COPYRIGHT BIT CORPORATION` in every image. |
| `$001E-$001F` | `$601E-$601F` | 2 | Variable/reserved or padding bytes with no established meaning. |
| `$0020-$0022` | `$6020-$6022` | 3 | Cartridge start stub; always an absolute `JMP`. |
| `$0023-$0025` | `$6023-$6025` | 3 | NMI callback stub; absolute `JMP` or `RTS` followed by padding. |
| `$0026-$0028` | `$6026-$6028` | 3 | IRQ callback stub; always an absolute `JMP`. |

All 58 stored checksums exactly equal the low 16 bits of the byte sum over ROM offsets
`$1000-$1FFF`. The checksum is therefore not over the complete image and does not cover the header
or entry stubs.

## Unknown header bytes

The corpus gives useful constraints without establishing semantics:

- Offset `$0002` is `$01` in all 58 images. The UMC BIOS contains an alternate startup path that
  reads `$6002` and compares it with `$01`; the ordinary path selected by the documented `$5A00`
  response bypasses this check. This makes `$01` a firmware-recognized format marker in at least one
  path, but does not establish a versioning scheme.
- Offset `$0003` is `$00` in 56 images, `$09` in Insect War, and `$99` in Tornado.
- Offset `$0004` takes 16 observed values. The most common are `$00` in 14 images, `$99` in 13,
  `$01` in 11, and `$11` in four; the remaining values occur one to three times each.
- Offset `$001E` is `$00` in 57 images and `$FF` in Enchanted Bricks.
- Offset `$001F` is `$FF` in 45 images. Eight other values occur in the remaining 13 images.

Neither BIOS's copyright loop extends beyond `$601D`, and the checksum routine reads only the
stored checksum at `$6000-$6001` plus the fixed data range `$7000-$7FFF`. No meaning should be
assigned to `$6003-$6004` or `$601E-$601F` without additional evidence.

## Cartridge callback ABI

Both authentic BIOS revisions implement the same cartridge-facing dispatch structure:

- After protection, signature, checksum, and logo processing, the BIOS jumps to `$6020`.
- The BIOS NMI wrapper reads the UART receive byte into BIOS RAM, then conditionally calls `$6023`.
- The BIOS periodic-IRQ wrapper updates its counters, then conditionally calls `$6026`.
- The callback stubs return to the BIOS wrapper with `RTS`; the wrapper performs the eventual
  `RTI`. A cartridge may use an absolute `JMP` stub so its real handler can end in `RTS`.

Corpus results:

- All 58 `$6020` stubs are absolute jumps. Every target lies in `$6000-$9FFF`, the fixed lower ROM
  window on the common mapper, so startup does not depend on the initial upper-bank mapping.
- All 58 `$6026` IRQ stubs are absolute jumps.
- 54 `$6023` NMI stubs are absolute jumps.
- GP Race, One Million Whys, QuizFighter, and Snowman Legend use `RTS; NOP; NOP` at `$6023`, making
  their cartridge NMI callback an explicit no-op.

The BIOS gates both callbacks on a shared RAM flag before calling the cartridge. The exact ownership
and complete lifecycle of that flag remain subjects for the later RAM/interrupt ABI analysis.

## The 4-in-1 contains component headers

The 128 KiB 4-in-1 is the only corpus image containing the copyright signature more than once. It
has valid component headers at offsets `$00000`, `$08000`, `$10000`, and `$18000`; every component's
stored checksum validates against its own `$1000-$1FFF` region.

- The component at `$08000` is byte-identical to the standalone Mini Golf dump.
- The component at `$10000` is byte-identical to the standalone Brick Blaster dump.
- The component at `$18000` is byte-identical to the standalone Vindicators dump.
- The first 32 KiB component is a modified/expanded Cube Up. Its second 16 KiB bank is entirely
  `$FF`, while its first bank differs from the standalone 16 KiB Cube Up image at 1,428 positions:
  three before `$1000` and 1,425 at or after `$2000`. There are no differences in the checksummed
  `$1000-$1FFF` region, so its stored checksum remains the same.

This is primary software evidence that the multicart image is organized as four 32 KiB component
slots and that its startup modification is not represented by the BIOS checksum.

## Padding and repeated banks

A 16 KiB bank-level comparison found these full-bank padding or repetition cases:

- 4-in-1: bank 1 is entirely `$FF`.
- Dinosaur Park: banks 21-31 are entirely `$FF`.
- Further Adventures of Hannibal The Cat: banks 29-31 are entirely `$FF`.
- Hot Hero: banks 24-31 are entirely `$FF`; banks 17-19 duplicate banks 21-23 respectively.
- Insect War: banks 30-31 are entirely `$00`.

No other complete 16 KiB bank in the 58-image corpus is uniform or duplicated within its image.
These observations establish dump contents, not physical ROM address decoding or intended mapper
limits.

## Bomb Blaster variants

The two catalogued Bomb Blaster images share the same copyright header and all three callback
stubs. They differ in 128 byte positions: the two checksum bytes and 126 payload positions. Seven
payload differences lie inside the checksummed `$1000-$1FFF` region, and each image carries the
corresponding valid checksum. The remaining differences include two identically repeated modified
data blocks within each image.

Static structure alone does not establish whether the alternate image is a deliberate software
revision, a repaired dump, or another cartridge variation. It should remain a distinct corpus
member until provenance or behavioral analysis resolves that question.

## Conclusions and limits

The corpus establishes a consistent commercial cartridge ABI: checksum, format marker, copyright
signature, and three fixed callback slots. It also shows that the checksum protects only one 4 KiB
region, so it is not an image identity or integrity hash.

The variable bytes at `$0003-$0004` and `$001E-$001F` remain undocumented. Correlation with title,
publisher, year, and ROM size does not currently yield a defensible interpretation. Hardware or
firmware behavior should not be inferred from those values alone.
