# Gamate ROM layout analysis

This report documents cartridge capacities, padding, repeated regions, possible mirroring, and the
limits of identifying unused ROM from static file structure. Header fields and the boot callback
ABI are documented separately in [`ROM_HEADER_ANALYSIS.md`](ROM_HEADER_ANALYSIS.md).

The analysis covers all 62 MAME-catalogued cartridge images identified in
[`ROM_CORPUS.md`](ROM_CORPUS.md).

## Reproduction

Run the layout analyzer against a directory containing legally obtained cartridge dumps:

```sh
python3 tools/gamate-rom-layout-report.py /path/to/gamate/cartridges
```

The tool recognizes the documented copyright signature and emits only filenames, hashes, sizes,
fill spans, uniform-bank descriptions, and aligned repetition. It does not retain ROM contents.

## ROM sizes and capacity convention

Every dump has a power-of-two size supported by the known cartridge arrangements:

| Dump size | Images |
| ---: | ---: |
| 16 KiB | 1 |
| 32 KiB | 34 |
| 64 KiB | 9 |
| 128 KiB | 7 |
| 256 KiB | 2 |
| 512 KiB | 9 |

The larger dumps preserve the apparent ROM-device capacity even when the final banks are erased.
They are not trimmed to the last non-fill byte or last populated 16 KiB bank. This makes file size a
useful capacity observation, but not a measurement of how much content the game uses.

## Mapper and padding categories

The mapper column below records Superwhizkid's current loading policy, not an identifier stored in
the ROM: images of 32 KiB or less use the plain mapping, larger images use the common banked mapping,
and the known 4-in-1 is selected by its full-image fingerprint.

For pattern analysis, a terminal padding convention means a run of at least 16 `$FF` or `$00`
bytes. Shorter repetitions are classified as no clear terminal padding.

| Mapper policy | `$FF` padding | `$00` padding | No clear padding | Total |
| --- | ---: | ---: | ---: | ---: |
| Plain | 18 | 1 | 16 | 35 |
| Common banked | 6 | 3 | 17 | 26 |
| 4-in-1 | 1 | 0 | 0 | 1 |
| **Total** | **25** | **4** | **33** | **62** |

This exposes a corpus-level pattern: long `$FF` tail padding is common among plain cartridges, while
two thirds of common-banked images carry data to the end without a clear terminal fill convention.
The four `$00`-padded images are Famous 7, Insect War, Time Warrior, and Volcano Panic.

## End padding conventions

Two erased-state values occur: `$FF` is dominant, while some later images use `$00`.

- 28 images end in `$FF`; 25 of those have an `$FF` run of at least 16 bytes.
- Ten images end in `$00`; four of those have a `$00` run of at least 16 bytes.
- 24 images have at least 256 bytes of trailing `$FF` or `$00` fill.
- The other 38 images either carry data to the last few bytes or have only a short terminal run, so
  there is no universal end-padding requirement.

Many early 16/32 KiB games use `$FF` from the end of meaningful content through the physical dump
boundary. Examples range from 334 trailing bytes in Vindicators to 10,682 in Bomb Blaster. This is
consistent with unused erased ROM space, but the ROM bytes alone do not show whether every byte
before the run is reachable.

Some long fill runs are internal rather than terminal and must not be treated as a logical end:

- Enchanted Bricks has 5,930 `$FF` bytes at `$63E1-$7B0A`, followed by data.
- Punk Boy has 4,225 `$FF` bytes at `$AF7F-$BFFF`, followed by another populated bank.
- The 4-in-1 has fill within component slots followed by later component games.

These are layout gaps or blank assets, not evidence that the remainder of the image is unused.

## Complete unused-looking banks

At 16 KiB granularity, five images contain wholly uniform banks:

| Image | Uniform banks | Apparent populated extent |
| --- | --- | ---: |
| 4-in-1 | Bank 1 is `$FF` | Internal gap; later component slots are populated |
| Dinosaur Park | Banks 21-31 are `$FF` | `$00000-$53FFF` (21 banks, 336 KiB) |
| Further Adventures of Hannibal The Cat | Banks 29-31 are `$FF` | `$00000-$73FFF` (29 banks, 464 KiB) |
| Hot Hero | Banks 24-31 are `$FF` | `$00000-$5FFFF` (24 banks, 384 KiB) |
| Insect War | Banks 30-31 are `$00` | `$00000-$77FFF` (30 banks, 480 KiB) |

