#!/usr/bin/env bash
# Copyright (c) 2026 Ry
# SPDX-License-Identifier: GPL-2.0-or-later

set -u

if (( $# < 2 || $# > 4 )); then
	echo "usage: $0 ROM_DIRECTORY OUTPUT_DIRECTORY [FRAMES=1000] [TRACER=build/gamatebioscalls]" >&2
	exit 2
fi

rom_directory=$1
output_directory=$2
frames=${3:-1000}
tracer=${4:-build/gamatebioscalls}
script_directory=${0%/*}

mkdir -p "$output_directory" || exit 2

# Masks are active high here and map to the physical active-low controller bits:
# Up, Down, Left, Right, A, B, Start, Select = 01, 02, 04, 08, 10, 20, 40, 80.
campaign_names=(no_input long_start menu_actions controller_cycle)
campaign_schedules=(
	""
	"90:0x40:60,400:0x40:60,750:0x40:60"
	"90:0x80:20,130:0x40:60,260:0x10:12,310:0x20:12,500:0x40:30,580:0x10:12,630:0x20:12"
	"80:0x01:12,120:0x02:12,160:0x04:12,200:0x08:12,240:0x10:12,280:0x20:12,320:0x40:60,400:0x80:20,460:0x11:12,500:0x18:12,540:0x21:12,580:0x28:12,700:0x40:60,800:0x10:12,840:0x20:12"
)

union_events=$output_directory/coverage-events.tsv
union_calls=$output_directory/coverage-calls.tsv
summary=$output_directory/coverage-summary.tsv
: > "$summary"

for index in "${!campaign_names[@]}"; do
	name=${campaign_names[index]}
	schedule=${campaign_schedules[index]}
	events=$output_directory/$name-events.tsv
	calls=$output_directory/$name-calls.tsv
	printf 'campaign\t%s\n' "$name" >> "$summary"
	if ! GAMATEBIOSCALLS_PRESS=$schedule "$script_directory/gamate-bios-call-sweep.sh" \
			"$rom_directory" "$frames" "$tracer" "$events" "$calls"; then
		echo "input campaign failed: $name" >&2
		exit 1
	fi
	if (( index == 0 )); then
		cp "$events" "$union_events" || exit 2
		cp "$calls" "$union_calls" || exit 2
		printf 'baseline\tretained\n' >> "$summary"
		continue
	fi
	comparison=$output_directory/$name-coverage.tsv
	if "$script_directory/gamate-bios-call-coverage.py" "$union_events" "$union_calls" \
			"$events" "$calls" > "$comparison"; then
		printf 'coverage\tadded\n' >> "$summary"
		tail -n +2 "$events" >> "$union_events"
		tail -n +2 "$calls" >> "$union_calls"
	else
		printf 'coverage\tno-change\n' >> "$summary"
		rm -f "$events" "$calls"
	fi
done

printf 'summary: %s\n' "$summary"
