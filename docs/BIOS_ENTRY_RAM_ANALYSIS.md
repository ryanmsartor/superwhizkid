# BIOS cartridge-entry RAM analysis

This document tracks controlled perturbation of the 1 KiB physical RAM state inherited when the
authentic BIOS enters a cartridge at `$6020`. It records aggregate behavior and title categories,
not authentic RAM contents or a reusable firmware-derived state image.

## Quarter-page sweep

Each 64-byte physical RAM block was independently cleared at cartridge entry while every other RAM
byte and all CPU, hardware, mapper, timing, and peripheral state remained authentic. All 58 locally
available cartridges were run under both recognized BIOS revisions for 1,000 post-entry frames,
giving 116 cases per block and 1,856 perturbed runs. Every run completed without an unsupported
opcode or blank-output failure.

“Changed execution” compares final CPU, video, audio, VRAM, mapper, peripheral, controller, and
activity observations while excluding only the deliberately changed final RAM hash. Counts are
BIOS/cartridge cases; the title column collapses matching results across BIOS revisions.

| Block | Physical range | Changed executions | Final-video changes | Final-VRAM changes | Affected titles |
| --- | --- | ---: | ---: | ---: | --- |
| `00` | `$000-$03F` | 106 | 24 | 29 | 53 titles; all except Bao Qing Tian, Basketball, Dinosaur Park, Riddle of the Ancient Tomb, and The Golden Pyramid |
| `01` | `$040-$07F` | 10 | 2 | 0 | Dino Bibo; Galaxy Invaders; Mars Voyage; Money Maze; Time Warrior |
| `02` | `$080-$0BF` | 14 | 2 | 0 | Famous 7; Galaxy Invaders; Insect War; Jewelriss; Money Maze; One Million Whys; Tornado |
| `03` | `$0C0-$0FF` | 38 | 2 | 3 | Baseball; Boom; Devil Castle; Dino Bibo; Fantasy Travel; Flying Goblin; Fortune 'n Luck; Galaxy Invaders; Kill Shot; Legend of Dragon Knight; Marauder; Money Maze; Myth of Asamia; Nightmare of Santa Claus; One Million Whys; Pipemania; Punk Boy; Snowman Legend; Tennis |
| `04` | `$100-$13F` | 2 | 0 | 0 | Time Warrior |
| `05` | `$140-$17F` | 0 | 0 | 0 | None observed |
| `06` | `$180-$1BF` | 0 | 0 | 0 | None observed |
| `07` | `$1C0-$1FF` | 0 | 0 | 0 | None observed |
| `08` | `$200-$23F` | 26 | 4 | 8 | Bomb Blaster and its alternate dump; Famous 7; Fist of Thunder; GP Race; Jewelriss; Kill Shot; Mars Voyage; Tornado; Tough Guy; Treasure Hunter; Witty Apee; World Cup Soccer |
| `09` | `$240-$27F` | 18 | 4 | 4 | Bomb Blaster and its alternate dump; Dino Ball; Famous 7; Flipuzzle; Mars Voyage; One Million Whys; Pipemania; Tough Guy |
| `0A` | `$280-$2BF` | 6 | 2 | 2 | Dino Ball; Hot Hero; Kill Shot |
| `0B` | `$2C0-$2FF` | 2 | 0 | 0 | Tornado |
| `0C` | `$300-$33F` | 2 | 0 | 0 | Punk Boy |
| `0D` | `$340-$37F` | 2 | 0 | 0 | Flying Goblin |
| `0E` | `$380-$3BF` | 7 | 0 | 0 | Flying Goblin; Insect War; World Cup Soccer under both BIOSes; Dino Bibo under UMC only |
| `0F` | `$3C0-$3FF` | 4 | 0 | 0 | Insect War; World Cup Soccer |

The Dino Bibo block-`0E` asymmetry is limited to one additional active AY-A sample and the resulting
audio hash in the UMC run. Its sampled PC, video, VRAM, and other compared observations match. This
is consistent with a phase-sensitive observation at the 1,000-frame boundary; it is not evidence
that the BIOS revisions produce different entry RAM, which direct comparison has already ruled out.

## Interpretation

The earlier page sweep is now substantially narrower:

- `$000-$03F` alone reproduces all 106 execution differences seen when the entire zero page was
  cleared. Later zero-page quarters still contain title-specific dependencies, but they do not add
  another affected case to that total.
- The stack-page result is entirely localized to `$100-$13F` and Time Warrior. Clearing
  `$140-$1FF` produced no compared difference in this sweep.
- Page 2 contains several distinct title work areas rather than one isolated common field. Its four
  quarters affect 13, 9, 3, and 1 title respectively.
- Page 3 is also title-specific. Its four quarters affect Punk Boy, Flying Goblin, and combinations
  of Flying Goblin, Insect War, World Cup Soccer, and Dino Bibo.
- Only blocks `00`, `01`, `02`, `03`, `08`, `09`, and `0A` changed the sampled final video in this
  experiment. A zero observed count remains coverage evidence, not proof that a block is irrelevant
  in all later gameplay.

The next efficient refinement is 16-byte perturbation within the affected blocks. Block `00` should
be first because it contains the shared BIOS callback flag and dominates the catalog. Blocks `03`,
`08`, and `09` follow because they affect the most additional titles; the single-title upper blocks
can be analyzed alongside their cartridge code rather than exhaustively subdividing every block at
once. After candidate fields are identified, inverse tests must preserve only those fields while
clearing their surrounding block to test sufficiency, not merely necessity.

## Reproduction and limits

`GAMATEBOOT_ENTRY_RAM=zero-block-NN` selects hexadecimal block `00` through `0F` in the diagnostic
`gamateboot` executable. `tools/gamate-ram-perturbation-summary.py` compares its aggregate reports
against an authentic-entry report. Raw reports and user-supplied software remain outside the
repository.

These are behavioral results from the current emulator and exercised paths, not physical hardware
traces. Independent block clearing detects a dependency when removing one region changes an
observation; it can miss redundant fields or interactions that only appear when multiple regions
are changed together. It also does not assign meaning to any byte. Clean-room replacement work must
derive named semantics rather than copying the invariant authentic RAM image.
