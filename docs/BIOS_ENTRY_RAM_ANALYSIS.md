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

## Byte and four-byte refinement

All 32 individual bytes in `$000-$01F` and all eight aligned four-byte groups in `$0C0-$0DF` were
then cleared independently. The same 116-case, 1,000-frame matrix produced 4,640 additional passing
runs. “Execution” here includes final VRAM; only the deliberately changed final RAM hash is excluded.

| Address | Changed executions | Final-video changes | Final-VRAM changes | Affected titles or observation |
| --- | ---: | ---: | ---: | --- |
| `$000-$003` | 0 each | 0 | 0 | No observed effect |
| `$004` | 2 | 0 | 0 | Further Adventures of Hannibal The Cat |
| `$005` | 2 | 0 | 0 | Further Adventures of Hannibal The Cat |
| `$006` | 2 | 0 | 2 | Myth of Asamia |
| `$007` | 2 | 0 | 2 | Myth of Asamia; retained VRAM only |
| `$008` | 2 | 0 | 2 | Myth of Asamia |
| `$009` | 2 | 0 | 2 | Myth of Asamia; retained VRAM only |
| `$00A-$00B` | 0 each | 0 | 0 | No observed effect; `$00B` was already zero |
| `$00C` | 106 | 20 | 23 | 53 titles; exactly the same affected-case membership as chunk `$000-$00F` |
| `$00D-$00F` | 0 each | 0 | 0 | No observed effect; `$00E-$00F` were already zero |
| `$010` | 0 | 0 | 0 | No observed effect |
| `$011` | 102 | 4 | 5 | 51 titles; exactly the same affected-case membership as chunk `$010-$01F` |
| `$012` | 4 | 2 | 2 | Hot Hero; Magic Jigsaw |
| `$013` | 2 | 2 | 2 | Magic Jigsaw |
| `$014-$017` | 0 each | 0 | 0 | Already zero at entry |
| `$018-$019` | 0 each | 0 | 0 | Nonzero at entry, with no observed effect when cleared |
| `$01A` | 0 | 0 | 0 | Already zero at entry |
| `$01B-$01D` | 0 each | 0 | 0 | Nonzero at entry, with no observed effect when cleared |
| `$01E-$01F` | 0 each | 0 | 0 | Already zero at entry |

All affected byte-level cases paired across both BIOS revisions. `$000C` is the already documented
callback-enable flag. Clearing it alone accounts for the broad membership of the first chunk's
result, consistent with disabling BIOS dispatch to cartridge callbacks; other bytes in that chunk
still produce narrower title-specific changes. `$0011` similarly accounts for the second chunk's
affected membership, but its precise meaning remains to be established. Its broad reach makes a
BIOS-maintained timing or interrupt field plausible, not proven.

| Four-byte group | Changed executions | Final-video changes | Final-VRAM changes | Affected titles |
| --- | ---: | ---: | ---: | --- |
| `$0C0-$0C3` | 0 | 0 | 0 | None observed |
| `$0C4-$0C7` | 0 | 0 | 0 | None observed |
| `$0C8-$0CB` | 0 | 0 | 0 | None observed |
| `$0CC-$0CF` | 18 | 2 | 3 | Baseball; Devil Castle; Fantasy Travel; Flying Goblin; Fortune 'n Luck; Galaxy Invaders; Money Maze; Punk Boy; Snowman Legend |
| `$0D0-$0D3` | 16 | 0 | 0 | Boom; Kill Shot; Legend of Dragon Knight; Marauder; Money Maze; Myth of Asamia; Nightmare of Santa Claus; Tennis |
| `$0D4-$0D7` | 0 | 0 | 0 | None observed |
| `$0D8-$0DB` | 8 | 0 | 0 | Dino Bibo; Money Maze; Nightmare of Santa Claus; Tennis |
| `$0DC-$0DF` | 4 | 0 | 0 | One Million Whys; Pipemania |

For chunks `$000-$00F`, `$010-$01F`, `$0C0-$0CF`, and `$0D0-$0DF`, the union of the child-level
affected cases exactly equals the parent's set. No parent-only dependency requiring multiple child
regions to be cleared together was observed at this resolution.

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

The next efficient step is semantic analysis of `$0011` using BIOS access traces and cartridge
callback behavior, followed by inverse tests that preserve `$000C` and `$0011` while clearing their
surrounding chunks. The smaller `$004-$009`, `$012-$013`, and `$0CC-$0DF` title groups should be
paired with static cartridge reads before further subdivision. The page-2 chunks are already
title-specific enough for the same approach. Inverse tests are necessary to establish sufficiency,
not merely the necessity demonstrated by independent clearing.

## Reproduction and limits

`GAMATEBOOT_ENTRY_RAM=zero-block-NN` selects hexadecimal block `00` through `0F` in the diagnostic
`gamateboot` executable. `GAMATEBOOT_ENTRY_RAM=zero-chunk-NN` selects a sixteen-byte physical chunk
from `00` through `3F`; `zero-byte-NNN` selects one physical byte, and `zero-quad-NN` selects an
aligned four-byte group by index. `tools/gamate-ram-perturbation-summary.py` compares aggregate
reports against an authentic-entry report. Raw reports and user-supplied software remain outside
the repository.

These are behavioral results from the current emulator and exercised paths, not physical hardware
traces. Independent block clearing detects a dependency when removing one region changes an
observation; it can miss redundant fields or interactions that only appear when multiple regions
are changed together. It also does not assign meaning to any byte. Clean-room replacement work must
derive named semantics rather than copying the invariant authentic RAM image.
