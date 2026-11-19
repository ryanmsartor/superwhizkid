# Research opportunities from static ROM analysis

The commercial ROM corpus is a primary source for how Gamate software actually used the machine.
It can support original findings about cartridge formats, programming conventions, game engines,
and hardware-feature usage that are not documented by the hardware sources currently known to the
project.

ROM analysis is not equivalent to measuring hardware. It can establish what reachable software
does or expects, but it cannot conclusively establish undocumented electrical behavior or prove
that unused hardware features do not exist.

## 1. Cartridge format and boot ABI

Initial corpus analysis is recorded in
[`ROM_HEADER_ANALYSIS.md`](ROM_HEADER_ANALYSIS.md). It establishes the checksum, constant format
marker, copyright field, and `$6020/$6023/$6026` callback table while retaining the unexplained
header bytes as unknowns.

Capacity, padding, repeated-bank, and mirroring findings are recorded separately in
[`ROM_LAYOUT_ANALYSIS.md`](ROM_LAYOUT_ANALYSIS.md).

Currently documented fields include:

- `$6000-$6001`: checksum
- `$6005-$601D`: `COPYRIGHT BIT CORPORATION`
- `$6020`: standard cartridge entry point
- `$7000-$7FFF`: region checked by the BIOS checksum routine

A corpus analysis could determine:

- The meaning or consistency of bytes `$6002-$6004` and `$601E-$601F`.
- Whether fields encode a version, publisher, region, cartridge size, or other metadata.
- Common entry-point instruction patterns.
- Differences among Bit Corporation, UMC, and Phinnex releases.
- Whether later cartridges extend the header.
- Padding, mirroring, and unused-ROM conventions.
- Relationships between alternate dumps and software revisions.

This is a strong initial project because the header has an unambiguous position in every dump.

## 2. Mapper use by commercial software

The physical-hardware research describes the mapper interfaces, but not all software conventions.
The corpus could establish:

- Every reachable address used for upper-bank selection.
- Whether games always write exactly `$C000`.
- Every bank value written and the largest selected bank.
- Whether software masks or transforms bank numbers.
- Cross-bank calls, data access, and return-trampoline conventions.
- Whether bank zero is explicitly selected after startup.
- When and how the 4-in-1 writes `$8000`.
- Whether an ordinary cartridge contains reachable writes to `$8000-$9FFF`.
- Whether reachable bank-selection behavior is consistent with ROM size.

This work directly supports the CRC-independent mapper-detection investigation documented in
[`MAPPER_DETECTION.md`](MAPPER_DETECTION.md). Static opcode scanning can find candidates, but it
cannot distinguish code from data or prove reachability. Dynamic traces should confirm every
behavioral conclusion used to change emulation.

## 3. Gamate software ABI and shared libraries

Code comparison may reveal a common SDK, internal library, or development template. Useful routines
to cluster across the corpus include:

- Startup and RAM initialization.
- IRQ handlers and frame synchronization.
- Controller polling.
- VRAM addressing and drawing.
- Audio initialization and playback.
- Bank switching.
- Random-number generation.
- Delay routines.
- Font rendering.
- Collision detection.

Byte-identical or structurally equivalent routines may identify engine families, relationships
among games, and distinct generations of Bit Corporation or UMC development practices.

## 4. Software graphics formats

The hardware has no tile or sprite engine, but games implement those concepts in software. ROM
analysis could document:

- Common sprite dimensions.
- One-bit and two-bit graphics formats.
- Interleaved versus planar storage.
- Transparency and mask formats.
- Tilemap representations.
- Font and character encodings.
- Clipping and flipping routines.
- Run-length, dictionary, or other compression.
- Full-screen image formats.
- Whether moving objects erase pixels, restore saved backgrounds, or redraw larger regions.

These findings would complement the hardware description in
[`VIDEO_HARDWARE.md`](VIDEO_HARDWARE.md).

## 5. Drawing strategies and LCD artifacts

Static analysis, confirmed with targeted traces, could classify:

- Erase-then-redraw sequences.
- Display blanking during updates.
- Offscreen framebuffer use resembling double buffering.
- Drawing synchronized to IRQs.
- Plane-by-plane update order.
- Games that deliberately depend on LCD persistence.
- Shared routines responsible for visible flashing.

Brick Blaster's paddle and ball are a known erase/redraw example. Its routine may belong to a
reused Bit Corporation graphics library.

## 6. Use of obscure LCD features

A reachable-code audit could identify commercial use of:

- The fixed 16-row window.
- The special one-to-eight-row Y-scroll window.
- Plane swapping.
- Vertical auto-increment.
- Display blanking.
- LCD refresh stopping.
- Unusual Y-scroll values.
- Register `$5000`.
- Undocumented LCD-control bits.

