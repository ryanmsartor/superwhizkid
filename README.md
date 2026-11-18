# superwhizkid

superwhizkid is an accuracy-focused emulator for the Bit Corporation Gamate handheld. It provides
a standalone emulation library, a libretro core, and headless validation tools.

The emulator includes cartridge protection and banking, progressive four-shade LCD refresh,
stereo AY-compatible audio, controller input, save states, and two-player Gamate link-cable play
through the libretro Netpacket interface. Its NMOS 6502-compatible CPU has been validated with
focused tests and the Klaus Dormann functional
test. Hardware research, comparison sources, and known uncertainties are recorded in
[`src/gamate/SOURCES.md`](src/gamate/SOURCES.md).

## Build

```sh
cmake -S . -B build
cmake --build build -j
ctest --test-dir build --output-on-failure
```

The libretro core is produced as `build/superwhizkid_libretro.so`. It requires an authentic
`gamate_bios_umc.bin` or `gamate_bios_bit.bin` in the frontend's system directory. No BIOS or game
images are included.

For link play, both players run the same game and core build, then host or join a standard RetroArch
netplay room. The transport accepts one remote peer and requires a frontend implementing libretro's
Netpacket interface.

For local compatibility testing with legally obtained images:

```sh
tools/gamate-sweep.sh /path/to/gamate/files 300 build/gamateboot
```

## Releases

The manual **Build and package cores** GitHub Action builds spruce-compatible ARMHF and ARM64
libretro cores. Supply a release tag when invoking it; after both builds and architecture/ABI checks
pass, it creates or replaces that tag and uploads a combined package to a draft GitHub release.

## Provenance and license

superwhizkid began development inside a fork of armNES, but its Gamate implementation was authored
by Ry as an independent emulator. External research and comparison implementations were used as
documented sources, not copied as implementation text.

The project is licensed under the GNU General Public License, version 2 or (at your option) any
later version. See [`LICENSE`](LICENSE). Included third-party interface material retains its own
copyright notice.
