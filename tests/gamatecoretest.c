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
static int16_t activeAnalogX, activeAnalogY;
static bool videoVaries, audioActive, audioChannelsDiffer, lastAudioChannelsEqual;
static int16_t lastAudio[2048 * 2];
static size_t lastAudioFrames;
static const char* paletteOption = "original";
static const char* contrastOption = "8";
static const char* audioOutputOption = "stereo";
static const char* tapLatchOption = "off";
static const char* ghostingOption = "off";
static const char* flickerOption = "off";
static const char* biosOption = "umc";
static const char* skipIntroOption = "off";
static bool optionUpdate, modernOptions, legacyOptions, supportModernOptions = true;
static bool sawPaletteLight, frameIsGreyscale;
static uint16_t firstVideoPixel;
static bool reportVideoHash;
static uint32_t videoHash = 2166136261U;
static unsigned messageCount;
static char lastMessage[128];
static struct retro_netpacket_callback netpacket;
static bool netpacketRegistered;
static uint8_t sentPacket[512];
static size_t sentPacketSize;
static int sentPacketFlags;
static uint16_t sentPacketClient;

static void netpacketSend(int flags, const void* data, size_t size, uint16_t clientId) {
	assert(data && size <= sizeof(sentPacket));
	memcpy(sentPacket, data, size);
	sentPacketSize = size;
	sentPacketFlags = flags;
	sentPacketClient = clientId;
}

