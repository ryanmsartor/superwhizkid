# Gamate mapper detection

## The hardware does not select a mapper

The Gamate console does not identify a cartridge and choose a mapper. Each physical cartridge
contains a ROM plus a protection/mapper chip. The chip is wired to the cartridge bus and determines
how reads and writes affect the ROM address lines. The console merely exposes its address and data
buses, read/write signal, reset, clock, and cartridge-select signal.

Kevtris's physical measurements describe two known arrangements:

- Flat 32 KiB cartridges map ROM directly across CPU addresses `$6000-$DFFF`.
- The common large-cartridge mapper fixes ROM bank 0 at `$6000-$9FFF` and maps a selectable 16 KiB
  bank at `$A000-$DFFF`. Known games write the bank number to `$C000`.
- The 4-in-1 mapper adds a selectable lower 16 KiB bank at `$6000-$9FFF`. Its software writes that
  bank number to `$8000`, while the upper bank retains the common `$C000` behavior.
- Both selectable banks start at bank zero after reset.

The notes suspect, but do not establish, that the chips may decode the wider `$8000-$9FFF` and
`$C000-$DFFF` ranges rather than only the exact addresses used by known software.

Source: Kevtris, “Gamate Inside,” version 1.00 (2016-06-29), archived at
<https://web.archive.org/web/20160630141941id_/http://blog.kevtris.org/blogfiles/Gamate%20Inside.txt>.
Its use and status as physical hardware research are also recorded in
[`src/gamate/SOURCES.md`](../src/gamate/SOURCES.md).

## Why a ROM dump cannot reproduce native identification

A raw dump contains bytes from the ROM chip, not a description of the cartridge PCB or its mapper
chip. On original hardware that missing physical circuit *is* the mapper identity. There is no
native identification exchange for the emulator to reproduce at content load.

ROM size is therefore an emulator inference:

- 32 KiB is sufficient for the flat mapping.
- Every known cartridge of 64 KiB or larger uses the common banked interface except the 4-in-1.
- The known 4-in-1 is 128 KiB, a size also used by ordinary banked games, so size cannot distinguish
  it.

The current implementation selects the known 4-in-1 using its exact size and full-image CRC32, then
uses size for the flat/common distinction. This is deterministic and safe for known dumps, but it
depends on a database-style identity and would not recognize a different dump or an unknown cartridge
using the same lower-bank mechanism.

## Candidate: lazy lower-bank activation

The common and 4-in-1 mappings are observably identical from reset until software writes the lower
bank register: both begin with ROM bank 0 at `$6000-$9FFF`. This permits a possible CRC-independent
policy:

1. Start every cartridge larger than 32 KiB with the common banked mapping.
2. Continue treating writes to `$C000` as upper-bank selection.
3. On the first post-unlock write to `$8000`, enable lower-bank switching and apply that write as the
   lower-bank selection.
4. Preserve the activated mapper and both bank values in machine save states.

This is behavioral auto-detection, not a native hardware identification command. It delays the
emulator-only decision until software exercises the additional bus behavior that distinguishes the
known mapper.

Potential advantages:

- Removes dependence on the known 4-in-1 CRC.
- Supports alternate valid dumps without adding fingerprints.
- May support an unknown multicart using the same mapper.
- Bases the decision on an observed cartridge-bus operation rather than filename or database entry.

Potential risk:

- A write to `$8000` is ignored by a physical common-mapper cartridge. Lazy activation would instead
  reinterpret an accidental, probing, or otherwise unrelated write as a lower-bank change.
- Static opcode scanning cannot resolve this safely because matching bytes may be data or unreachable
  code.
- Expanding detection to the suspected `$8000-$9FFF` range would rely on an unconfirmed decode range
  and increase the chance of a false activation.
- Save-state compatibility must be considered if mapper representation changes.

Emulating the 4-in-1 mapper unconditionally is not appropriate for the same reason: it would give
ordinary cartridges lower-bank behavior that their physical mapper chips do not have.

## Required validation

Do not replace the fingerprint until the behavioral policy has been tested against the entire local
library:

1. Instrument post-unlock cartridge writes without changing mapper behavior.
2. Record the address, value, PC, and master clock of every write in `$8000-$9FFF` for all 58 local
   cartridges under both validated BIOS revisions.
3. Confirm whether the 4-in-1 is the only title that writes the range and whether it uses exactly
   `$8000`.
4. Record when its first lower-bank write occurs and verify that execution before that point is
   identical under the common and 4-in-1 mappings.
5. Implement lazy activation initially for exact `$8000` writes only, matching observed software
   rather than the wider suspected decode.
6. Run the complete 116-case, two-BIOS, 300-frame compatibility sweep.
7. Compare video hashes, audio hashes, PCs, diagnostics, mapper state, and bank activity with the
   current implementation.
8. Run longer targeted sessions for the 4-in-1 and any ordinary cartridge that writes the candidate
   range.
9. Test reset and save/load before and after activation.

## Decision rule

Prefer lazy activation over the CRC only if:

- No ordinary cartridge performs an ambiguous `$8000` write in the validated runs.
- The 4-in-1 activates reliably before it needs a nonzero lower bank.
- Full compatibility and deterministic-output comparisons remain exact.
- Mapper activation and state serialization have focused regression coverage.

Retain the fingerprint if ordinary software produces ambiguous writes or if behavioral activation
causes any compatibility or state-lifecycle regression. A sidecar/database mapper declaration would
remain another deterministic option for future unknown dumps, but raw ROM content alone cannot
restore the cartridge PCB identity lost during dumping.
