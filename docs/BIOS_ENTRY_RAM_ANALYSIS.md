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
affected membership. The IRQ trace below identifies it as the low byte of the BIOS-maintained
periodic-IRQ counter.

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
affected cases exactly equals the parent's set. This set equality does not prove that the children
are independently sufficient: inverse testing below found a low-RAM interaction hidden by titles
that already differed through `$000C` or `$0011`.

## IRQ counter trace and inverse policy

Diagnostic-only access tracing establishes `$0010-$0011` as a 16-bit, big-endian-in-memory periodic
IRQ counter. Both BIOS revisions increment `$0011` on every periodic IRQ and increment `$0010` when
the low byte wraps. At cartridge entry the observed value is `$0394`; after 108 further IRQs the
trace records `$0011` wrapping from `$FF` to `$00`, followed by `$0010` changing from `$03` to `$04`.
The BIT handler performs these read-modify-writes at `$FFBF/$FFC4`; the UMC handler uses
`$FFA7/$FFAC`. Every traced access is inside IRQ context.

A cartridge-only 1,000-frame sweep found direct `$0011` access in fourteen titles under both BIOSes:

| Cartridge | Executing locations | Observed access |
| --- | --- | --- |
| Bao Qing Tian | `$6335` | One startup write |
| Basketball | `$63D1`, `$6451` | Startup read/write, then sustained polling |
| Devil Castle | `$8F50` | One write |
| Dino Ball | `$8C41` | Reads |
| Dino Bibo | `$A8E4` | Reads |
| Dinosaur Park | `$68C4` | Startup read/write |
| Fortune 'n Luck | `$636F` | One read |
| Kill Shot | `$8DC6`, `$9E8E` | Repeated reads |
| Magic Jigsaw | `$9B44`, `$D5AB`, `$D5C1` | Reads |
| Riddle of the Ancient Tomb | `$6377` | One write |
| Robin Hood | `$68AC` | Startup read/write |
| The Golden Pyramid | `$64A8` | One write |
| Witty Apee | `$8401`, `$8F81` | Reads |
| World Cup Soccer | `$A594` | Reads |

This makes the counter part of the cartridge-visible BIOS ABI, not merely private firmware residue.
The entry value is meaningful counter phase derived from elapsed IRQs during boot; clean-room code
may calculate or deliberately redefine its phase, but should not copy it as an unexplained byte.

Inverse tests cleared all of `$000-$01F` and selectively restored authentic functional fields:

| Preserved entry fields | Changed executions | Final-video changes | Final-VRAM changes |
| --- | ---: | ---: | ---: |
| None | 106 | 24 | 29 |
| `$000C` | 102 | 4 | 7 |
| `$0011` | 106 | 22 | 27 |
| `$000C`, `$0011` | 10 | 2 | 4 |
| `$0004-$000A`, `$000C`, `$0010-$0013` | 0 | 0 | 0 |

The ten residual cases after preserving `$000C/$0011` were Fortune 'n Luck, Hannibal, Hot Hero,
Magic Jigsaw, and Myth of Asamia under both BIOSes. Restoring the independently localized smaller
fields removed all unattended differences. Automated input then exposed `$000A` in Treasure Hunter;
preserving it as part of `$0004-$000A` removed that final two-BIOS difference. The final candidate
policy matches authentic entry across all 116 unattended cases and all 116 broad-input cases for
execution, final video, and final VRAM while clearing every other byte in `$000-$01F`.

### Semantics of the preserved fields

Pre-entry last-writer traces and focused cartridge access traces give the preserved regions a
functional description:

