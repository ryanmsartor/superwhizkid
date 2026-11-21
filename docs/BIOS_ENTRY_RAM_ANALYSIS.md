# BIOS cartridge-entry RAM analysis

This document tracks controlled perturbation of the 1 KiB physical RAM state inherited when the
authentic BIOS enters a cartridge at `$6020`. It records aggregate behavior and title categories,
not authentic RAM contents or a reusable firmware-derived state image.

## Quarter-page sweep

Each 64-byte physical RAM block was independently cleared at cartridge entry while every other RAM
byte and all CPU, hardware, mapper, timing, and peripheral state remained authentic. All 62
cartridges were run under both recognized BIOS revisions for 1,000 post-entry frames, giving 124
cases per block and 1,984 perturbed runs. Every run completed without an unsupported opcode or
blank-output failure.

“Changed execution” compares final CPU, video, audio, VRAM, mapper, peripheral, controller, and
activity observations while excluding only the deliberately changed final RAM hash. Counts are
BIOS/cartridge cases; the title column collapses matching results across BIOS revisions.

| Block | Physical range | Changed executions | Final-video changes | Final-VRAM changes | Affected titles |
| --- | --- | ---: | ---: | ---: | --- |
| `00` | `$000-$03F` | 114 | 24 | 29 | 57 titles; all except Bao Qing Tian, Basketball, Dinosaur Park, Riddle of the Ancient Tomb, and The Golden Pyramid |
| `01` | `$040-$07F` | 10 | 2 | 0 | Dino Bibo; Galaxy Invaders; Mars Voyage; Money Maze; Time Warrior |
| `02` | `$080-$0BF` | 14 | 2 | 0 | Famous 7; Galaxy Invaders; Insect War; Jewelriss; Money Maze; One Million Whys; Tornado |
| `03` | `$0C0-$0FF` | 46 | 2 | 3 | Baseball; Boom; Devil Castle; Dino Bibo; Fantasy Travel; Flying Goblin; Fortress of Fierceness; Fortune 'n Luck; Galaxy Invaders; Incantational Couple; Kiki Inland; Kill Shot; Legend of Dragon Knight; Marauder; Mighty Boxer; Money Maze; Myth of Asamia; Nightmare of Santa Claus; One Million Whys; Pipemania; Punk Boy; Snowman Legend; Tennis |
| `04` | `$100-$13F` | 2 | 0 | 0 | Time Warrior |
| `05` | `$140-$17F` | 0 | 0 | 0 | None observed |
| `06` | `$180-$1BF` | 0 | 0 | 0 | None observed |
| `07` | `$1C0-$1FF` | 0 | 0 | 0 | None observed |
| `08` | `$200-$23F` | 28 | 4 | 8 | Bomb Blaster and its alternate dump; Famous 7; Fist of Thunder; GP Race; Jewelriss; Kill Shot; Mars Voyage; Mighty Boxer; Tornado; Tough Guy; Treasure Hunter; Witty Apee; World Cup Soccer |
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

The four highest-priority 64-byte blocks were subdivided into sixteen-byte chunks. All listed chunks
were cleared for the 62-cartridge corpus under both BIOS revisions, giving 124 cases per chunk.
For every parent block, the union of its four changed-case sets exactly equals the
parent's changed-case set: no title changed only when multiple chunks in the parent were cleared
together.

| Chunk | Physical range | Changed executions | Final-video changes | Final-VRAM changes | Affected titles |
| --- | --- | ---: | ---: | ---: | --- |
| `00` | `$000-$00F` | 114 | 20 | 25 | 57 titles; all except Bao Qing Tian, Basketball, Dinosaur Park, Riddle of the Ancient Tomb, and The Golden Pyramid |
| `01` | `$010-$01F` | 110 | 4 | 5 | 55 titles; all except Bao Qing Tian, Basketball, Devil Castle, Dinosaur Park, Riddle of the Ancient Tomb, Robin Hood, and The Golden Pyramid |
| `02` | `$020-$02F` | 8 | 2 | 2 | Galaxy Invaders; Magic Jigsaw; Mars Voyage; Time Warrior |
| `03` | `$030-$03F` | 4 | 0 | 0 | Magic Jigsaw; Mars Voyage |
| `0C` | `$0C0-$0CF` | 26 | 2 | 3 | Baseball; Devil Castle; Fantasy Travel; Flying Goblin; Fortress of Fierceness; Fortune 'n Luck; Galaxy Invaders; Incantational Couple; Kiki Inland; Mighty Boxer; Money Maze; Punk Boy; Snowman Legend |
| `0D` | `$0D0-$0DF` | 22 | 0 | 0 | Boom; Dino Bibo; Kill Shot; Legend of Dragon Knight; Marauder; Money Maze; Myth of Asamia; Nightmare of Santa Claus; One Million Whys; Pipemania; Tennis |
| `0E` | `$0E0-$0EF` | 0 | 0 | 0 | None observed |
| `0F` | `$0F0-$0FF` | 0 | 0 | 0 | None observed |
| `20` | `$200-$20F` | 2 | 0 | 0 | GP Race |
| `21` | `$210-$21F` | 14 | 0 | 4 | Bomb Blaster and its alternate dump; Jewelriss; Mars Voyage; Mighty Boxer; Witty Apee; World Cup Soccer |
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
to the other fifteen bytes. `$010-$01F` independently affects 55 titles and therefore contains at
least one additional broadly consumed field or counter.

