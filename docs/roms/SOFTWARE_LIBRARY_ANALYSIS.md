# Gamate software-library analysis

This report begins point 3 of [`ROM_STATIC_ANALYSIS.md`](ROM_STATIC_ANALYSIS.md): identifying shared
software libraries, engine families, and cartridge-facing ABI conventions across commercial games.

The first pass deliberately examines only exact reuse within each image's first 16 KiB ROM bank.
That bank is fixed on the common mapper and is the initial lower bank on the 4-in-1, so its startup
mapping does not depend on run-time upper-bank state. Relocated routines, switchable-bank code, and
semantically equivalent but modified routines require later disassembly work.

## Reproduction and method

Run the fixed-bank comparison against legally obtained cartridge dumps:

```sh
python3 tools/gamate-rom-shared-report.py /path/to/gamate/cartridges
```

For each image, the tool indexes every overlapping 32-byte window in ROM offsets `$0000-$3FFF`.
Low-information windows containing fewer than five distinct byte values are excluded. Windows found
in more than eight games are also excluded from pair scores so ubiquitous padding and generic tables
do not dominate the result.

For each reported pair, the tool records:

- The number of selectively shared 32-byte windows.
- Runs of matching windows that are unique within both fixed banks.
- Total and longest exact unique spans, with offsets in both images.

Overlapping-window counts are a similarity score, not a byte count. Exact spans may contain 6502
code, graphics, maps, fonts, audio data, or lookup tables. A pair is a reuse candidate until
disassembly or data-format analysis classifies the shared regions.

## Strong initial reuse candidates

The table below records the strongest non-identical cross-title pairs from the original 58-image
corpus.
Bomb Blaster and its alternate dump are omitted from this summary because they are variants of the
same title; the 4-in-1/Cube Up relationship is already documented as a modified embedded component.

| Pair | Shared windows | Exact unique spans | Exact-span bytes | Longest span | Longest offsets A/B |
| --- | ---: | ---: | ---: | ---: | --- |
| Brick Blaster / Enchanted Bricks | 4,775 | 99 | 7,629 | 825 | `$0CD6` / `$0CCE` |
| One Million Whys / Riddle of the Ancient Tomb | 2,490 | 28 | 3,012 | 352 | `$3A78` / `$3A78` |
| Hot Hero / Robin Hood | 2,254 | 28 | 2,915 | 311 | `$3D92` / `$3D92` |
| Mighty Tank / Vindicators | 2,247 | 83 | 5,027 | 305 | `$0028` / `$0028` |
| Insect War / QuizFighter | 1,951 | 34 | 2,820 | 290 | `$3DA7` / `$3DA7` |
| Bao Qing Tian / Insect War | 1,946 | 31 | 3,170 | 496 | `$0029` / `$0029` |
| Hot Hero / Insect War | 1,696 | 24 | 2,706 | 552 | `$0029` / `$0029` |
| Bao Qing Tian / Hot Hero | 1,622 | 28 | 2,804 | 496 | `$0029` / `$0029` |
| Bao Qing Tian / QuizFighter | 1,615 | 27 | 2,439 | 289 | `$3D68` / `$3DA8` |
| Further Adventures of Hannibal The Cat / Insect War | 1,587 | 44 | 3,046 | 553 | `$0028` / `$0028` |
| QuizFighter / Robin Hood | 1,527 | 22 | 2,196 | 289 | `$3DA8` / `$3DA8` |
| Hot Hero / QuizFighter | 1,502 | 22 | 2,171 | 289 | `$3DA8` / `$3DA8` |
| Further Adventures of Hannibal The Cat / Riddle of the Ancient Tomb | 1,417 | 19 | 1,852 | 408 | `$0081` / `$0081` |
| Insect War / Robin Hood | 1,403 | 22 | 2,032 | 289 | `$3DA8` / `$3DA8` |
| Bao Qing Tian / Robin Hood | 1,401 | 26 | 2,199 | 289 | `$3D68` / `$3DA8` |
| Basketball / QuizFighter | 1,345 | 37 | 2,269 | 543 | `$2225` / `$1A09` |
| Treasure Hunter / Witty Apee | 1,332 | 21 | 2,425 | 521 | `$0409` / `$0409` |
| Further Adventures of Hannibal The Cat / One Million Whys | 1,142 | 14 | 1,272 | 203 | `$3C34` / `$3C34` |
| Dinosaur Park / Robin Hood | 1,112 | 16 | 1,602 | 321 | `$320B` / `$2F43` |
| Flying Goblin / Robin Hood | 960 | 19 | 1,600 | 289 | `$3DA8` / `$3DA8` |
| Bao Qing Tian / Flying Goblin | 892 | 17 | 1,755 | 296 | `$0029` / `$0029` |
| Flying Goblin / Hot Hero | 886 | 17 | 1,747 | 296 | `$0029` / `$0029` |
| Flying Goblin / Insect War | 886 | 17 | 1,705 | 296 | `$0029` / `$0029` |
| Flying Goblin / QuizFighter | 844 | 13 | 1,272 | 289 | `$3DA8` / `$3DA8` |
| Basketball / Dinosaur Park | 730 | 15 | 1,186 | 416 | `$0081` / `$0081` |
| Riddle of the Ancient Tomb / Robin Hood | 664 | 10 | 968 | 324 | `$2AAA` / `$2F43` |
| Dino Bibo / Punk Boy | 614 | 11 | 892 | 186 | `$20D2` / `$0756` |
| GP Race / QuizFighter | 585 | 13 | 1,163 | 289 | `$3DA8` / `$3DA8` |
| GP Race / Hot Hero | 571 | 12 | 1,164 | 289 | `$3DA8` / `$3DA8` |
| GP Race / Robin Hood | 571 | 11 | 1,101 | 289 | `$3DA8` / `$3DA8` |
| Dinosaur Park / Riddle of the Ancient Tomb | 570 | 5 | 725 | 321 | `$320B` / `$2AAA` |
| Bao Qing Tian / GP Race | 566 | 12 | 1,162 | 289 | `$3D68` / `$3DA8` |
| GP Race / Insect War | 566 | 11 | 1,087 | 289 | `$3DA8` / `$3DA8` |
| Flying Goblin / GP Race | 550 | 11 | 1,121 | 289 | `$3DA8` / `$3DA8` |
| Marauder / Tornado | 516 | 14 | 973 | 185 | `$34D4` / `$2BA1` |

