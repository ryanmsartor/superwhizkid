#!/usr/bin/env python3
# Copyright (c) 2026 Ry
#
# SPDX-License-Identifier: GPL-2.0-or-later

"""Report padding and repeated aligned regions in Gamate cartridge dumps."""

import argparse
import hashlib
from pathlib import Path
import sys


COPYRIGHT = b"COPYRIGHT BIT CORPORATION"
BANK_SIZE = 0x4000
FOUR_IN_ONE_SHA1 = "4dd20632eb31023e18241290eca7d5a5acb1fa84"


def cartridge_files(inputs):
	files = []
	for value in inputs:
		path = Path(value)
		if path.is_dir():
			files.extend(candidate for candidate in path.iterdir() if candidate.is_file())
		elif path.is_file():
			files.append(path)
		else:
			raise FileNotFoundError(value)
	return sorted(files, key=lambda path: path.name.casefold())


def trailing_fill(data):
	value = data[-1]
	if value not in (0x00, 0xFF):
		return "-", 0
	length = 1
	while length < len(data) and data[-1 - length] == value:
		length += 1
	return f"{value:02X}", length


def chunks(data, size):
	return [data[offset:offset + size] for offset in range(0, len(data), size)]


def uniform_banks(data):
	result = []
	for index, bank in enumerate(chunks(data, BANK_SIZE)):
		if len(bank) == BANK_SIZE and len(set(bank)) == 1:
			result.append(f"{index}:{bank[0]:02X}")
	return ",".join(result) or "-"


def duplicate_groups(data, size):
	groups = {}
	for index, block in enumerate(chunks(data, size)):
		if len(block) != size or len(set(block)) == 1:
			continue
		digest = hashlib.sha1(block).digest()
		groups.setdefault(digest, []).append(index)
	duplicates = (indices for indices in groups.values() if len(indices) > 1)
	return ";".join(",".join(str(index) for index in indices) for indices in duplicates) or "-"


def report(path, data):
	digest = hashlib.sha1(data).hexdigest()
	fill_byte, fill_length = trailing_fill(data)
	half_mirror = len(data) % 2 == 0 and data[:len(data) // 2] == data[len(data) // 2:]
	mapper = "4-in-1" if digest == FOUR_IN_ONE_SHA1 else "banked" if len(data) > 0x8000 else "plain"
	padding = f"{fill_byte}:{fill_length}" if fill_length >= 16 else "none"
	return (
		path.name,
		str(len(data)),
		digest,
		mapper,
		padding,
		fill_byte,
		str(fill_length),
		uniform_banks(data),
		duplicate_groups(data, 0x10000),
		duplicate_groups(data, 0x8000),
		duplicate_groups(data, BANK_SIZE),
		"yes" if half_mirror else "no",
	)


def main():
	parser = argparse.ArgumentParser(
		description="Print a TSV report of Gamate ROM padding and aligned repetition.")
	parser.add_argument("inputs", nargs="+", help="cartridge files or directories to scan")
	args = parser.parse_args()

	print("\t".join((
		"file", "size", "sha1", "mapper_policy", "padding_convention",
		"trailing_fill_byte", "trailing_fill_length", "uniform_16k_banks",
		"duplicate_64k_blocks", "duplicate_32k_blocks", "duplicate_16k_banks",
		"identical_halves")))
	found = 0
	try:
		files = cartridge_files(args.inputs)
	except FileNotFoundError as error:
		parser.error(f"input does not exist: {error.args[0]}")

	for path in files:
		data = path.read_bytes()
		if len(data) < 0x2000 or data[5:0x1E] != COPYRIGHT:
			continue
		print("\t".join(report(path, data)))
		found += 1

	if not found:
		print("no Gamate cartridge headers found", file=sys.stderr)
		return 1
	return 0


if __name__ == "__main__":
	sys.exit(main())
