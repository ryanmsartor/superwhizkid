#!/usr/bin/env python3
# Copyright (c) 2026 Ry
# SPDX-License-Identifier: GPL-2.0-or-later

"""Summarize a local completed-call TSV without retaining memory or firmware bytes."""

import csv
import sys
from collections import Counter, defaultdict


def main() -> int:
    if len(sys.argv) not in (2, 3):
        print(f"usage: {sys.argv[0]} COMPLETED_CALL_REPORT [SERVICE_SLOT]", file=sys.stderr)
        return 2

    selected = sys.argv[2].upper().removeprefix("$") if len(sys.argv) == 3 else None
    slots = defaultdict(lambda: {
        "calls": 0, "bioses": defaultdict(int), "min_clocks": None, "max_clocks": 0,
        "ram": 0, "vram": 0, "ay": 0, "hardware": 0, "interrupt": 0,
    })
    signatures = Counter()
    selected_rows = []
    with open(sys.argv[1], newline="", encoding="utf-8") as report:
        for row in csv.DictReader(report, delimiter="\t"):
            slot = slots[row["target"]]
            clocks = int(row["clocks"])
            slot["calls"] += 1
            slot["bioses"][row["bios"]] += 1
            slot["min_clocks"] = clocks if slot["min_clocks"] is None else min(
                slot["min_clocks"], clocks)
            slot["max_clocks"] = max(slot["max_clocks"], clocks)
            slot["ram"] += int(row["ram_changed"]) != 0
            slot["vram"] += int(row["vram_changed"]) != 0
            slot["ay"] += row["ay_changed_mask"] != "0000"
            slot["hardware"] += any(int(row[field]) != 0 for field in (
                "ay_writes", "lcd_writes", "vram_writes", "bank_writes", "uart_clocks"))
            slot["interrupt"] += int(row["irqs"]) != 0 or int(row["nmis"]) != 0
            if selected == row["target"]:
                selected_rows.append(row)
                signature = (
                    row["in_a"], row["in_x"], row["in_y"], row["in_s"], row["in_p"],
                    row["out_a"], row["out_x"], row["out_y"], row["out_s"], row["out_p"],
                    int(row["ram_changed"]) != 0, int(row["vram_changed"]) != 0,
                    row["ay_changed_mask"] != "0000",
                    any(int(row[field]) != 0 for field in (
                        "ay_writes", "lcd_writes", "vram_writes", "bank_writes", "uart_clocks")),
                    int(row["irqs"]) != 0 or int(row["nmis"]) != 0,
                )
                signatures[signature] += 1

    bios_names = sorted({name for slot in slots.values() for name in slot["bioses"]})
    print("slot\tcalls\t" + "\t".join(bios_names) +
          "\tmin_clocks\tmax_clocks\tram_calls\tvram_calls\tay_calls\t"
          "hardware_calls\tinterrupted_calls")
    for target in sorted(slots):
        slot = slots[target]
        values = [target, str(slot["calls"])]
        values.extend(str(slot["bioses"][name]) for name in bios_names)
        values.extend(str(slot[field]) for field in (
            "min_clocks", "max_clocks", "ram", "vram", "ay", "hardware", "interrupt"))
        print("\t".join(values))
    if selected is not None:
        print("\n# selected-slot preservation")
        print("field\tpreserved\tcalls")
        for field in ("a", "x", "y", "s", "p"):
            preserved = sum(row[f"in_{field}"] == row[f"out_{field}"]
                            for row in selected_rows)
            print(f"{field.upper()}\t{preserved}\t{len(selected_rows)}")

        print("\n# selected-slot status behavior")
        print("property\tmatches\tcalls")
        for name, mask in (("C preserved", 0x01), ("Z preserved", 0x02),
                           ("I preserved", 0x04), ("D preserved", 0x08),
                           ("V preserved", 0x40), ("N preserved", 0x80)):
            matches = sum((int(row["in_p"], 16) & mask) ==
                          (int(row["out_p"], 16) & mask) for row in selected_rows)
            print(f"{name}\t{matches}\t{len(selected_rows)}")
        zero_matches = sum(bool(int(row["out_p"], 16) & 0x02) ==
                           (int(row["out_a"], 16) == 0) for row in selected_rows)
        negative_matches = sum(bool(int(row["out_p"], 16) & 0x80) ==
                               bool(int(row["out_a"], 16) & 0x80)
                               for row in selected_rows)
        print(f"Z reflects output A\t{zero_matches}\t{len(selected_rows)}")
        print(f"N reflects output A\t{negative_matches}\t{len(selected_rows)}")

        print("\n# selected-slot input-A timing")
        print("in_a\tcalls\tmin_clocks\tmax_clocks\tinterrupted")
        by_a = defaultdict(list)
        for row in selected_rows:
            by_a[row["in_a"]].append(row)
        for value, rows in sorted(by_a.items()):
            clocks = [int(row["clocks"]) for row in rows]
            interrupted = sum(int(row["irqs"]) != 0 or int(row["nmis"]) != 0 for row in rows)
            print(f"{value}\t{len(rows)}\t{min(clocks)}\t{max(clocks)}\t{interrupted}")

        if selected_rows and "in_vram_address" in selected_rows[0]:
            transitions = Counter((
                row["in_vram_address"], row["out_vram_address"],
                row["in_scroll_x"], row["out_scroll_x"],
                row["in_scroll_y"], row["out_scroll_y"],
                row["in_lcd_flags"], row["out_lcd_flags"],
                row["in_ay_address"], row["out_ay_address"],
                row["in_bank"], row["out_bank"],
            ) for row in selected_rows)
            print("\n# selected-slot hardware-state transitions")
            print("calls\tin_vram\tout_vram\tin_scroll_x\tout_scroll_x\t"
                  "in_scroll_y\tout_scroll_y\tin_lcd\tout_lcd\tin_ay\tout_ay\t"
                  "in_bank\tout_bank")
            for transition, count in transitions.most_common(32):
                print("\t".join([str(count), *transition]))

        print("\n# selected-slot register/effect signatures")
        print("calls\tin_a\tin_x\tin_y\tin_s\tin_p\tout_a\tout_x\tout_y\tout_s\t"
              "out_p\tram\tvram\tay\thardware\tinterrupt")
        for signature, count in signatures.most_common():
            print("\t".join([str(count), *map(str, signature)]))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
