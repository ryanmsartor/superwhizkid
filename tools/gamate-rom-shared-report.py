#!/usr/bin/env python3
# Copyright (c) 2026 Ry
#
# SPDX-License-Identifier: GPL-2.0-or-later

"""Find exact cross-title reuse candidates in the fixed Gamate ROM bank."""

import argparse
from collections import Counter, defaultdict
from pathlib import Path
import sys


COPYRIGHT = b"COPYRIGHT BIT CORPORATION"
FIXED_BANK_SIZE = 0x4000
WINDOW_SIZE = 32
MIN_DISTINCT_BYTES = 5
MAX_WINDOW_OWNERS = 8


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


def windows(data):
	result = defaultdict(list)
	for offset in range(len(data) - WINDOW_SIZE + 1):
		window = data[offset:offset + WINDOW_SIZE]
		if len(set(window)) >= MIN_DISTINCT_BYTES:
			result[window].append(offset)
	return result


def shared_spans(first, second):
	first_windows = windows(first)
	second_windows = windows(second)
	points = []
	for window, first_offsets in first_windows.items():
		second_offsets = second_windows.get(window)
		if len(first_offsets) == 1 and second_offsets and len(second_offsets) == 1:
			points.append((first_offsets[0] - second_offsets[0], first_offsets[0], second_offsets[0]))
	points.sort()

	spans = []
	for delta, first_offset, second_offset in points:
		if spans and spans[-1][0] == delta and spans[-1][2] + 1 == first_offset:
			old = spans[-1]
			spans[-1] = (delta, old[1], first_offset, old[3], second_offset)
		else:
			spans.append((delta, first_offset, first_offset, second_offset, second_offset))

	lengths = []
	for _, first_start, first_end, second_start, _ in spans:
		length = first_end - first_start + WINDOW_SIZE
		lengths.append((length, first_start, second_start))
	return sorted(lengths, reverse=True)


def main():
	parser = argparse.ArgumentParser(description=(
		"Print exact fixed-bank reuse candidates as TSV. Matches may be code or data."))
	parser.add_argument("inputs", nargs="+", help="cartridge files or directories to scan")
	parser.add_argument("--min-windows", type=int, default=64,
		help="minimum selectively shared 32-byte windows for a pair (default: 64)")
	args = parser.parse_args()

	try:
		files = cartridge_files(args.inputs)
	except FileNotFoundError as error:
		parser.error(f"input does not exist: {error.args[0]}")

	cartridges = []
	for path in files:
		data = path.read_bytes()
		if len(data) >= FIXED_BANK_SIZE and data[5:0x1E] == COPYRIGHT:
			cartridges.append((path.name, data[:FIXED_BANK_SIZE]))
	if not cartridges:
		print("no Gamate cartridge headers found", file=sys.stderr)
		return 1

	owners = defaultdict(set)
	for index, (_, data) in enumerate(cartridges):
		for window in windows(data):
			owners[window].add(index)

	pairs = Counter()
	for indices in owners.values():
		if len(indices) < 2 or len(indices) > MAX_WINDOW_OWNERS:
			continue
		ordered = sorted(indices)
		for first_position, first in enumerate(ordered):
			for second in ordered[first_position + 1:]:
				pairs[first, second] += 1

	print("\t".join((
		"file_a", "file_b", "selective_shared_32b_windows", "unique_span_count",
		"unique_span_bytes", "longest_unique_span", "longest_offset_a",
		"longest_offset_b")))
	for (first, second), count in pairs.most_common():
		if count < args.min_windows:
			break
		spans = shared_spans(cartridges[first][1], cartridges[second][1])
		longest = spans[0] if spans else (0, 0, 0)
		print("\t".join((
			cartridges[first][0], cartridges[second][0], str(count), str(len(spans)),
			str(sum(span[0] for span in spans)), str(longest[0]), f"0x{longest[1]:04X}",
			f"0x{longest[2]:04X}")))
	return 0


if __name__ == "__main__":
	sys.exit(main())
