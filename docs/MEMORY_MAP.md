# Gamate CPU memory map

This document describes the Gamate's 16-bit CPU-visible address space, the behavior represented
by superwhizkid, and the areas that remain uncertain. It is not a complete pin-level bus decode.

## Confidence and terminology

- **Measured/documented** identifies behavior attributed to physical Gamate research.
- **Core behavior** describes the current implementation; it is not by itself proof of hardware
  behavior.
- **Unknown** means the available evidence does not establish a hardware function or decode range.

The principal physical source for the map is Kevtris, “Gamate Inside,” version 1.00 (2016-06-29),
archived at <https://web.archive.org/web/20160630141941id_/http://blog.kevtris.org/blogfiles/Gamate%20Inside.txt>.
Its status and use are recorded in [`../src/gamate/SOURCES.md`](../src/gamate/SOURCES.md).

## Overview

```text
$FFFF ┌──────────────────────────────────────────────┐
      │ BIOS: 4 KiB image mirrored twice             │ $E000-$FFFF
$E000 ├──────────────────────────────────────────────┤
      │ Cartridge ROM / protection / mapper window   │ $6000-$DFFF
$6000 ├──────────────────────────────────────────────┤
      │ No device mapped by core; hardware unknown   │ $5B00-$5FFF
      │ Status-like read; purpose uncertain          │ $5A00-$5AFF
      │ Writes observed; purpose unknown; core ignores│ $5900-$59FF
      │ Open-bus reads; bit-5 write locks up hardware│ $5800-$58FF
      │ No device mapped; core reads as $FF          │ $5400-$57FF
      │ LCD registers and VRAM data port             │ $5000-$53FF
      │ UART clock                                   │ $4C00-$4FFF
      │ UART receive data                            │ $4800-$4BFF
      │ Controller reads / UART transmit writes      │ $4400-$47FF
      │ AY/PSG writes; reads are undriven            │ $4000-$43FF
$4000 ├──────────────────────────────────────────────┤
      │ No device mapped by core; hardware unknown   │ $2000-$3FFF
$2000 ├──────────────────────────────────────────────┤
      │ 1 KiB RAM mirrored 8 times                   │ $0000-$1FFF
$0000 └──────────────────────────────────────────────┘
```

The RAM mirror is a 1 KiB region repeated every `$0400`. The BIOS window contains two copies of
the 4 KiB BIOS image: `$E000-$EFFF` and `$F000-$FFFF`.

The ranges `$2000-$3FFF`, `$5400-$57FF`, and `$5800-$5FFF` have no device handlers in the core.
For otherwise unhandled reads, it returns the address's high byte; `$5400-$57FF` instead returns
`$FF`. Unhandled writes have no effect in the core. In particular, that write fallback does not
reproduce the documented `$5800-$58FF` lockup behavior described below.

## Known WRAM layout

The Gamate's 1 KiB physical work RAM is `$000-$3FF`. CPU addresses `$0000-$1FFF` access it through
the eight-way mirror described above. The following is a map of behaviors established by BIOS or
cartridge probing, not a claim that every byte has a hardware-defined meaning:

```text
Physical address   Established or observed use
─────────────────  ─────────────────────────────────────────────────────────────────────
$000-$003          Pointer/scratch fields; $000-$001 and $002-$003 serve as caller parameters
                   or scratch for measured BIOS services
$004-$005          Protection reply and reset-paired software field
$006-$009          Reset sentinels and service pointer/control fields
$00A-$00B          $00A runtime sentinel; $00B IRQ-gated wait threshold
$00C               Cartridge-callback enable flag
$00D-$00F          BIOS scratch/reserved fields in the exercised startup ABI
$010-$011          32-bit IRQ-counter low bytes: $011 low byte, carry toward $010
$012-$014          16-bit LFSR at $012-$013; $014 scratch feedback
$015               Cached raw LCD-control value used by BIOS drawing services
$018-$01A          Common BIOS service scratch
$01B-$01D          Mixed service scratch, dimensions, and caller-control fields
$01E-$01F          Common BIOS service loop/scratch fields
$021               BIOS-initialized zero; horizontal-position input in selected $F027 calls
$022               Reset-filled $FF at entry; vertical-position input in selected $F027 calls
$023/$028/$030     Empirical parameter or scratch sites for some BIOS services
$0CC-$0CF          Title-specific work areas; $0CE is a countdown-like variable in 13 titles
$100-$13F          Stack-adjacent and title-specific sensitive work; $140-$3FF otherwise title work
$21C-$21F          Title-specific work areas observed for Mighty Boxer and Witty Apee
$0E0-$0E1/$0E5-$0E6
                   Inherited cartridge-checksum scratch copies established by replacement analysis
```

