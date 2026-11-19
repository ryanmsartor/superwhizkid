# Authentic BIOS revision differences

This report compares the two recognized 4 KiB Gamate BIOS dumps as software artifacts. It records
derived structure and behavior without reproducing firmware bytes, instruction listings, graphics,
or reconstructable binary patches. The BIOS images remain user-supplied validation inputs and are
not distributed by this project.

## Images compared

| Variant | Size | SHA-1 |
| --- | ---: | --- |
| UMC/NCR | 4,096 bytes | `ea449dc607601f9a68d855ad6ab53800d2e99297` |
| Bit Corporation | 4,096 bytes | `4e9dfbfe916ca485530ef4221593ab68738e2217` |

Both images map at `$F000-$FFFF`. File offsets in the local comparison were translated to these CPU
addresses throughout this report.

Of 4,096 positions, 3,238 are identical and 858 differ. Thus 79.05% is byte-identical and 20.95%
differs. The differences form 36 contiguous runs, but they are strongly localized: only seven
single-byte address operands differ below `$FC00`; 851 of the 858 changes lie at `$FC2F-$FFFF`.

## Difference density

| CPU range | Differing bytes | Share of 256-byte page | Interpretation |
| --- | ---: | ---: | --- |
| `$F000-$F0FF` | 5 | 1.95% | Relocated-helper call targets |
| `$F100-$FAFF` | 0 | 0% | Byte-identical |
| `$FB00-$FBFF` | 2 | 0.78% | Relocated-helper call targets |
| `$FC00-$FCFF` | 201 | 78.52% | Reset/startup policy and unused-data differences |
| `$FD00-$FDFF` | 163 | 63.67% | Protection and failure-path relocation |
| `$FE00-$FEFF` | 244 | 95.31% | Header/checksum relocation and opaque/fill area |
| `$FF00-$FFFF` | 243 | 94.92% | Added failure paths, relocated helpers, interrupt wrappers, and vectors |

For exact localization without firmware content, the 36 changed runs are:

| Page | Changed address runs |
| --- | --- |
| `$F0xx` | `$F02B`, `$F02E`, `$F034`, `$F0E4`, `$F0F5` |
| `$FBxx` | `$FB76`, `$FBD9` |
| `$FCxx` | `$FC2F-$FC65`, `$FC67-$FC81`, `$FC83-$FCA4`, `$FCA6-$FCAF`, `$FCB2-$FCD5`, `$FCD8-$FCE5`, `$FCE7-$FCFF` |
| `$FDxx` | `$FD59`, `$FD5B-$FDBC`, `$FDBE-$FDD7`, `$FDD9-$FDFB`, `$FDFD-$FDFF` |
| `$FExx` | `$FE00-$FE1C`, `$FE1E-$FE2F`, `$FE31-$FE38`, `$FE3A-$FE83`, `$FE85-$FEC1`, `$FEC3-$FEE1`, `$FEE3-$FEE4`, `$FEE6-$FEE7`, `$FEE9-$FEEA`, `$FEEC-$FEED`, `$FEEF-$FEF7`, `$FEF9-$FEFA`, `$FEFC-$FEFF` |
| `$FFxx` | `$FF00-$FF70`, `$FF72-$FF80`, `$FF82-$FFCB`, `$FFD1`, `$FFD4-$FFFA`, `$FFFE` |

An address in this table says only that the stored byte differs. It does not imply that the byte is
an opcode, reachable code, or independently meaningful; many changes are operands, moved data, or
padding.

## Shared and relocated material

The long common prefix is not an accidental similarity. `$F000-$FC2E` is identical except for the
seven isolated bytes listed above. Inspection of their containing instructions shows that all seven
are address-operand adjustments to helpers relocated in the BIT image; they do not represent seven
independent algorithm changes.

Exact matching blocks in the divergent area show additional relocation:

| UMC/NCR range | Bit Corporation range | Length | Finding |
| --- | --- | ---: | --- |
| `$FD63-$FD92` | `$FD5E-$FD8D` | 48 | Protection/communication code moved 5 bytes earlier |
| `$FDCA-$FDF3` | `$FDC5-$FDEE` | 42 | Later portion of the same path moved 5 bytes earlier |
| `$FE0B-$FE44` | `$FE02-$FE3B` | 58 | Header-validation material moved 9 bytes earlier |
| `$FE46-$FE6F` | `$FE3D-$FE66` | 42 | Checksum material moved 9 bytes earlier |
| `$FF08-$FF70` | `$FF20-$FF88` | 105 | Common helper code moved 24 bytes later |
| `$FF72-$FFD5` | `$FF8A-$FFED` | 100 | Formatting and interrupt-tail material moved 24 bytes later |

