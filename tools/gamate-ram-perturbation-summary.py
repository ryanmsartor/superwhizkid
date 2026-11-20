#!/usr/bin/env python3
# Copyright (c) 2026 Ry
# SPDX-License-Identifier: GPL-2.0-or-later

"""Compare gamateboot reports from entry-RAM perturbation runs."""

import re
import sys


FIELDS = ("frame", "post", "ay", "ay_writes", "ay_active", "controller", "first", "final")


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
                elif line.startswith("RAM coverage:"):
                    cases[current]["coverage"] = line
                elif line.startswith("RAM read-before-write addresses (cartridge):"):
                    cases[current]["cartridge_addresses"] = line.split(":", 1)[1].split()
                elif line.startswith("RAM read-before-write addresses (BIOS):"):
                    cases[current]["bios_addresses"] = line.split(":", 1)[1].split()
    return cases


def execution_signature(case):
    values = [case[name] for name in FIELDS]
    values[-1] = re.sub(r"RAM=[0-9A-F]+ ", "", values[-1])
    return tuple(values)


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
    if any(any(field not in case for field in FIELDS) for case in reference.values()):
        print("reference report contains incomplete cases", file=sys.stderr)
        return 2

    print("policy\tfailed\texecution_differences\tvideo_differences\tvram_differences")
    all_differences = {}
    for name in names:
        failed = 0
        differences = []
        video = 0
        vram = 0
        for key, reference_case in reference.items():
            case = reports[name][key]
            failed += case["status"] != "PASS"
            if execution_signature(case) != execution_signature(reference_case):
                differences.append(key)
            video += value(case["frame"], "video", 16) != value(reference_case["frame"], "video", 16)
            vram += value(case["final"], "VRAM", 16) != value(reference_case["final"], "VRAM", 16)
        all_differences[name] = differences
        print(f"{name}\t{failed}\t{len(differences)}\t{video}\t{vram}")

    print("\n# Execution differences from " + names[0])
    for name in names[1:]:
        print(f"\n## {name}")
        if not all_differences[name]:
            print("none")
        for bios, cartridge in all_differences[name]:
            print(f"{bios}\t{cartridge}")

    frequencies = {"cartridge": {}, "BIOS": {}}
    for case in reference.values():
        for owner, field in (("cartridge", "cartridge_addresses"), ("BIOS", "bios_addresses")):
            for address in case.get(field, []):
                frequencies[owner][address] = frequencies[owner].get(address, 0) + 1
    print("\n# Read-before-write address frequency in reference")
    print("owner\taddress\tcases")
    for owner in frequencies:
        for address, count in sorted(frequencies[owner].items()):
            print(f"{owner}\t{address}\t{count}")
    return 1 if any(all_differences[name] for name in names[1:]) else 0


if __name__ == "__main__":
    raise SystemExit(main())