The low-RAM ABI is covered by [`BIOS_ENTRY_RAM_ANALYSIS.md`](BIOS_ENTRY_RAM_ANALYSIS.md) and
[`BIOS_RUNTIME_API_ANALYSIS.md`](BIOS_RUNTIME_API_ANALYSIS.md). Physical values above `$01F` are
largely title-defined. Cartridge/block sweeps found broad or repeated sensitivity in particular
ranges, including `$000-$03F`, `$0C0-$0FF`, `$200-$23F`, and `$240-$27F`, but those observations
do not establish shared meanings or reserved bytes.

At cartridge entry, the tested replacement policy preserves behavior by initializing callback,
protection, reset-sentinel, IRQ-counter, LFSR, and `$021` fields rather than reproducing every
unrelated entry byte. The reset-fill policy is currently `$FF`, but physical RAM power-on values
remain unmeasured. The runtime ABI also uses `$018-$01A`, `$01C-$01F`, and stack bytes as
BIOS-service scratch; these bytes must not be treated as permanently allocated cartridge variables.

## Known VRAM layout

Gamate VRAM (16 KiB physical backing)
══════════════════════════════════════════════════════════════════

Two 8 KiB logical planes, interleaved byte-by-byte in backing RAM.
Each plane is a 256 × 256 pixel, 1-bpp bitmap.

Logical address    Plane 0       Plane 1       Physical backing
──────────────────────────────────────────────────────────────────
$0000-$001F        Row 0         Row 0         $0000-$003F
$0020-$003F        Row 1         Row 1         $0040-$007F
$0040-$005F        Row 2         Row 2         $0080-$00BF
   ...              ...           ...              ...
$1FC0-$1FDF        Row 254       Row 254       $3F80-$3FBF
$1FE0-$1FFF        Row 255       Row 255       $3FC0-$3FFF
──────────────────────────────────────────────────────────────────

Physical backing byte layout (one scanline):

        Plane 0             Plane 1
      ┌──────────┐        ┌──────────┐
      │ Byte  0  │        │ Byte  0  │
      │ Byte  1  │        │ Byte  1  │
      │   ...    │        │   ...    │
      │ Byte 31  │        │ Byte 31  │
      └──────────┘        └──────────┘
       32 bytes             32 bytes
       256 pixels           256 pixels

Interleaved physical layout:

$0000  P0 Row 0, Byte 0
$0001  P1 Row 0, Byte 0
$0002  P0 Row 0, Byte 1
$0003  P1 Row 0, Byte 1
  ...
$003E  P0 Row 0, Byte 31
$003F  P1 Row 0, Byte 31

$0040  P0 Row 1, Byte 0
$0041  P1 Row 1, Byte 0
  ...
$3FFE  P0 Row 255, Byte 31
$3FFF  P1 Row 255, Byte 31

──────────────────────────────────────────────────────────────────

Display organization (256 × 256 backing bitmap):
```
  Y=$00 ┌──────────────────────────────────────┐
        │                                      │
        │                                      │
        │                                      │
        │                                      │
        │     Normal scrollable region         │
        │     Y=$00-$C7 (200 rows)             │
        │                                      │
        │                                      │
        │                                      │
        │                                      │
  Y=$C8 ├──────────────────────────────────────┤
        │     Other backing rows               │
  Y=$CF ├──────────────────────────────────────┤
        │     Window region Y=$D0-$DF (16 rows)│
        │     Replaces top 16 displayed rows   │
        │     when window mode is enabled      │
  Y=$E0 ├──────────────────────────────────────┤
        │     Other backing rows               │
  Y=$F7 ├──────────────────────────────────────┤
        │     Fixed Y-scroll region            │
        │     Y=$F8-$FF (8 rows)               │
  Y=$FF └──────────────────────────────────────┘
```
Normal viewport: 160 × 150 pixels
Horizontal backing width: 256 pixels
Vertical scrolling: wraps within normal Y=$00-$C7 range

