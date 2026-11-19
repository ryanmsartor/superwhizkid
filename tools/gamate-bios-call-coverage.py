#!/usr/bin/env python3
# Copyright (c) 2026 Ry
# SPDX-License-Identifier: GPL-2.0-or-later

"""Compare derived BIOS-call coverage reports without inspecting ROM or BIOS bytes."""

import csv
import sys
from collections import Counter


EVENT_FIELDS = (
    "cartridge", "direction", "kind", "source", "target", "opcode", "bank", "first_bank",
)
def rows(path, fields):
    with open(path, newline="", encoding="utf-8") as report:
        for row in csv.DictReader(report, delimiter="\t"):
            if row.get("bios") == "bios":
                continue
            yield tuple(row[field] for field in fields)


def call_rows(path):
    fields = (
        "cartridge", "source", "target", "opcode", "bank", "first_bank",
        "in_a", "in_x", "in_y", "in_s", "in_p",
        "out_a", "out_x", "out_y", "out_s", "out_p",
    )
    with open(path, newline="", encoding="utf-8") as report:
        for row in csv.DictReader(report, delimiter="\t"):
            if row.get("bios") == "bios":
                continue
            hardware = any(int(row[field]) != 0 for field in (
                "ay_writes", "lcd_writes", "vram_writes", "bank_writes", "uart_clocks"))
            interrupted = int(row["irqs"]) != 0 or int(row["nmis"]) != 0
            yield tuple(row[field] for field in fields) + (
                int(row["ram_changed"]) != 0,
                int(row["vram_changed"]) != 0,
                row["ay_changed_mask"] != "0000",
                hardware,
                interrupted,
            )


def main() -> int:
    if len(sys.argv) != 5:
        print(f"usage: {sys.argv[0]} BASE_EVENTS BASE_CALLS CANDIDATE_EVENTS CANDIDATE_CALLS",
              file=sys.stderr)
        return 2

    base_events = set(rows(sys.argv[1], EVENT_FIELDS))
    base_calls = set(call_rows(sys.argv[2]))
    candidate_events = set(rows(sys.argv[3], EVENT_FIELDS))
    candidate_calls = set(call_rows(sys.argv[4]))
    new_events = candidate_events - base_events
    new_calls = candidate_calls - base_calls
    def abi_signature(call):
        return (call[2], call[3], call[6], call[7], call[8], call[10],
                call[11], call[12], call[13], call[15], *call[16:20])

    base_abi = {abi_signature(call) for call in base_calls}
    candidate_abi = {abi_signature(call) for call in candidate_calls}
    new_abi = candidate_abi - base_abi
    def service_call(event):
        return (event[1] == "cartridge_to_bios" and event[2] in ("jsr", "jmp") and
                0xF000 <= int(event[4], 16) <= 0xF033 and
                (int(event[4], 16) - 0xF000) % 3 == 0)

    base_slots = {event[4] for event in base_events if service_call(event)}
    candidate_slots = {event[4] for event in candidate_events if service_call(event)}
    base_sources = {(event[0], event[3], event[4], event[6], event[7]) for event in base_events
                    if service_call(event)}
    candidate_sources = {(event[0], event[3], event[4], event[6], event[7])
                         for event in candidate_events if service_call(event)}

    print("metric\tbaseline\tcandidate\tnew")
    print(f"boundary_events\t{len(base_events)}\t{len(candidate_events)}\t{len(new_events)}")
    print(f"service_slots\t{len(base_slots)}\t{len(candidate_slots)}\t"
          f"{len(candidate_slots - base_slots)}")
    print(f"service_callsites\t{len(base_sources)}\t{len(candidate_sources)}\t"
          f"{len(candidate_sources - base_sources)}")
    print(f"callsite_signatures\t{len(base_calls)}\t{len(candidate_calls)}\t{len(new_calls)}")
    print(f"abi_signatures\t{len(base_abi)}\t{len(candidate_abi)}\t{len(new_abi)}")
    for slot in sorted(candidate_slots - base_slots):
        print(f"new_service_slot\t{slot}\t\t")
    event_titles = Counter(event[0] for event in new_events)
    call_titles = Counter(call[0] for call in new_calls)
    for cartridge in sorted(set(event_titles) | set(call_titles)):
        print(f"cartridge_additions\t{cartridge}\t{event_titles[cartridge]}\t"
              f"{call_titles[cartridge]}")
    return 0 if candidate_sources - base_sources or new_abi else 1


if __name__ == "__main__":
    raise SystemExit(main())
