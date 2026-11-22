# Experimental Gamate BIOS prototype

This independently authored, 4 KiB resident firmware is an **incomplete experiment**, initially
targeting the standalone Cube Up cartridge. It is not an authentic BIOS, a drop-in replacement, or
a claim of hardware accuracy. The libretro core recognizes it as `gamate_bios_swk.bin`; UMC remains
the default BIOS priority, with SWK used as a fallback when neither authentic BIOS is available.

The source implements published protocol and behavioral observations, not the original BIOS
instructions, startup graphics, or cartridge program data. The protection identifier and cartridge
header text are exact format/protocol checks, not a copied firmware or game image. Do not copy
commercial BIOS or cartridge files into the repository or rename this prototype as an authentic
BIOS.

## Building and running

The optional build requires `ca65` and `ld65` from [cc65](https://cc65.github.io/). Default builds
do not require them and do not build or install this firmware.

```sh
cmake -S . -B build-bios -DSUPERWHIZKID_EXPERIMENTAL_BIOS=ON
cmake --build build-bios --parallel --target superwhizkid_libretro gamateboot gamatebiosprototypetest
ctest --test-dir build-bios --output-on-failure -R '^gamate-bios-prototype$'
GAMATEBOOT_EXPERIMENTAL_BIOS=1 build-bios/gamateboot \
  build-bios/gamate_bios_swk.bin path/to/cartridge.bin 1000
```

Replace the cartridge argument with a legally obtained local image. `GAMATEBOOT_EXPERIMENTAL_BIOS=1`
enables diagnostic trap reporting in `gamateboot`; it must only be used with this experimental
image. Protection, identity and checksum failures and unsupported service/NMI traps then print an
error and return a nonzero exit status. Without this option, `gamateboot` does not recognize these
prototype-specific trap addresses. The image is also usable explicitly through the library's
`gamateLoad` BIOS argument, but unsupported calls will halt in the firmware rather than produce a
frontend notification.

For interactive testing in RetroArch, place the generated `gamate_bios_swk.bin` in the frontend's
system directory and load standalone Cube Up with the newly built core. Select **BIOS Priority:
SWK** and reload content to prefer the replacement without changing either authentic BIOS file.
UMC priority tries UMC, Bit, then SWK; Bit priority tries Bit, UMC, then SWK; SWK priority tries SWK,
UMC, then Bit. Missing or incorrectly sized files are skipped; each BIOS must be exactly 4 KiB.
Invalid option values use UMC priority. Reset retains the BIOS loaded with the content.

The frontend displays an experimental-BIOS notice when SWK is loaded, including through fallback.
It does not detect the prototype's unsupported-service traps, which can leave the game halted.
Interactive gameplay remains unvalidated; this integration enables testing, not a compatibility claim.

## Implemented subset

- Reset performs the documented serial protection exchange, checks the exact cartridge identity
  field and the `$7000-$7FFF` checksum, then constructs the tested RAM policy, clears VRAM,
  initializes AY/LCD state and enters at `$6020`. It does not show the authentic startup sequence
  or reproduce its duration and phase.
- The periodic IRQ handler increments the `$00B` wait counter and the big-endian `$00E-$011`
  counter, calling `$6026` when `$00C` is nonzero. Its register and callback behavior is tested
  with a synthetic cartridge; broader interrupt interference remains unverified.
- The seven observed Cube Up service slots have initial implementations: `$F000` (fixed bitmap
  destination), `$F009` (IRQ wait), `$F00C` (two-plane clear), `$F012` (glyph run), `$F01E`
  (two-pass tile stream), `$F024` (record initialization) and `$F02A` (rectangle fill).
  `$F000` writes a deliberately **blank, independently authored placeholder** rather than the
  authentic asset. The other services implement their documented ordinary data effects, not their
  exact instruction or event timings; scratch and arbitrary interrupt interactions are not claimed
  equivalent. Decimal-mode calls, side-effectful or aliased sources, and unvisited game paths are
  not generally validated.
- All other slots and link NMI enter the explicit `$F036` unsupported trap. Protection, identity,
  and checksum failures enter distinct traps at `$F039`, `$F03C`, and `$F03F`, respectively.
  Link reception and BRK-specific behavior are not implemented; other games and interactive
  gameplay are not validated.

The chosen low-RAM initialization is a compatibility policy, not measured physical power-on state.
The common entry contract and its uncertainty are described in
[`BIOS_REPLACEMENT_ANALYSIS.md`](BIOS_REPLACEMENT_ANALYSIS.md); service contracts and their
limitations are in [`BIOS_RUNTIME_API_ANALYSIS.md`](BIOS_RUNTIME_API_ANALYSIS.md).

## Validation and limits

The synthetic test suite checks successful and rejected boots, reset, dirty initial VRAM, IRQ
counter carry, callback gating and register preservation, unsupported traps, IRQ waits, and the
seven service implementations. It exercises all 256 glyph selectors with both incoming overflow
states, a 256-tile run, wrapped stacks and streams, both LCD-clear modes, and rectangle boundaries.
The synthetic cartridge and firmware are purpose-written and contain no commercial images.

Four 1,000-frame post-entry runs of standalone Cube Up were compared with the same runs under each
authentic BIOS: no input, long Start, menu actions, and the controller cycle defined in the runtime
analysis. The prototype reached the same per-slot call counts in every campaign, with no detected
unsupported trap or NMI. Frame numbers, execution timing, PCs, RAM and audio hashes differ. In the
three input-driven runs, the frame-1,000 video and final VRAM hashes matched both authentic BIOSes;
the unattended title-screen hashes differed. Hash equality at one endpoint does not imply matching
intermediate frames, audio, timing, complete game state, or successful interactive play.

For the first observed `$F01E`, `$F012`, and `$F02A` calls, controlled IRQ-masked probes used
the same VRAM fill and compared output registers/status, final VRAM hash and address, and LCD/VRAM
write counts against both BIOS revisions. Those properties matched, while their respective
prototype/authentic durations were 314,334/324,556, 3,218/2,300, and 1,532/1,540 master clocks.
The probe covered one call context per service, not every possible call.

These are emulator observations of one standalone cartridge, not physical measurements. No full
gameplay, other cartridges, 4-in-1 selection, save-state continuity, link/network behavior,
arbitrary callbacks, or exhaustive runtime ABI coverage is established.
