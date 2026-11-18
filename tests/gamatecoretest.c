/* Copyright (c) 2026 Ry
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
#include "libretro.h"

#include <assert.h>
#include <dlfcn.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static char systemDirectory[] = "/tmp/superwhizkid-coretest-XXXXXX";
static unsigned activeButton = ~0u, polls, videoFrames, audioBatches;
static bool videoVaries, audioActive;
static const char* paletteOption = "original";
static const char* tapLatchOption = "off";
static const char* ghostingOption = "off";
static const char* biosOption = "umc";
static const char* skipIntroOption = "off";
static bool optionUpdate, modernOptions, legacyOptions, supportModernOptions = true;
static bool sawPaletteLight, frameIsGreyscale;
static uint16_t firstVideoPixel;

static unsigned optionMask(const char* key) {
	static const char* keys[] = {
		"superwhizkid_palette", "superwhizkid_tap_latch", "superwhizkid_lcd_ghosting",
		"superwhizkid_bios", "superwhizkid_skip_intro"
	};
	for (unsigned i = 0; i < sizeof(keys) / sizeof(*keys); i++) {
		if (!strcmp(key, keys[i])) return 1u << i;
	}
	return 0;
}

static unsigned colorDistance(uint16_t a, uint16_t b) {
	int ar = a >> 11, ag = a >> 5 & 63, ab = a & 31;
	int br = b >> 11, bg = b >> 5 & 63, bb = b & 31;
	return (unsigned) (abs(ar - br) + abs(ag - bg) + abs(ab - bb));
}

static bool environment(unsigned command, void* data) {
	switch (command) {
	case RETRO_ENVIRONMENT_GET_CORE_OPTIONS_VERSION:
		if (!supportModernOptions) return false;
		*(unsigned*) data = 1;
		return true;
	case RETRO_ENVIRONMENT_SET_CORE_OPTIONS: {
		const struct retro_core_option_definition* options = data;
		unsigned mask = 0;
		bool defaults = true;
		for (; options && options->key; options++) {
			mask |= optionMask(options->key);
			const char* expected = !strcmp(options->key, "superwhizkid_palette") ? "original" :
				!strcmp(options->key, "superwhizkid_bios") ? "umc" : "off";
			defaults &= options->default_value && !strcmp(options->default_value, expected);
		}
		modernOptions = mask == 0x1F && defaults;
		return true;
	}
	case RETRO_ENVIRONMENT_SET_VARIABLES: {
		const struct retro_variable* variables = data;
		unsigned mask = 0;
		for (; variables && variables->key; variables++) mask |= optionMask(variables->key);
		legacyOptions = mask == 0x1F;
		return true;
	}
	case RETRO_ENVIRONMENT_GET_VARIABLE: {
		struct retro_variable* variable = data;
		if (!strcmp(variable->key, "superwhizkid_palette")) {
			variable->value = paletteOption;
			return true;
		}
		if (!strcmp(variable->key, "superwhizkid_tap_latch")) {
			variable->value = tapLatchOption;
			return true;
		}
		if (!strcmp(variable->key, "superwhizkid_lcd_ghosting")) {
			variable->value = ghostingOption;
			return true;
		}
		if (!strcmp(variable->key, "superwhizkid_bios")) {
			variable->value = biosOption;
			return true;
		}
		if (!strcmp(variable->key, "superwhizkid_skip_intro")) {
			variable->value = skipIntroOption;
			return true;
		}
		return false;
	}
	case RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE:
		*(bool*) data = optionUpdate;
		optionUpdate = false;
		return true;
	case RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY:
		*(const char**) data = systemDirectory;
		return true;
	case RETRO_ENVIRONMENT_SET_PIXEL_FORMAT:
		return *(const enum retro_pixel_format*) data == RETRO_PIXEL_FORMAT_RGB565;
	case RETRO_ENVIRONMENT_SET_CONTROLLER_INFO:
	case RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS:
		return true;
	case RETRO_ENVIRONMENT_SET_MESSAGE:
		fprintf(stderr, "core message: %s\n", ((const struct retro_message*) data)->msg);
		return true;
	default:
		return false;
	}
}

static void video(const void* data, unsigned width, unsigned height, size_t pitch) {
	assert(data && width == 160 && height == 150 && pitch >= width * sizeof(uint16_t));
	const uint16_t* first = data;
	firstVideoPixel = first[0];
	frameIsGreyscale = true;
	for (unsigned y = 0; y < height; y++) {
		const uint16_t* row = (const uint16_t*) ((const uint8_t*) data + y * pitch);
		for (unsigned x = 0; x < width; x++) videoVaries |= row[x] != first[0];
		for (unsigned x = 0; x < width; x++) {
			sawPaletteLight |= row[x] == 0xFFFF;
			frameIsGreyscale &= row[x] == 0xFFFF || row[x] == 0xAD55 ||
				row[x] == 0x52AA || row[x] == 0x0000;
		}
	}
	videoFrames++;
}

static void audioSample(int16_t left, int16_t right) { (void) left; (void) right; }

static size_t audioBatch(const int16_t* data, size_t frames) {
	for (size_t i = 0; i < frames * 2; i++) audioActive |= data[i] != 0;
	audioBatches++;
	return frames;
}

static void inputPoll(void) { polls++; }

static int16_t inputState(unsigned port, unsigned device, unsigned index, unsigned id) {
	return port == 0 && device == RETRO_DEVICE_JOYPAD && index == 0 && id == activeButton;
}

static void writeFirmwareFile(const char* name, const uint8_t* firmware) {
	char path[sizeof(systemDirectory) + 24];
	snprintf(path, sizeof(path), "%s/%s", systemDirectory, name);
	FILE* file = fopen(path, "wb");
	assert(file && fwrite(firmware, 1, 0x1000, file) == 0x1000);
	assert(fclose(file) == 0);
}

static void writeTestFirmware(const char* name, uint8_t marker) {
	uint8_t firmware[0x1000] = { 0 };
	// Purpose-written test program: identify its selected BIOS in RAM $0003, initialize one AY tone
	// and a visible pixel, then mirror $4400
	// to RAM whenever RAM $0002 is nonzero. Tests can pause controller reads by clearing that byte.
	// RAM $0004 controls the first displayed byte so LCD response can be tested deterministically.
	// The temporary discovery filename below does not imply commercial firmware contents.
	uint8_t program[] = {
		0x78,                         // SEI
		0xD8,                         // CLD
		0xA9, marker, 0x8D, 0x03, 0x00, // identify selected synthetic BIOS
		0xA9, 0x20, 0x8D, 0x00, 0x40, // tone A period low
		0xA9, 0x00, 0x8D, 0x01, 0x40, // tone A period high
		0xA9, 0x3E, 0x8D, 0x07, 0x40, // enable tone A only
		0xA9, 0x0F, 0x8D, 0x08, 0x40, // channel A volume
		0xA9, 0x00, 0x8D, 0x04, 0x50, // plane 0, X address 0
		0xA9, 0x0A, 0x8D, 0x05, 0x50, // row 10 (initial visible top row)
		0xA9, 0x80, 0x8D, 0x07, 0x50, // set its first pixel
		0xAD, 0x02, 0x00,             // loop ($E02A): LDA $0002
		0xF0, 0xFB,                   // BEQ loop
		0xAD, 0x00, 0x44,             // LDA $4400
		0x8D, 0x00, 0x00,             // STA $0000
		0xEE, 0x01, 0x00,             // INC $0001
		0xA9, 0x00, 0x8D, 0x04, 0x50, // reset plane 0/X address
		0xA9, 0x0A, 0x8D, 0x05, 0x50, // reset row 10
		0xAD, 0x04, 0x00,             // read controlled pixel byte
		0x8D, 0x07, 0x50,             // write controlled pixel byte
		0x4C, 0x2A, 0xE0              // JMP loop
	};
	memcpy(firmware, program, sizeof(program));
	firmware[0xFFC] = 0x00;
	firmware[0xFFD] = 0xE0;
	writeFirmwareFile(name, firmware);
}

static void writeIntroFirmware(const char* name, uint8_t marker) {
	uint8_t firmware[0x1000] = { 0 };
	size_t p = 0;
#define EMIT(byte) do { assert(p < 0xFFC); firmware[p++] = (byte); } while (0)
	EMIT(0x78); // SEI
	EMIT(0xA9); EMIT(marker); EMIT(0x8D); EMIT(0x03); EMIT(0x00); // BIOS marker
	static const uint8_t key[] = "BIT CORPORATION";
	for (size_t i = 0; i < sizeof(key) - 1; i++) {
		for (int bit = 7; bit >= 0; bit--) {
			EMIT(0xA9); EMIT((key[i] >> bit & 1) ? 4 : 0);
			EMIT(0x8D); EMIT(0x00); EMIT(0x60);
		}
		EMIT(0xA9); EMIT(0); EMIT(0x8D); EMIT(0x00); EMIT(0x60);
	}
	for (unsigned i = 0; i < 8; i++) { EMIT(0xAD); EMIT(0x00); EMIT(0x60); }
	EMIT(0x4C); EMIT(0x20); EMIT(0x60); // cartridge entry
#undef EMIT
	firmware[0xFFC] = 0x00;
	firmware[0xFFD] = 0xE0;
	writeFirmwareFile(name, firmware);
}

#define LOAD_SYMBOL(name) do { *(void**) (&p_##name) = dlsym(core, #name); assert(p_##name); } while (0)

int main(int argc, char** argv) {
	assert(argc == 2 && mkdtemp(systemDirectory));
	writeTestFirmware("gamate_bios_umc.bin", 0x55);
	writeTestFirmware("gamate_bios_bit.bin", 0xAA);
	void* core = dlopen(argv[1], RTLD_NOW);
	assert(core);

	typeof(&retro_set_environment) p_retro_set_environment;
	typeof(&retro_set_video_refresh) p_retro_set_video_refresh;
	typeof(&retro_set_audio_sample) p_retro_set_audio_sample;
	typeof(&retro_set_audio_sample_batch) p_retro_set_audio_sample_batch;
	typeof(&retro_set_input_poll) p_retro_set_input_poll;
	typeof(&retro_set_input_state) p_retro_set_input_state;
	typeof(&retro_init) p_retro_init;
	typeof(&retro_deinit) p_retro_deinit;
	typeof(&retro_load_game) p_retro_load_game;
	typeof(&retro_unload_game) p_retro_unload_game;
	typeof(&retro_run) p_retro_run;
	typeof(&retro_reset) p_retro_reset;
	typeof(&retro_serialize_size) p_retro_serialize_size;
	typeof(&retro_serialize) p_retro_serialize;
	typeof(&retro_unserialize) p_retro_unserialize;
	typeof(&retro_get_memory_data) p_retro_get_memory_data;
	LOAD_SYMBOL(retro_set_environment);
	LOAD_SYMBOL(retro_set_video_refresh);
	LOAD_SYMBOL(retro_set_audio_sample);
	LOAD_SYMBOL(retro_set_audio_sample_batch);
	LOAD_SYMBOL(retro_set_input_poll);
	LOAD_SYMBOL(retro_set_input_state);
	LOAD_SYMBOL(retro_init);
	LOAD_SYMBOL(retro_deinit);
	LOAD_SYMBOL(retro_load_game);
	LOAD_SYMBOL(retro_unload_game);
	LOAD_SYMBOL(retro_run);
	LOAD_SYMBOL(retro_reset);
	LOAD_SYMBOL(retro_serialize_size);
	LOAD_SYMBOL(retro_serialize);
	LOAD_SYMBOL(retro_unserialize);
	LOAD_SYMBOL(retro_get_memory_data);

	p_retro_set_environment(environment);
	assert(modernOptions);
	supportModernOptions = false;
	p_retro_set_environment(environment);
	assert(legacyOptions);
	supportModernOptions = true;
	p_retro_set_environment(environment);
	p_retro_set_video_refresh(video);
	p_retro_set_audio_sample(audioSample);
	p_retro_set_audio_sample_batch(audioBatch);
	p_retro_set_input_poll(inputPoll);
	p_retro_set_input_state(inputState);
	p_retro_init();
	size_t stateSizeBeforeLoad = p_retro_serialize_size();
	assert(stateSizeBeforeLoad > 0);

	uint8_t rom[0x8000] = { 0 };
	struct retro_game_info game = { "test-cartridge.gam", rom, sizeof(rom), NULL };
	assert(p_retro_load_game(&game));
	assert(p_retro_serialize_size() == stateSizeBeforeLoad);
	uint8_t* ram = p_retro_get_memory_data(RETRO_MEMORY_SYSTEM_RAM);
	assert(ram);
	ram[2] = 1;
	// All defaults: UMC BIOS, original palette, raw LCD output, no tap latch, no intro skip.
	activeButton = ~0u;
	p_retro_run();
	assert(ram[3] == 0x55 && firstVideoPixel == 0x8CEB);
	static const unsigned ids[8] = {
		RETRO_DEVICE_ID_JOYPAD_UP, RETRO_DEVICE_ID_JOYPAD_DOWN,
		RETRO_DEVICE_ID_JOYPAD_LEFT, RETRO_DEVICE_ID_JOYPAD_RIGHT,
		RETRO_DEVICE_ID_JOYPAD_A, RETRO_DEVICE_ID_JOYPAD_B,
		RETRO_DEVICE_ID_JOYPAD_START, RETRO_DEVICE_ID_JOYPAD_SELECT
	};
	unsigned pollsBeforeButtons = polls, videoBeforeButtons = videoFrames, audioBeforeButtons = audioBatches;
	for (unsigned bit = 0; bit < 8; bit++) {
		activeButton = ids[bit];
		p_retro_run();
		assert(ram[0] == (uint8_t) ~(1u << bit));
	}
	assert(polls == pollsBeforeButtons + 8 && videoFrames == videoBeforeButtons + 8 &&
		audioBatches == audioBeforeButtons + 8 && videoVaries && audioActive);
	paletteOption = "accurate";
	optionUpdate = true;
	p_retro_run();
	assert(firstVideoPixel == 0x6B89);
	paletteOption = "greyscale";
	optionUpdate = true;
	p_retro_run();
	assert(sawPaletteLight && frameIsGreyscale);
	assert(firstVideoPixel == 0xAD55);
	paletteOption = "invalid";
	optionUpdate = true;
	p_retro_run();
	assert(firstVideoPixel == 0x8CEB); // invalid palette falls back to Original

	// With the latch disabled, a press entirely between controller reads is intentionally missed.
	ram[2] = 0;
	activeButton = ~0u;
	p_retro_run();
	activeButton = RETRO_DEVICE_ID_JOYPAD_START;
	p_retro_run();
	activeButton = ~0u;
	p_retro_run();
	ram[2] = 1;
	p_retro_run();
	assert(ram[0] == 0xFF);

	// Every persistence level must retain progressively more of the previous driven shade.
	paletteOption = "original";
	unsigned previousDistance = 0, previousDarkDistance = 0, firstFilteredDarkDistance = 0;
	static const char* ghostLevels[] = { "off", "1", "2", "3", "4", "5" };
	for (unsigned i = 0; i < sizeof(ghostLevels) / sizeof(*ghostLevels); i++) {
		ghostingOption = ghostLevels[i];
		optionUpdate = true;
		p_retro_reset();
		activeButton = ~0u;
		p_retro_run();
		ram = p_retro_get_memory_data(RETRO_MEMORY_SYSTEM_RAM);
		ram[4] = 0;
		p_retro_run();
		unsigned distance = colorDistance(firstVideoPixel, 0xCE93);
		if (i == 0) assert(distance == 0);
		else assert(distance > previousDistance);
		previousDistance = distance;
		for (unsigned frame = 0; frame < 32; frame++) p_retro_run();
		assert(firstVideoPixel == 0xCE93); // fading reaches the new endpoint

		ram[4] = 0xFF;
		p_retro_run();
		unsigned darkDistance = colorDistance(firstVideoPixel, 0x8CEB);
		if (i <= 1) assert(darkDistance == 0);
		else assert(darkDistance >= previousDarkDistance);
		if (i == 2) firstFilteredDarkDistance = darkDistance;
		previousDarkDistance = darkDistance;
		for (unsigned frame = 0; frame < 32; frame++) p_retro_run();
		assert(firstVideoPixel == 0x8CEB); // darkening also reaches its endpoint
	}
	assert(previousDarkDistance > firstFilteredDarkDistance); // quantized output remains level-ordered

	// Palette interpolation remains valid while persistence is active.
	paletteOption = "greyscale";
	ghostingOption = "5";
	optionUpdate = true;
	p_retro_reset();
	p_retro_run();
	ram = p_retro_get_memory_data(RETRO_MEMORY_SYSTEM_RAM);
	ram[4] = 0;
	p_retro_run();
	assert(firstVideoPixel != 0xFFFF && firstVideoPixel != 0xAD55);

	void* state = malloc(stateSizeBeforeLoad);
	assert(state);
	assert(p_retro_serialize(state, stateSizeBeforeLoad));
	assert(!p_retro_unserialize(state, stateSizeBeforeLoad - 1));
	p_retro_run();
	assert(firstVideoPixel != 0xFFFF); // failed load did not force the LCD response to snap

	paletteOption = "original";
	ghostingOption = "off";
	optionUpdate = true;
	p_retro_reset();
	ram = p_retro_get_memory_data(RETRO_MEMORY_SYSTEM_RAM);

	// The convenience latch belongs to the libretro adapter: loading machine state neither restores
	// a latch saved earlier nor clears a press accumulated after the save.
	tapLatchOption = "on";
	optionUpdate = true;
	ram[2] = 0;
	activeButton = ~0u;
	p_retro_run(); // let the test firmware settle into its no-controller-read loop
	activeButton = RETRO_DEVICE_ID_JOYPAD_START;
	p_retro_run();
	assert(p_retro_serialize(state, stateSizeBeforeLoad));
	activeButton = ~0u;
	ram[2] = 1;
	p_retro_run();
	assert(ram[0] == 0xBF);
	assert(p_retro_unserialize(state, stateSizeBeforeLoad));
	ram[2] = 1;
	p_retro_run();
	assert(ram[0] == 0xFF); // loading did not restore the saved frontend latch

	ram[2] = 0;
	assert(p_retro_serialize(state, stateSizeBeforeLoad));
	activeButton = RETRO_DEVICE_ID_JOYPAD_START;
	p_retro_run();
	assert(p_retro_unserialize(state, stateSizeBeforeLoad));
	activeButton = ~0u;
	ram[2] = 1;
	p_retro_run();
	assert(ram[0] == 0xBF); // loading did not clear the newer frontend latch

	tapLatchOption = "off";
	optionUpdate = true;
	assert(p_retro_serialize(state, stateSizeBeforeLoad));
	uint8_t savedCounter = ram[1];
	activeButton = RETRO_DEVICE_ID_JOYPAD_A;
	p_retro_run();
	assert(ram[1] != savedCounter);
	assert(p_retro_unserialize(state, stateSizeBeforeLoad) && ram[1] == savedCounter);
	free(state);

	p_retro_reset();
	assert(ram[0] == 0xFF && ram[1] == 0xFF);
	p_retro_run();
	assert(ram[0] == 0xEF);
	p_retro_unload_game();

	// BIOS preference selects the requested file and falls back to the other valid BIOS if missing.
	biosOption = "bit";
	rom[0] = 0xA5;
	assert(p_retro_load_game(&game));
	activeButton = ~0u;
	p_retro_run();
	ram = p_retro_get_memory_data(RETRO_MEMORY_SYSTEM_RAM);
	assert(ram && ram[3] == 0xAA);
	biosOption = "umc";
	optionUpdate = true;
	ram[3] = 0;
	p_retro_reset();
	p_retro_run();
	assert(ram[3] == 0xAA); // reset retains the BIOS selected when content was loaded
	p_retro_unload_game();
	assert(p_retro_load_game(&game));
	p_retro_run();
	ram = p_retro_get_memory_data(RETRO_MEMORY_SYSTEM_RAM);
	assert(ram && ram[3] == 0x55); // content reload applies the changed preference
	p_retro_unload_game();
	biosOption = "bit";
	char bitFirmwarePath[sizeof(systemDirectory) + 24];
	snprintf(bitFirmwarePath, sizeof(bitFirmwarePath), "%s/gamate_bios_bit.bin", systemDirectory);
	assert(unlink(bitFirmwarePath) == 0);
	assert(p_retro_load_game(&game));
	p_retro_run();
	ram = p_retro_get_memory_data(RETRO_MEMORY_SYSTEM_RAM);
	assert(ram && ram[3] == 0x55);
	p_retro_unload_game();
	writeTestFirmware("gamate_bios_bit.bin", 0xAA);

	// Invalid option values use documented defaults rather than enabling features accidentally.
	biosOption = "invalid";
	paletteOption = "invalid";
	tapLatchOption = "invalid";
	ghostingOption = "invalid";
	skipIntroOption = "invalid";
	assert(p_retro_load_game(&game));
	ram = p_retro_get_memory_data(RETRO_MEMORY_SYSTEM_RAM);
	assert(ram && ram[3] == 0xFF); // invalid skip value falls back to off
	p_retro_run();
	assert(ram && ram[3] == 0x55 && firstVideoPixel == 0x8CEB);
	ram[2] = 0;
	activeButton = ~0u;
	p_retro_run();
	activeButton = RETRO_DEVICE_ID_JOYPAD_START;
	p_retro_run();
	activeButton = ~0u;
	p_retro_run();
	ram[2] = 1;
	p_retro_run();
	assert(ram[0] == 0xFF); // invalid tap-latch value falls back to off
	p_retro_unload_game();

	// A purpose-written BIOS performs the protection handshake and jumps to the authentic cartridge
	// entry, allowing intro skip to be checked synchronously on both load and reset.
	writeIntroFirmware("gamate_bios_umc.bin", 0x55);
	writeIntroFirmware("gamate_bios_bit.bin", 0xAA);
	biosOption = "bit";
	skipIntroOption = "on";
	assert(p_retro_load_game(&game));
	ram = p_retro_get_memory_data(RETRO_MEMORY_SYSTEM_RAM);
	assert(ram && ram[3] == 0xAA);
	ram[3] = 0;
	p_retro_reset();
	assert(ram[3] == 0xAA);
	p_retro_unload_game();

	skipIntroOption = "off";
	biosOption = "umc";
	assert(p_retro_load_game(&game));
	ram = p_retro_get_memory_data(RETRO_MEMORY_SYSTEM_RAM);
	assert(ram && ram[3] == 0xFF); // ordinary boot remains at reset until the frontend runs it
	p_retro_unload_game();
	p_retro_deinit();
	dlclose(core);

	char umcFirmwarePath[sizeof(systemDirectory) + 24];
	snprintf(umcFirmwarePath, sizeof(umcFirmwarePath), "%s/gamate_bios_umc.bin", systemDirectory);
	assert(unlink(umcFirmwarePath) == 0 && unlink(bitFirmwarePath) == 0 && rmdir(systemDirectory) == 0);
	puts("Gamate libretro integration test passed");
	return 0;
}
