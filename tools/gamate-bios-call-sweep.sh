#!/usr/bin/env bash
# Copyright (c) 2026 Ry
# SPDX-License-Identifier: GPL-2.0-or-later

set -u

if (( $# < 1 || $# > 5 )); then
	echo "usage: $0 ROM_DIRECTORY [FRAMES=1000] [TRACER=build/gamatebioscalls] [REPORT] [COMPLETED_CALL_REPORT]" >&2
	exit 2
fi

rom_directory=$1
frames=${2:-1000}
tracer=${3:-build/gamatebioscalls}
report=${4:-/tmp/superwhizkid-bios-calls.tsv}
completed_report=${5:-}

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
temporary_completed=$(mktemp /tmp/superwhizkid-bios-call-completed-XXXXXX)
trap 'rm -f "$temporary" "$temporary_completed"' EXIT
printf 'bios\tcartridge\tdirection\tkind\tsource\ttarget\topcode\tbank\tfirst_bank\tcount\tfirst_frame\n' > "$report" || exit 2
if [[ -n $completed_report ]]; then
	printf 'bios\tcartridge\tsource\ttarget\texpected_return\topcode\tbank\tfirst_bank\tentry_frame\treturn_frame\tentry_clock\treturn_clock\tin_wait_counter\tin_irq_clock\tin_irq_pending\tin_a\tin_x\tin_y\tin_s\tin_p\tout_a\tout_x\tout_y\tout_s\tout_p\tin_vram_address\tin_scroll_x\tin_scroll_y\tin_lcd_flags\tin_ay_address\tin_bank\tin_first_bank\tout_vram_address\tout_scroll_x\tout_scroll_y\tout_lcd_flags\tout_ay_address\tout_bank\tout_first_bank\tclocks\tram_changed\tram_first\tram_last\tvram_changed\tvram_first\tvram_last\tay_changed_mask\tay_writes\tlcd_writes\tvram_writes\tbank_writes\tuart_clocks\tirqs\tnmis\n' > "$completed_report" || exit 2
fi

passes=0
failures=0
for bios in "${bioses[@]}"; do
	bios_name=${bios##*/}
	for cartridge in "${cartridges[@]}"; do
		cartridge_name=${cartridge##*/}
		tracer_arguments=("$bios" "$cartridge" "$frames")
		[[ -n $completed_report ]] && tracer_arguments+=("$temporary_completed")
		if "$tracer" "${tracer_arguments[@]}" > "$temporary"; then
			while IFS= read -r line; do
				[[ $line == direction$'\t'* || $line == \#* || -z $line ]] && continue
				printf '%s\t%s\t%s\n' "$bios_name" "$cartridge_name" "$line" >> "$report"
			done < "$temporary"
			if [[ -n $completed_report ]]; then
				while IFS= read -r line; do
					[[ $line == source$'\t'* || -z $line ]] && continue
					printf '%s\t%s\t%s\n' "$bios_name" "$cartridge_name" "$line" >> "$completed_report"
				done < "$temporary_completed"
			fi
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
