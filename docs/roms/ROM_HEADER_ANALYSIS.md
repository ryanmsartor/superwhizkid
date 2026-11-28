# Gamate cartridge header and boot ABI

This is the first static-analysis report for the local Gamate cartridge corpus. It covers point 1
of [`ROM_STATIC_ANALYSIS.md`](ROM_STATIC_ANALYSIS.md): the cartridge format and boot ABI.

The analysis covers all 62 MAME-catalogued cartridge images identified in
[`ROM_CORPUS.md`](ROM_CORPUS.md), including both Bomb Blaster variants and the 4-in-1.

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
| `$0002` | `$6002` | 1 | Format value `$01` in all 62 images. |
| `$0003-$0004` | `$6003-$6004` | 2 | Variable fields with no established meaning. |
| `$0005-$001D` | `$6005-$601D` | 25 | ASCII `COPYRIGHT BIT CORPORATION` in every image. |
| `$001E-$001F` | `$601E-$601F` | 2 | Variable/reserved or padding bytes with no established meaning. |
| `$0020-$0022` | `$6020-$6022` | 3 | Cartridge start stub; always an absolute `JMP`. |
| `$0023-$0025` | `$6023-$6025` | 3 | NMI callback stub; absolute `JMP` or `RTS` followed by padding. |
| `$0026-$0028` | `$6026-$6028` | 3 | IRQ callback stub; always an absolute `JMP`. |

All 62 stored checksums exactly equal the low 16 bits of the byte sum over ROM offsets
`$1000-$1FFF`. The checksum is therefore not over the complete image and does not cover the header
or entry stubs.

## Unknown header bytes

The corpus gives useful constraints without establishing semantics:

- Offset `$0002` is `$01` in all 62 images. The UMC BIOS contains an alternate startup path that
  reads `$6002` and compares it with `$01`; the ordinary path selected by the documented `$5A00`
  response bypasses this check. This makes `$01` a firmware-recognized format marker in at least one
  path, but does not establish a versioning scheme.
- Offset `$0003` is `$00` in 60 images, `$09` in Insect War, and `$99` in Tornado.
- Offset `$0004` takes 17 observed values. The most common are `$99` in 15 images, `$00` in 14,
  `$01` in 12, and `$11` in four; the remaining values occur one to three times each.
- Offset `$001E` is `$00` in 61 images and `$FF` in Enchanted Bricks.
- Offset `$001F` is `$FF` in 47 images. Nine other values occur in the remaining 15 images.

Neither BIOS's copyright loop extends beyond `$601D`, and the checksum routine reads only the
stored checksum at `$6000-$6001` plus the fixed data range `$7000-$7FFF`. No meaning should be
assigned to `$6003-$6004` or `$601E-$601F` without additional evidence.

## Per-image header and callback matrix

This table is derived from `gamate-rom-header-report.py`. It exposes field combinations and callback
families without assigning meaning to the unknown bytes.

