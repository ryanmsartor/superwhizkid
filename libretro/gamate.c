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
static uint16_t video[GAMATE_WIDTH * GAMATE_HEIGHT];
static int16_t audio[2048 * 2];
static bool tapLatch, skipIntro;
static uint8_t pendingPresses;
static const uint16_t* activePalette;
static const uint16_t originalPalette[4] = { 0xCE93, 0x8CEB, 0x5327, 0x1102 };
static const uint16_t accuratePalette[4] = { 0x9CEC, 0x6B89, 0x3A08, 0x10C4 };
static const uint16_t greyscalePalette[4] = { 0xFFFF, 0xAD55, 0x52AA, 0x0000 };
static uint16_t shades[3 * 256 + 1];
static uint16_t lcdLevel[GAMATE_WIDTH * GAMATE_HEIGHT];
static unsigned lcdDarken = 256, lcdFade = 256;
static bool lcdSnap = true;

enum { INTRO_SKIP_CLOCK_LIMIT = 4433000 * 12 };

static void message(const char* text) {
	struct retro_message msg = { text, 300 };
	environCb(RETRO_ENVIRONMENT_SET_MESSAGE, &msg);
}

void retro_set_environment(retro_environment_t cb) {
	environCb = cb;
	static const struct retro_core_option_definition options[] = {
		{
			"superwhizkid_palette", "Palette",
			"Selects the colors used for the Gamate's four LCD shades. Accurate is a representative estimate from photographs of real hardware; Original preserves the core's initial palette.",
			{
				{ "original", "Original" },
				{ "accurate", "Accurate" },
				{ "greyscale", "Greyscale" },
				{ NULL, NULL }
			},
			"original"
		},
		{ "superwhizkid_tap_latch", "Catch quick taps", "Not hardware accurate. Holds a press until the game next reads the controller, so taps between its reads aren't lost.",
			{ { "off", NULL }, { "on", NULL }, { NULL, NULL } }, "off" },
		{ "superwhizkid_lcd_ghosting", "LCD Ghosting", "Applies an estimated slow-panel response to reduce redraw flicker. Higher settings retain the previous shade longer; Off shows raw LCD-controller output.",
			{ { "off", "Off" }, { "1", "Very light" }, { "2", "Light" }, { "3", "Medium" }, { "4", "Strong" }, { "5", "Very strong" }, { NULL, NULL } }, "off" },
		{ "superwhizkid_bios", "BIOS Priority", "Selects the preferred BIOS at content load; the other is used if it is missing. Reload content to apply a change. Reset continues using the currently loaded BIOS.",
			{ { "umc", "UMC (gamate_bios_umc.bin)" }, { "bit", "Bit (gamate_bios_bit.bin)" }, { NULL, NULL } }, "umc" },
		{ "superwhizkid_skip_intro", "Skip BIOS intro", "Runs the BIOS logo sequence instantly at power-on and reset, starting at the cartridge.",
			{ { "off", NULL }, { "on", NULL }, { NULL, NULL } }, "off" },
		{ NULL, NULL, NULL, { { NULL, NULL } }, NULL }
	};
	static const struct retro_variable variables[] = {
		{ "superwhizkid_palette", "Palette; original|accurate|greyscale" },
		{ "superwhizkid_tap_latch", "Catch quick taps; off|on" },
		{ "superwhizkid_lcd_ghosting", "LCD Ghosting; off|1|2|3|4|5" },
		{ "superwhizkid_bios", "BIOS Priority; umc|bit" },
		{ "superwhizkid_skip_intro", "Skip BIOS intro; off|on" },
		{ NULL, NULL }
	};
	unsigned version = 0;
	if (environCb(RETRO_ENVIRONMENT_GET_CORE_OPTIONS_VERSION, &version) && version >= 1) {
		environCb(RETRO_ENVIRONMENT_SET_CORE_OPTIONS, (void*) options);
	} else {
		environCb(RETRO_ENVIRONMENT_SET_VARIABLES, (void*) variables);
	}
}

static uint16_t blend565(uint16_t a, uint16_t b, unsigned t) {
	unsigned r = ((a >> 11) * (256 - t) + (b >> 11) * t) >> 8;
	unsigned g = (((a >> 5) & 63) * (256 - t) + ((b >> 5) & 63) * t) >> 8;
	unsigned bl = ((a & 31) * (256 - t) + (b & 31) * t) >> 8;
	return (uint16_t) (r << 11 | g << 5 | bl);
}

static void buildShades(void) {
	for (unsigned i = 0; i < sizeof(shades) / sizeof(*shades); i++) {
		shades[i] = i == 3 * 256 ? activePalette[3] :
			blend565(activePalette[i >> 8], activePalette[(i >> 8) + 1], i & 255);
	}
}

static void readOptions(void) {
	struct retro_variable var = { "superwhizkid_palette", NULL };
	const char* palette = environCb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value ?
		var.value : "original";
	activePalette = !strcmp(palette, "accurate") ? accuratePalette :
		!strcmp(palette, "greyscale") ? greyscalePalette : originalPalette;
	buildShades();

	var.key = "superwhizkid_tap_latch";
	var.value = NULL;
	tapLatch = environCb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value && !strcmp(var.value, "on");

	var.key = "superwhizkid_skip_intro";
	var.value = NULL;
	skipIntro = environCb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value && !strcmp(var.value, "on");

	var.key = "superwhizkid_lcd_ghosting";
	var.value = NULL;
	const char* value = environCb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value ? var.value : "off";
	static const unsigned darken[] = { 256, 256, 248, 240, 224, 208 };
	static const unsigned fade[] = { 256, 224, 192, 160, 128, 96 };
	int level = value[0] >= '1' && value[0] <= '5' && !value[1] ? value[0] - '0' : 0;
	lcdDarken = darken[level];
	lcdFade = fade[level];
}

