#!/usr/bin/env python3
# Copyright (c) 2026 Ry
# SPDX-License-Identifier: GPL-2.0-or-later

"""Validate the measured F012 fixed-height tile-run contract."""

import csv
import sys
from collections import Counter


def main() -> int:
    if len(sys.argv) < 2:
        print(f"usage: {sys.argv[0]} COMPLETED_CALL_REPORT [...]", file=sys.stderr)
        return 2

    calls = clean = interrupted = mismatches = 0
    widths = Counter()
    written_rows = Counter()
    clocks = {}
    mismatch_reasons = Counter()
    bioses = Counter()
    for path in sys.argv[1:]:
        with open(path, newline="", encoding="utf-8") as report:
            for row in csv.DictReader(report, delimiter="\t"):
                if row["target"] != "F012":
                    continue
                calls += 1
                bioses[row["bios"]] += 1
                if int(row["irqs"]) or int(row["nmis"]):
                    interrupted += 1
                    continue
                clean += 1
                lcd_writes = int(row["lcd_writes"])
                vram_writes = int(row["vram_writes"])
                reasons = []
                descriptor_writes = lcd_writes - vram_writes
                if descriptor_writes < 3 or descriptor_writes % 3:
                    reasons.append("write-shape")
                    width = 0
                else:
                    width = descriptor_writes // 3
                    widths[width] += 1
                    written_rows[(width, vram_writes)] += 1
                    if vram_writes != 8 * width or lcd_writes != 11 * width:
                        reasons.append("write-shape")
                    clock_range = clocks.setdefault(width, [int(row["clocks"])] * 2)
                    clock_range[0] = min(clock_range[0], int(row["clocks"]))
                    clock_range[1] = max(clock_range[1], int(row["clocks"]))
                    expected = f"{width & 0xFF:02X}"
                    if row["out_a"] != expected or row["out_y"] != expected:
                        reasons.append("A/Y")
                if int(row["out_x"], 16) & 7:
                    reasons.append("X-alignment")
                if row["out_s"] != row["in_s"]:
                    reasons.append("S")
                in_p, out_p = int(row["in_p"], 16), int(row["out_p"], 16)
                if (out_p & 0x0C) != (in_p & 0x0C) or out_p & 0x80 or not out_p & 0x02:
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
    print("\nwidth_tiles\tcalls\tminimum_clocks\tmaximum_clocks")
    for width, count in sorted(widths.items()):
        print(f"{width}\t{count}\t{clocks[width][0]}\t{clocks[width][1]}")
    print("\nwidth_tiles\twritten_rows\tcalls")
    for (width, rows), count in sorted(written_rows.items()):
        print(f"{width}\t{rows}\t{count}")
    if mismatch_reasons:
        print("\nreason\tcalls")
        for reason, count in mismatch_reasons.most_common():
            print(f"{reason}\t{count}")
    return mismatches != 0


if __name__ == "__main__":
    raise SystemExit(main())
