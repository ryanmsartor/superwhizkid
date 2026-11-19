# Exploratory ROM graphics rendering

The Gamate LCD combines two one-bit framebuffer planes into four shade indices. Cartridge programs
must eventually transform their graphics into that representation, but the framebuffer interface
does not establish how source art is stored in ROM. It may be planar, row-interleaved, one-bit,
masked, compressed, procedural, or use a format not yet recognized.

`tools/gamate_rom_render_graphics.py` renders every selected ROM byte under explicit, deliberately
simple assumptions so visually coherent regions can be located and correlated with their ROM
offsets. A recognizable image is evidence for a candidate storage convention, not proof that all
games or even the surrounding data use that convention.

## Usage

Pillow is the only non-standard Python dependency. To render a complete cartridge using the default
two-plane interpretation:

```sh
python3 tools/gamate_rom_render_graphics.py GAME.bin
```

The default output is `GAME-graphics.png`. It is divided into labeled 16 KiB cartridge banks. The
left labels give the absolute ROM address at the beginning of each output row; the top labels give
the byte displacement of each graphics group within that row. Default groups contain 2 x 2 tiles,
and each assumed 8 x 8 two-bit tile consumes 16 bytes. Top labels rotate vertically when narrow
groups would otherwise make adjacent addresses overlap.

Useful alternatives include:

```sh
# Produce separate planar and row-interleaved comparison images.
python3 tools/gamate_rom_render_graphics.py GAME.bin --layout both

# Remove the 2 x 2 grouping assumption and show each candidate tile independently.
python3 tools/gamate_rom_render_graphics.py GAME.bin --group 1x1

# Inspect a known range at a larger scale.
python3 tools/gamate_rom_render_graphics.py GAME.bin range.png \
    --start 0x8000 --end 0xC000 --scale 4
```

The planar layout treats bytes 0-7 as plane 0 rows and bytes 8-15 as the corresponding plane 1
rows. The interleaved layout treats each successive byte pair as plane 0 and plane 1 for one row.
Within each byte, bit 7 is the leftmost pixel. The shade index is `plane0 + 2 * plane1`, matching the
normal LCD plane weights. Neutral greyscale is used to avoid confusing storage discovery with any
particular physical-panel palette estimate. Magenta marks an incomplete final tile.

`--group-order column-major` is available when a multi-tile object may store tiles down a column
before moving right. Grouping only changes placement in the output; it never changes the decoded
pixels or consumes padding between groups.

## Interpretation limits

The two supported layouts are hypotheses chosen because they are common, easy to inspect, and
consistent with the destination framebuffer. They are not derived cartridge-format facts. Failure
to see recognizable art does not imply that a ROM lacks graphics: one-bit assets, separate masks,
larger plane blocks, transformations, and compression will all appear noisy or misleading under
these assumptions. Candidate offsets found visually should be checked against disassembly and
runtime reads before documenting a format as used by game software.

## Sources and method

- Kevtris, [“Gamate Inside,” version 1.00 (2016-06-29)](https://web.archive.org/web/20160630141941id_/http://blog.kevtris.org/blogfiles/Gamate%20Inside.txt): used for the measured two-plane framebuffer,
  eight horizontal pixels per byte, and plane weights. It does not describe ROM graphics storage.
- The local `rc2tools` `rc2t render-graphics` command was consulted as a user-interface and output-
  organization reference: full-ROM rendering, address axes, bank sections, separated tile groups,
  nearest-neighbor scaling, and range selection. No `rc2tools` implementation code was copied or
  translated. Its Game Boy tile decoder was specifically not reused because Gamate storage remains
  an open research question.
- [`VIDEO_HARDWARE.md`](VIDEO_HARDWARE.md) records the hardware model that constrains the decoder.

