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

## Sixteen-byte refinement

The four highest-priority 64-byte blocks were subdivided into sixteen-byte chunks and rerun through
the same 116-case, 1,000-frame matrix. These 1,856 additional runs all completed. For every parent
block, the union of its four changed-case sets exactly equals the parent's changed-case set: no title
changed only when multiple chunks in the parent were cleared together.

| Chunk | Physical range | Changed executions | Final-video changes | Final-VRAM changes | Affected titles |
| --- | --- | ---: | ---: | ---: | --- |
| `00` | `$000-$00F` | 106 | 20 | 25 | 53 titles; all except Bao Qing Tian, Basketball, Dinosaur Park, Riddle of the Ancient Tomb, and The Golden Pyramid |
| `01` | `$010-$01F` | 102 | 4 | 5 | 51 titles; all except Bao Qing Tian, Basketball, Devil Castle, Dinosaur Park, Riddle of the Ancient Tomb, Robin Hood, and The Golden Pyramid |
| `02` | `$020-$02F` | 8 | 2 | 2 | Galaxy Invaders; Magic Jigsaw; Mars Voyage; Time Warrior |
| `03` | `$030-$03F` | 4 | 0 | 0 | Magic Jigsaw; Mars Voyage |
| `0C` | `$0C0-$0CF` | 18 | 2 | 3 | Baseball; Devil Castle; Fantasy Travel; Flying Goblin; Fortune 'n Luck; Galaxy Invaders; Money Maze; Punk Boy; Snowman Legend |
| `0D` | `$0D0-$0DF` | 22 | 0 | 0 | Boom; Dino Bibo; Kill Shot; Legend of Dragon Knight; Marauder; Money Maze; Myth of Asamia; Nightmare of Santa Claus; One Million Whys; Pipemania; Tennis |
| `0E` | `$0E0-$0EF` | 0 | 0 | 0 | None observed |
| `0F` | `$0F0-$0FF` | 0 | 0 | 0 | None observed |
| `20` | `$200-$20F` | 2 | 0 | 0 | GP Race |
| `21` | `$210-$21F` | 12 | 0 | 4 | Bomb Blaster and its alternate dump; Jewelriss; Mars Voyage; Witty Apee; World Cup Soccer |
| `22` | `$220-$22F` | 12 | 0 | 0 | Bomb Blaster and its alternate dump; Famous 7; Fist of Thunder; Tornado; Tough Guy |
| `23` | `$230-$23F` | 6 | 0 | 0 | Kill Shot; Tornado; Treasure Hunter |
| `24` | `$240-$24F` | 4 | 0 | 0 | Famous 7; Pipemania |
| `25` | `$250-$25F` | 10 | 2 | 2 | Bomb Blaster and its alternate dump; Flipuzzle; Mars Voyage; One Million Whys |
| `26` | `$260-$26F` | 4 | 2 | 2 | Dino Ball; Tough Guy |
| `27` | `$270-$27F` | 0 | 0 | 0 | None observed |

The `$0E0-$0EF` result includes the duplicated cartridge-checksum fields at `$00E0-$00E1` and
`$00E5-$00E6`. Clearing this chunk after the BIOS has already validated the cartridge produced no
observed post-entry difference. This does not remove the checksum from the boot contract: it only
shows that the inherited copies were not needed by these exercised post-entry paths. Conversely,
`$000-$00F` includes the callback-enable byte at `$000C`, so its broad effect cannot yet be assigned
to the other fifteen bytes. `$010-$01F` independently affects 51 titles and therefore contains at
least one additional broadly consumed field or counter.

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

The next efficient refinement is individual-byte perturbation of `$000-$01F`, beginning with the
known callback byte `$000C`, followed by 4-byte subdivisions of `$0C0-$0DF`. The page-2 chunks are
already title-specific enough to pair with static cartridge-code analysis before subdividing them
further. After candidate fields are identified, inverse tests must preserve only those fields while
clearing their surrounding chunk to test sufficiency, not merely necessity.

## Reproduction and limits

`GAMATEBOOT_ENTRY_RAM=zero-block-NN` selects hexadecimal block `00` through `0F` in the diagnostic
`gamateboot` executable. `GAMATEBOOT_ENTRY_RAM=zero-chunk-NN` selects a sixteen-byte physical chunk
from `00` through `3F`. `tools/gamate-ram-perturbation-summary.py` compares aggregate reports against
an authentic-entry report. Raw reports and user-supplied software remain outside the repository.

These are behavioral results from the current emulator and exercised paths, not physical hardware
traces. Independent block clearing detects a dependency when removing one region changes an
observation; it can miss redundant fields or interactions that only appear when multiple regions
are changed together. It also does not assign meaning to any byte. Clean-room replacement work must
derive named semantics rather than copying the invariant authentic RAM image.
