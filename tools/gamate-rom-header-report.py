#!/usr/bin/env python3
# Copyright (c) 2026 Ry
#
# SPDX-License-Identifier: GPL-2.0-or-later

"""Report structural Gamate cartridge-header metadata without retaining ROM data."""

import argparse
import hashlib
from pathlib import Path
import sys


COPYRIGHT = b"COPYRIGHT BIT CORPORATION"
CHECKSUM_START = 0x1000
CHECKSUM_END = 0x2000


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


def describe_stub(data, offset):
	opcode = data[offset]
	if opcode == 0x4C:
		target = int.from_bytes(data[offset + 1:offset + 3], "little")
		return f"JMP_${target:04X}"
	if opcode == 0x60:
		return "RTS"
	return data[offset:offset + 3].hex().upper()


def header_offsets(data):
	offsets = []
	start = 0
	while True:
		offset = data.find(COPYRIGHT, start)
		if offset < 0:
			return offsets
		offsets.append(offset - 5)
		start = offset + 1


def report(path, data):
	stored = int.from_bytes(data[0:2], "little")
	calculated = sum(data[CHECKSUM_START:CHECKSUM_END]) & 0xFFFF
	offsets = ",".join(f"0x{offset:X}" for offset in header_offsets(data))
	return (
		path.name,
		str(len(data)),
		hashlib.sha1(data).hexdigest(),
		f"{stored:04X}",
		f"{calculated:04X}",
		"yes" if stored == calculated else "no",
		f"{data[2]:02X}",
		f"{data[3]:02X}",
		f"{data[4]:02X}",
		f"{data[0x1E]:02X}",
		f"{data[0x1F]:02X}",
		describe_stub(data, 0x20),
		describe_stub(data, 0x23),
		describe_stub(data, 0x26),
		offsets,
	)


def main():
	parser = argparse.ArgumentParser(
		description="Print a TSV inventory of Gamate cartridge header metadata.")
	parser.add_argument("inputs", nargs="+", help="cartridge files or directories to scan")
	args = parser.parse_args()

	print("\t".join((
		"file", "size", "sha1", "stored_sum", "calculated_sum", "sum_ok",
		"byte_02", "byte_03", "byte_04", "byte_1e", "byte_1f", "entry_6020",
		"nmi_6023", "irq_6026", "embedded_header_offsets")))
	found = 0
	try:
		files = cartridge_files(args.inputs)
	except FileNotFoundError as error:
		parser.error(f"input does not exist: {error.args[0]}")

	for path in files:
		data = path.read_bytes()
		if len(data) < CHECKSUM_END or data[5:0x1E] != COPYRIGHT:
			continue
		print("\t".join(report(path, data)))
		found += 1

	if not found:
		print("no Gamate cartridge headers found", file=sys.stderr)
		return 1
	return 0


if __name__ == "__main__":
	sys.exit(main())
