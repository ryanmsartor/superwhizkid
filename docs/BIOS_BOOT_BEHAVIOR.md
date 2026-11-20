# Gamate BIOS boot behavior

This document is a high-level behavioral specification of the two recognized authentic Gamate BIOS
revisions. It is intended to explain the firmware/core/cartridge interface and support independent
emulation. It is not a decompilation, substitute firmware, or instruction-by-instruction translation.

The pseudocode deliberately normalizes incidental implementation details. Named operations describe
observable purposes rather than preserve the original instruction sequence, register allocation,
private RAM layout, delay-loop construction, or embedded data.

## Shared successful boot path

Both revisions follow this externally visible sequence for recognized commercial cartridges:

```text
on_reset:
    disable or defer normal cartridge callbacks
    initialize the CPU stack and BIOS working state
    initialize display-related state used by the startup sequence
    initialize BIOS timing counters
    allow the periodic interrupt mechanism to begin running

    perform_initial_hardware_and_cartridge_checks()

    if cartridge_protection_exchange_fails:
        enter_revision_specific_failure_path(PROTECTION_FAILURE)

    if cartridge_identity_field_does_not_match:
        enter_revision_specific_failure_path(IDENTITY_FAILURE)

    expected_checksum = cartridge_header.checksum
    observed_checksum = sum_bytes(cartridge[$7000 through $7FFF]) modulo 65536
    if observed_checksum != expected_checksum:
        enter_revision_specific_failure_path(CHECKSUM_FAILURE)

    present_startup_logo_and_delay()

    leave BIOS-managed machine state ready for cartridge execution
    transfer_control_to_cartridge($6020)
```

The names `perform_initial_hardware_and_cartridge_checks` and
`present_startup_logo_and_delay` intentionally hide drawing primitives, delay-loop construction,
and private working variables. Those implementation details are not necessary to state the boot
contract.

The checksum range is fixed and does not cover the complete cartridge. The identity comparison ends
at cartridge address `$601D`; bytes `$601E-$601F` are not part of that comparison. Successful boot
always transfers through `$6020`, whose commercial cartridge contents are an absolute jump to the
game's real entry point.

### Low-RAM entry contract

Controlled perturbation and access tracing reduce the required low-RAM state to named behavior:

```text
RAM[$004] := accepted cartridge-protection reply
RAM[$005 through $009] := $FF reset-fill sentinels
RAM[$00A] := $FF runtime sentinel
RAM[$00C] := callbacks enabled
RAM[$010 through $011] := periodic-IRQ counter phase
RAM[$012 through $013] := nonzero BIOS pseudorandom/LFSR state
```

The authentic entry values reflect the authentic boot path, but a clean-room implementation can
derive the protection result and IRQ phase, deliberately initialize the sentinels, and choose a
documented nonzero LFSR seed. `$014` is temporary LFSR feedback, overwritten by service `$F030`, and
does not need to be inherited at cartridge entry.

Constructing only these fields after clearing `$000-$01F` matched authentic-entry behavior in all
116 tested BIOS/cartridge combinations, both unattended and with broad automated input for 1,000
frames. The equivalence test used the observed authentic counter and LFSR phases so that phase alone
could not obscure the result; alternate valid phases remain a separate compatibility experiment.

## Cartridge-protection exchange

The protection check can be represented without reproducing its firmware implementation:

```text
cartridge_protection_exchange:
    transmit the documented cartridge-unlock identifier through the protection interface
    receive the protection reply one bit at a time

    if the assembled reply is not the expected hardware response:
        return FAILURE

    mark cartridge ROM as accessible
    return SUCCESS
```

This describes the protocol boundary, not the original loop structure. Superwhizkid models the
measured interface independently: the identifier is sent serially, the expected reply is `$47`, and
normal cartridge reads become available only after a successful exchange.

## Revision-specific startup selection

The revisions share their reset entry at `$FC0D`, but differ in how one early cartridge-startup path
is selected:

