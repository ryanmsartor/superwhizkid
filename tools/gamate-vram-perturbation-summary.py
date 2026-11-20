#!/usr/bin/env python3
# Copyright (c) 2026 Ry
# SPDX-License-Identifier: GPL-2.0-or-later

"""Compare gamateboot reports from entry-VRAM perturbation runs."""

import re
import sys


def parse(path):
    cases = {}
    current = None
    with open(path, encoding="utf-8") as report:
        for line in report:
            line = line.rstrip("\n")
            if line.startswith("=== "):
                parts = line.split(" | ")
                current = (parts[1], parts[2].removesuffix(" ==="))
                cases[current] = {"status": parts[0][4:]}
            elif current is not None:
                if line.startswith("frame 1000:"):
                    cases[current]["frame"] = line
                elif line.startswith("post-entry:"):
                    cases[current]["post"] = line
                elif line.startswith("AY registers:"):
                    cases[current]["ay"] = line
                elif line.startswith("AY writes/register:"):
                    cases[current]["ay_writes"] = line
                elif line.startswith("AY active samples:"):
                    cases[current]["ay_active"] = line
                elif line.startswith("controller reads:"):
                    cases[current]["controller"] = line
                elif line.startswith("first events:"):
                    cases[current]["first"] = line
                elif line.startswith("final state:"):
                    cases[current]["final"] = line
                elif line.startswith("VRAM coverage:"):
                    cases[current]["coverage"] = line
    return cases


def execution_signature(case):
    frame = re.sub(r"video=[0-9A-F]+ ", "", case["frame"])
    post = re.sub(r"nonblank=\d+/\d+, ", "", case["post"])
    final = re.sub(r"VRAM=[0-9A-F]+ ", "", case["final"])
    return tuple([frame, post, final] + [case[name] for name in (
        "ay", "ay_writes", "ay_active", "controller", "first")])


def value(line, name, base=10):
    match = re.search(rf"{re.escape(name)}=([0-9A-F]+)", line)
    return int(match.group(1), base) if match else 0


def main():
    if len(sys.argv) < 3 or any("=" not in argument for argument in sys.argv[1:]):
        print(f"usage: {sys.argv[0]} NAME=REPORT NAME=REPORT [...]", file=sys.stderr)
        return 2
    reports = {name: parse(path) for name, path in
               (argument.split("=", 1) for argument in sys.argv[1:])}
    names = list(reports)
    reference = reports[names[0]]
    if any(set(report) != set(reference) for report in reports.values()):
        print("reports do not contain the same cases", file=sys.stderr)
        return 2

    execution_differences = []
    video_differences = []
    vram_differences = []
    cpu_read_before_write = []
    for key in reference:
        cases = [reports[name][key] for name in names]
        if len({execution_signature(case) for case in cases}) != 1:
            execution_differences.append(key)
        if len({value(case["frame"], "video", 16) for case in cases}) != 1:
            video_differences.append(key)
        if len({value(case["final"], "VRAM", 16) for case in cases}) != 1:
            vram_differences.append(key)
        coverage = reference[key]["coverage"]
        reads = value(coverage, "read-before-write")
        unique_match = re.search(r"read-before-write=\d+/(\d+)", coverage)
        unique = int(unique_match.group(1)) if unique_match else 0
        if reads:
            cpu_read_before_write.append((key, reads, unique))

    print("metric\tcount")
    print(f"cases\t{len(reference)}")
    failed = sum(any(report[key]["status"] != "PASS" for report in reports.values())
                 for key in reference)
    print(f"failed_cases\t{failed}")
    print(f"execution_differences\t{len(execution_differences)}")
    print(f"final_video_differences\t{len(video_differences)}")
    print(f"final_vram_differences\t{len(vram_differences)}")
    print(f"cpu_read_before_write_cases\t{len(cpu_read_before_write)}")
    print("\n# CPU read-before-write cases")
    print("bios\tcartridge\treads\tunique_bytes")
    for (bios, cartridge), reads, unique in sorted(cpu_read_before_write):
        print(f"{bios}\t{cartridge}\t{reads}\t{unique}")
    if execution_differences:
        print("\n# Execution differences")
        for bios, cartridge in execution_differences:
            print(f"{bios}\t{cartridge}")
    if video_differences:
        print("\n# Final-video differences")
        for bios, cartridge in video_differences:
            print(f"{bios}\t{cartridge}")
    return 1 if execution_differences else 0


if __name__ == "__main__":
    raise SystemExit(main())
