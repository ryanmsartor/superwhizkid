# 1,000-frame compatibility sweep

## Scope

The current unattended baseline runs all 58 locally available cartridges with each of the two known
BIOS revisions for 1,000 frames after cartridge entry at `$6020`. The resulting 116-case report is
kept outside the repository because it includes local filenames and hashes; it contains counters and
derived state, not ROM or BIOS bytes.

This baseline supersedes the former 300-frame general-compatibility sweep. Shorter or input-driven
traces remain relevant only when they exercise a path an unattended run does not reach.

## Results

All 116 combinations completed successfully. Every run produced nonblank video and controller reads,
and none reported an unsupported opcode or persistent self-loop. Cartridge sizes span 16 through
512 KiB and include the fingerprint-selected 4-in-1 mapper.

| Finding | UMC/NCR BIOS | Bit Corporation BIOS | Interpretation |
| --- | ---: | ---: | --- |
| Passing combinations | 58/58 | 58/58 | No failure in the observed window |
| Titles with controller reads | 58/58 | 58/58 | The longer window reaches late title-screen polling |
| Titles with audible output | 57/58 | 57/58 | Tornado remains silent until input advances its title state |
| Titles exercising UART clocks | 1/58 | 1/58 | World Cup Soccer clocks the link UART 1,136 times |
| Titles with persistent self-loops | 0/58 | 0/58 | No observed execution trap |

### Late behavior exposed by the longer window

| Title or group | First observed event | 1,000-frame result | Consequence |
| --- | ---: | --- | --- |
| Further Adventures of Hannibal The Cat | Audio at frame 357 | Audible after its long silent introduction | A 300-frame audio check gives a false failure |
| Famous 7 | Audio at frame 277; controller polling at frame 564 | 724 audible frames | Both audio and input require a sufficiently long title-screen run |
| One Million Whys | Audio at frame 277 | 513 audible frames | Delayed sound is title sequencing, not a PSG failure |
| QuizFighter | Audio at frame 277; controller polling at frame 564 | 521 audible frames | The longer run reaches previously unseen input polling |
| BIOS service slots `$F030` and `$F033` | First direct call at frame 408 | Used by the catalog | Replacement-firmware analysis cannot rely on a 300-frame trace |

Tornado is the sole silent unattended case. A retained input-driven trace shows that a sufficiently
long Start press enters gameplay under both BIOS revisions and produces active output on all three
PSG channels. Its unattended silence is therefore a game-state result rather than evidence of an
audio emulation failure.

## BIOS comparison

For each title, both BIOS revisions produced the same nonblank-frame count, IRQ count, AY-write count,
UART-clock count, mapper-bank-change count, and self-loop diagnosis. The two startup paths reach
`$6020` 58 master clocks apart, so reports can differ slightly in phase-sensitive observations:

- nine titles differed by one or two audible frames;
- four differed by two or three LCD writes;
- three of those differed by two VRAM writes;
- instantaneous audio peaks and final frame/audio hashes commonly differed.

These small differences are consistent with sampling two executions that inherited different display,
audio, and interrupt phase. They are not evidence that the BIOS revisions initialize different RAM,
VRAM, mapper, or peripheral state; the controlled entry-state comparison addresses that separately.

## Limits

The sweep is a broad regression and reachability check, not proof of playability or hardware accuracy.
It supplies no gameplay input, link peer, or exhaustive control flow. Later and input-dependent paths
still require targeted traces, and the BIOS comparison must preserve the authentic entry-phase offset.