| Image | `$02-$04` | `$1E-$1F` | `$6020` start | `$6023` NMI | `$6026` IRQ |
| --- | --- | --- | --- | --- | --- |
| 4-in-1 (Mini Golf, Cube-Up, Brick Blaster, and Vindicators) | `01 00 99` | `00 FF` | `JMP $8F7B` | `JMP $61B9` | `JMP $7F0E` |
| Bao Qing Tian | `01 00 00` | `00 FF` | `JMP $6330` | `JMP $6896` | `JMP $6897` |
| Baseball | `01 00 01` | `00 00` | `JMP $62BD` | `JMP $8769` | `JMP $8765` |
| Basketball | `01 00 00` | `00 FF` | `JMP $63CC` | `JMP $6391` | `JMP $6392` |
| Bomb Blaster (Alt) | `01 00 00` | `00 FF` | `JMP $6E38` | `JMP $6161` | `JMP $617D` |
| Bomb Blaster | `01 00 00` | `00 FF` | `JMP $6E38` | `JMP $6161` | `JMP $617D` |
| Boom | `01 00 06` | `00 FF` | `JMP $6164` | `JMP $6151` | `JMP $6152` |
| Box Forum | `01 00 99` | `00 20` | `JMP $61AC` | `JMP $6199` | `JMP $619A` |
| Brick Blaster | `01 00 05` | `00 FF` | `JMP $717D` | `JMP $61B1` | `JMP $61CC` |
| Cosmic Fighter | `01 00 05` | `00 F5` | `JMP $62AB` | `JMP $6161` | `JMP $6162` |
| Cube Up | `01 00 01` | `00 FF` | `JMP $61BA` | `JMP $61B9` | `JMP $7F0E` |
| Devil Castle | `01 00 99` | `00 FF` | `JMP $7920` | `JMP $7CB9` | `JMP $7CBA` |
| Dino Ball | `01 00 99` | `00 FF` | `JMP $8A34` | `JMP $C251` | `JMP $C252` |
| Dino Bibo | `01 00 99` | `00 FF` | `JMP $7180` | `JMP $73AF` | `JMP $73B0` |
| Dinosaur Park | `01 00 00` | `00 FF` | `JMP $68BF` | `JMP $63BB` | `JMP $63BC` |
| Enchanted Bricks | `01 00 05` | `FF FF` | `JMP $7175` | `JMP $61A9` | `JMP $61C4` |
| Famous 7 | `01 00 00` | `00 FF` | `JMP $7CD3` | `JMP $7F31` | `JMP $7ECC` |
| Fantasy Travel | `01 00 01` | `00 00` | `JMP $638C` | `JMP $8B54` | `JMP $8B50` |
| Fist of Thunder | `01 00 99` | `00 FF` | `JMP $6F61` | `JMP $713E` | `JMP $713F` |
| Flipuzzle | `01 00 99` | `00 FF` | `JMP $9441` | `JMP $BFB4` | `JMP $BFB5` |
| Flying Goblin | `01 00 01` | `00 FF` | `JMP $8FAB` | `JMP $708C` | `JMP $705B` |
| Fortress of Fierceness | `01 00 99` | `00 FF` | `JMP $61CC` | `JMP $61B9` | `JMP $61BA` |
| Fortune 'n Luck | `01 00 01` | `00 00` | `JMP $62F3` | `JMP $87FB` | `JMP $87E6` |
| Further Adventures of Hannibal The Cat | `01 00 00` | `00 FF` | `JMP $63F2` | `JMP $63B5` | `JMP $63B6` |
| Galaxy Invaders | `01 00 07` | `00 99` | `JMP $6171` | `JMP $C857` | `JMP $C858` |
| GP Race | `01 00 01` | `00 D7` | `JMP $6251` | `RTS` | `JMP $6E72` |
| Hot Hero | `01 00 01` | `00 FF` | `JMP $6366` | `JMP $67B9` | `JMP $67BA` |
| Incantational Couple | `01 00 99` | `00 00` | `JMP $6171` | `JMP $8766` | `JMP $8762` |
| Insect War | `01 09 76` | `00 FF` | `JMP $63EF` | `JMP $63B2` | `JMP $63B3` |
| Jewelriss | `01 00 99` | `00 FF` | `JMP $89C9` | `JMP $B7E1` | `JMP $B7E2` |
| Kiki Inland | `01 00 01` | `00 FF` | `JMP $627C` | `JMP $6221` | `JMP $6222` |
| Kill Shot | `01 00 33` | `00 FF` | `JMP $7400` | `JMP $8DBF` | `JMP $8DC0` |
| Kung-Fu Fighter | `01 00 11` | `00 AC` | `JMP $616E` | `JMP $616D` | `JMP $6169` |
| Legend of Dragon Knight | `01 00 99` | `00 FF` | `JMP $61C7` | `JMP $6151` | `JMP $6152` |
| Magic Jigsaw | `01 00 99` | `00 20` | `JMP $6229` | `JMP $6169` | `JMP $616A` |
| Marauder | `01 00 99` | `00 FF` | `JMP $6604` | `JMP $6161` | `JMP $617D` |
| Mars Voyage | `01 00 67` | `00 FF` | `JMP $8D78` | `JMP $930C` | `JMP $930D` |
| Mighty Boxer | `01 00 77` | `00 FA` | `JMP $9284` | `JMP $9610` | `JMP $9665` |
| Mighty Tank | `01 00 04` | `00 FF` | `JMP $6152` | `JMP $FC44` | `JMP $AC32` |
| Mini Golf | `01 00 01` | `00 00` | `JMP $6161` | `JMP $970A` | `JMP $9706` |
| Money Maze | `01 00 11` | `00 FF` | `JMP $7E0F` | `JMP $6151` | `JMP $616D` |
| Monster Pitfall | `01 00 02` | `00 FF` | `JMP $6D29` | `JMP $6151` | `JMP $6152` |
| Myth of Asamia | `01 00 88` | `00 FF` | `JMP $64AF` | `JMP $6151` | `JMP $6152` |
| Nightmare of Santa Claus | `01 00 11` | `00 FF` | `JMP $6979` | `JMP $6151` | `JMP $616D` |
| One Million Whys | `01 00 00` | `00 8E` | `JMP $6398` | `RTS` | `JMP $6367` |
| Pharaoh Revenger | `01 00 00` | `00 FF` | `JMP $99CB` | `JMP $9D92` | `JMP $D58D` |
| Pipemania | `01 00 59` | `00 FF` | `JMP $88FA` | `JMP $80D2` | `JMP $80B0` |
| Punk Boy | `01 00 01` | `00 FF` | `JMP $62BC` | `JMP $62A9` | `JMP $62AA` |
| QuizFighter | `01 00 00` | `00 FF` | `JMP $60BC` | `RTS` | `JMP $6075` |
| Riddle of the Ancient Tomb | `01 00 00` | `00 FF` | `JMP $6372` | `JMP $6885` | `JMP $68DE` |
| Robin Hood | `01 00 01` | `00 FF` | `JMP $68A4` | `JMP $63A6` | `JMP $63A7` |
| Snowman Legend | `01 00 96` | `00 FF` | `JMP $7060` | `RTS` | `JMP $965D` |
| Tennis | `01 00 03` | `00 FF` | `JMP $61D1` | `JMP $6151` | `JMP $D57B` |
| The Golden Pyramid | `01 00 00` | `00 FF` | `JMP $64A3` | `JMP $63C9` | `JMP $63CA` |
| Time Warrior | `01 00 11` | `00 F7` | `JMP $6151` | `JMP $C1C8` | `JMP $C1C9` |
| Tornado | `01 99 99` | `00 FF` | `JMP $64FA` | `JMP $61CC` | `JMP $61CD` |
| Tough Guy | `01 00 00` | `00 8E` | `JMP $6281` | `JMP $9E75` | `JMP $9E76` |
| Treasure Hunter | `01 00 99` | `00 FF` | `JMP $6EF1` | `JMP $6151` | `JMP $616D` |
| Vindicators | `01 00 04` | `00 FF` | `JMP $6152` | `JMP $6151` | `JMP $AC67` |
| Volcano Panic | `01 00 00` | `00 FF` | `JMP $61CC` | `JMP $9FE2` | `JMP $9FDE` |
| Witty Apee | `01 00 02` | `00 FF` | `JMP $6EF1` | `JMP $6151` | `JMP $616D` |
| World Cup Soccer | `01 00 01` | `00 FF` | `JMP $7B6C` | `JMP $7C88` | `JMP $7C8B` |

## Cartridge callback ABI

Both authentic BIOS revisions implement the same cartridge-facing dispatch structure:

- After protection, signature, checksum, and logo processing, the BIOS jumps to `$6020`.
- The BIOS NMI wrapper reads the UART receive byte into BIOS RAM, then conditionally calls `$6023`.
- The BIOS periodic-IRQ wrapper updates its counters, then conditionally calls `$6026`.
- The callback stubs return to the BIOS wrapper with `RTS`; the wrapper performs the eventual
  `RTI`. A cartridge may use an absolute `JMP` stub so its real handler can end in `RTS`.

Corpus results:

- All 62 `$6020` stubs are absolute jumps. Every target lies in `$6000-$9FFF`, the fixed lower ROM
  window on the common mapper, so startup does not depend on the initial upper-bank mapping.
- All 62 `$6026` IRQ stubs are absolute jumps.
- 58 `$6023` NMI stubs are absolute jumps.
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

Runtime mapper policy uses this four-component layout rather than a whole-image checksum: a 128 KiB
image selects the 4-in-1 mapper only when each 32 KiB component has the copyright signature at its
header position and a valid component checksum. This recognizes equivalent dumps without treating
an incidental embedded signature as mapper evidence.

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