## Byte and four-byte refinement

All 32 individual bytes in `$000-$01F`, the four bytes in `$00CC-$00CF`, and the four bytes in
`$021C-$021F` were independently cleared across the 62-cartridge corpus. The eight aligned
four-byte groups in `$0C0-$0DF` and four groups in `$0210-$021F` were also cleared. Each test covered
both BIOS revisions and 1,000 post-entry frames, for 4,960 byte-address/case and 1,488 four-byte
group/case comparisons. “Execution” includes final VRAM; only the deliberately changed final RAM
hash is excluded.

| Address | Changed executions | Final-video changes | Final-VRAM changes | Affected titles or observation |
| --- | ---: | ---: | ---: | --- |
| `$000-$003` | 0 each | 0 | 0 | No observed effect |
| `$004` | 2 | 0 | 0 | Further Adventures of Hannibal The Cat |
| `$005` | 2 | 0 | 0 | Further Adventures of Hannibal The Cat |
| `$006` | 2 | 0 | 2 | Myth of Asamia |
| `$007` | 2 | 0 | 2 | Myth of Asamia; retained VRAM only |
| `$008` | 2 | 0 | 2 | Myth of Asamia |
| `$009` | 2 | 0 | 2 | Myth of Asamia; retained VRAM only |
| `$00A-$00B` | 0 each | 0 | 0 | No observed effect |
| `$00C` | 114 | 20 | 23 | 57 titles; same affected-case membership as chunk `$000-$00F` |
| `$00CC` | 0 | 0 | 0 | No observed effect |
| `$00CD` | 0 | 0 | 0 | No observed effect |
| `$00CE` | 26 | 2 | 3 | Baseball; Devil Castle; Fantasy Travel; Flying Goblin; Fortress of Fierceness; Fortune 'n Luck; Galaxy Invaders; Incantational Couple; Kiki Inland; Mighty Boxer; Money Maze; Punk Boy; Snowman Legend |
| `$00CF` | 2 | 0 | 0 | Money Maze |
| `$00D-$00F` | 0 each | 0 | 0 | No observed effect |
| `$010` | 0 | 0 | 0 | No observed effect |
| `$011` | 110 | 4 | 5 | 55 titles; same affected-case membership as chunk `$010-$01F` |
| `$012` | 4 | 2 | 2 | Hot Hero; Magic Jigsaw |
| `$013` | 2 | 2 | 2 | Magic Jigsaw |
| `$014-$017` | 0 each | 0 | 0 | No observed effect |
| `$018-$019` | 0 each | 0 | 0 | No observed effect |
| `$01A` | 0 | 0 | 0 | No observed effect |
| `$01B-$01D` | 0 each | 0 | 0 | No observed effect |
| `$01E-$01F` | 0 each | 0 | 0 | No observed effect |
| `$021C` | 0 | 0 | 0 | No observed effect |
| `$021D` | 2 | 0 | 0 | Witty Apee |
| `$021E` | 0 | 0 | 0 | No observed effect |
| `$021F` | 4 | 0 | 0 | Mighty Boxer; Witty Apee |

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
| `$0CC-$0CF` | 26 | 2 | 3 | Baseball; Devil Castle; Fantasy Travel; Flying Goblin; Fortress of Fierceness; Fortune 'n Luck; Galaxy Invaders; Incantational Couple; Kiki Inland; Mighty Boxer; Money Maze; Punk Boy; Snowman Legend |
| `$0D0-$0D3` | 16 | 0 | 0 | Boom; Kill Shot; Legend of Dragon Knight; Marauder; Money Maze; Myth of Asamia; Nightmare of Santa Claus; Tennis |
| `$0D4-$0D7` | 0 | 0 | 0 | None observed |
| `$0D8-$0DB` | 8 | 0 | 0 | Dino Bibo; Money Maze; Nightmare of Santa Claus; Tennis |
| `$0DC-$0DF` | 4 | 0 | 0 | One Million Whys; Pipemania |
| `$210-$213` | 0 | 0 | 0 | None observed |
| `$214-$217` | 4 | 0 | 0 | Mars Voyage; World Cup Soccer |
| `$218-$21B` | 6 | 0 | 2 | Bomb Blaster and its alternate dump; Jewelriss |
| `$21C-$21F` | 4 | 0 | 2 | Mighty Boxer; Witty Apee |