The shorter exact matches around relocation boundaries omit instructions whose embedded absolute or
branch operands necessarily changed. Manual control-flow inspection confirms that the adjacent code
continues the same routines; the table deliberately reports only byte-exact spans.

## Functional changes

### Reset and cartridge validation

Both revisions reset through `$FC0D`. Their early initialization is mostly shared, but the code at
`$FC2F-$FCA0` differs in policy and layout:

- The BIT revision explicitly initializes the BIOS callback/control flag in RAM at two additional
  points.
- The UMC revision contains the alternate cartridge-format-marker path already identified in
  [`ROM_HEADER_ANALYSIS.md`](ROM_HEADER_ANALYSIS.md). It examines cartridge offset `$0002` and can
  bypass the small RAM trampoline for the recognized `$01` marker. The BIT revision unconditionally
  installs and enters its trampoline on this path.
- Branches, trampoline source addresses, and subsequent helper calls were adjusted to accommodate
  these insertions and removals.

Both revisions still perform the same externally relevant protection exchange, copyright-field
comparison, checksum over cartridge `$7000-$7FFF`, logo sequence, and final transfer to cartridge
entry `$6020` for the validated commercial corpus.

### Failure paths and unused regions

The BIT revision adds two short routines at `$FF00-$FF17`. They perform an additional write to
`$5900` before entering failure loops used by rejected cartridge paths. The purpose of that write is
not established by the current hardware sources, so this report does not label it as display,
interrupt, or other control. The UMC revision keeps the corresponding loops inline in the earlier
validation area and lacks that preceding write. This accounts for both code relocation and several
changed call targets.

Two areas that contain non-fill data in the UMC dump are predominantly `$FF` padding in the BIT
dump: approximately `$FCA1-$FCFF` and `$FE69-$FEFF`. The latter is one continuous 151-byte `$FF`
run in the BIT image. The UMC-only contents were not executed as instruction entry points during the
observed commercial boot, do not need to be reproduced to describe the revision's behavior, and are
therefore treated here as opaque/unused data rather than published or speculatively identified.

### Interrupt vectors and wrappers

| Vector | UMC/NCR target | Bit Corporation target | Meaning |
| --- | --- | --- | --- |
| NMI | `$FFC1` | `$FFD9` | Same wrapper family relocated by 24 bytes |
| RESET | `$FC0D` | `$FC0D` | Identical reset entry |
| IRQ/BRK | `$FF9D` | `$FFB5` | Same wrapper family relocated by 24 bytes |

The NMI and IRQ wrapper bodies retain the established cartridge ABI: the NMI path receives the link
byte and conditionally calls `$6023`; the periodic IRQ path updates BIOS counters and conditionally
calls `$6026`. Their different vector values are relocation, not a different cartridge callback
contract.

## Observed boot effect

A controlled boot of the same Brick Blaster image reached `$6020` under both revisions with
identical RAM, VRAM, scrolling, LCD control, mapper, and IRQ phase. Relative to the UMC BIOS, the BIT
BIOS took two more instructions and 29 more CPU cycles, or 58 master clocks. That shifts the
progressive LCD and audio sampling phase at cartridge entry without changing the initialized memory
contents.

The project's complete 58-cartridge/two-BIOS compatibility sweep previously reached cartridge
execution in all 116 cases. This establishes broad compatibility with both paths, not equivalence of
every failure screen, intermediate logo frame, or hardware bus cycle.

## Method and legal boundary

The local analysis used byte comparison, SHA-1 identification, contiguous-run and page-density
counts, exact matching-block detection, control-flow inspection, and execution through the
independently authored Superwhizkid CPU/machine model. Temporary byte listings and diagnostics were
kept outside the repository.

This committed report includes hashes, addresses, lengths, vectors, control-flow facts, and observed
state/timing results. It intentionally excludes:

- Firmware bytes, hexdumps, or binary patches.
- Full disassembly or source-like reconstruction.
- Embedded graphics or opaque UMC-only data.
- Any extract that could substitute for either BIOS image.

Addresses and behavioral descriptions are interoperability and research facts. They are sufficient
to reproduce the comparison when a user supplies lawful local dumps, while the copyrighted firmware
remains undistributed.

A deliberately abstract, non-decompilation representation of the resulting behavior is maintained
in [`BIOS_BOOT_BEHAVIOR.md`](BIOS_BOOT_BEHAVIOR.md).

