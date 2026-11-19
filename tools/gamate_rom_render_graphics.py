#!/usr/bin/env python3
# Copyright (c) 2026 Ry
#
# SPDX-License-Identifier: GPL-2.0-or-later

"""Render an entire Gamate ROM as assumed two-bitplane 8x8 graphics."""

import argparse
from pathlib import Path
import sys

try:
	from PIL import Image, ImageDraw, ImageFont
except ImportError:
	print("error: this tool requires Pillow (the PIL Python package)", file=sys.stderr)
	sys.exit(2)


BANK_SIZE = 0x4000
TILE_BYTES = 16
TILE_SIZE = 8
PALETTE = (
	(245, 245, 245),
	(178, 178, 178),
	(104, 104, 104),
	(24, 24, 24),
)
MISSING = (229, 89, 255)
BACKGROUND = (250, 250, 250)
TEXT = (24, 24, 24)
RULE = (120, 120, 120)


def parse_integer(value):
	try:
		return int(value, 0)
	except ValueError as error:
		raise argparse.ArgumentTypeError(f"invalid integer: {value}") from error


def parse_group(value):
	parts = value.lower().split("x")
	if len(parts) != 2:
		raise argparse.ArgumentTypeError("group must be WIDTHxHEIGHT")
	try:
		width, height = (int(part) for part in parts)
	except ValueError as error:
		raise argparse.ArgumentTypeError("group must be WIDTHxHEIGHT") from error
	if not 1 <= width <= 16 or not 1 <= height <= 16:
		raise argparse.ArgumentTypeError("group dimensions must be between 1 and 16 tiles")
	return width, height


def decode_tile(data, layout):
	"""Return 64 shade indices; absent bytes produce the missing-data shade."""
	pixels = []
	for row in range(TILE_SIZE):
		if layout == "planar":
			plane0_index = row
			plane1_index = TILE_SIZE + row
		else:
			plane0_index = row * 2
			plane1_index = row * 2 + 1
		if plane1_index >= len(data):
			pixels.extend([None] * TILE_SIZE)
			continue
		plane0 = data[plane0_index]
		plane1 = data[plane1_index]
		for bit in range(7, -1, -1):
			pixels.append(((plane0 >> bit) & 1) | (((plane1 >> bit) & 1) << 1))
	return pixels


def group_tile_index(x, y, width, height, order):
	if order == "column-major":
		return x * height + y
	return y * width + x


def render_group(data, offset, group, layout, order):
	group_width, group_height = group
	image = Image.new("RGB", (group_width * TILE_SIZE, group_height * TILE_SIZE), MISSING)
	pixels = image.load()
	for tile_y in range(group_height):
		for tile_x in range(group_width):
			tile_index = group_tile_index(tile_x, tile_y, group_width, group_height, order)
			start = offset + tile_index * TILE_BYTES
			tile = decode_tile(data[start:start + TILE_BYTES], layout)
			for y in range(TILE_SIZE):
				for x in range(TILE_SIZE):
					shade = tile[y * TILE_SIZE + x]
					pixels[tile_x * TILE_SIZE + x, tile_y * TILE_SIZE + y] = (
						MISSING if shade is None else PALETTE[shade])
	return image


def text_size(draw, text, font):
	left, top, right, bottom = draw.textbbox((0, 0), text, font=font)
	return right - left, bottom - top


def labels_need_rotation(draw, font, cell_width, gap, bytes_per_group, columns):
	last_label = f"+{(columns - 1) * bytes_per_group:04X}"
	label_width, _ = text_size(draw, last_label, font)
	return label_width > cell_width + gap - 2


