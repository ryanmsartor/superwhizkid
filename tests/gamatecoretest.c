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
static bool optionUpdate, paletteOptionRegistered;
static bool sawPaletteLight, frameIsGreyscale;

static bool environment(unsigned command, void* data) {
	switch (command) {
	case RETRO_ENVIRONMENT_GET_CORE_OPTIONS_VERSION:
		*(unsigned*) data = 1;
		return true;
	case RETRO_ENVIRONMENT_SET_CORE_OPTIONS: {
		const struct retro_core_option_definition* options = data;
		paletteOptionRegistered = options && options[0].key &&
			!strcmp(options[0].key, "superwhizkid_palette");
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

static void writeTestFirmware(void) {
	uint8_t firmware[0x1000] = { 0 };
	// Purpose-written test program: initialize one AY tone and a visible pixel, then mirror $4400
	// to RAM whenever RAM $0002 is nonzero. Tests can pause controller reads by clearing that byte.
	// The temporary discovery filename below does not imply commercial firmware contents.
	static const uint8_t program[] = {
		0x78,                         // SEI
		0xD8,                         // CLD
		0xA9, 0x20, 0x8D, 0x00, 0x40, // tone A period low
		0xA9, 0x00, 0x8D, 0x01, 0x40, // tone A period high
		0xA9, 0x3E, 0x8D, 0x07, 0x40, // enable tone A only
		0xA9, 0x0F, 0x8D, 0x08, 0x40, // channel A volume
		0xA9, 0x00, 0x8D, 0x04, 0x50, // plane 0, X address 0
		0xA9, 0x0A, 0x8D, 0x05, 0x50, // row 10 (initial visible top row)
		0xA9, 0x80, 0x8D, 0x07, 0x50, // set its first pixel
		0xAD, 0x02, 0x00,             // loop: LDA $0002
		0xF0, 0xFB,                   // BEQ loop
		0xAD, 0x00, 0x44,             // LDA $4400
		0x8D, 0x00, 0x00,             // STA $0000
		0xEE, 0x01, 0x00,             // INC $0001
		0x4C, 0x25, 0xE0              // JMP loop
	};
	memcpy(firmware, program, sizeof(program));
	firmware[0xFFC] = 0x00;
	firmware[0xFFD] = 0xE0;
	char path[sizeof(systemDirectory) + 24];
	snprintf(path, sizeof(path), "%s/gamate_bios_umc.bin", systemDirectory);
	FILE* file = fopen(path, "wb");
	assert(file && fwrite(firmware, 1, sizeof(firmware), file) == sizeof(firmware));
	assert(fclose(file) == 0);
}

#define LOAD_SYMBOL(name) do { *(void**) (&p_##name) = dlsym(core, #name); assert(p_##name); } while (0)

int main(int argc, char** argv) {
	assert(argc == 2 && mkdtemp(systemDirectory));
	writeTestFirmware();
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
	p_retro_set_video_refresh(video);
	p_retro_set_audio_sample(audioSample);
	p_retro_set_audio_sample_batch(audioBatch);
	p_retro_set_input_poll(inputPoll);
	p_retro_set_input_state(inputState);
	p_retro_init();
	assert(paletteOptionRegistered);
	size_t stateSizeBeforeLoad = p_retro_serialize_size();
	assert(stateSizeBeforeLoad > 0);

	uint8_t rom[0x8000] = { 0 };
	struct retro_game_info game = { "test-cartridge.gam", rom, sizeof(rom), NULL };
	assert(p_retro_load_game(&game));
	assert(p_retro_serialize_size() == stateSizeBeforeLoad);
	uint8_t* ram = p_retro_get_memory_data(RETRO_MEMORY_SYSTEM_RAM);
	assert(ram);
	ram[2] = 1;
	static const unsigned ids[8] = {
		RETRO_DEVICE_ID_JOYPAD_UP, RETRO_DEVICE_ID_JOYPAD_DOWN,
		RETRO_DEVICE_ID_JOYPAD_LEFT, RETRO_DEVICE_ID_JOYPAD_RIGHT,
		RETRO_DEVICE_ID_JOYPAD_A, RETRO_DEVICE_ID_JOYPAD_B,
		RETRO_DEVICE_ID_JOYPAD_START, RETRO_DEVICE_ID_JOYPAD_SELECT
	};
	for (unsigned bit = 0; bit < 8; bit++) {
		activeButton = ids[bit];
		p_retro_run();
		assert(ram[0] == (uint8_t) ~(1u << bit));
	}
	assert(polls == 8 && videoFrames == 8 && audioBatches == 8 && videoVaries && audioActive);
	paletteOption = "greyscale";
	optionUpdate = true;
	p_retro_run();
	assert(sawPaletteLight && frameIsGreyscale);

	void* state = malloc(stateSizeBeforeLoad);
	assert(state);

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
	rom[0] = 0xA5;
	assert(p_retro_load_game(&game));
	activeButton = RETRO_DEVICE_ID_JOYPAD_SELECT;
	p_retro_run();
	ram = p_retro_get_memory_data(RETRO_MEMORY_SYSTEM_RAM);
	assert(ram && ram[0] == 0x7F);
	p_retro_unload_game();
	p_retro_deinit();
	dlclose(core);

	char firmwarePath[sizeof(systemDirectory) + 24];
	snprintf(firmwarePath, sizeof(firmwarePath), "%s/gamate_bios_umc.bin", systemDirectory);
	assert(unlink(firmwarePath) == 0 && rmdir(systemDirectory) == 0);
	puts("Gamate libretro integration test passed");
	return 0;
}
