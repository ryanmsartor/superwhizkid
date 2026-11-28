/* Copyright (c) 2026 Ry
 * Copyright (c) 2026 SundownerSport
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
#include "libretro.h"
#include "gamate/gamate.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static retro_environment_t environCb;
static retro_video_refresh_t videoCb;
static retro_audio_sample_batch_t audioCb;
static retro_input_poll_t inputPollCb;
static retro_input_state_t inputStateCb;
static struct Gamate gamate;
static bool loaded;
enum { DEBUG_FRAMEBUFFER_WIDTH = 256, DEBUG_FRAMEBUFFER_HEIGHT = 256 };
static uint16_t video[DEBUG_FRAMEBUFFER_WIDTH * DEBUG_FRAMEBUFFER_HEIGHT];
static int16_t audio[2048 * 2];
static bool tapLatch, skipIntro;
static bool fullFramebuffer;
static bool fastForwardRequested;
static unsigned audioOutput;
static int32_t speakerBass, speakerTreble[2];
static unsigned flickerReduction;
static unsigned contrastLevel = 8;
static uint8_t contrastShortcutButtons;
static uint8_t pendingPresses;
static const uint16_t* selectedPalette;
static uint16_t contrastPalette[4];
static const uint16_t* activePalette = contrastPalette;
static const uint16_t originalPalette[4] = { 0xCE93, 0x8CEB, 0x5327, 0x1102 };
static const uint16_t gamateLcdPalette[4] = { 0x8C6A, 0x6348, 0x3208, 0x1906 };
static const uint16_t greyscalePalette[4] = { 0xFFFF, 0xAD55, 0x52AA, 0x0000 };
static const uint16_t yellowRgbPalette[4] = { 0xD654, 0xEBF1, 0x2B47, 0x5C19 };
static const uint16_t maximumContrastInk[4] = { 0x0040, 0x0848, 0x0000, 0x0000 };
static uint16_t customPalette[4] = { 0xFFFF, 0xAD55, 0x52AA, 0x0000 };
static uint16_t customMaximumContrastInk;
static unsigned selectedPaletteIndex;
static uint16_t shades[3 * 256 + 1];
static uint16_t lcdLevel[GAMATE_WIDTH * GAMATE_HEIGHT];
static unsigned lcdDarken = 256, lcdFade = 256;
static uint8_t flickerPixel[GAMATE_WIDTH * GAMATE_HEIGHT];
static uint8_t flickerPending[GAMATE_WIDTH * GAMATE_HEIGHT];
static uint8_t flickerPendingFrames[GAMATE_WIDTH * GAMATE_HEIGHT];
static bool lcdSnap = true, flickerSnap = true;
static retro_netpacket_send_t linkSend;
static uint16_t linkLocalId, linkPeerId;
static bool linkActive, linkPeerConnected, linkCanSend;
static uint8_t linkBits[2048];
static uint64_t linkBitClocks[2048];
static unsigned linkBitCount;
static uint8_t linkReceivedBits[4096];
static uint64_t linkReceivedClocks[4096];
static unsigned linkReceiveHead, linkReceiveCount;
static uint64_t linkRemoteBase, linkLocalBase, linkLastRemoteClock;
static bool linkClockSynchronized;

enum { INTRO_SKIP_CLOCK_LIMIT = 4433000 * 12, LINK_PACKET_HEADER = 13 };
enum { AUDIO_STEREO, AUDIO_MONO, AUDIO_SPEAKER };

static const char* customColorKeys[5][3] = {
	{ "superwhizkid_custom_shade0_red", "superwhizkid_custom_shade0_green",
		"superwhizkid_custom_shade0_blue" },
	{ "superwhizkid_custom_shade1_red", "superwhizkid_custom_shade1_green",
		"superwhizkid_custom_shade1_blue" },
	{ "superwhizkid_custom_shade2_red", "superwhizkid_custom_shade2_green",
		"superwhizkid_custom_shade2_blue" },
	{ "superwhizkid_custom_shade3_red", "superwhizkid_custom_shade3_green",
		"superwhizkid_custom_shade3_blue" },
	{ "superwhizkid_custom_ink_red", "superwhizkid_custom_ink_green",
		"superwhizkid_custom_ink_blue" }
};
static const char* customColorNames[5] = {
	"Shade 0 (Background)", "Shade 1", "Shade 2", "Shade 3", "Maximum Contrast Ink"
};
static bool customOptionsVisibilityKnown, customOptionsVisible;

static void setFastForwardOverride(bool enabled) {
	if (fastForwardRequested == enabled) return;
	struct retro_fastforwarding_override override = {
		.ratio = -1.0f,
		.fastforward = enabled,
		.notification = enabled,
		.inhibit_toggle = false
	};
	environCb(RETRO_ENVIRONMENT_SET_FASTFORWARDING_OVERRIDE, &override);
	fastForwardRequested = enabled;
}

static void resetAudioPresentation(void) {
	speakerBass = speakerTreble[0] = speakerTreble[1] = 0;
}

static int32_t distortSpeaker(int32_t sample) {
	// A moderate drive into a continuous soft knee approximates the audible overload in hardware
	// recordings without the discontinuity and excessive high harmonics of hard clipping.
	int32_t driven = sample * 13 / 8;
	int32_t sign = driven < 0 ? -1 : 1;
	int32_t magnitude = driven < 0 ? -driven : driven;
	if (magnitude > 15360) {
		int32_t excess = magnitude - 15360;
		magnitude = 15360 + excess * 15360 / (excess + 15360);
	}
	return magnitude * sign;
}

static void write64le(uint8_t* out, uint64_t value) {
	for (unsigned i = 0; i < 8; i++) out[i] = (uint8_t) (value >> (i * 8));
}

static uint64_t read64le(const uint8_t* in) {
	uint64_t value = 0;
	for (unsigned i = 0; i < 8; i++) value |= (uint64_t) in[i] << (i * 8);
	return value;
}

static void clearReceivedLink(void) {
	linkReceiveHead = linkReceiveCount = 0;
	linkClockSynchronized = false;
	linkLastRemoteClock = 0;
}

static void flushLink(void) {
	if (!linkBitCount) return;
	if (linkSend && linkPeerConnected) {
		uint8_t packet[LINK_PACKET_HEADER + sizeof(linkBits) * 5];
		packet[0] = 'G';
		packet[1] = 'L';
		packet[2] = 1;
		packet[3] = (uint8_t) linkBitCount;
		packet[4] = (uint8_t) (linkBitCount >> 8);
		uint64_t base = linkBitClocks[0];
		write64le(packet + 5, base);
		for (unsigned i = 0; i < linkBitCount; i++) {
			uint32_t delta = (uint32_t) (linkBitClocks[i] - base);
			uint8_t* event = packet + LINK_PACKET_HEADER + i * 5;
			event[0] = (uint8_t) delta;
			event[1] = (uint8_t) (delta >> 8);
			event[2] = (uint8_t) (delta >> 16);
			event[3] = (uint8_t) (delta >> 24);
			event[4] = linkBits[i];
		}
		linkSend(RETRO_NETPACKET_RELIABLE | RETRO_NETPACKET_FLUSH_HINT, packet,
			LINK_PACKET_HEADER + linkBitCount * 5, linkPeerId);
	}
	linkBitCount = 0;
}

static void queueLinkBit(void* context, bool bit) {
	(void) context;
	if (!linkActive || !linkPeerConnected || !linkCanSend) return;
	linkBits[linkBitCount] = bit;
	linkBitClocks[linkBitCount] = gamate.masterClocks;
	if (++linkBitCount == sizeof(linkBits)) flushLink();
}

static void linkStart(uint16_t clientId, retro_netpacket_send_t send,
		retro_netpacket_poll_receive_t pollReceive) {
	(void) pollReceive;
	linkLocalId = clientId;
	linkSend = send;
	linkActive = true;
	linkPeerConnected = clientId != 0;
	linkPeerId = 0;
	linkBitCount = 0;
	clearReceivedLink();
}

static void linkReceive(const void* data, size_t size, uint16_t clientId) {
	if (!linkActive || !linkPeerConnected || !loaded || clientId != linkPeerId || !data ||
			size < LINK_PACKET_HEADER) return;
	const uint8_t* packet = data;
	unsigned count = packet[3] | (unsigned) packet[4] << 8;
	if (packet[0] != 'G' || packet[1] != 'L' || packet[2] != 1 ||
			!count || count > sizeof(linkBits) || size != LINK_PACKET_HEADER + count * 5 ||
			count > sizeof(linkReceivedBits) - linkReceiveCount) return;
	uint64_t remoteBase = read64le(packet + 5);
	if (!linkClockSynchronized) {
		linkRemoteBase = remoteBase;
		linkLocalBase = gamate.masterClocks;
		linkLastRemoteClock = remoteBase;
		linkClockSynchronized = true;
	} else if (remoteBase < linkLastRemoteClock) {
		return;
	}
	if (!linkReceiveCount) {
		uint64_t mappedBase = linkLocalBase + (remoteBase - linkRemoteBase);
		if (mappedBase < gamate.masterClocks) linkLocalBase += gamate.masterClocks - mappedBase;
	}
	uint64_t previousClock = linkLastRemoteClock;
	for (unsigned i = 0; i < count; i++) {
		const uint8_t* event = packet + LINK_PACKET_HEADER + i * 5;
		uint32_t delta = event[0] | (uint32_t) event[1] << 8 | (uint32_t) event[2] << 16 |
			(uint32_t) event[3] << 24;
		uint64_t remoteClock = remoteBase + delta;
		if (remoteClock < remoteBase || remoteClock < previousClock) return;
		previousClock = remoteClock;
	}
	for (unsigned i = 0; i < count; i++) {
		const uint8_t* event = packet + LINK_PACKET_HEADER + i * 5;
		uint32_t delta = event[0] | (uint32_t) event[1] << 8 | (uint32_t) event[2] << 16 |
			(uint32_t) event[3] << 24;
		uint64_t remoteClock = remoteBase + delta;
		unsigned tail = (linkReceiveHead + linkReceiveCount) % sizeof(linkReceivedBits);
		linkReceivedBits[tail] = event[4] != 0;
		linkReceivedClocks[tail] = linkLocalBase + (remoteClock - linkRemoteBase);
		linkReceiveCount++;
	}
	linkLastRemoteClock = previousClock;
}

static void linkStop(void) {
	linkSend = NULL;
	linkActive = linkPeerConnected = linkCanSend = false;
	linkBitCount = 0;
	clearReceivedLink();
}

static void linkPoll(void) { flushLink(); }

static bool linkConnected(uint16_t clientId) {
	if (!linkActive || linkLocalId != 0 || linkPeerConnected || clientId == 0) return false;
	linkPeerId = clientId;
	linkPeerConnected = true;
	linkBitCount = 0;
	clearReceivedLink();
	return true;
}

static void linkDisconnected(uint16_t clientId) {
	if (linkLocalId == 0 && linkPeerConnected && clientId == linkPeerId) {
		linkPeerConnected = false;
		linkBitCount = 0;
		clearReceivedLink();
	}
}

static void runLinkedFrame(void) {
	gamate.frameDone = false;
	gamate.audioCount = 0;
	while (!gamate.frameDone) {
		while (linkReceiveCount && linkReceivedClocks[linkReceiveHead] <= gamate.masterClocks) {
			gamateUartReceiveBit(&gamate, linkReceivedBits[linkReceiveHead]);
			linkReceiveHead = (linkReceiveHead + 1) % sizeof(linkReceivedBits);
			linkReceiveCount--;
		}
		unsigned clocks = gamateCpuStep(&gamate) * 2;
		while (linkReceiveCount && linkReceivedClocks[linkReceiveHead] < gamate.masterClocks + clocks) {
			unsigned beforeEdge = (unsigned) (linkReceivedClocks[linkReceiveHead] - gamate.masterClocks);
			if (beforeEdge) {
				gamateRunClocks(&gamate, beforeEdge);
				clocks -= beforeEdge;
			}
			gamateUartReceiveBit(&gamate, linkReceivedBits[linkReceiveHead]);
			linkReceiveHead = (linkReceiveHead + 1) % sizeof(linkReceivedBits);
			linkReceiveCount--;
		}
		gamateRunClocks(&gamate, clocks);
	}
}

static void message(const char* text) {
	struct retro_message msg = { text, 300 };
	environCb(RETRO_ENVIRONMENT_SET_MESSAGE, &msg);
}

static bool updateCustomPaletteVisibility(void) {
	struct retro_variable palette = { "superwhizkid_palette", NULL };
	bool visible = environCb(RETRO_ENVIRONMENT_GET_VARIABLE, &palette) && palette.value &&
		!strcmp(palette.value, "custom");
	bool changed = !customOptionsVisibilityKnown || visible != customOptionsVisible;
	if (changed) {
		for (unsigned color = 0; color < 5; color++) {
			for (unsigned channel = 0; channel < 3; channel++) {
				struct retro_core_option_display display = {
					.key = customColorKeys[color][channel], .visible = visible
				};
				environCb(RETRO_ENVIRONMENT_SET_CORE_OPTIONS_DISPLAY, &display);
			}
		}
	}
	customOptionsVisibilityKnown = true;
	customOptionsVisible = visible;
	return changed;
}

void retro_set_environment(retro_environment_t cb) {
	environCb = cb;
	static const struct retro_core_option_definition options[] = {
		{
			"superwhizkid_palette", "Palette",
			"Selects colors for the four LCD shades. Gamate LCD is an uncalibrated interpretation of real-panel references; Original preserves the core's initial palette.",
			{
				{ "original", "Original" },
				{ "accurate", "Gamate LCD" },
				{ "greyscale", "Greyscale" },
				{ "yellow_rgb", "RGB" },
				{ "custom", "Custom" },
				{ NULL, NULL }
			},
			"original"
		},
		{ "superwhizkid_contrast", "Contrast", "Adjusts the distance between the three ink shades and the unchanged background color. At 16, all ink shades merge. L3 decreases and R3 increases the level during play.",
			{ { "1", NULL }, { "2", NULL }, { "3", NULL },
				{ "4", NULL }, { "5", NULL }, { "6", NULL }, { "7", NULL },
				{ "8", "8 (default)" }, { "9", NULL }, { "10", NULL }, { "11", NULL },
				{ "12", NULL }, { "13", NULL }, { "14", NULL }, { "15", NULL },
				{ "16", NULL }, { NULL, NULL } }, "8" },
		{ "superwhizkid_audio_output", "Audio Output", "Selects the output mix. Stereo preserves the Gamate's channel panning; Mono combines both sides equally; Built-in Speaker applies an estimated band-limited, softly distorting response based on recordings of real hardware.",
			{ { "stereo", "Stereo" }, { "mono", "Mono" },
				{ "speaker", "Built-in Speaker" }, { NULL, NULL } }, "stereo" },
		{ "superwhizkid_tap_latch", "Catch quick taps", "Not hardware accurate. Holds a press until the game next reads the controller, so taps between its reads aren't lost.",
			{ { "off", NULL }, { "on", NULL }, { NULL, NULL } }, "off" },
		{ "superwhizkid_lcd_ghosting", "LCD Ghosting", "Applies an estimated slow-panel response to reduce redraw flicker. Higher settings retain the previous shade longer; Off shows raw LCD-controller output.",
			{ { "off", "Off" }, { "1", "Very light" }, { "2", "Light" }, { "3", "Medium" }, { "4", "Strong" }, { "5", "Very strong" }, { NULL, NULL } }, "off" },
		{ "superwhizkid_flicker_reduction", "Redraw Flicker Reduction", "Suppresses brief light flashes caused by games clearing and redrawing objects. The selected number is the maximum transient duration suppressed; genuine lightening changes are delayed by the same number of frames.",
			{ { "off", "Off" }, { "1", "1 Frame" }, { "2", "2 Frames" },
				{ "3", "3 Frames" }, { NULL, NULL } }, "off" },
		{ "superwhizkid_debug_full_framebuffer", "Debug: Display Full Framebuffer", "Displays the raw 256x256 two-plane bitmap. Red outlines mark the ordinary display region and yellow outlines mark fixed window regions. LCD ghosting and redraw filtering do not alter this debug view.",
			{ { "off", "Off" }, { "on", "On" }, { NULL, NULL } }, "off" },
		{ "superwhizkid_bios", "BIOS Priority", "Selects the preferred BIOS at content load. UMC and Bit fall back to each other, then SWK; SWK falls back to UMC, then Bit. SWK is currently experimental and targets standalone Cube Up. Reload content to apply a change. Reset continues using the currently loaded BIOS.",
			{ { "umc", "UMC (gamate_bios_umc.bin)" }, { "bit", "Bit (gamate_bios_bit.bin)" },
				{ "swk", "SWK (gamate_bios_swk.bin)" }, { NULL, NULL } }, "umc" },
		{ "superwhizkid_skip_intro", "Skip BIOS intro", "Runs the BIOS logo sequence instantly at power-on and reset, starting at the cartridge.",
			{ { "off", NULL }, { "on", NULL }, { NULL, NULL } }, "off" },
		{ NULL, NULL, NULL, { { NULL, NULL } }, NULL }
	};
	static const struct retro_variable baseVariables[] = {
		{ "superwhizkid_palette", "Palette; original|accurate|greyscale|yellow_rgb|custom" },
		{ "superwhizkid_contrast", "Contrast; 8|1|2|3|4|5|6|7|9|10|11|12|13|14|15|16" },
		{ "superwhizkid_audio_output", "Audio Output; stereo|mono|speaker" },
		{ "superwhizkid_tap_latch", "Catch quick taps; off|on" },
		{ "superwhizkid_lcd_ghosting", "LCD Ghosting; off|1|2|3|4|5" },
		{ "superwhizkid_flicker_reduction", "Redraw Flicker Reduction; off|1|2|3" },
		{ "superwhizkid_debug_full_framebuffer", "Debug: Display Full Framebuffer; off|on" },
		{ "superwhizkid_bios", "BIOS Priority; umc|bit|swk" },
		{ "superwhizkid_skip_intro", "Skip BIOS intro; off|on" },
		{ NULL, NULL }
	};
	static struct retro_core_option_definition optionsV1[25];
	static struct retro_core_option_v2_definition optionsV2[25];
	static struct retro_variable variables[25];
	static struct retro_core_option_v2_category categories[] = {
		{ "custom_palette", "Custom Palette", "RGB565 channels for the four LCD shades and maximum-contrast ink." },
		{ NULL, NULL, NULL }
	};
	static char numbers[64][3];
	static char greenLabels[64][5];
	static char descriptions[15][64];
	static char legacyValues[15][256];
	static bool initialized;
	if (!initialized) {
		for (unsigned i = 0; i < 64; i++) {
			snprintf(numbers[i], sizeof(numbers[i]), "%u", i);
			snprintf(greenLabels[i], sizeof(greenLabels[i]), "%.1f", i * 0.5);
		}
		unsigned baseCount = 0;
		while (options[baseCount].key) baseCount++;
		for (unsigned base = 0; base < baseCount; base++) {
			unsigned i = base == 0 ? 0 : base + 15;
			optionsV1[i] = options[base];
			optionsV2[i].key = options[base].key;
			optionsV2[i].desc = options[base].desc;
			optionsV2[i].info = options[base].info;
			optionsV2[i].default_value = options[base].default_value;
			memcpy(optionsV2[i].values, options[base].values,
				sizeof(options[base].values));
			variables[i] = baseVariables[base];
		}
		static const char* channelNames[] = { "Red", "Green", "Blue" };
		static const unsigned defaults[5][3] = {
			{ 31, 63, 31 }, { 21, 42, 21 }, { 10, 21, 10 }, { 0, 0, 0 }, { 0, 0, 0 }
		};
		for (unsigned color = 0; color < 5; color++) {
			for (unsigned channel = 0; channel < 3; channel++) {
				unsigned i = 1 + color * 3 + channel;
				unsigned limit = channel == 1 ? 64 : 32;
				snprintf(descriptions[color * 3 + channel], sizeof(descriptions[0]), "%s %s",
					customColorNames[color], channelNames[channel]);
				optionsV2[i].key = customColorKeys[color][channel];
				optionsV2[i].desc = descriptions[color * 3 + channel];
				optionsV2[i].desc_categorized = descriptions[color * 3 + channel];
				optionsV2[i].info = "Sets this RGB565 color channel. Changes apply immediately.";
				optionsV2[i].category_key = "custom_palette";
				optionsV2[i].default_value = numbers[defaults[color][channel]];
				for (unsigned value = 0; value < limit; value++) {
					optionsV2[i].values[value].value = numbers[value];
					optionsV2[i].values[value].label = channel == 1 ? greenLabels[value] : NULL;
				}
				optionsV1[i].key = optionsV2[i].key;
				optionsV1[i].desc = optionsV2[i].desc;
				optionsV1[i].info = optionsV2[i].info;
				optionsV1[i].default_value = optionsV2[i].default_value;
				memcpy(optionsV1[i].values, optionsV2[i].values, sizeof(optionsV1[i].values));
				size_t used = (size_t) snprintf(legacyValues[color * 3 + channel],
					sizeof(legacyValues[0]), "%s; %u", descriptions[color * 3 + channel],
					defaults[color][channel]);
				for (unsigned value = 0; value < limit && used < sizeof(legacyValues[0]); value++) {
					if (value == defaults[color][channel]) continue;
					used += (size_t) snprintf(legacyValues[color * 3 + channel] + used,
						sizeof(legacyValues[0]) - used, "|%u", value);
				}
				variables[i].key = customColorKeys[color][channel];
				variables[i].value = legacyValues[color * 3 + channel];
			}
		}
		initialized = true;
	}
	unsigned version = 0;
	if (environCb(RETRO_ENVIRONMENT_GET_CORE_OPTIONS_VERSION, &version) && version >= 2) {
		struct retro_core_options_v2 optionsSet = { categories, optionsV2 };
		environCb(RETRO_ENVIRONMENT_SET_CORE_OPTIONS_V2, &optionsSet);
	} else if (version >= 1) {
		environCb(RETRO_ENVIRONMENT_SET_CORE_OPTIONS, optionsV1);
	} else {
		environCb(RETRO_ENVIRONMENT_SET_VARIABLES, variables);
	}
	static const struct retro_core_options_update_display_callback displayCallback = {
		updateCustomPaletteVisibility
	};
	environCb(RETRO_ENVIRONMENT_SET_CORE_OPTIONS_UPDATE_DISPLAY_CALLBACK,
		(void*) &displayCallback);
}

static uint16_t blend565(uint16_t a, uint16_t b, unsigned t) {
	unsigned r = ((a >> 11) * (256 - t) + (b >> 11) * t) >> 8;
	unsigned g = (((a >> 5) & 63) * (256 - t) + ((b >> 5) & 63) * t) >> 8;
	unsigned bl = ((a & 31) * (256 - t) + (b & 31) * t) >> 8;
	return (uint16_t) (r << 11 | g << 5 | bl);
}

static unsigned contrastChannel(unsigned background, unsigned ink, unsigned endpoint,
		unsigned maximum) {
	int value;
	if (contrastLevel <= 8) {
		value = (int) background +
			((int) ink - (int) background) * (int) contrastLevel / 8;
	} else {
		unsigned inkWeight = 2 * (16 - contrastLevel);
		if (contrastLevel == 15) inkWeight++;
		value = (int) (ink * inkWeight + endpoint * (16 - inkWeight)) / 16;
	}
	if (value < 0) return 0;
	return (unsigned) value > maximum ? maximum : (unsigned) value;
}

static void buildContrastPalette(void) {
	contrastPalette[0] = selectedPalette[0];
	unsigned br = selectedPalette[0] >> 11;
	unsigned bg = selectedPalette[0] >> 5 & 63;
	unsigned bb = selectedPalette[0] & 31;
	uint16_t endpoint = selectedPaletteIndex == 4 ? customMaximumContrastInk :
		maximumContrastInk[selectedPaletteIndex];
	unsigned er = endpoint >> 11;
	unsigned eg = endpoint >> 5 & 63;
	unsigned eb = endpoint & 31;
	for (unsigned i = 1; i < 4; i++) {
		unsigned r = contrastChannel(br, selectedPalette[i] >> 11, er, 31);
		unsigned g = contrastChannel(bg, selectedPalette[i] >> 5 & 63, eg, 63);
		unsigned b = contrastChannel(bb, selectedPalette[i] & 31, eb, 31);
		contrastPalette[i] = (uint16_t) (r << 11 | g << 5 | b);
	}
}

static unsigned readCustomChannel(const char* key, unsigned maximum, unsigned fallback) {
	struct retro_variable var = { key, NULL };
	if (!environCb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) || !var.value) return fallback;
	char* end;
	unsigned value = (unsigned) strtoul(var.value, &end, 10);
	return *var.value && !*end && value <= maximum ? value : fallback;
}

static void buildShades(void) {
	for (unsigned i = 0; i < sizeof(shades) / sizeof(*shades); i++) {
		shades[i] = i == 3 * 256 ? activePalette[3] :
			blend565(activePalette[i >> 8], activePalette[(i >> 8) + 1], i & 255);
	}
}

static void setVideoGeometry(void) {
	struct retro_game_geometry geometry = {
		.base_width = fullFramebuffer ? DEBUG_FRAMEBUFFER_WIDTH : GAMATE_WIDTH,
		.base_height = fullFramebuffer ? DEBUG_FRAMEBUFFER_HEIGHT : GAMATE_HEIGHT,
		.max_width = DEBUG_FRAMEBUFFER_WIDTH,
		.max_height = DEBUG_FRAMEBUFFER_HEIGHT,
		.aspect_ratio = fullFramebuffer ? 1.0f : 160.0f / 150.0f
	};
	environCb(RETRO_ENVIRONMENT_SET_GEOMETRY, &geometry);
}

static void readOptions(void) {
	bool wasRawPresentation = !flickerReduction && lcdDarken == 256 && lcdFade == 256;
	struct retro_variable var = { "superwhizkid_palette", NULL };
	const char* palette = environCb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value ?
		var.value : "original";
	selectedPaletteIndex = !strcmp(palette, "accurate") ? 1 :
		!strcmp(palette, "greyscale") ? 2 :
		!strcmp(palette, "yellow_rgb") ? 3 :
		!strcmp(palette, "custom") ? 4 : 0;
	updateCustomPaletteVisibility();
	static const unsigned defaults[5][3] = {
		{ 31, 63, 31 }, { 21, 42, 21 }, { 10, 21, 10 }, { 0, 0, 0 }, { 0, 0, 0 }
	};
	if (selectedPaletteIndex == 4) {
		for (unsigned color = 0; color < 5; color++) {
			unsigned red = readCustomChannel(customColorKeys[color][0], 31, defaults[color][0]);
			unsigned green = readCustomChannel(customColorKeys[color][1], 63, defaults[color][1]);
			unsigned blue = readCustomChannel(customColorKeys[color][2], 31, defaults[color][2]);
			uint16_t value = (uint16_t) (red << 11 | green << 5 | blue);
			if (color < 4) customPalette[color] = value;
			else customMaximumContrastInk = value;
		}
	}
	selectedPalette = selectedPaletteIndex == 1 ? gamateLcdPalette :
		selectedPaletteIndex == 2 ? greyscalePalette :
		selectedPaletteIndex == 3 ? yellowRgbPalette :
		selectedPaletteIndex == 4 ? customPalette : originalPalette;

	var.key = "superwhizkid_contrast";
	var.value = NULL;
	const char* contrastValue = environCb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value ?
		var.value : "8";
	char* contrastEnd;
	unsigned newContrast = (unsigned) strtoul(contrastValue, &contrastEnd, 10);
	contrastLevel = *contrastValue && !*contrastEnd && newContrast >= 1 && newContrast <= 16 ?
		newContrast : 8;
	buildContrastPalette();
	buildShades();

	var.key = "superwhizkid_audio_output";
	var.value = NULL;
	const char* audioValue = environCb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value ?
		var.value : "stereo";
	unsigned newAudioOutput = !strcmp(audioValue, "mono") ? AUDIO_MONO :
		!strcmp(audioValue, "speaker") ? AUDIO_SPEAKER : AUDIO_STEREO;
	if (newAudioOutput != audioOutput) resetAudioPresentation();
	audioOutput = newAudioOutput;

	var.key = "superwhizkid_tap_latch";
	var.value = NULL;
	tapLatch = environCb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value && !strcmp(var.value, "on");

	var.key = "superwhizkid_skip_intro";
	var.value = NULL;
	skipIntro = environCb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value && !strcmp(var.value, "on");

	var.key = "superwhizkid_lcd_ghosting";
	var.value = NULL;
	const char* value = environCb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value ? var.value : "off";
	// darken = going from light to dark; fade = going from dark to light.
	// lower values result in longer transition times.
	static const unsigned darken[] = { 256, 256, 248, 240, 224, 208 };
	static const unsigned fade[] = { 256, 220, 184, 148, 112, 76 };
	int level = value[0] >= '1' && value[0] <= '5' && !value[1] ? value[0] - '0' : 0;
	lcdDarken = darken[level];
	lcdFade = fade[level];

	var.key = "superwhizkid_flicker_reduction";
	var.value = NULL;
	const char* flickerValue = environCb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value ?
		var.value : "off";
	unsigned reduceFlicker = !strcmp(flickerValue, "on") ? 1 :
		flickerValue[0] >= '1' && flickerValue[0] <= '3' && !flickerValue[1] ?
		(unsigned) (flickerValue[0] - '0') : 0;
	if (reduceFlicker != flickerReduction) flickerSnap = true;
	flickerReduction = reduceFlicker;

	var.key = "superwhizkid_debug_full_framebuffer";
	var.value = NULL;
	bool showFullFramebuffer = environCb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value &&
		!strcmp(var.value, "on");
	if (showFullFramebuffer != fullFramebuffer) {
		fullFramebuffer = showFullFramebuffer;
		setVideoGeometry();
	}
	if (loaded && wasRawPresentation &&
			(flickerReduction || lcdDarken != 256 || lcdFade != 256)) {
		for (size_t i = 0; i < GAMATE_WIDTH * GAMATE_HEIGHT; i++) {
			uint8_t raw = gamate.frameBuffer[i] & 3;
			flickerPixel[i] = raw;
			flickerPendingFrames[i] = 0;
			lcdLevel[i] = raw * 256;
		}
		lcdSnap = flickerSnap = false;
	}
}

// Estimated physical-panel response; raw LCD-controller output remains available with rates at 256.
static uint8_t reduceFlickerPixel(size_t i, uint8_t raw) {
	if (flickerSnap) {
		flickerPixel[i] = raw;
		flickerPendingFrames[i] = 0;
	} else if (raw >= flickerPixel[i]) {
		flickerPixel[i] = raw;
		flickerPendingFrames[i] = 0;
	} else if (flickerPendingFrames[i] && flickerPending[i] == raw &&
			++flickerPendingFrames[i] > flickerReduction) {
		flickerPixel[i] = raw;
		flickerPendingFrames[i] = 0;
	} else if (!flickerPendingFrames[i] || flickerPending[i] != raw) {
		flickerPending[i] = raw;
		flickerPendingFrames[i] = 1;
	}
	return flickerPixel[i];
}

static void renderLcd(void) {
	if (!flickerReduction && lcdDarken == 256 && lcdFade == 256) {
		for (size_t i = 0; i < GAMATE_WIDTH * GAMATE_HEIGHT; i++) {
			uint8_t raw = gamate.frameBuffer[i] & 3;
			video[i] = activePalette[raw];
		}
		lcdSnap = flickerSnap = false;
		return;
	}
	if (flickerReduction && lcdDarken == 256 && lcdFade == 256) {
		for (size_t i = 0; i < GAMATE_WIDTH * GAMATE_HEIGHT; i++) {
			uint8_t pixel = reduceFlickerPixel(i, gamate.frameBuffer[i] & 3);
			lcdLevel[i] = pixel * 256;
			video[i] = activePalette[pixel];
		}
		lcdSnap = flickerSnap = false;
		return;
	}
	if (!flickerReduction) {
		for (size_t i = 0; i < GAMATE_WIDTH * GAMATE_HEIGHT; i++) {
			int target = (gamate.frameBuffer[i] & 3) * 256, level = lcdLevel[i];
			int rate = target > level ? lcdDarken : lcdFade;
			int step = (target - level) * rate / 256;
			level = lcdSnap || !step ? target : level + step;
			lcdLevel[i] = (uint16_t) level;
			video[i] = shades[level];
		}
		lcdSnap = flickerSnap = false;
		return;
	}
	for (size_t i = 0; i < GAMATE_WIDTH * GAMATE_HEIGHT; i++) {
		int target = reduceFlickerPixel(i, gamate.frameBuffer[i] & 3) * 256;
		int level = lcdLevel[i];
		int rate = target > level ? lcdDarken : lcdFade;
		int step = (target - level) * rate / 256;
		level = lcdSnap || !step ? target : level + step;
		lcdLevel[i] = (uint16_t) level;
		video[i] = shades[level];
	}
	lcdSnap = false;
	flickerSnap = false;
}

static void drawDebugBox(unsigned x, unsigned y, unsigned width, unsigned height,
		unsigned verticalWrap, uint16_t color) {
	unsigned left = (x - 1) & 0xFF;
	unsigned right = (x + width) & 0xFF;
	unsigned top = (y + verticalWrap - 1) % verticalWrap;
	unsigned bottom = (y + height) % verticalWrap;
	for (unsigned i = 0; i < width + 2; i++) {
		unsigned px = (x + i - 1) & 0xFF;
		video[top * DEBUG_FRAMEBUFFER_WIDTH + px] = color;
		video[bottom * DEBUG_FRAMEBUFFER_WIDTH + px] = color;
	}
	for (unsigned i = 0; i < height; i++) {
		unsigned py = (y + i) % verticalWrap;
		video[py * DEBUG_FRAMEBUFFER_WIDTH + left] = color;
		video[py * DEBUG_FRAMEBUFFER_WIDTH + right] = color;
	}
}

static void renderFullFramebuffer(void) {
	for (unsigned y = 0; y < DEBUG_FRAMEBUFFER_HEIGHT; y++) {
		for (unsigned x = 0; x < DEBUG_FRAMEBUFFER_WIDTH; x++) {
			unsigned offset = (y * 32 + (x >> 3)) << 1;
			unsigned shift = 7 - (x & 7);
			unsigned plane0 = gamate.vram[offset] >> shift & 1;
			unsigned plane1 = gamate.vram[offset + 1] >> shift & 1;
			unsigned shade = gamate.swapPlanes ? plane1 | plane0 << 1 : plane0 | plane1 << 1;
			video[y * DEBUG_FRAMEBUFFER_WIDTH + x] = activePalette[shade];
		}
	}

	const uint16_t mainOutline = 0xF800;
	const uint16_t windowOutline = 0xFFE0;
	unsigned firstLine = 0;
	if (gamate.window) {
		drawDebugBox(0, 0xD0, GAMATE_WIDTH, 0x10, 256, windowOutline);
		firstLine = 0x10;
	}
	if (gamate.scrollY < 0xC8) {
		drawDebugBox(gamate.scrollX, (gamate.scrollY + firstLine) % 0xC8,
			GAMATE_WIDTH, GAMATE_HEIGHT - firstLine, 0xC8, mainOutline);
	} else if (gamate.scrollY & 8) {
		drawDebugBox(gamate.scrollX, firstLine, GAMATE_WIDTH,
			GAMATE_HEIGHT - firstLine, 256, mainOutline);
	} else {
		unsigned first = gamate.scrollY & 7;
		unsigned fixedLines = 8 - first;
		if (!firstLine) {
			drawDebugBox(0, 0xF8 + first, GAMATE_WIDTH, fixedLines, 256, windowOutline);
			drawDebugBox(gamate.scrollX, 0, GAMATE_WIDTH,
				GAMATE_HEIGHT - fixedLines, 256, mainOutline);
		} else {
			drawDebugBox(gamate.scrollX, firstLine - fixedLines, GAMATE_WIDTH,
				GAMATE_HEIGHT - firstLine, 256, mainOutline);
		}
	}
}
void retro_set_video_refresh(retro_video_refresh_t cb) { videoCb = cb; }
void retro_set_audio_sample(retro_audio_sample_t cb) { (void) cb; }
void retro_set_audio_sample_batch(retro_audio_sample_batch_t cb) { audioCb = cb; }
void retro_set_input_poll(retro_input_poll_t cb) { inputPollCb = cb; }
void retro_set_input_state(retro_input_state_t cb) { inputStateCb = cb; }

void retro_init(void) {
	gamateInit(&gamate);
	gamateSetLink(&gamate, queueLinkBit, NULL);
	selectedPalette = originalPalette;
	contrastLevel = 8;
	buildContrastPalette();
	buildShades();
	enum retro_pixel_format format = RETRO_PIXEL_FORMAT_RGB565;
	environCb(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &format);
	static const struct retro_controller_description ports[] = { { "Gamate", RETRO_DEVICE_JOYPAD } };
	static const struct retro_controller_info controllers[] = { { ports, 1 }, { NULL, 0 } };
	environCb(RETRO_ENVIRONMENT_SET_CONTROLLER_INFO, (void*) controllers);
	static const struct retro_input_descriptor inputs[] = {
		{ 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_UP, "Up" },
		{ 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_DOWN, "Down" },
		{ 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_LEFT, "Left" },
		{ 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_RIGHT, "Right" },
		{ 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_A, "A" },
		{ 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_B, "B" },
		{ 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_START, "Start" },
		{ 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_SELECT, "Select" },
		{ 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L3, "Contrast Down" },
		{ 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R3, "Contrast Up" },
		{ 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R2, "Fast Forward (hold)" },
		{ 0, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_LEFT,
			RETRO_DEVICE_ID_ANALOG_X, "Left Stick X" },
		{ 0, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_LEFT,
			RETRO_DEVICE_ID_ANALOG_Y, "Left Stick Y" },
		{ 0 }
	};
	environCb(RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS, (void*) inputs);
	static const struct retro_netpacket_callback netpacket = {
		linkStart, linkReceive, linkStop, linkPoll, linkConnected, linkDisconnected,
		"superwhizkid-gamate-link-1"
	};
	environCb(RETRO_ENVIRONMENT_SET_NETPACKET_INTERFACE, (void*) &netpacket);
}

void retro_deinit(void) {
	setFastForwardOverride(false);
	linkStop();
	gamateFree(&gamate);
}
unsigned retro_api_version(void) { return RETRO_API_VERSION; }

void retro_get_system_info(struct retro_system_info* info) {
	memset(info, 0, sizeof(*info));
	info->library_name = "superwhizkid";
	info->library_version = "0.5.1";
	info->valid_extensions = "bin|gam";
	info->need_fullpath = false;
	info->block_extract = false;
}

void retro_get_system_av_info(struct retro_system_av_info* info) {
	memset(info, 0, sizeof(*info));
	info->geometry.base_width = fullFramebuffer ? DEBUG_FRAMEBUFFER_WIDTH : GAMATE_WIDTH;
	info->geometry.base_height = fullFramebuffer ? DEBUG_FRAMEBUFFER_HEIGHT : GAMATE_HEIGHT;
	info->geometry.max_width = DEBUG_FRAMEBUFFER_WIDTH;
	info->geometry.max_height = DEBUG_FRAMEBUFFER_HEIGHT;
	info->geometry.aspect_ratio = fullFramebuffer ? 1.0f : 160.0f / 150.0f;
	info->timing.fps = 4433000.0 / 72900.0;
	info->timing.sample_rate = 48000.0;
}

void retro_set_controller_port_device(unsigned port, unsigned device) {
	(void) port;
	(void) device;
}

// Runs the real BIOS boot unseen until it jumps to the cartridge entry.
static bool runIntro(void) {
	uint64_t start = gamate.masterClocks;
	gamateSetAudio(&gamate, audio, sizeof(audio) / sizeof(audio[0]) / 2, 48000);
	while (!(gamate.protectionPassed && gamate.cpu.pc == 0x6020) &&
			gamate.masterClocks - start < INTRO_SKIP_CLOCK_LIMIT) {
		gamate.audioCount = 0;
		gamateRunClocks(&gamate, gamateCpuStep(&gamate) * 2);
	}
	gamate.audioCount = 0;
	return gamate.protectionPassed && gamate.cpu.pc == 0x6020;
}

static void skipIntroOrReset(void) {
	if (runIntro()) return;
	gamateReset(&gamate);
	message("superwhizkid: BIOS intro skip timed out; using normal boot");
}

void retro_reset(void) {
	if (!loaded) return;
	pendingPresses = 0;
	contrastShortcutButtons = 0;
	clearReceivedLink();
	gamateReset(&gamate);
	resetAudioPresentation();
	if (skipIntro) skipIntroOrReset();
	lcdSnap = flickerSnap = true;
}

static void readInput(void) {
	inputPollCb();
	setFastForwardOverride(!linkActive &&
		inputStateCb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R2));
	uint8_t shortcuts = 0;
	shortcuts |= inputStateCb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L3) ? 1 : 0;
	shortcuts |= inputStateCb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R3) ? 2 : 0;
	unsigned oldContrast = contrastLevel;
	if ((shortcuts & 1) && !(contrastShortcutButtons & 1) && contrastLevel > 1) contrastLevel--;
	if ((shortcuts & 2) && !(contrastShortcutButtons & 2) && contrastLevel < 16) contrastLevel++;
	if (contrastLevel != oldContrast) {
		static const char* values[] = { NULL, "1", "2", "3", "4", "5", "6", "7",
			"8", "9", "10", "11", "12", "13", "14", "15", "16" };
		struct retro_variable contrast = { "superwhizkid_contrast", values[contrastLevel] };
		buildContrastPalette();
		buildShades();
		environCb(RETRO_ENVIRONMENT_SET_VARIABLE, &contrast);
	}
	contrastShortcutButtons = shortcuts;
	uint8_t buttons = 0xFF;
#define BUTTON(id, bit) do { if (inputStateCb(0, RETRO_DEVICE_JOYPAD, 0, id)) buttons &= ~(1 << bit); } while (0)
	BUTTON(RETRO_DEVICE_ID_JOYPAD_UP, 0);
	BUTTON(RETRO_DEVICE_ID_JOYPAD_DOWN, 1);
	BUTTON(RETRO_DEVICE_ID_JOYPAD_LEFT, 2);
	BUTTON(RETRO_DEVICE_ID_JOYPAD_RIGHT, 3);
	BUTTON(RETRO_DEVICE_ID_JOYPAD_A, 4);
	BUTTON(RETRO_DEVICE_ID_JOYPAD_B, 5);
	BUTTON(RETRO_DEVICE_ID_JOYPAD_START, 6);
	BUTTON(RETRO_DEVICE_ID_JOYPAD_SELECT, 7);
#undef BUTTON
	int16_t analogX = inputStateCb(0, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_LEFT,
		RETRO_DEVICE_ID_ANALOG_X);
	int16_t analogY = inputStateCb(0, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_LEFT,
		RETRO_DEVICE_ID_ANALOG_Y);
	if (analogY < -0x4000) buttons &= ~(1 << 0);
	if (analogY > 0x4000) buttons &= ~(1 << 1);
	if (analogX < -0x4000) buttons &= ~(1 << 2);
	if (analogX > 0x4000) buttons &= ~(1 << 3);
	if (tapLatch) {
		pendingPresses |= (uint8_t) ~buttons;
		gamate.buttons = buttons & (uint8_t) ~pendingPresses;
	} else {
		gamate.buttons = buttons;
		pendingPresses = 0;
	}
}

void retro_run(void) {
	if (!loaded) return;
	bool updated = false;
	if (environCb(RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE, &updated) && updated) readOptions();
	readInput();
	uint64_t controllerReads = gamate.diagnostics.controllerReads;
	gamateSetAudio(&gamate, audio, sizeof(audio) / sizeof(audio[0]) / 2, 48000);
	linkCanSend = true;
	if (linkReceiveCount) runLinkedFrame();
	else gamateRunFrame(&gamate);
	flushLink();
	linkCanSend = false;
	if (gamate.diagnostics.controllerReads != controllerReads) pendingPresses = 0;
	renderLcd();
	if (fullFramebuffer) renderFullFramebuffer();
	unsigned videoWidth = fullFramebuffer ? DEBUG_FRAMEBUFFER_WIDTH : GAMATE_WIDTH;
	unsigned videoHeight = fullFramebuffer ? DEBUG_FRAMEBUFFER_HEIGHT : GAMATE_HEIGHT;
	videoCb(video, videoWidth, videoHeight, videoWidth * sizeof(*video));
	size_t frames = gamate.audioCount < gamate.audioCapacity ? gamate.audioCount : gamate.audioCapacity;
	if (audioOutput != AUDIO_STEREO) {
		for (size_t i = 0; i < frames; i++) {
			int32_t mixed = ((int32_t) audio[i * 2] + audio[i * 2 + 1]) / 2;
			if (audioOutput == AUDIO_SPEAKER) {
				// Estimated small-speaker response at 48 kHz: approximately 300 Hz high-pass,
				// followed by two approximately 6 kHz low-pass stages.
				speakerBass += (mixed - speakerBass) * 1262 / 32768;
				mixed -= speakerBass;
				for (unsigned stage = 0; stage < 2; stage++) {
					speakerTreble[stage] += (mixed - speakerTreble[stage]) * 17828 / 32768;
					mixed = speakerTreble[stage];
				}
				mixed = distortSpeaker(mixed);
			}
			if (mixed > 32767) mixed = 32767;
			if (mixed < -32768) mixed = -32768;
			audio[i * 2] = audio[i * 2 + 1] = mixed;
		}
	}
	audioCb(audio, frames);
}

static bool readBios(uint8_t* out) {
	const char* system = NULL;
	if (!environCb(RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY, &system) || !system) return false;
	struct retro_variable var = { "superwhizkid_bios", NULL };
	const char* preference = environCb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value ?
		var.value : "umc";
	const char* names[] = { "gamate_bios_umc.bin", "gamate_bios_bit.bin", "gamate_bios_swk.bin" };
	if (!strcmp(preference, "bit")) {
		names[0] = "gamate_bios_bit.bin";
		names[1] = "gamate_bios_umc.bin";
	} else if (!strcmp(preference, "swk")) {
		names[0] = "gamate_bios_swk.bin";
		names[1] = "gamate_bios_umc.bin";
		names[2] = "gamate_bios_bit.bin";
	}
	char path[4096];
	for (size_t i = 0; i < sizeof(names) / sizeof(*names); i++) {
		if (snprintf(path, sizeof(path), "%s/%s", system, names[i]) >= (int) sizeof(path)) continue;
		FILE* file = fopen(path, "rb");
		if (!file) continue;
		size_t got = fread(out, 1, GAMATE_BIOS_SIZE, file);
		int extra = fgetc(file);
		fclose(file);
		if (got == GAMATE_BIOS_SIZE && extra == EOF) {
			if (!strcmp(names[i], "gamate_bios_swk.bin"))
				message("superwhizkid: using SWK replacement BIOS (experimental)");
			return true;
		}
	}
	return false;
}

bool retro_load_game(const struct retro_game_info* game) {
	if (!game || !game->data || !game->size) return false;
	uint8_t bios[GAMATE_BIOS_SIZE];
	if (!readBios(bios)) {
		message("superwhizkid: gamate_bios_umc.bin, gamate_bios_bit.bin or gamate_bios_swk.bin is required");
		return false;
	}
	if (!gamateLoad(&gamate, game->data, game->size, bios, sizeof(bios))) {
		message("superwhizkid: unsupported cartridge size");
		return false;
	}
	readOptions();
	pendingPresses = 0;
	contrastShortcutButtons = 0;
	clearReceivedLink();
	if (skipIntro) skipIntroOrReset();
	lcdSnap = flickerSnap = true;
	resetAudioPresentation();
	loaded = true;
	return true;
}

bool retro_load_game_special(unsigned type, const struct retro_game_info* info, size_t count) {
	(void) type; (void) info; (void) count;
	return false;
}

void retro_unload_game(void) {
	setFastForwardOverride(false);
	pendingPresses = 0;
	contrastShortcutButtons = 0;
	linkBitCount = 0;
	clearReceivedLink();
	resetAudioPresentation();
	gamateFree(&gamate);
	loaded = false;
}

unsigned retro_get_region(void) { return RETRO_REGION_NTSC; }
size_t retro_serialize_size(void) { return gamateStateSize(&gamate); }
bool retro_serialize(void* data, size_t size) { return loaded && gamateSaveState(&gamate, data, size); }
bool retro_unserialize(const void* data, size_t size) {
	if (!loaded || !gamateLoadState(&gamate, data, size)) return false;
	clearReceivedLink();
	contrastShortcutButtons = 0;
	lcdSnap = flickerSnap = true;
	resetAudioPresentation();
	return true;
}
void retro_cheat_reset(void) { }
void retro_cheat_set(unsigned index, bool enabled, const char* code) { (void) index; (void) enabled; (void) code; }

void* retro_get_memory_data(unsigned id) {
	return loaded && id == RETRO_MEMORY_SYSTEM_RAM ? gamate.ram : NULL;
}
size_t retro_get_memory_size(unsigned id) {
	return loaded && id == RETRO_MEMORY_SYSTEM_RAM ? sizeof(gamate.ram) : 0;
}