The table includes every distinct-title pair with at least 500 selectively shared windows. The tool
also reports the two expected same-lineage cases above this threshold: the two Bomb Blaster dumps,
and the modified Cube Up component in the 4-in-1.

## Complete-corpus extension

The comparison was rerun after Mighty Boxer, Kiki Inland, Fortress of Fierceness, and Incantational
Couple completed the 62-image catalogued corpus. No new pair crossed the original table's threshold
of 500 selectively shared windows, but several lower-scoring matches identify useful classification
targets.

| New title | Strongest candidate | Shared windows | Exact unique spans | Exact-span bytes | Longest span | Longest offsets A/B |
| --- | --- | ---: | ---: | ---: | ---: | --- |
| Mighty Boxer | QuizFighter | 141 | 7 | 834 | 288 | `$3DB9` / `$3DA9` |
| Kiki Inland | Box Forum | 411 | 2 | 479 | 257 | `$0031` / `$0031` |
| Fortress of Fierceness | Boom | 219 | 7 | 359 | 81 | `$04C7` / `$03EF` |
| Incantational Couple | Dino Bibo | 26 | 0 | 0 | 0 | — |

Mighty Boxer shares the same 288-byte late-fixed-bank block with QuizFighter, Bao Qing Tian, Insect
War, Mars Voyage, Hot Hero, and Robin Hood. Across those comparisons the block appears near
`$3DA9-$3DB9`; Mighty Boxer/QuizFighter has the strongest aggregate match at 834 exact bytes across
seven spans. This extends the established large-ROM shared-library network to Mighty Boxer, but does
not by itself establish a shared game engine. The consistently placed block is more cautiously
classified as a candidate common runtime, SDK component, or data package pending disassembly.

Kiki Inland and Box Forum share a 257-byte span at the same `$0031` offset, immediately after the
fixed cartridge header and callback table. Smaller Kiki Inland matches with Punk Boy and One Million
Whys also touch early fixed-bank material. The position favors a shared startup or low-level library
shell over an engine relationship, although executable-code classification is still required.