Reachable use would be strong primary evidence about a feature's software purpose. Failure to find
use would not prove that the feature or behavior is absent from the hardware.

## 7. RAM and zero-page conventions

The Gamate has only 1 KiB of general-purpose RAM. Per-game memory maps could document:

- Zero-page pointer conventions.
- Stack depth and placement.
- Interrupt-shared variables.
- Controller state.
- Object tables.
- Audio state.
- Decompression buffers.
- Collision maps.
- Bank-selection bookkeeping.

Common layouts would provide additional evidence for shared engines and development libraries.

## 8. Interrupt architecture

Validated disassembly could document:

- IRQ entry points and handler families.
- Register-preservation conventions.
- Timer or frame counters.
- Use of `CLI`, `SEI`, `BRK`, and `RTI`.
- Expectations around the first IRQ after interrupt enable.
- Main-loop synchronization patterns.
- IRQ-driven rendering or audio.
- Intentional dependence on interrupt latency.

This would help prioritize the remaining CPU and interrupt accuracy questions.

## 9. Input handling and responsiveness

Game code could explain perceived control latency by revealing:

- Polling frequency.
- Edge-triggered versus level-triggered controls.
- Debounce behavior.
- Button-repeat delays.
- Input sampling restricted to an IRQ handler.
- Animation phases during which actions are ignored.
- Handling of simultaneous directions or buttons.

This would help separate authentic game logic from frontend or emulation latency.

## 10. Audio engines and music formats

The hardware sources describe the AY-compatible sound interface, but not commercial audio software.
The corpus could be classified by:

- Music-driver family.
- Note and instrument encoding.
- Playback tick rate.
- Hardware-envelope and noise use.
- Channel allocation.
- Adherence to the physical left, right, and centered-channel routing.
- Sound-effect priority and channel stealing.
- Reuse of audio engines across games.

## 11. Link-cable protocol

World Cup Soccer is a promising primary source for the software protocol layered over the measured
UART. Analysis could determine:

- Packet size and structure.
- Host and client roles.
- Synchronization and retry behavior.
- Checksums or acknowledgements.
- Transfer cadence.
- Link-loss handling.
- Which gameplay state is exchanged.
- Whether other games contain dormant UART support.

## 12. BIOS dependencies after cartridge entry

The corpus could reveal whether cartridges only inherit BIOS-created state or continue using BIOS
code after `$6020`:

- Reachable `JSR` or `JMP` targets in `$E000-$FFFF`.
- BIOS-owned RAM variables expected by games.
- Expected LCD, stack, interrupt, and audio state.
- Differences between early and late software.
- Whether `$F009` or other routines constitute an unofficial BIOS API.

This bears directly on the compatibility limits of a clean-room firmware or BIOS-free boot path.

## 13. CPU instruction usage

A reachable-code census could establish:

- The documented NMOS 6502 instructions used by commercial games.
- Real use of decimal mode.
- Intentional use of undocumented NMOS opcodes.
- Any use of the reported NCR 65CX02 bit-manipulation instructions.
- Opcode-frequency and addressing-mode profiles.

A raw byte census is insufficient because ROM data can resemble instructions. Findings should come
from validated code discovery and, where consequential, execution traces.

## Evidence limits

ROMs are primary evidence for software behavior and expectations, but only indirect evidence for
hardware. Static ROM analysis cannot conclusively establish:

- Wider mapper address decoding.
- Electrical timing.
- LCD response characteristics.
- Undocumented register behavior.
- Whether an unused hardware facility exists.
- Exact bus sequences without executing the code.
- Cartridge circuitry absent from the ROM dump.

Those questions still require physical measurements, purpose-built test cartridges, or
logic-analyzer traces.

## Recommended first corpus report

The initial corpus scope and exact local hashes are recorded in
[`ROM_CORPUS.md`](ROM_CORPUS.md).

An initial reproducible analyzer should produce:

1. A header and checksum report.
2. A reachable mapper-write report.
3. An LCD-register usage report.
4. A BIOS-call report.
5. A reachable-opcode census.
6. Shared-routine clustering.

The report should distinguish raw byte-pattern candidates from reachable instructions and
dynamically confirmed behavior. It should record file hashes so results are reproducible without
redistributing commercial ROM bytes.

## Source handling

- The local 58-cartridge collection is user-supplied and must remain read-only. Do not copy ROM
  bytes into the repository, retained traces, reports, tests, or releases.
- Publish hashes, addresses, decoded metadata, statistics, control-flow findings, and original
  analysis rather than copyrighted game data.
- Record new findings and methodology in [`../src/gamate/SOURCES.md`](../src/gamate/SOURCES.md) when
  they influence implementation, tests, or compatibility claims.
