/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef GAMATEBIOSINPUT_H
#define GAMATEBIOSINPUT_H

#include <stdint.h>
#include <stdlib.h>

/* Entries are frame:mask:frames relative to cartridge entry. Masks are active high; returned
 * controller bits are active low. */
static inline uint8_t gamateBiosButtonsForFrame(unsigned frame, const char* schedule) {
	uint8_t buttons = 0xFF;
	while (schedule && *schedule) {
		char* end;
		unsigned from = (unsigned) strtoul(schedule, &end, 0);
		if (*end++ != ':') break;
		unsigned mask = (unsigned) strtoul(end, &end, 0);
		if (*end++ != ':') break;
		unsigned count = (unsigned) strtoul(end, &end, 0);
		if (frame >= from && frame < from + count) buttons &= (uint8_t) ~(mask & 0xFF);
		schedule = *end == ',' ? end + 1 : NULL;
	}
	return buttons;
}

#endif