No other image contains a complete uniform 16 KiB bank. For the four 512 KiB games with terminal
uniform banks, the simplest interpretation is a partially populated fixed-capacity ROM image.
Static bytes cannot prove that software never intentionally selects a uniform bank, so “unused” is
a layout classification pending bank-selection traces.

## Repetition and mirroring

There is no byte-identical whole-image half mirror in the corpus, and no image repeats an aligned
64 KiB block. Therefore none of the dumps appears to be a smaller image expanded to its listed size
by systematic file-level mirroring.

Hot Hero is the only image with repeated nonuniform regions at 32 or 16 KiB granularity:

- 16 KiB banks 17, 18, and 19 exactly duplicate banks 21, 22, and 23 respectively.
- Consequently, aligned 32 KiB blocks 9 and 11 are identical.
- Bank 20, between the two copies, is different, and banks 24-31 are erased `$FF`.

This selective 48 KiB repetition is content duplication, not whole-chip address-line mirroring. A
likely software explanation is duplication to make common code or data available under different
bank layouts, but that requires control-flow and bank-write analysis before it can be asserted.

Exact repeated 4 or 8 KiB blocks occur in several larger games. At those smaller granularities,
graphics, maps, lookup tables, and common routines can naturally repeat, so block equality alone is
not classified as mirroring.

## The 16 KiB Cube Up question

Cube Up is the only 16 KiB dump, while the flat cartridge CPU window spans 32 KiB. The current core
and MAME both wrap the ROM offset by the loaded size, which presents the 16 KiB image twice, but that
is emulator policy rather than proof of the cartridge wiring.

The Cube Up start, NMI, and IRQ targets all lie in `$6000-$9FFF`, and the BIOS validation range also
lies within that first 16 KiB. The known software therefore does not need the upper half of the flat
window during startup. The dump cannot distinguish among a physically mirrored 16 KiB ROM, open or
chip-defined reads in the upper half, or a board on which the upper half was simply irrelevant.
PCB tracing or a test that executes or reads `$A000-$DFFF` on the original cartridge is still
needed.

## Per-image layout matrix

This table is derived from `gamate-rom-layout-report.py`. `Terminal fill` is shown only for runs of
at least 16 bytes. Bank and block numbers are zero-based. A dash means that the analyzer found no
feature in that category.