Fortress of Fierceness has smaller, fragmented matches with Boom, Punk Boy, Box Forum, and Kiki
Inland. Its strongest candidate contains only 359 exact bytes across seven spans, with an 81-byte
longest span, so the current evidence does not justify naming an engine family. Incantational Couple
is effectively isolated under this method: its only reported new-title pair has no span unique in
both fixed banks.

None of the four additions shares a complete start/NMI/IRQ callback tuple with another catalogued
image. The highest-priority classification targets from this extension are Mighty Boxer's late-bank
288-byte block, the Box Forum/Kiki Inland `$0031` span, and the scattered Boom/Fortress of Fierceness
matches.

## Initial family hypotheses

### Brick-game family

Brick Blaster and Enchanted Bricks share the largest amount of fixed-bank material among distinct
standalone titles: 7,629 bytes across exact unique spans, including an 825-byte region at nearly the
same offset. Their header callback targets are also close but not identical. This is strong evidence
of a shared engine, shared asset library, or one title being derived from the other. Code/data
classification is still required before naming specific shared routines.

### Tank-game family

Mighty Tank and Vindicators share 5,027 exact-span bytes and have the same `$6152` startup target.
Their 305-byte longest span begins at ROM offset `$0028`, the final byte of the cartridge callback
table, and continues into the post-header area. Their IRQ targets differ, so they are related builds
rather than byte-identical program shells.

### Lode Runner-like family

Treasure Hunter and Witty Apee have identical startup, NMI, and IRQ stubs and share 2,425 bytes of
unique exact spans. This is direct evidence of a shared low-level program shell and a strong engine
relationship candidate.

### Later large-ROM family

Bao Qing Tian, Basketball, Dinosaur Park, Further Adventures of Hannibal The Cat, Flying Goblin, GP
Race, Hot Hero, Insect War, One Million Whys, QuizFighter, Riddle of the Ancient Tomb, and Robin Hood
form a dense network of fixed-bank matches. Several matches occur at identical late-bank offsets
near `$3A00-$3FFF`, suggesting a common library or data package placed consistently by a shared
build process.

This network crosses catalogue publisher labels, so publisher alone is not a reliable development-
lineage marker. It may represent a UMC-era SDK, contracted development reuse, shared assets, or a
combination. Routine-level disassembly is needed before treating it as one engine family.

### Additional candidates

Dino Bibo/Punk Boy and Marauder/Tornado share smaller but nontrivial relocated spans. Because their
largest matches occur at different offsets, these pairs are useful candidates for testing a future
relocation-aware routine matcher.

## Exact callback-stub families

The fixed callback table independently identifies these exact address-level families:

| Shared convention | Games |
| --- | --- |
| Complete start/NMI/IRQ tuple | Bomb Blaster and Bomb Blaster (Alt) |
| Complete start/NMI/IRQ tuple | Treasure Hunter and Witty Apee |
| NMI/IRQ pair `$61B9/$7F0E` | 4-in-1 and Cube Up |
| NMI/IRQ pair `$6161/$617D` | Both Bomb Blaster dumps and Marauder |
| NMI/IRQ pair `$6151/$6152` | Boom, Legend of Dragon Knight, Monster Pitfall, Myth of Asamia |
| NMI/IRQ pair `$6151/$616D` | Money Maze, Nightmare of Santa Claus, Treasure Hunter, Witty Apee |

Shared target addresses are weaker evidence than shared bytes because different images can place
unrelated routines at the same address. They become meaningful when combined with exact-span or
disassembly evidence.

## Next classification work

The next pass should disassemble the shared spans and label only those that can be shown to be code.
Priority targets are:

1. The post-header Mighty Tank/Vindicators span.
2. Treasure Hunter/Witty Apee startup and interrupt routines.
3. Brick Blaster/Enchanted Bricks drawing and input code.
4. The repeated late-bank package in the large-ROM network.
5. Relocation-aware matching for Dino Bibo/Punk Boy and Marauder/Tornado.

Dynamic execution traces should confirm that candidate routines are reachable before their behavior
is used to make emulator compatibility or timing claims.
