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

Boot establishes a small cartridge-visible low-RAM ABI rather than an opaque RAM image. The
canonical byte-by-byte entry policy, authentic values, perturbation results, and cartridge consumers
are specified in [`BIOS_ENTRY_RAM_ANALYSIS.md`](BIOS_ENTRY_RAM_ANALYSIS.md). Replacement-firmware
startup requirements are summarized in
[`BIOS_REPLACEMENT_ANALYSIS.md`](BIOS_REPLACEMENT_ANALYSIS.md). This boot document intentionally
does not duplicate those tables.

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
    update BIOS-maintained timing counters

    if cartridge_callbacks_are_enabled:
        call cartridge IRQ callback at $6026

    return from interrupt
```

```text
on_link_receive_nmi:
    read the completed link byte into BIOS-managed RAM

    if cartridge_callbacks_are_enabled:
        call cartridge NMI callback at $6023

    return from interrupt
```

The cartridge callback returns to the BIOS wrapper as a subroutine; the wrapper performs the final
interrupt return. Commercial cartridges may make `$6023` an immediate no-op return when they do not
use link reception. The canonical callback enable rule, register effects, and service interactions
are specified in [`BIOS_RUNTIME_API_ANALYSIS.md`](BIOS_RUNTIME_API_ANALYSIS.md). This section records
only the boot document's high-level control flow.

### Boot-time runtime-ABI initialization

| State | Successful-boot action |
| --- | --- |
| Callback enable, RAM `$00C` | Set to `$01` immediately before cartridge entry. Zero disables callbacks; any nonzero value enables them. |
| Periodic counter, RAM `$00E-$011` | Maintained as a big-endian 32-bit IRQ counter and exposed at the boot-derived entry phase. |

The per-title access evidence and phase sensitivity are maintained in
[`BIOS_ENTRY_RAM_ANALYSIS.md`](BIOS_ENTRY_RAM_ANALYSIS.md) and the source record in
[`../../src/gamate/SOURCES.md`](../../src/gamate/SOURCES.md), rather than repeated here.

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
- Whether nonzero callback-flag values encode anything beyond the wrapper's boolean test.
- The purpose of opaque UMC-only regions replaced by fill in the BIT revision.
- Exact physical bus timing within each firmware instruction.

Those points should remain unknown rather than be inferred from convenient names in this pseudocode.

## Method and publication boundary

This specification derives from the local two-BIOS comparison in
[`BIOS_DIFFERENCE_ANALYSIS.md`](BIOS_DIFFERENCE_ANALYSIS.md), cartridge-header results in
[`ROM_HEADER_ANALYSIS.md`](../roms/ROM_HEADER_ANALYSIS.md), measured hardware behavior recorded in
[`VIDEO_HARDWARE.md`](../VIDEO_HARDWARE.md), and controlled Superwhizkid execution. It preserves
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