static unsigned optionMask(const char* key) {
	static const char* keys[] = {
		"superwhizkid_palette", "superwhizkid_contrast", "superwhizkid_audio_output",
		"superwhizkid_tap_latch",
		"superwhizkid_lcd_ghosting",
		"superwhizkid_flicker_reduction", "superwhizkid_bios", "superwhizkid_skip_intro"
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
		bool defaults = true, audioValues = false, paletteLabels = false;
		bool contrastValues = false, flickerValues = false;
		for (; options && options->key; options++) {
			mask |= optionMask(options->key);
			if (!strcmp(options->key, "superwhizkid_audio_output")) {
				audioValues = options->values[0].value &&
					!strcmp(options->values[0].value, "stereo") &&
					options->values[1].value && !strcmp(options->values[1].value, "mono") &&
					options->values[2].value && !strcmp(options->values[2].value, "speaker") &&
					!options->values[3].value;
			}
			if (!strcmp(options->key, "superwhizkid_palette")) {
				paletteLabels = options->values[1].label &&
					!strcmp(options->values[1].label, "Gamate LCD");
			}
			if (!strcmp(options->key, "superwhizkid_contrast")) {
				contrastValues = true;
				for (unsigned i = 0; i < 16; i++) {
					char expectedValue[3];
					snprintf(expectedValue, sizeof(expectedValue), "%u", i + 1);
					contrastValues &= options->values[i].value &&
						!strcmp(options->values[i].value, expectedValue);
				}
				contrastValues &= !options->values[16].value && options->values[7].label &&
					!strcmp(options->values[7].label, "8 (default)");
			}
			if (!strcmp(options->key, "superwhizkid_flicker_reduction")) {
				flickerValues = options->values[3].value &&
					!strcmp(options->values[3].value, "3") && !options->values[4].value;
			}
			const char* expected = !strcmp(options->key, "superwhizkid_palette") ? "original" :
				!strcmp(options->key, "superwhizkid_contrast") ? "8" :
				!strcmp(options->key, "superwhizkid_audio_output") ? "stereo" :
				!strcmp(options->key, "superwhizkid_bios") ? "umc" : "off";
			defaults &= options->default_value && !strcmp(options->default_value, expected);
		}
		modernOptions = mask == 0xFF && defaults && audioValues && paletteLabels &&
			contrastValues && flickerValues;
		return true;
	}
	case RETRO_ENVIRONMENT_SET_VARIABLES: {
		const struct retro_variable* variables = data;
		unsigned mask = 0;
		for (; variables && variables->key; variables++) mask |= optionMask(variables->key);
		legacyOptions = mask == 0xFF;
		return true;
	}
	case RETRO_ENVIRONMENT_SET_NETPACKET_INTERFACE:
		netpacket = *(const struct retro_netpacket_callback*) data;
		netpacketRegistered = netpacket.start && netpacket.receive && netpacket.stop &&
			netpacket.poll && netpacket.connected && netpacket.disconnected &&
			netpacket.protocol_version && !strcmp(netpacket.protocol_version,
				"superwhizkid-gamate-link-1");
		return true;
	case RETRO_ENVIRONMENT_GET_VARIABLE: {
		struct retro_variable* variable = data;
		if (!strcmp(variable->key, "superwhizkid_palette")) {
			variable->value = paletteOption;
			return true;
		}
		if (!strcmp(variable->key, "superwhizkid_contrast")) {
			variable->value = contrastOption;
			return true;
		}
		if (!strcmp(variable->key, "superwhizkid_audio_output")) {
			variable->value = audioOutputOption;
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
		if (!strcmp(variable->key, "superwhizkid_flicker_reduction")) {
			variable->value = flickerOption;
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
	case RETRO_ENVIRONMENT_SET_VARIABLE: {
		const struct retro_variable* variable = data;
		if (variable && !strcmp(variable->key, "superwhizkid_contrast")) {
			contrastOption = variable->value;
			return true;
		}
		return false;
	}
	case RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY:
		*(const char**) data = systemDirectory;
		return true;
	case RETRO_ENVIRONMENT_SET_PIXEL_FORMAT:
		return *(const enum retro_pixel_format*) data == RETRO_PIXEL_FORMAT_RGB565;
	case RETRO_ENVIRONMENT_SET_CONTROLLER_INFO:
	case RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS:
		return true;
	case RETRO_ENVIRONMENT_SET_MESSAGE:
		messageCount++;
		snprintf(lastMessage, sizeof(lastMessage), "%s", ((const struct retro_message*) data)->msg);
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
			if (reportVideoHash) {
				videoHash = (videoHash ^ (uint8_t) row[x]) * 16777619U;
				videoHash = (videoHash ^ (row[x] >> 8)) * 16777619U;
			}
			sawPaletteLight |= row[x] == 0xFFFF;
			frameIsGreyscale &= row[x] == 0xFFFF || row[x] == 0xAD55 ||
				row[x] == 0x52AA || row[x] == 0x0000;
		}
	}
	videoFrames++;
}

static void audioSample(int16_t left, int16_t right) { (void) left; (void) right; }

static size_t audioBatch(const int16_t* data, size_t frames) {
	assert(frames <= sizeof(lastAudio) / sizeof(lastAudio[0]) / 2);
	memcpy(lastAudio, data, frames * 2 * sizeof(*data));
	lastAudioFrames = frames;
	lastAudioChannelsEqual = true;
	for (size_t i = 0; i < frames; i++) {
		audioActive |= data[i * 2] != 0 || data[i * 2 + 1] != 0;
		audioChannelsDiffer |= data[i * 2] != data[i * 2 + 1];
		lastAudioChannelsEqual &= data[i * 2] == data[i * 2 + 1];
	}
	audioBatches++;
	return frames;
}

static void inputPoll(void) { polls++; }

static int16_t inputState(unsigned port, unsigned device, unsigned index, unsigned id) {
	if (port) return 0;
	if (device == RETRO_DEVICE_JOYPAD) return index == 0 && id == activeButton;
	if (device == RETRO_DEVICE_ANALOG && index == RETRO_DEVICE_INDEX_ANALOG_LEFT) {
		if (id == RETRO_DEVICE_ID_ANALOG_X) return activeAnalogX;
		if (id == RETRO_DEVICE_ID_ANALOG_Y) return activeAnalogY;
	}
	return 0;
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
	// Setting RAM $0005 writes RAM $0006 to the UART and emits its eight clocked data bits once.
	// The temporary discovery filename below does not imply commercial firmware contents.
	uint8_t program[] = {
		0x78,                         // SEI
		0xD8,                         // CLD
		0xA9, marker, 0x8D, 0x03, 0x00, // identify selected synthetic BIOS
		0xA9, 0x20, 0x8D, 0x00, 0x40, // tone A period low
		0xA9, 0x00, 0x8D, 0x01, 0x40, // tone A period high
		0xA9, 0x3E, 0x8D, 0x07, 0x40, // enable tone A only
		0xA9, 0x0F, 0x8D, 0x08, 0x40, // channel A volume
		0xA9, 0x00, 0x8D, 0x09, 0x00, // default controlled plane-1 byte to zero
		0xA9, 0x00, 0x8D, 0x04, 0x50, // plane 0, X address 0
		0xA9, 0x0A, 0x8D, 0x05, 0x50, // row 10 (initial visible top row)
		0xA9, 0x80, 0x8D, 0x07, 0x50, // set its first pixel
		0xAD, 0x02, 0x00,             // loop ($E02F): LDA $0002
		0xF0, 0xFB,                   // BEQ loop
		0xAD, 0x00, 0x44,             // LDA $4400
		0x8D, 0x00, 0x00,             // STA $0000
		0xEE, 0x01, 0x00,             // INC $0001
		0xA9, 0x00, 0x8D, 0x04, 0x50, // reset plane 0/X address
		0xA9, 0x0A, 0x8D, 0x05, 0x50, // reset row 10
		0xAD, 0x04, 0x00,             // read controlled pixel byte
		0x8D, 0x07, 0x50,             // write controlled pixel byte
		0xA9, 0x80, 0x8D, 0x04, 0x50, // reset plane 1/X address
		0xA9, 0x0A, 0x8D, 0x05, 0x50, // reset row 10
		0xAD, 0x09, 0x00,             // read controlled plane-1 byte
		0x8D, 0x07, 0x50,             // write controlled plane-1 byte
		0xAD, 0x05, 0x00,             // check one-shot UART request
		0xF0, 0x23,                   // skip UART block when zero
		0xAD, 0x06, 0x00,             // transmitter data
		0x8D, 0x00, 0x44,             // load UART transmitter
		0xAD, 0x00, 0x4C, 0xAD, 0x00, 0x4C, // emit eight UART clocks
		0xAD, 0x00, 0x4C, 0xAD, 0x00, 0x4C,
		0xAD, 0x00, 0x4C, 0xAD, 0x00, 0x4C,
		0xAD, 0x00, 0x4C, 0xAD, 0x00, 0x4C,
		0xA9, 0x00, 0x8D, 0x05, 0x00, // clear one-shot request
		0x4C, 0x2F, 0xE0              // JMP loop
	};
	memcpy(firmware, program, sizeof(program));
	uint8_t nmi[] = {
		0xAD, 0x00, 0x48, 0x8D, 0x07, 0x00, // save received byte
		0xEE, 0x08, 0x00, 0x40              // count received-byte NMIs, RTI
	};
	memcpy(firmware + 0x100, nmi, sizeof(nmi));
	firmware[0xFFA] = 0x00;
	firmware[0xFFB] = 0xE1;
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
	reportVideoHash = getenv("GAMATECORETEST_VIDEO_HASH") != NULL;
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
	assert(netpacketRegistered);
	size_t stateSizeBeforeLoad = p_retro_serialize_size();
	assert(stateSizeBeforeLoad > 0);

	uint8_t rom[0x8000] = { 0 };
	struct retro_game_info game = { "test-cartridge.gam", rom, sizeof(rom), NULL };
	assert(p_retro_load_game(&game));
	assert(p_retro_serialize_size() == stateSizeBeforeLoad);
	uint8_t* ram = p_retro_get_memory_data(RETRO_MEMORY_SYSTEM_RAM);
	assert(ram);
	// Netpacket transports each locally clocked UART bit reliably and accepts only one peer.
	netpacket.start(0, netpacketSend, NULL);
	assert(netpacket.connected(7));
	assert(!netpacket.connected(8));
	ram[5] = 1;
	ram[6] = 0xA5;
	p_retro_run();
	assert(sentPacketSize == 53 && sentPacket[0] == 'G' && sentPacket[1] == 'L' &&
		sentPacket[2] == 1 && sentPacket[3] == 8 && sentPacket[4] == 0);
	uint8_t sentByte = 0;
	for (unsigned bit = 0; bit < 8; bit++) sentByte = (uint8_t) (sentByte << 1 | sentPacket[17 + bit * 5]);
	assert(sentByte == 0xA5);
	assert((sentPacketFlags & (RETRO_NETPACKET_RELIABLE | RETRO_NETPACKET_FLUSH_HINT)) ==
		(RETRO_NETPACKET_RELIABLE | RETRO_NETPACKET_FLUSH_HINT));
	assert(sentPacketClient == 7 && ram[5] == 0);
	ram[8] = 0;
	uint8_t receivedPacket[53] = { 'G', 'L', 1, 8, 0 };
	for (unsigned bit = 0; bit < 8; bit++) {
		receivedPacket[13 + bit * 5] = (uint8_t) (bit * 2); // sender-clock delta
		receivedPacket[17 + bit * 5] = (0x3C >> (7 - bit)) & 1;
	}
	netpacket.receive(receivedPacket, sizeof(receivedPacket), 7);
	p_retro_run();
	assert(ram[7] == 0x3C && ram[8] == 1); // eight bits requested NMI and exposed the UART byte
	uint8_t twoBytePacket[93] = { 'G', 'L', 1, 16, 0 };
	uint64_t secondBase = 73000;
	for (unsigned byte = 0; byte < 8; byte++) twoBytePacket[5 + byte] = secondBase >> (byte * 8);
	for (unsigned bit = 0; bit < 16; bit++) {
		unsigned delta = bit < 8 ? bit * 4 : 200 + (bit - 8) * 4;
		twoBytePacket[13 + bit * 5] = (uint8_t) delta;
		twoBytePacket[14 + bit * 5] = (uint8_t) (delta >> 8);
		uint8_t value = bit < 8 ? 0xA5 : 0x5A;
		twoBytePacket[17 + bit * 5] = (value >> (7 - (bit & 7))) & 1;
	}
	netpacket.receive(twoBytePacket, sizeof(twoBytePacket), 7);
	p_retro_run();
	assert(ram[7] == 0x5A && ram[8] == 3); // both NMI edges survived packet delivery
	netpacket.disconnected(7);
	assert(netpacket.connected(8));
	netpacket.disconnected(8);
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
		audioBatches == audioBeforeButtons + 8 && videoVaries && audioActive && audioChannelsDiffer);
	activeButton = ~0u;
	static const int16_t analogDirections[][2] = {
		{ 0, -32767 }, { 0, 32767 }, { -32767, 0 }, { 32767, 0 }
	};
	for (unsigned bit = 0; bit < 4; bit++) {
		activeAnalogX = analogDirections[bit][0];
		activeAnalogY = analogDirections[bit][1];
		p_retro_run();
		assert(ram[0] == (uint8_t) ~(1u << bit));
	}
	activeAnalogX = activeAnalogY = 0;
	activeAnalogX = 0x3000;
	p_retro_run();
	assert(ram[0] == 0xFF); // ordinary stick drift remains inside the dead zone
	activeAnalogX = 0;
	// Mono is a presentation-layer 1:1 downmix and takes effect without reloading content.
	audioOutputOption = "mono";
	optionUpdate = true;
	p_retro_reset();
	p_retro_run();
	assert(lastAudioChannelsEqual);
	int16_t monoAudio[sizeof(lastAudio) / sizeof(lastAudio[0])];
	size_t monoFrames = lastAudioFrames;
	assert(monoFrames > 0);
	memcpy(monoAudio, lastAudio, monoFrames * 2 * sizeof(*monoAudio));
	audioOutputOption = "speaker";
	optionUpdate = true;
	p_retro_reset();
	p_retro_run();
	assert(lastAudioChannelsEqual && lastAudioFrames == monoFrames);
	bool speakerFiltered = false;
	for (size_t i = 0; i < monoFrames * 2; i++) speakerFiltered |= lastAudio[i] != monoAudio[i];
	assert(speakerFiltered);
	audioOutputOption = "stereo";
	optionUpdate = true;
	p_retro_reset();
	p_retro_run();
	assert(!lastAudioChannelsEqual);
	paletteOption = "accurate";
	optionUpdate = true;
	p_retro_run();
	assert(firstVideoPixel == 0x6348);
	paletteOption = "greyscale";
	optionUpdate = true;
	p_retro_run();
	assert(sawPaletteLight && frameIsGreyscale);
	assert(firstVideoPixel == 0xAD55);
	paletteOption = "invalid";
	optionUpdate = true;
	p_retro_run();
	assert(firstVideoPixel == 0x8CEB); // invalid palette falls back to Original

	// Contrast level 8 preserves the selected palette exactly. Level 1 is the minimum, and level 16
	// collapses all three ink shades to a palette-specific dark endpoint while leaving the background
	// unchanged.
	contrastOption = "1";
	optionUpdate = true;
	p_retro_run();
	assert(firstVideoPixel != 0xCE93 &&
		colorDistance(firstVideoPixel, 0xCE93) < colorDistance(0x8CEB, 0xCE93));
	activeButton = RETRO_DEVICE_ID_JOYPAD_L3;
	p_retro_run();
	assert(!strcmp(contrastOption, "1"));
	activeButton = ~0u;
	p_retro_run();
	contrastOption = "0"; // removed legacy level is invalid and falls back to the default
	optionUpdate = true;
	p_retro_run();
	assert(firstVideoPixel == 0x8CEB);
	unsigned defaultContrastDistance = colorDistance(firstVideoPixel, 0xCE93);
	contrastOption = "16";
	optionUpdate = true;
	p_retro_run();
	assert(firstVideoPixel == 0x0040); // Original retains its green-black ink character
	ram[4] = 0;
	ram[9] = 0xFF;
	p_retro_run();
	assert(firstVideoPixel == 0x0040);
	ram[4] = 0xFF;
	p_retro_run();
	assert(firstVideoPixel == 0x0040);
	paletteOption = "accurate";
	optionUpdate = true;
	p_retro_run();
	assert(firstVideoPixel == 0x0848); // Gamate LCD converges on blue-black
	paletteOption = "greyscale";
	optionUpdate = true;
	p_retro_run();
	assert(firstVideoPixel == 0x0000 && frameIsGreyscale); // neutral through absolute black
	paletteOption = "original";
	optionUpdate = true;
	p_retro_run();
	assert(firstVideoPixel == 0x0040);
	activeButton = RETRO_DEVICE_ID_JOYPAD_R3;
	p_retro_run();
	assert(!strcmp(contrastOption, "16") && firstVideoPixel == 0x0040);
	activeButton = ~0u;
	p_retro_run();
	ram[4] = 0;
	ram[9] = 0;
	p_retro_run();
	assert(firstVideoPixel == 0xCE93); // the background color never changes
	ram[4] = 0xFF;
	contrastOption = "8";
	optionUpdate = true;
	p_retro_run();
	assert(firstVideoPixel == 0x8CEB);
	activeButton = RETRO_DEVICE_ID_JOYPAD_L3;
	p_retro_run();
	assert(!strcmp(contrastOption, "7") &&
		colorDistance(firstVideoPixel, 0xCE93) < defaultContrastDistance);
	activeButton = ~0u;
	p_retro_run();
	activeButton = RETRO_DEVICE_ID_JOYPAD_R3;
	p_retro_run();
	assert(!strcmp(contrastOption, "8") && firstVideoPixel == 0x8CEB);
	activeButton = ~0u;

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

	// The one-frame filter accepts sustained lightening on its second frame and still applies a
	// darker change immediately.
	flickerOption = "1";
	optionUpdate = true;
	p_retro_reset();
	p_retro_run();
	ram = p_retro_get_memory_data(RETRO_MEMORY_SYSTEM_RAM);
	ram[4] = 0;
	p_retro_run();
	assert(firstVideoPixel == 0x8CEB);
	ram[4] = 0xFF;
	p_retro_run();
	assert(firstVideoPixel == 0x8CEB);
	ram[4] = 0;
	p_retro_run();
	assert(firstVideoPixel == 0x8CEB);
	p_retro_run();
	assert(firstVideoPixel == 0xCE93);
	ram[4] = 0xFF;
	p_retro_run();
	assert(firstVideoPixel == 0x8CEB);

	// The two-frame filter also rejects a two-frame light flash, accepting the change only on its
	// third consecutive frame. This covers the longest transient cursor erasure in the Brick
	// Blaster trace without changing the immediate-darkening rule.
	flickerOption = "2";
	optionUpdate = true;
	p_retro_reset();
	p_retro_run();
	ram = p_retro_get_memory_data(RETRO_MEMORY_SYSTEM_RAM);
	ram[4] = 0;
	p_retro_run();
	assert(firstVideoPixel == 0x8CEB);
	p_retro_run();
	assert(firstVideoPixel == 0x8CEB);
	ram[4] = 0xFF;
	p_retro_run();
	assert(firstVideoPixel == 0x8CEB);
	ram[4] = 0;
	p_retro_run();
	assert(firstVideoPixel == 0x8CEB);
	p_retro_run();
	assert(firstVideoPixel == 0x8CEB);
	p_retro_run();
	assert(firstVideoPixel == 0xCE93);
	ram[4] = 0xFF;
	p_retro_run();
	assert(firstVideoPixel == 0x8CEB);

	// The three-frame setting accepts a sustained lightening only on its fourth frame.
	flickerOption = "3";
	optionUpdate = true;
	p_retro_reset();
	p_retro_run();
	ram = p_retro_get_memory_data(RETRO_MEMORY_SYSTEM_RAM);
	ram[4] = 0;
	for (unsigned frame = 0; frame < 3; frame++) {
		p_retro_run();
		assert(firstVideoPixel == 0x8CEB);
	}
	p_retro_run();
	assert(firstVideoPixel == 0xCE93);
	ram[4] = 0xFF;
	p_retro_run();
	assert(firstVideoPixel == 0x8CEB);
	flickerOption = "off";
	optionUpdate = true;
	ram[4] = 0;
	p_retro_run();
	assert(firstVideoPixel == 0xCE93);

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
	contrastOption = "invalid";
	audioOutputOption = "invalid";
	tapLatchOption = "invalid";
	ghostingOption = "invalid";
	flickerOption = "invalid";
	skipIntroOption = "invalid";
	assert(p_retro_load_game(&game));
	ram = p_retro_get_memory_data(RETRO_MEMORY_SYSTEM_RAM);
	assert(ram && ram[3] == 0xFF); // invalid skip value falls back to off
	p_retro_run();
	assert(ram && ram[3] == 0x55 && firstVideoPixel == 0x8CEB && !lastAudioChannelsEqual);
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

	// A BIOS that never reaches cartridge entry hits the master-clock deadline. Load/reset both
	// report the failure and return to a clean ordinary reset state instead of continuing mid-BIOS.
	writeTestFirmware("gamate_bios_umc.bin", 0x55);
	writeTestFirmware("gamate_bios_bit.bin", 0xAA);
	biosOption = "umc";
	unsigned messagesBeforeTimeout = messageCount;
	assert(p_retro_load_game(&game));
	ram = p_retro_get_memory_data(RETRO_MEMORY_SYSTEM_RAM);
	assert(ram && ram[3] == 0xFF && messageCount == messagesBeforeTimeout + 1);
	assert(strstr(lastMessage, "timed out") && strstr(lastMessage, "normal boot"));
	ram[3] = 0;
	p_retro_reset();
	assert(ram[3] == 0xFF && messageCount == messagesBeforeTimeout + 2);
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
	if (reportVideoHash) printf("presentation video hash: %08X (%u frames)\n", videoHash, videoFrames);
	puts("Gamate libretro integration test passed");
	return 0;
}