```text
select_startup_path(revision, hardware_status, cartridge):
    if hardware_status selects the ordinary protected path:
        continue with protection and validation

    if revision is UMC_NCR:
        if cartridge.format_marker_at_$6002 == $01:
            use the UMC direct validation path
        else:
            install the small startup trampoline in RAM
            execute the RAM trampoline

    if revision is BIT_CORPORATION:
        initialize the callback/control flag explicitly
        install the BIT startup trampoline in RAM
        execute the RAM trampoline
```

The meaning of every possible hardware-status value is not yet established. The pseudocode states
only the branch distinction observed in the two images. The `$01` comparison establishes a
firmware-recognized marker, not a general version or region numbering system.

The BIT revision initializes the callback/control flag at an additional point later in this startup
area. Both revisions nevertheless reach the same successful cartridge-facing state for the tested
commercial corpus.

## Failure behavior

Failure handling is less completely characterized than successful boot:

```text
enter_revision_specific_failure_path(reason):
    if revision is BIT_CORPORATION:
        write zero to hardware address $5900

    remain in the failure behavior selected for reason
    do not transfer control to cartridge entry $6020
```

The BIT revision factors two such paths into routines at `$FF00-$FF17`; the UMC revision keeps the
corresponding loops inline. The purpose of `$5900` is unknown. It must not be described as an LCD,
interrupt, or other control register without new hardware evidence. This abstraction also does not
claim that all failure reasons produce the same display, bus activity, or loop.

## Runtime interrupt contract

After the BIOS enables cartridge callbacks, its interrupt wrappers provide a small runtime ABI.
Their addresses differ between revisions, but their cartridge-facing behavior is shared.

```text
on_periodic_irq:
    preserve the interrupted cartridge context
    update BIOS-maintained timing counters

    if cartridge_callbacks_are_enabled:
        call cartridge IRQ callback at $6026

    restore the interrupted context
    return from interrupt
```

```text
on_link_receive_nmi:
    preserve the interrupted cartridge context
    read the completed link byte into BIOS-managed RAM

    if cartridge_callbacks_are_enabled:
        call cartridge NMI callback at $6023

    restore the interrupted context
    return from interrupt
```

The cartridge callback returns to the BIOS wrapper as a subroutine; the wrapper performs the final
interrupt return. Commercial cartridges may make `$6023` an immediate no-op return when they do not
use link reception.

### Periodic IRQ counter

The timing counter updated by both periodic IRQ wrappers is the 16-bit value at `$0010-$0011`, with
the high byte at `$0010`. Each IRQ increments `$0011`; wrapping it from `$FF` to `$00` increments
`$0010`. Both BIOS revisions expose `$0394` at cartridge entry in the controlled sweep, reflecting
the IRQs elapsed during boot. Their handler addresses differ, but their counter behavior matches.

Fourteen tested cartridges directly access `$0011` within 1,000 unattended frames, including games
that poll it repeatedly. It is therefore cartridge-visible ABI state. See
[`BIOS_ENTRY_RAM_ANALYSIS.md`](BIOS_ENTRY_RAM_ANALYSIS.md) for the access table and perturbation
results. The exact entry phase is an observed authentic-boot contract; replacement firmware may
derive a compatible counter from its own elapsed IRQs rather than reproduce opaque boot work.

### Cartridge control of the callback flag

The wrapper condition is the BIOS-owned RAM byte at `$000C`:

```text
cartridge_callbacks_are_enabled := RAM[$000C] != 0
```

Both BIOS revisions clear `$000C` near reset and set it to `$01` near the end of successful startup,
three executed instructions before reaching `$6020` in the controlled boot trace. Cartridge software
can access the same physical RAM cell, including through RAM mirrors.

A 1,000-frame, no-input execution scan of all 58 locally available cartridges found six titles that
actually accessed `$000C` from cartridge code. Each result was repeated with both BIOS revisions:

