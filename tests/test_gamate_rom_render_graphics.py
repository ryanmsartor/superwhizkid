#!/usr/bin/env python3
# Copyright (c) 2026 Ry
#
# SPDX-License-Identifier: GPL-2.0-or-later

"""Focused tests for the exploratory Gamate ROM graphics renderer."""

import importlib.util
from pathlib import Path
import unittest


TOOL = Path(__file__).parents[1] / "tools" / "gamate_rom_render_graphics.py"
SPEC = importlib.util.spec_from_file_location("gamate_rom_render_graphics", TOOL)
MODULE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MODULE)


class GraphicsRendererTests(unittest.TestCase):
	def test_planar_decoding_is_msb_first_and_weights_plane_one_twice(self):
		data = bytes([0x80] + [0] * 7 + [0x40] + [0] * 7)
		pixels = MODULE.decode_tile(data, "planar")
		self.assertEqual(pixels[:8], [1, 2, 0, 0, 0, 0, 0, 0])

	def test_interleaved_decoding_uses_a_pair_per_row(self):
		data = bytes([0x80, 0x40] + [0] * 14)
		pixels = MODULE.decode_tile(data, "interleaved")
		self.assertEqual(pixels[:8], [1, 2, 0, 0, 0, 0, 0, 0])
		self.assertEqual(pixels[8:16], [0] * 8)

	def test_partial_tile_is_marked_missing(self):
		pixels = MODULE.decode_tile(bytes(15), "planar")
		self.assertEqual(pixels[:56], [0] * 56)
		self.assertEqual(pixels[56:], [None] * 8)

	def test_group_order(self):
		self.assertEqual(MODULE.group_tile_index(1, 0, 2, 3, "row-major"), 1)
		self.assertEqual(MODULE.group_tile_index(1, 0, 2, 3, "column-major"), 3)

	def test_narrow_columns_rotate_axis_labels(self):
		image = MODULE.Image.new("RGB", (1, 1))
		draw = MODULE.ImageDraw.Draw(image)
		font = MODULE.ImageFont.load_default()
		self.assertTrue(MODULE.labels_need_rotation(draw, font, 16, 2, 16, 32))
		self.assertFalse(MODULE.labels_need_rotation(draw, font, 32, 2, 64, 32))


if __name__ == "__main__":
	unittest.main()