For chunks `$000-$00F`, `$010-$01F`, `$0C0-$0CF`, and `$0D0-$0DF`, the union of the child-level
affected cases exactly equals the parent's set. This set equality does not prove that the children
are independently sufficient: inverse testing below found a low-RAM interaction hidden by titles
that already differed through `$000C` or `$0011`. The `$00CE` and `$00CF` effects overlap in Money
Maze, while the four `$0210-$021F` groups account for the 14 changed cases in chunk `$21`.

## IRQ counter trace and inverse policy

Diagnostic-only access tracing and inspection of the executed handler establish `$000E-$0011` as a
32-bit, big-endian-in-memory periodic IRQ counter. Both BIOS revisions increment `$0011` on every
periodic IRQ and propagate carry toward `$000E`. At cartridge entry the observed value is
`$00000394`; after 108 further IRQs the
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

The later phase-control trace also finds Kill Shot reading `$0010` repeatedly at `$8DCA`. This makes
the counter part of the cartridge-visible BIOS ABI, not merely private firmware residue. The entry
value is meaningful counter phase derived from elapsed IRQs during boot; clean-room code
may calculate or deliberately redefine its phase, but should not copy it as an unexplained byte.

Inverse tests cleared all of `$000-$01F` across the 62-cartridge corpus and selectively restored
authentic functional fields:

| Preserved entry fields | Changed executions | Final-video changes | Final-VRAM changes |
| --- | ---: | ---: | ---: |
| None | 114 | 24 | 29 |
| `$000C` | 110 | 4 | 7 |
| `$0011` | 114 | 22 | 27 |
| `$000C`, `$0011` | 10 | 2 | 4 |
| `$0004-$000A`, `$000C`, `$0010-$0013` | 0 | 0 | 0 |

The ten residual cases after preserving `$000C/$0011` were Fortune 'n Luck, Hannibal, Hot Hero,
Magic Jigsaw, and Myth of Asamia under both BIOSes. Restoring the independently localized smaller
fields removed all unattended differences. Automated input then exposed `$000A` in Treasure Hunter;
preserving it as part of `$0004-$000A` removed that difference. The final candidate policy matches
authentic entry across all 124 no-input cases and all 124 all-buttons-held cases for execution, final
video, and final VRAM while clearing every other byte in `$000-$01F`.

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
| `$00E-$011` | 32-bit periodic-IRQ counter | `$011` increments each IRQ and propagates carry toward `$00E`; fourteen titles directly access `$011`, and Kill Shot reads `$010` on an input-driven path. |
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
`$004=$47`, `$005-$00A=$FF`, `$00C=$01`, IRQ-counter phase `$00000394`, and LFSR state `$358A`.
It did not copy any entry-RAM block. All 62 cartridges under both BIOS revisions then matched the
authentic-entry observations through 1,000 frames across all 124 no-input cases and all 124
all-buttons-held cases (`GAMATEBOOT_PRESS="1:255:1000"`). Both schedules matched CPU, video, audio,
VRAM, mapper, peripheral, controller, and activity results. The whole-RAM hashes intentionally
differ because unrelated boot residue remains cleared. The test establishes sufficiency for this
low-RAM policy; upper WRAM remains authentic and is not covered by this result.

The exact counter and LFSR values in this equivalence test reproduce authentic phase for controlled
comparison. They are documented state variables, not a claim that replacement firmware must hardcode
those constants: a clean implementation can derive the counter from its own elapsed IRQs and choose
and document a nonzero random seed, with resulting phase differences tested separately.

### Counter- and random-phase sensitivity

The semantic policy was rerun with independently varied counter and LFSR states across all 124
BIOS/cartridge cases without a boot or execution failure.

| Entry-state variation | No-input execution/video/VRAM differences | Interpretation |
| --- | ---: | --- |
| Counter `$00000000` | 110 / 4 / 5 | Changes the low-byte carry schedule and therefore BIOS IRQ-handler timing. |
| Counter `$0000FFFF` | 110 / 8 / 9 | Immediately exercises carries into both `$010` and `$00F`, producing a larger phase shift. |
| Counter `$0000FF94` | 106 / 0 / 0 | Preserves low-byte wrap times, but the first carry reaches `$00F` and lengthens that handler path. |
| Counter `$00000494` | 0 / 0 / 0 | Preserves low-byte wrap timing and avoids a carry into `$00F` in the window. |
| LFSR `$0001` | 4 / 2 / 2 | Changes deterministic random results only on exercised consumer paths. |
| LFSR `$FFFF` | 4 / 2 / 2 | Same affected-case count as `$0001`, with different deterministic random state. |

