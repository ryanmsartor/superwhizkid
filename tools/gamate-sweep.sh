#!/usr/bin/env bash
# Copyright (c) 2026 Ry
# SPDX-License-Identifier: GPL-2.0-or-later

set -u

if (( $# < 1 || $# > 4 )); then
	echo "usage: $0 ROM_DIRECTORY [FRAMES=1000] [GAMATEBOOT=build/gamateboot] [REPORT]" >&2
	exit 2
fi

rom_directory=$1
frames=${2:-1000}
runner=${3:-build/gamateboot}
report=${4:-/tmp/superwhizkid-sweep.txt}

if [[ ! -d $rom_directory || ! -x $runner || ! $frames =~ ^[0-9]+$ || $frames == 0 ]]; then
	echo "invalid ROM directory, frame count, or gamateboot executable" >&2
	exit 2
fi

bioses=("$rom_directory/gamate_bios_umc.bin" "$rom_directory/gamate_bios_bit.bin")
for bios in "${bioses[@]}"; do
	if [[ ! -f $bios ]]; then
		echo "missing required local validation input: $bios" >&2
		exit 2
	fi
done

mapfile -d '' cartridges < <(find "$rom_directory" -maxdepth 1 -type f -name '*.bin' \
	! -name 'gamate_bios_umc.bin' ! -name 'gamate_bios_bit.bin' -print0 | sort -z)
if (( ${#cartridges[@]} == 0 )); then
	echo "no .bin cartridges found in $rom_directory" >&2
	exit 2
fi

temporary=$(mktemp /tmp/superwhizkid-sweep-run-XXXXXX)
trap 'rm -f "$temporary"' EXIT
: > "$report" || exit 2
printf 'superwhizkid local compatibility sweep\nframes-after-entry: %s\ncartridges: %s\nbioses: %s\n\n' \
	"$frames" "${#cartridges[@]}" "${#bioses[@]}" >> "$report"

passes=0
failures=0
for bios in "${bioses[@]}"; do
	for cartridge in "${cartridges[@]}"; do
		bios_name=${bios##*/}
		cartridge_name=${cartridge##*/}
		if "$runner" "$bios" "$cartridge" "$frames" > "$temporary" 2>&1; then
			status=PASS
			((passes++))
		else
			status=FAIL
			((failures++))
		fi
		printf '=== %s | %s | %s ===\n' "$status" "$bios_name" "$cartridge_name" >> "$report"
		cat "$temporary" >> "$report"
		printf '\n' >> "$report"
		printf '%-4s %-20s %s\n' "$status" "$bios_name" "$cartridge_name"
	done
done

printf '\nsummary: %d passed, %d failed, %d total\n' "$passes" "$failures" "$((passes + failures))" | tee -a "$report"
printf 'report: %s\n' "$report"
(( failures == 0 ))
