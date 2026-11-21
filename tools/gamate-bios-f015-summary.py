#!/usr/bin/env python3
# Copyright (c) 2026 Ry
# SPDX-License-Identifier: GPL-2.0-or-later

"""Validate the measured F015 two-plane copy contract in completed-call reports."""

import csv
import sys
from collections import Counter


def main() -> int:
    if len(sys.argv) < 2:
        print(f"usage: {sys.argv[0]} COMPLETED_CALL_REPORT [...]", file=sys.stderr)
        return 2

    calls = clean = interrupted = mismatches = 0
    dimensions = Counter()
    page_crossings = Counter()
    mismatch_reasons = Counter()
    bioses = Counter()
    for path in sys.argv[1:]:
        with open(path, newline="", encoding="utf-8") as report:
            for row in csv.DictReader(report, delimiter="\t"):
                if row["target"] != "F015":
                    continue
                calls += 1
                bioses[row["bios"]] += 1
                if int(row["irqs"]) or int(row["nmis"]):
                    interrupted += 1
                    continue
                clean += 1
                vram_writes = int(row["vram_writes"])
                lcd_writes = int(row["lcd_writes"])
                column_numerator = lcd_writes - 1 - vram_writes
                reasons = []
                if column_numerator < 4 or column_numerator % 4:
                    reasons.append("write-shape")
                    columns = rows = 0
                else:
                    columns = column_numerator // 4
                    if not columns or vram_writes % (2 * columns):
                        reasons.append("write-shape")
                        rows = 0
                    else:
                        rows = vram_writes // (2 * columns)
                        dimensions[(columns, rows)] += 1
                        baseline = 234 + columns * (132 + 76 * rows)
                        extra_clocks = int(row["clocks"]) - baseline
                        if extra_clocks < 0 or extra_clocks % 2 or extra_clocks > 2 * vram_writes:
                            reasons.append("timing")
                        else:
                            page_crossings[extra_clocks // 2] += 1
                        expected_y = f"{columns * rows & 0xFF:02X}"
                        if row["out_y"] != expected_y:
                            reasons.append("Y")
                if row["out_x"] != row["in_x"]:
                    reasons.append("X")
                expected_s = int(row["in_s"], 16)
                if row["opcode"] == "4C":
                    expected_s = (expected_s + 2) & 0xFF
                if int(row["out_s"], 16) != expected_s:
                    reasons.append("S")
                in_p, out_p = int(row["in_p"], 16), int(row["out_p"], 16)
                if (out_p & 0x4C) != (in_p & 0x4C) or out_p & 0x81 or not out_p & 0x02:
                    reasons.append("status")
                if any(int(row[field]) for field in
                       ("ay_writes", "bank_writes", "uart_clocks")):
                    reasons.append("unrelated-hardware")
                if reasons:
                    mismatches += 1
                    mismatch_reasons.update(reasons)

    print("metric\tvalue")
    print(f"calls\t{calls}")
    print(f"clean_calls\t{clean}")
    print(f"interrupted_calls\t{interrupted}")
    print(f"contract_mismatches\t{mismatches}")
    for bios, count in sorted(bioses.items()):
        print(f"bios:{bios}\t{count}")
    print("\ncolumns\trows\tcalls\tbaseline_clocks")
    for (columns, rows), count in sorted(dimensions.items()):
        clocks = 234 + columns * (132 + 76 * rows)
        print(f"{columns}\t{rows}\t{count}\t{clocks}")
    print("\npage_crossings\tcalls")
    for crossings, count in sorted(page_crossings.items()):
        print(f"{crossings}\t{count}")
    if mismatch_reasons:
        print("\nreason\tcalls")
        for reason, count in mismatch_reasons.most_common():
            print(f"{reason}\t{count}")
    return mismatches != 0


if __name__ == "__main__":
    raise SystemExit(main())
