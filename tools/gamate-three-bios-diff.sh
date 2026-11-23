#!/usr/bin/env bash
# Copyright (c) 2026 Ry
# SPDX-License-Identifier: GPL-2.0-or-later

set -euo pipefail

if (( $# < 2 || $# > 3 )); then
	echo "usage: $0 ROM CORE [BIOS_DIRECTORY]" >&2
	exit 2
fi

rom=$1
core=$2
bios_directory=${3:-$(dirname "$rom")}
script_directory=${0%/*}

python3 "$script_directory/gamate-three-bios-diff.py" \
	"$rom" \
	--core "$core" \
	--bios-dir "$bios_directory"
