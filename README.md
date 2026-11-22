# superwhizkid

superwhizkid is an accuracy-focused emulator for the Bit Corporation Gamate handheld with
additional quality of life and presentation options. This repo provides a standalone emulation
library in C, a libretro core, and headless probing and validation tools.

The emulator includes cartridge protection and banking, progressive four-shade LCD refresh,
stereo AY-compatible audio, controller input, save states, and two-player Gamate link-cable play
through the libretro Netpacket interface. Its NMOS 6502-compatible CPU has been validated with
focused tests and the Klaus Dormann functional test. Hardware research, comparison sources, and 
known uncertainties are recorded in [`src/gamate/SOURCES.md`](src/gamate/SOURCES.md).

## Libretro Core

Gamate emulation requires an authentic `gamate_bios_umc.bin` or `gamate_bios_bit.bin` in the frontend's system directory.
No BIOS or game images are included with superwhizkid.

### Default Controls

These are the core’s default libretro inputs; frontends can remap them.

| Input | Action |
|---|---|
| D-pad / left stick | Move up, down, left, or right. |
| A / B | Gamate A / B buttons |
| Start / Select | Gamate Start / Select |
| L3 / R3 | Decrease / increase contrast by one level |
| Hold R2 | Fast-forward, if supported by the frontend; disabled during link sessions |

### Core Options

Defaults generally favor machine emulation accuracy. The following options can be adjusted to enable
quality of life features or change presentation.

| Option | Values (default in **bold**) | Description |
|---|---|---|
| Palette | **Original**, Gamate LCD, Greyscale, RGB | Gamate LCD is an interpretation of the physical panel; the other palettes are just for fun. |
| Contrast | 1–16 (**8**) | Adjusts ink contrast. L3/R3 also change it during play. |
| Audio Output | **Stereo**, Mono, Built-in Speaker | Selects stereo, mono, or an estimated built-in-speaker sound. |
| Catch quick taps | **Off**, On | Holds button presses until the game next reads input. May help in games whose code reads input sparsely. |
| LCD Ghosting | **Off**, Very light, Light, Medium, Strong, Very strong | Applies an estimated slow-panel response. |
| Redraw Flicker Reduction | **Off**, 1 Frame, 2 Frames, 3 Frames | Reduces brief redraw flashes; may delay genuine lightening changes. |
| BIOS Priority | **UMC**, Bit | Chooses the preferred BIOS at content load; falls back if it is unavailable. Reload content to apply. |
| Skip BIOS intro | **Off**, On | Runs the BIOS logo sequence instantly at power-on and reset. |

### Link Cable support

For link play, both players run the same game and core build, then host or join a standard RetroArch
netplay room. The transport accepts one remote peer and requires a frontend implementing libretro's
Netpacket interface.

## Research and Development

In addition to building the most accurate and enjoyable Gamate emulator around, this project also
focuses on targeted research on understanding the Gamate hardware and software.

Current research revolves around implementing an open-source, clean room boot firmware replacement
that is 100% compatible across the library. See the `docs/` folder for work that has been done
so far in this area. A separately built, incomplete
[experimental BIOS prototype](docs/EXPERIMENTAL_BIOS.md) currently targets standalone Cube Up;
it is not used by the libretro core.

Future lines of research and development will hopefully allow us to create tools to make homebrewing
for this system more feasible.

## Build

```sh
cmake -S . -B build
cmake --build build -j
ctest --test-dir build --output-on-failure
```

The libretro core is produced as `build/superwhizkid_libretro.so`. 

For local compatibility testing with legally obtained images:

```sh
tools/gamate-sweep.sh /path/to/gamate/files 1000 build/gamateboot
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


## Support the author

The superwhizkid project is, and will remain indefinitely, free and open source. 

That said, if you like my work and would like to support it, donations are welcome. 

[![ko-fi](https://ko-fi.com/img/githubbutton_sm.svg)](https://ko-fi.com/H2H7BO8ZE)
