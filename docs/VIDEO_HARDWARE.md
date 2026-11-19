# Gamate video hardware

The Gamate does not have a conventional tile-and-sprite graphics processor. Its display hardware
is a scrolling, two-bitplane framebuffer, so concepts such as tile size, sprite count, and
sprites-per-line limits do not apply at the hardware level.

## Resources

| Resource | Hardware capability |
| --- | --- |
| Physical LCD | 160 x 150 pixels |
| Backing framebuffer | 256 x 256 pixels |
| Color depth | 2 bits per pixel; four shades |
| VRAM | 16 KiB total |
| VRAM organization | Two 8 KiB bitplanes |
| Tile layers | None |
| Hardware tile size | Not applicable |
| Hardware sprites | None |
| Sprites per scanline | Not applicable; there is no hardware sprite list or scanline limit |
| Scrolling | One framebuffer viewport with X and Y scrolling |
| Window facility | Fixed, non-scrolling rows at the top of the display |
| Palette RAM | None known |
| DMA or blitter | None known; the CPU draws through an indexed VRAM data port |

The complete framebuffer consumes exactly 16 KiB:

```text
256 x 256 x 2 bits = 131,072 bits = 16,384 bytes
```

Each plane is an 8 KiB, 256 x 256 one-bit bitmap. A byte holds eight horizontally adjacent pixels
from one plane. Corresponding bits from the two planes select one of four LCD drive levels. The
planes are therefore components of one image, not independently composited graphics layers.

Software selects a 13-bit VRAM byte address and a bitplane, then accesses the selected byte through
the LCD data register. After an access, the address can automatically advance by either one byte
(eight pixels horizontally) or 32 bytes (one 256-pixel row vertically).

## Software graphics

Anything that looks like a tile or sprite is drawn by game software into the framebuffer. There is
no object attribute memory, hardware sprite size, sprite/background priority, collision unit, or
per-scanline sprite limit. Games may use graphics of any dimensions their routines support, but the
approximately 2.22 MHz CPU must perform the drawing, so CPU and VRAM-write bandwidth are the real
limits.

Moving an object generally requires erasing its old pixels and drawing it at the new position. On a
modern instantaneous display, intermediate framebuffer states can therefore expose flashing that
the slow original LCD concealed. Brick Blaster's paddle and ball are a confirmed example; this is
not sprite overflow.

## Viewport, scrolling, and windows

The LCD displays a 160 x 150 viewport into the 256-pixel-wide backing bitmap. Horizontal scrolling
wraps over the complete 256-pixel width. Normal vertical scrolling wraps over a 200-row region,
rather than all 256 rows.

The LCD controller also provides two window-like behaviors:

- Setting the window bit replaces the top 16 displayed rows with fixed VRAM rows `$D0-$DF`. These
  rows do not follow either X or Y scrolling.
- Particular Y-scroll values can select one to eight fixed rows from `$F8-$FF`. These rows do not
  follow X scrolling. The physical-hardware notes report no known commercial use of this mode.

These facilities can provide a fixed status area, but they still read the same framebuffer. They
are not additional tilemaps or layers.

## LCD shade generation and timing

Each of the 150 visible rows is driven three times per frame. Bitplane 0 is emitted once and
bitplane 1 twice, producing zero through three LCD activations from the two stored bits. A control
bit swaps the planes and therefore their effective weights.

The controller performs 450 row-latch events per frame at 162 master clocks each, for 72,900 master
clocks per frame. With the measured 4.433 MHz crystal, the resulting refresh rate is approximately
60.8093 Hz. Because VRAM is sampled progressively during these passes, writes made during a frame
can affect later scan activity without retroactively changing pixels already driven.

## Sources and use

- Kevtris, [“Gamate Inside,” version 1.00 (2016-06-29)](https://web.archive.org/web/20160630141941id_/http://blog.kevtris.org/blogfiles/Gamate%20Inside.txt): physical-hardware measurements used for
  VRAM size and organization, LCD dimensions and timing, address-port behavior, scrolling, window
  modes, and bitplane shade generation. This is the primary source.
- [emulators.org Gamate machine description](https://emulators.org/emulator/gamate/): independently
  authored emulator documentation used to corroborate the 16 KiB, two-bitplane framebuffer and
  eight-pixels-per-byte interface. It is not treated as a replacement for the physical measurements.
- [`src/gamate/SOURCES.md`](../src/gamate/SOURCES.md): the project's complete provenance record,
  including implementation comparisons, disagreements, and validation performed by Superwhizkid.

The conclusions that there are no hardware tiles, hardware sprites, or sprite-per-line limits follow
from the measured framebuffer architecture and absence of any documented tile, sprite, or object
interface. They should be revised if future chip-level research identifies an additional graphics
facility.