The LFSR differences pair across both BIOSes and occur in Hot Hero and Magic Jigsaw. Focused counter
tracing shows Kill Shot repeatedly reading `$010` at cartridge PC `$8DCA` during its IRQ callback.
This direct software access remains observable independently of the phase matrix.

Counter changes require special interpretation. `$011` increments on every periodic IRQ; when it
wraps, the BIOS executes the additional `$010` increment path. A phase change therefore alters the
number and placement of handler cycles even in software that never reads the counter. Wrapping
`$010` introduces another distinct branch outcome. The widespread PC/audio/activity differences are
thus expected deterministic timing consequences, not evidence that every affected game validates
the literal entry number.

For maximum authentic compatibility, replacement startup should reproduce the observed `$00000394`
counter phase and `$358A` LFSR state, preferably as named, documented ABI initialization or as the
derived result of its own boot behavior. Other nonzero LFSR seeds and non-wrapping counter phases
remain operational in this test, but they are observably different and should not be described as
equivalent to authentic startup.

## Post-entry read-before-write coverage

Authentic-state access tracking found at least one cartridge read-before-write in every one of the
124 BIOS/cartridge cases; the combined cartridge-owned union covered all 1,024 physical RAM bytes.
BIOS reads of inherited bytes before their first post-entry write occurred in 118 cases. Repeating
tracking with controller input added no addresses to either owner union. These counts describe
observed accesses, not which values are required; perturbation results above provide the dependency
evidence. The `$00CE` and `$021F` tests show software sensitivity in 13 and two titles respectively,
but do not by themselves establish the bytes' meaning, initialization source, or physical power-on
values. No common BIOS initialization requirement is inferred from them.

## Interpretation

The earlier page sweep is now substantially narrower:

- `$000-$03F` alone reproduces all 114 execution differences seen when the entire zero page was
  cleared. Later zero-page quarters still contain title-specific dependencies, but they do not add
  another affected case to that total.
- The stack-page result is entirely localized to `$100-$13F` and Time Warrior. Clearing
  `$140-$1FF` produced no compared difference in this sweep.
- Page 2 contains several distinct title work areas rather than one isolated common field. The chunk,
  byte, and four-byte tables show their title-specific effects across the whole corpus.
- Page 3 is also title-specific. Its four quarters affect Punk Boy, Flying Goblin, and combinations
  of Flying Goblin, Insect War, World Cup Soccer, and Dino Bibo. Clearing `$00CE` changes 26 cases,
  including final-video and VRAM observations in a small subset.
- Only blocks `00`, `01`, `02`, `03`, `08`, `09`, and `0A` changed the sampled final video in this
  experiment. A zero observed count remains coverage evidence, not proof that a block is irrelevant
  in all later gameplay.

The 62-cartridge results leave other title-specific `$0CC-$0DF` and page-2 dependencies for
static/runtime correlation. The effects at `$00CE` and `$021F` are documented as observed
perturbation sensitivity; their semantics and physical power-on values remain unresolved.

## Reproduction and limits

`GAMATEBOOT_ENTRY_RAM=zero-block-NN` selects hexadecimal block `00` through `0F` in the diagnostic
`gamateboot` executable. `GAMATEBOOT_ENTRY_RAM=zero-chunk-NN` selects a sixteen-byte physical chunk
from `00` through `3F`; `zero-byte-NNN` selects one physical byte, and `zero-quad-NN` selects an
aligned four-byte group by index. `zero-low32` and its `keep-*` variants exercise inverse policies.
`semantic-low32` constructs the documented low-RAM fields without copying an entry-RAM block.
`GAMATEBOOT_ENTRY_IRQ_COUNTER` and `GAMATEBOOT_ENTRY_LFSR` optionally replace its hexadecimal
32-bit counter and 16-bit random phases; the LFSR override rejects zero because the documented
service requires nonzero state.
`GAMATEBOOT_RAM_WATCH` selects a physical address for per-access tracing; the optional
`GAMATEBOOT_RAM_WATCH_OWNER` filters `bios`, `cartridge`, `other`, or `all` execution owners.
`tools/gamate-ram-perturbation-summary.py` compares aggregate reports against an authentic-entry
report. Raw reports and user-supplied software remain outside the repository.

The diagnostic attributes accesses to the PC of the instruction being executed, so IRQ-entry stack
writes are attributed to the interrupted instruction; later handler accesses are attributed to BIOS
code. Read-modify-write instructions count their read before their write. This affects owner-specific
attribution, not the aggregate read-before-write coverage.

These are behavioral results from the current emulator and exercised paths, not physical hardware
traces. Independent block clearing detects a dependency when removing one region changes an
observation; it can miss redundant fields or interactions that only appear when multiple regions
are changed together. It also does not assign meaning to any byte. Clean-room replacement work must
derive named semantics rather than copying the invariant authentic RAM image.