| Image | Size | Mapper policy | Terminal fill | Uniform 16 KiB banks | Repeated aligned regions |
| --- | ---: | --- | --- | --- | --- |
| 4-in-1 (Mini Golf, Cube-Up, Brick Blaster, and Vindicators) | 128 KiB | 4-in-1 | `$FF` × 334 | 1:FF | — |
| Bao Qing Tian | 512 KiB | Common banked | — | — | — |
| Baseball | 64 KiB | Common banked | — | — | — |
| Basketball | 256 KiB | Common banked | — | — | — |
| Bomb Blaster (Alt) | 32 KiB | Plain | `$FF` × 10,682 | — | — |
| Bomb Blaster | 32 KiB | Plain | `$FF` × 10,682 | — | — |
| Boom | 32 KiB | Plain | `$FF` × 399 | — | — |
| Box Forum | 32 KiB | Plain | `$FF` × 2,973 | — | — |
| Brick Blaster | 32 KiB | Plain | `$FF` × 4,684 | — | — |
| Cosmic Fighter | 32 KiB | Plain | — | — | — |
| Cube Up | 16 KiB | Plain | `$FF` × 4,230 | — | — |
| Devil Castle | 32 KiB | Plain | — | — | — |
| Dino Ball | 32 KiB | Plain | — | — | — |
| Dino Bibo | 32 KiB | Plain | — | — | — |
| Dinosaur Park | 512 KiB | Common banked | `$FF` × 180,224 | 21:FF, 22:FF, 23:FF, 24:FF, 25:FF, 26:FF, 27:FF, 28:FF, 29:FF, 30:FF, 31:FF | — |
| Enchanted Bricks | 32 KiB | Plain | `$FF` × 581 | — | — |
| Famous 7 | 64 KiB | Common banked | `$00` × 3,225 | — | — |
| Fantasy Travel | 128 KiB | Common banked | — | — | — |
| Fist of Thunder | 64 KiB | Common banked | — | — | — |
| Flipuzzle | 32 KiB | Plain | — | — | — |
| Flying Goblin | 64 KiB | Common banked | — | — | — |
| Fortress of Fierceness | 32 KiB | Plain | — | — | — |
| Fortune 'n Luck | 128 KiB | Common banked | — | — | — |
| Further Adventures of Hannibal The Cat | 512 KiB | Common banked | `$FF` × 49,152 | 29:FF, 30:FF, 31:FF | — |
| Galaxy Invaders | 32 KiB | Plain | — | — | — |
| GP Race | 128 KiB | Common banked | — | — | — |
| Hot Hero | 512 KiB | Common banked | `$FF` × 131,072 | 24:FF, 25:FF, 26:FF, 27:FF, 28:FF, 29:FF, 30:FF, 31:FF | 32 KiB: 9=11; 16 KiB: 17=21, 18=22, 19=23 |
| Incantational Couple | 32 KiB | Plain | — | — | — |
| Insect War | 512 KiB | Common banked | `$00` × 32,768 | 30:00, 31:00 | — |
| Jewelriss | 32 KiB | Plain | `$FF` × 5,348 | — | — |
| Kiki Inland | 64 KiB | Common banked | `$FF` × 8,167 | — | — |
| Kill Shot | 32 KiB | Plain | — | — | — |
| Kung-Fu Fighter | 32 KiB | Plain | — | — | — |
| Legend of Dragon Knight | 64 KiB | Common banked | `$FF` × 2,393 | — | — |
| Magic Jigsaw | 32 KiB | Plain | `$FF` × 151 | — | — |
| Marauder | 32 KiB | Plain | `$FF` × 171 | — | — |
| Mars Voyage | 64 KiB | Common banked | — | — | — |
| Mighty Boxer | 128 KiB | Common banked | — | — | — |
| Mighty Tank | 32 KiB | Plain | `$FF` × 18 | — | — |
| Mini Golf | 32 KiB | Plain | — | — | — |
| Money Maze | 32 KiB | Plain | `$FF` × 1,659 | — | — |
| Monster Pitfall | 32 KiB | Plain | `$FF` × 519 | — | — |
| Myth of Asamia | 32 KiB | Plain | — | — | — |
| Nightmare of Santa Claus | 32 KiB | Plain | `$FF` × 1,065 | — | — |
| One Million Whys | 512 KiB | Common banked | — | — | — |
| Pharaoh Revenger | 32 KiB | Plain | — | — | — |
| Pipemania | 32 KiB | Plain | — | — | — |
| Punk Boy | 64 KiB | Common banked | `$FF` × 3,775 | — | — |
| QuizFighter | 256 KiB | Common banked | — | — | — |
| Riddle of the Ancient Tomb | 512 KiB | Common banked | — | — | — |
| Robin Hood | 512 KiB | Common banked | — | — | — |
| Snowman Legend | 32 KiB | Plain | — | — | — |
| Tennis | 32 KiB | Plain | — | — | — |
| The Golden Pyramid | 512 KiB | Common banked | — | — | — |
| Time Warrior | 32 KiB | Plain | `$00` × 44 | — | — |
| Tornado | 32 KiB | Plain | `$FF` × 336 | — | — |
| Tough Guy | 128 KiB | Common banked | — | — | — |
| Treasure Hunter | 32 KiB | Plain | `$FF` × 988 | — | — |
| Vindicators | 32 KiB | Plain | `$FF` × 334 | — | — |
| Volcano Panic | 128 KiB | Common banked | `$00` × 113 | — | — |
| Witty Apee | 32 KiB | Plain | `$FF` × 2,204 | — | — |
| World Cup Soccer | 64 KiB | Common banked | — | — | — |

## What can be called unused

The static corpus supports three progressively weaker labels:

1. **Erased padding:** contiguous `$FF` or `$00` fill, directly observable.
2. **Uniform banks:** plausible unpopulated capacity, but still selectable data from the mapper's
   perspective.
3. **Unreachable content:** not established by byte layout. This requires reachable disassembly and
   dynamic logging of selected banks and ROM reads.

Accordingly, the report does not trim dumps, infer smaller mapper masks from fill, or treat repeated
content as hardware mirroring. Those decisions would exceed what static file structure proves.