def draw_column_label(canvas, draw, text, x, y, cell_width, axis_height, font, vertical):
	label_width, label_height = text_size(draw, text, font)
	if not vertical:
		draw.text((x + (cell_width - label_width) // 2, y), text, fill=TEXT, font=font)
		return
	label = Image.new("RGB", (label_width + 2, label_height + 2), BACKGROUND)
	ImageDraw.Draw(label).text((1, 1), text, fill=TEXT, font=font)
	label = label.rotate(90, expand=True)
	label_x = x + (cell_width - label.width) // 2
	label_y = y + axis_height - label.height
	canvas.paste(label, (label_x, label_y))


def render(data, start, end, layout, group, order, columns, scale, bank_size):
	group_width, group_height = group
	bytes_per_group = group_width * group_height * TILE_BYTES
	cell_width = group_width * TILE_SIZE * scale
	cell_height = group_height * TILE_SIZE * scale
	gap = max(1, scale)
	font = ImageFont.load_default()
	probe = Image.new("RGB", (1, 1))
	probe_draw = ImageDraw.Draw(probe)
	label_width, label_height = text_size(probe_draw, "$000000", font)
	left_margin = label_width + 10
	vertical_labels = labels_need_rotation(
		probe_draw, font, cell_width, gap, bytes_per_group, columns)
	column_label_width, _ = text_size(
		probe_draw, f"+{(columns - 1) * bytes_per_group:04X}", font)
	top_axis = (column_label_width if vertical_labels else label_height) + 8
	bank_header = label_height + 12
	bank_gap = 12

	sections = []
	bank_first = start // bank_size
	bank_last = (end - 1) // bank_size
	for bank in range(bank_first, bank_last + 1):
		section_start = max(start, bank * bank_size)
		section_end = min(end, (bank + 1) * bank_size)
		group_count = (section_end - section_start + bytes_per_group - 1) // bytes_per_group
		rows = (group_count + columns - 1) // columns
		section_height = bank_header + top_axis + rows * (cell_height + gap) - gap
		sections.append((bank, section_start, section_end, group_count, rows, section_height))

	canvas_width = left_margin + columns * (cell_width + gap) - gap + 8
	canvas_height = sum(section[5] for section in sections) + bank_gap * (len(sections) - 1) + 8
	canvas = Image.new("RGB", (canvas_width, canvas_height), BACKGROUND)
	draw = ImageDraw.Draw(canvas)
	y_origin = 4
	resampling = getattr(Image, "Resampling", Image).NEAREST

	for bank, section_start, section_end, group_count, rows, section_height in sections:
		header = (f"{layout}  bank {bank}  "
			f"ROM ${section_start:06X}-${section_end - 1:06X}")
		draw.text((4, y_origin), header, fill=TEXT, font=font)
		grid_y = y_origin + bank_header + top_axis
		for column in range(columns):
			offset = section_start + column * bytes_per_group
			if offset >= section_end:
				break
			label = f"+{column * bytes_per_group:04X}"
			x = left_margin + column * (cell_width + gap)
			draw_column_label(canvas, draw, label, x, y_origin + bank_header,
				cell_width, top_axis, font, vertical_labels)
		for row in range(rows):
			row_offset = section_start + row * columns * bytes_per_group
			draw.text((4, grid_y + row * (cell_height + gap) + 2),
				f"${row_offset:06X}", fill=TEXT, font=font)
		for index in range(group_count):
			row, column = divmod(index, columns)
			offset = section_start + index * bytes_per_group
			group_image = render_group(data, offset, group, layout, order)
			if scale != 1:
				group_image = group_image.resize((cell_width, cell_height), resampling)
			x = left_margin + column * (cell_width + gap)
			y = grid_y + row * (cell_height + gap)
			canvas.paste(group_image, (x, y))
		y_origin += section_height + bank_gap
		if bank != sections[-1][0]:
			draw.line((4, y_origin - bank_gap // 2, canvas_width - 5,
				y_origin - bank_gap // 2), fill=RULE)
	return canvas


def output_path_for_layout(base, layout, multiple):
	if not multiple:
		return base
	suffix = base.suffix or ".png"
	stem = base.name[:-len(suffix)] if base.name.endswith(suffix) else base.name
	return base.with_name(f"{stem}-{layout}{suffix}")


def main():
	parser = argparse.ArgumentParser(
		description="Render a complete ROM as assumed Gamate two-bitplane 8x8 graphics.")
	parser.add_argument("rom", type=Path, help="ROM dump to render")
	parser.add_argument("output", nargs="?", type=Path,
		help="output PNG (default: ROM name with -graphics.png)")
	parser.add_argument("--layout", choices=("planar", "interleaved", "both"), default="planar",
		help="plane-byte arrangement to assume (default: planar)")
	parser.add_argument("--group", type=parse_group, default=(2, 2), metavar="WIDTHxHEIGHT",
		help="tiles per visually separated group (default: 2x2)")
	parser.add_argument("--group-order", choices=("row-major", "column-major"),
		default="row-major", help="tile order inside each group (default: row-major)")
	parser.add_argument("--columns", type=int, default=32,
		help="groups per output row (default: 32)")
	parser.add_argument("--scale", type=int, default=2,
		help="nearest-neighbor pixel scale (default: 2)")
	parser.add_argument("--start", type=parse_integer, default=0,
		help="first ROM offset, accepting decimal or 0x notation")
	parser.add_argument("--end", type=parse_integer,
		help="exclusive final ROM offset (default: end of file)")
	parser.add_argument("--bank-size", type=parse_integer, default=BANK_SIZE,
		help="labeled section size (default: 0x4000)")
	args = parser.parse_args()

	if not args.rom.is_file():
		parser.error(f"ROM does not exist: {args.rom}")
	data = args.rom.read_bytes()
	end = len(data) if args.end is None else args.end
	if not 0 <= args.start < end <= len(data):
		parser.error(f"range must satisfy 0 <= start < end <= {len(data)}")
	if not 1 <= args.columns <= 256:
		parser.error("columns must be between 1 and 256")
	if not 1 <= args.scale <= 16:
		parser.error("scale must be between 1 and 16")
	if args.bank_size <= 0:
		parser.error("bank size must be positive")

	output = args.output or args.rom.with_name(f"{args.rom.stem}-graphics.png")
	layouts = ("planar", "interleaved") if args.layout == "both" else (args.layout,)
	for layout in layouts:
		path = output_path_for_layout(output, layout, len(layouts) > 1)
		path.parent.mkdir(parents=True, exist_ok=True)
		image = render(data, args.start, end, layout, args.group, args.group_order,
			args.columns, args.scale, args.bank_size)
		image.save(path, "PNG")
		print(f"{path}: {image.width}x{image.height}")
	return 0


if __name__ == "__main__":
	sys.exit(main())
