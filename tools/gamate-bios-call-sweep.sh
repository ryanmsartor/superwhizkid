#!/usr/bin/env bash
# Copyright (c) 2026 Ry
# SPDX-License-Identifier: GPL-2.0-or-later

set -u

if (( $# < 1 || $# > 4 )); then
	echo "usage: $0 ROM_DIRECTORY [FRAMES=1000] [TRACER=build/gamatebioscalls] [REPORT]" >&2
	exit 2
fi

rom_directory=$1
frames=${2:-1000}
tracer=${3:-build/gamatebioscalls}
report=${4:-/tmp/superwhizkid-bios-calls.tsv}

if [[ ! -d $rom_directory || ! -x $tracer || ! $frames =~ ^[0-9]+$ || $frames == 0 ]]; then
	echo "invalid ROM directory, frame count, or tracer executable" >&2
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

temporary=$(mktemp /tmp/superwhizkid-bios-call-run-XXXXXX)
trap 'rm -f "$temporary"' EXIT
printf 'bios\tcartridge\tdirection\tkind\tsource\ttarget\topcode\tbank\tfirst_bank\tcount\tfirst_frame\n' > "$report" || exit 2

passes=0
failures=0
for bios in "${bioses[@]}"; do
	bios_name=${bios##*/}
	for cartridge in "${cartridges[@]}"; do
		cartridge_name=${cartridge##*/}
		if "$tracer" "$bios" "$cartridge" "$frames" > "$temporary"; then
			while IFS= read -r line; do
				[[ $line == direction$'\t'* || $line == \#* || -z $line ]] && continue
				printf '%s\t%s\t%s\n' "$bios_name" "$cartridge_name" "$line" >> "$report"
			done < "$temporary"
			((passes++))
		else
			((failures++))
			printf 'FAIL %-20s %s\n' "$bios_name" "$cartridge_name" >&2
		fi
	done
done

printf 'summary: %d passed, %d failed, %d total\n' "$passes" "$failures" "$((passes + failures))"
printf 'report: %s\n' "$report"
(( failures == 0 ))