// Estimated physical-panel response; raw LCD-controller output remains available with rates at 256.
static void renderLcd(void) {
	for (size_t i = 0; i < GAMATE_WIDTH * GAMATE_HEIGHT; i++) {
		int target = (gamate.frameBuffer[i] & 3) * 256, level = lcdLevel[i];
		int rate = target > level ? lcdDarken : lcdFade;
		int step = (target - level) * rate / 256;
		level = lcdSnap || !step ? target : level + step;
		lcdLevel[i] = (uint16_t) level;
		video[i] = shades[level];
	}
	lcdSnap = false;
}
void retro_set_video_refresh(retro_video_refresh_t cb) { videoCb = cb; }
void retro_set_audio_sample(retro_audio_sample_t cb) { (void) cb; }
void retro_set_audio_sample_batch(retro_audio_sample_batch_t cb) { audioCb = cb; }
void retro_set_input_poll(retro_input_poll_t cb) { inputPollCb = cb; }
void retro_set_input_state(retro_input_state_t cb) { inputStateCb = cb; }

void retro_init(void) {
	gamateInit(&gamate);
	activePalette = originalPalette;
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
		{ 0 }
	};
	environCb(RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS, (void*) inputs);
}

void retro_deinit(void) { gamateFree(&gamate); }
unsigned retro_api_version(void) { return RETRO_API_VERSION; }

void retro_get_system_info(struct retro_system_info* info) {
	memset(info, 0, sizeof(*info));
	info->library_name = "superwhizkid";
	info->library_version = "0.1";
	info->valid_extensions = "bin|gam";
	info->need_fullpath = false;
	info->block_extract = false;
}

void retro_get_system_av_info(struct retro_system_av_info* info) {
	memset(info, 0, sizeof(*info));
	info->geometry.base_width = info->geometry.max_width = GAMATE_WIDTH;
	info->geometry.base_height = info->geometry.max_height = GAMATE_HEIGHT;
	info->geometry.aspect_ratio = 160.0f / 150.0f;
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
	gamateReset(&gamate);
	if (skipIntro) skipIntroOrReset();
	lcdSnap = true;
}

static void readInput(void) {
	inputPollCb();
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
	gamateRunFrame(&gamate);
	if (gamate.diagnostics.controllerReads != controllerReads) pendingPresses = 0;
	renderLcd();
	videoCb(video, GAMATE_WIDTH, GAMATE_HEIGHT, GAMATE_WIDTH * sizeof(*video));
	size_t frames = gamate.audioCount < gamate.audioCapacity ? gamate.audioCount : gamate.audioCapacity;
	audioCb(audio, frames);
}

static bool readBios(uint8_t* out) {
	const char* system = NULL;
	if (!environCb(RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY, &system) || !system) return false;
	struct retro_variable var = { "superwhizkid_bios", NULL };
	bool bitFirst = environCb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value && !strcmp(var.value, "bit");
	const char* names[] = { "gamate_bios_umc.bin", "gamate_bios_bit.bin" };
	if (bitFirst) {
		names[0] = "gamate_bios_bit.bin";
		names[1] = "gamate_bios_umc.bin";
	}
	char path[4096];
	for (size_t i = 0; i < sizeof(names) / sizeof(*names); i++) {
		if (snprintf(path, sizeof(path), "%s/%s", system, names[i]) >= (int) sizeof(path)) continue;
		FILE* file = fopen(path, "rb");
		if (!file) continue;
		size_t got = fread(out, 1, GAMATE_BIOS_SIZE, file);
		int extra = fgetc(file);
		fclose(file);
		if (got == GAMATE_BIOS_SIZE && extra == EOF) return true;
	}
	return false;
}

bool retro_load_game(const struct retro_game_info* game) {
	if (!game || !game->data || !game->size) return false;
	uint8_t bios[GAMATE_BIOS_SIZE];
	if (!readBios(bios)) {
		message("superwhizkid: gamate_bios_umc.bin or gamate_bios_bit.bin is required");
		return false;
	}
	if (!gamateLoad(&gamate, game->data, game->size, bios, sizeof(bios))) {
		message("superwhizkid: unsupported cartridge size");
		return false;
	}
	readOptions();
	pendingPresses = 0;
	if (skipIntro) skipIntroOrReset();
	lcdSnap = true;
	loaded = true;
	return true;
}

bool retro_load_game_special(unsigned type, const struct retro_game_info* info, size_t count) {
	(void) type; (void) info; (void) count;
	return false;
}

void retro_unload_game(void) {
	pendingPresses = 0;
	gamateFree(&gamate);
	loaded = false;
}

unsigned retro_get_region(void) { return RETRO_REGION_NTSC; }
size_t retro_serialize_size(void) { return gamateStateSize(&gamate); }
bool retro_serialize(void* data, size_t size) { return loaded && gamateSaveState(&gamate, data, size); }
bool retro_unserialize(const void* data, size_t size) {
	if (!loaded || !gamateLoadState(&gamate, data, size)) return false;
	lcdSnap = true;
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
