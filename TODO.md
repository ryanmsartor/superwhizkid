# To-do

- Explore CRC-independent 4-in-1 mapper detection using lazy `$8000` lower-bank activation; instrument
  the full 58-cartridge/two-BIOS set before changing behavior. See
  [`docs/MAPPER_DETECTION.md`](docs/MAPPER_DETECTION.md).
- Complete the post-entry BIOS dependency audit: cluster the now-paired input/output register and side-effect signatures for the 18
  observed `$F000-$F033` service slots, extend the deterministic input campaign with adaptive
  gameplay and controlled link/NMI paths, perturb invariant entry RAM/VRAM state, and test a
  canonical replacement timing phase. See
  [`docs/BIOS_REPLACEMENT_ANALYSIS.md`](docs/BIOS_REPLACEMENT_ANALYSIS.md).
- investigate crash (towards end of?) level 1 of bao qing tian w/ UMC bios - occurred while testing on RG Cube XX
- Explore hard clipping instead of soft clipping for audio output
- Explore adjustable audio distortion
- auto palette mode
- auto flicker mode
- make all ghosting settings darker/longer
- standalone emulator / debugger / dev tool
- screenshots on the readme