| RAM | Entry role | Evidence |
| --- | --- | --- |
| `$004` | Cartridge-protection reply accumulator | The BIOS shifts the eight returned protection bits into this byte and accepts the resulting `$47`; both revisions use the same behavior. |
| `$005` | Reset-fill byte paired with `$004` by software | Neither BIOS writes it before entry. Hannibal reads `$004-$005` through an indirect cartridge access; Fortune 'n Luck uses both extensively as runtime scratch. |
| `$006-$009` | Reset-fill sentinels | Neither BIOS writes them before entry, so they remain `$FF`. Myth of Asamia reads them before installing its own two word-like values; Fortune 'n Luck also initializes them later. |
| `$00A` | Explicit `$FF` runtime sentinel | Both BIOSes write `$FF` immediately before enabling callbacks and jumping to `$6020`. Treasure Hunter reaches it through an indirect read on an input-driven IRQ-callback path. |
| `$00C` | Cartridge-callback enable flag | Zero disables BIOS dispatch; any observed nonzero value enables it. Successful boot writes `$01`. |
| `$010-$011` | 16-bit periodic-IRQ counter | `$011` increments each IRQ and carries into `$010`; fourteen titles directly access `$011`. |
| `$012-$013` | BIOS pseudorandom/LFSR state | Service `$F030` ensures a nonzero state, advances a 16-bit feedback shift register, preserves X/Y, and returns the new low byte in A. `$014` is overwritten scratch feedback and need not be inherited. |

The authentic entry state leaves the LFSR at `$358A` (high byte `$013`, low byte `$012`) after the
startup sequence. That exact seed is useful for authentic phase comparison, but replacement firmware
can initialize a documented nonzero seed and advance it according to its own independently authored
startup behavior. Similarly, `$005-$009` are not meaningful firmware output bytes; their observed
`$FF` values are a reset initialization policy that commercial software happens to consume.

The resulting field set is therefore expressible as behavior rather than an opaque RAM template:
validate protection into `$004`, initialize defined sentinels, enable callbacks, establish the IRQ
counter phase, and initialize the BIOS random-generator state.

### Semantic-policy validation

A final diagnostic policy cleared `$000-$01F` and constructed the named fields above directly:
`$004=$47`, `$005-$00A=$FF`, `$00C=$01`, IRQ-counter phase `$0394`, and LFSR state `$358A`.
It did not copy any entry-RAM block. All 58 cartridges under both BIOS revisions then matched the
authentic-entry observations through 1,000 frames in both the unattended and broad automated-input
matrices: 116 of 116 cases in each matrix had identical CPU, video, audio, VRAM, mapper, peripheral,
controller, and activity results. The whole-RAM hashes intentionally differed because unrelated
boot residue remained cleared.

The exact counter and LFSR values in this equivalence test reproduce authentic phase for controlled
comparison. They are documented state variables, not a claim that replacement firmware must hardcode
those constants: a clean implementation can derive the counter from its own elapsed IRQs and choose
and document a nonzero random seed, with resulting phase differences tested separately.

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

The next efficient step is testing alternate valid LFSR/counter phases, followed by static/runtime
correlation for the `$0CC-$0DF` and page-2 title groups. Those later regions are cartridge work
areas, not part of the common low-RAM BIOS contract established here.

## Reproduction and limits

`GAMATEBOOT_ENTRY_RAM=zero-block-NN` selects hexadecimal block `00` through `0F` in the diagnostic
`gamateboot` executable. `GAMATEBOOT_ENTRY_RAM=zero-chunk-NN` selects a sixteen-byte physical chunk
from `00` through `3F`; `zero-byte-NNN` selects one physical byte, and `zero-quad-NN` selects an
aligned four-byte group by index. `zero-low32` and its `keep-*` variants exercise inverse policies.
`semantic-low32` constructs the documented low-RAM fields without copying an entry-RAM block.
`GAMATEBOOT_RAM_WATCH` selects a physical address for per-access tracing; the optional
`GAMATEBOOT_RAM_WATCH_OWNER` filters `bios`, `cartridge`, `other`, or `all` execution owners.
`tools/gamate-ram-perturbation-summary.py` compares aggregate reports against an authentic-entry
report. Raw reports and user-supplied software remain outside the repository.

These are behavioral results from the current emulator and exercised paths, not physical hardware
traces. Independent block clearing detects a dependency when removing one region changes an
observation; it can miss redundant fields or interactions that only appear when multiple regions
are changed together. It also does not assign meaning to any byte. Clean-room replacement work must
derive named semantics rather than copying the invariant authentic RAM image.