══════════════════════════════════════════════════════════════════

```
// Logical VRAM address: 0x0000-0x1FFF
// Plane: 0 or 1

physical_offset = (logical_address << 1) | plane;

// Row and byte position within a plane
row         = logical_address / 32;
byte_column = logical_address % 32;

// Physical backing offset for the start of a row
plane0_row = row * 0x40;
plane1_row = row * 0x40 + 1;
```

The LCD backing VRAM is 16 KiB: `$0000-$3FFF` in a byte-oriented emulator array representing two
8 KiB logical planes. The LCD address registers expose a 13-bit logical VRAM address
`$0000-$1FFF`, and the selected plane selects which byte is written or read:

```text
Logical address + plane   Emulator/physical backing byte
$0000-$1FFF, plane 0      backing offset (logical address << 1)
$0000-$1FFF, plane 1      backing offset (logical address << 1) | 1
```

Each plane is a 256-column by 256-row one-bit bitmap. A byte contains eight horizontally adjacent
pixels, so the row layout is 32 bytes:

```text
Plane 0 row Y: backing $0000 + Y * $40
Plane 1 row Y: backing $0001 + Y * $40
```

More explicitly, logical row `Y` starts at byte column 0 in plane 0 at
`(Y * $20) << 1` and in plane 1 at that offset plus one. Logical address `A` therefore maps to
`(A << 1) | plane` for CPU VRAM-port access.

The established display regions are:

| Region | VRAM meaning |
| --- | --- |
| Normal 160 x 150 viewport | Horizontal row from the 256-pixel backing bitmap; vertical scroll wraps in the normal `$00-$C7` range |
| Window rows | Rows `$D0-$DF` replace the top 16 displayed rows when the window bit is set |
| Fixed Y-scroll rows | Rows `$F8-$FF` are available when special Y-scroll values select fixed rows |
| Plane 0 | Backing offsets `$0000, $0002, ...`, one bit per pixel |
| Plane 1 | Backing offsets `$0001, $0003, ...`, one bit per pixel |

Known BIOS-service behavior includes large plane-0/plane-1 drawing or copy operations, but VRAM
contents and addresses are caller/service dependent; they are not fixed software VRAM reservations.
For example, the `$F01B` service has tested plane-0 coverage from `$1CC0-$1FDF` in ordinary mode,
with sparse mode reaching `$0000-$03A0` and `$1CC0-$1FE0` at `$20` intervals. These ranges describe
measured service behavior and must not be interpreted as permanent sprite or screen allocation.

VRAM organization and display behavior are also documented in
[`VIDEO_HARDWARE.md`](VIDEO_HARDWARE.md). The CPU VRAM data port selects and advances logical
addresses and is not a separate 32 KiB CPU-visible RAM region.

## I/O ranges

| CPU addresses | Reads | Writes | Evidence and limits |
| --- | --- | --- | --- |
| `$4000-$43FF` | Undriven/open-bus behavior | AY/PSG register interface | Register access is modeled across this range. |
| `$4400-$47FF` | Active-low controller state | UART transmit byte | Read/write functions share the address range. |
| `$4800-$4BFF` | UART receive byte | No mapped function | UART behavior is modeled. |
| `$4C00-$4FFF` | Performs a UART clock and returns undriven data | Performs a UART clock | A read has a clocking side effect in the core. |
| `$5000-$53FF` | VRAM data on register offset `+6`; other reads are undriven | LCD control and VRAM data ports | Register selection repeats every 8 bytes in the core. |
| `$5400-$57FF` | `$FF` in the core | No mapped function | Hardware function not established here. |
| `$5800-$58FF` | Open-bus behavior | A write with bit 5 set locks up the system on hardware; the core ignores writes | The lockup is documented in the hardware notes but is not emulated. |
| `$5900-$59FF` | High address byte in the core; hardware read behavior not established | Writes are observed but “don't seem to do much”; the core ignores them | `$5900`'s purpose remains unknown. |
| `$5A00-$5AFF` | `$xx11`-style status; core returns `$5B` | No mapped function | The status function is uncertain. |
| `$5B00-$5FFF` | High address byte in the core | No mapped function | No hardware function established. |