| Cartridge | Executing locations | Observed behavior |
| --- | --- | --- |
| Bao Qing Tian | `$6335`, `$6345` | A RAM-clearing loop overwrites the flag with zero; startup explicitly restores `$01` |
| Basketball | `$63D1`, `$63E1`; bank 9 `$A00B`, `$A0CE` | Startup clears/restores `$01`; later code changes `$01` to `$00`, then `$00` to `$07` |
| Dinosaur Park | `$68C4`, `$68D4` | A RAM-clearing loop overwrites the flag with zero; startup explicitly restores `$01` |
| Riddle of the Ancient Tomb | `$6377`, `$6387` | A RAM-clearing loop overwrites the flag with zero; startup explicitly restores `$01` |
| Robin Hood | `$68A5`, `$68AC`, `$68BB` | Startup reads and saves the prior flag, clears the RAM range, then restores the flag |
| The Golden Pyramid | `$64A8`, `$64B8` | A RAM-clearing loop overwrites the flag with zero; startup explicitly restores `$01` |

These accesses show that `$000C` is not merely a private BIOS implementation detail. Multiple
software families preserve or restore it while clearing shared RAM, and Basketball deliberately
uses zero and a nonzero value later. The evidence supports a boolean ABI interpretation—zero blocks
callbacks and any nonzero value permits them—but does not establish whether Basketball's `$07` has
additional game-specific meaning.

The other 52 titles made no cartridge-side access during the observed window. That is not proof they
never access the flag: unattended title screens, unvisited gameplay paths, and the 1,000-frame limit
leave later or input-dependent code untested.

| Vector | UMC/NCR wrapper | Bit Corporation wrapper |
| --- | --- | --- |
| NMI | `$FFC1` | `$FFD9` |
| RESET | `$FC0D` | `$FC0D` |
| IRQ/BRK | `$FF9D` | `$FFB5` |

The 24-byte NMI/IRQ address shift reflects relocation, not a different callback ABI.

## Successful entry state

Controlled execution of the same cartridge under both BIOSes found identical RAM and VRAM contents,
scroll registers, LCD control, mapper state, and IRQ phase at `$6020`. The BIT path took two more
instructions and 29 more CPU cycles, so it reached cartridge entry 58 master clocks later. The
remaining difference is oscillator/display/audio phase rather than initialized memory content.

This result describes the observed accepted-cartridge path. It does not claim identical intermediate
logo frames, failure behavior, or precise physical bus timing.

The complete 58-cartridge/two-BIOS boundary sweep and its implications for replacement firmware and
BIOS-free startup are documented in
[`BIOS_REPLACEMENT_ANALYSIS.md`](BIOS_REPLACEMENT_ANALYSIS.md).

## Unknowns and non-claims

The current evidence does not establish:

- The hardware meaning of `$5900`.
- Complete semantics for every startup hardware-status branch.
- A public or supported BIOS API beyond the observed entry and interrupt callbacks.
- Whether cartridges call other BIOS helpers after `$6020`.
- Whether nonzero callback-flag values encode anything beyond the wrapper's boolean test.
- The purpose of opaque UMC-only regions replaced by fill in the BIT revision.
- Exact physical bus timing within each firmware instruction.

Those points should remain unknown rather than be inferred from convenient names in this pseudocode.

## Method and publication boundary

This specification derives from the local two-BIOS comparison in
[`BIOS_DIFFERENCE_ANALYSIS.md`](BIOS_DIFFERENCE_ANALYSIS.md), cartridge-header results in
[`ROM_HEADER_ANALYSIS.md`](ROM_HEADER_ANALYSIS.md), measured hardware behavior recorded in
[`VIDEO_HARDWARE.md`](VIDEO_HARDWARE.md), and controlled Superwhizkid execution. It preserves
behavioral inputs, outputs, conditions, and interoperability addresses while deliberately excluding:

- Firmware bytes, instruction listings, and binary patches.
- Original instruction ordering when ordering is not externally observable.
- Register allocation, private temporary variables, and reconstructable private RAM layouts.
- Delay-loop bodies, drawing primitives, startup graphics, and opaque data.
- A comprehensive source-like representation of either BIOS.

This boundary follows the distinction between functional behavior and program expression described
by 17 U.S.C. § 102(b) and the U.S. Copyright Office's guidance on computer programs. Section 1201(f)
also addresses reverse engineering and sharing information for interoperability under its stated
conditions. These references informed the documentation policy; they are not a legal determination
about any reader's firmware acquisition or use.

