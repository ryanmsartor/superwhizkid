# To-do

- Complete the post-entry BIOS dependency audit: cluster the now-paired input/output register and side-effect signatures for the 18
  observed `$F000-$F033` service slots, extend the deterministic input campaign with adaptive
  gameplay and controlled link/NMI paths, perturb invariant entry RAM/VRAM state, and test a
  canonical replacement timing phase. See
  [`docs/bios/BIOS_REPLACEMENT_ANALYSIS.md`](docs/bios/BIOS_REPLACEMENT_ANALYSIS.md).
- investigate crash (towards end of?) level 1 of bao qing tian w/ UMC bios - occurred while testing on RG Cube XX
- Explore hard clipping instead of soft clipping for audio output
- Explore adjustable audio distortion
- auto palette mode
- auto flicker reduce mode
- make all ghosting settings darker/longer
- standalone emulator / debugger / dev tool - needs a UI next
- explore "widescreen hacks":
```
ratio   smallest    integer
-----------------------------
native  160x150
1:1     160x160     180x180
4:3     200x150     213x160
3:2     225x150     240x160
16:9    256x144     
```