The `$5800-$58FF` open-bus and bit-5 write-lockup behavior, along with the `$5900-$59FF` write
observation and `$5A00` `xx11` reads, are noted in the physical research record. The core models
`$5A00-$5AFF` as returning the address high byte with bits 0 and 1 set, which produces `$5B`.
Although the BIOS writes to `$5900` on one failure path, its purpose remains unknown. These
observations should not be generalized into a fully confirmed decode of the surrounding pages.

Within `$5000-$53FF`, the low three address bits select the LCD register. The implemented write
registers are:

| Offset | Function |
| --- | --- |
| `+1` | Display control |
| `+2` | Horizontal scroll |
| `+3` | Vertical scroll |
| `+4` | VRAM address low bits and plane |
| `+5` | VRAM address high bits |
| `+7` | VRAM data write |

Reading offset `+6` reads VRAM data and advances the VRAM address. The LCD's backing VRAM is 16 KiB;
its measured two-plane organization and display behavior are covered in
[`VIDEO_HARDWARE.md`](VIDEO_HARDWARE.md).

## Cartridge window

Cartridge reads occupy `$6000-$DFFF`, a 32 KiB CPU window. Protected ROM reads become available
after the cartridge protection exchange succeeds. The mapper arrangements documented for known
cartridges are:

```text
CPU address   Flat 32 KiB        Common large mapper    Known 4-in-1
───────────   ────────────       ───────────────────    ────────────
$DFFF         ROM offset $7FFF   selected upper bank    selected upper bank
   │                             ┌──────────────────┐   ┌──────────────────┐
$C000         flat ROM           │ upper-bank write │   │ upper-bank write │
   │                             └──────────────────┘   └──────────────────┘
$A000         flat ROM           selected upper bank    selected upper bank
   │
$9FFF         flat ROM           fixed bank 0            selectable lower bank
   │                                                   (bank write at $8000)
$8000         flat ROM           fixed bank 0            ───────────────────
   │
$6000         flat ROM           fixed bank 0            selectable lower bank
```

- A flat 32 KiB cartridge maps its image across the full window.
- The common large mapper fixes bank 0 at `$6000-$9FFF` and selects a 16 KiB bank at
  `$A000-$DFFF`, using writes to `$C000`.
- The known 4-in-1 adds a selectable lower 16 KiB bank, using writes to `$8000`; its upper bank
  uses the common `$C000` selection.
- The measured software write addresses are known, but the cartridge chips' wider address decode
  ranges are not established. The core currently responds to exact `$8000` and `$C000` writes.

The only known 16 KiB cartridge is Cube Up. The core wraps its ROM offset by image size, thereby
presenting the image twice in the 32 KiB window. That is emulator policy, not proof of physical
mirroring; the behavior of `$A000-$DFFF` on the original cartridge remains unresolved. See
[`MAPPER_DETECTION.md`](MAPPER_DETECTION.md) and
[`ROM_LAYOUT_ANALYSIS.md`](ROM_LAYOUT_ANALYSIS.md).

## References

- Kevtris, “Gamate Inside,” physical reverse-engineering notes, archived URL above.
- [`src/gamate/gamate.c`](../src/gamate/gamate.c): implemented CPU read/write map and cartridge
  offset selection.
- [`MAPPER_DETECTION.md`](MAPPER_DETECTION.md): known mapper arrangements and decode uncertainty.
- [`VIDEO_HARDWARE.md`](VIDEO_HARDWARE.md): LCD and VRAM organization.
- [`BIOS_BOOT_BEHAVIOR.md`](BIOS_BOOT_BEHAVIOR.md): BIOS behavior and the unresolved `$5900`
  function.
