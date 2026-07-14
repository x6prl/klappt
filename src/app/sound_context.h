#pragma once

#include "SDL3/SDL_atomic.h"
#include "SDL3/SDL_audio.h"
#include "base/arena.h"
#include <cstdint>

struct SherpaOnnxOfflineTts;
struct SherpaOnnxOfflineStream;
struct SherpaOnnxOfflineRecognizer;

struct SoundContext {
	constexpr static auto FREQUENCY = 16000; // default for sherpa onnx
	constexpr static int TRUE = 1;
	constexpr static int FALSE = 0;

	struct {
		void *data{nullptr};
		Size size_bytes{0};
		const Size capacity_bytes{0};
	} audio;
	struct {
		const SherpaOnnxOfflineTts *tts{};
		const SherpaOnnxOfflineStream *offline_stream{};
		const SherpaOnnxOfflineRecognizer *asr{};
	} sherpa_ctx;
	/*
	 * =====================================
	 * ui thread controlled variables
	 */
	bool is_recording_button_pressed{false};
	uint64_t recording_start_ticks_ms{0};
	/*
	 * =====================================
	 * dedicated thread controlled variables
	 */
	SDL_AtomicInt is_initialized{.value = 0};
	SDL_AtomicInt is_recording{.value = 0};
	SDL_AtomicInt is_asr_in_progress{.value = 0};
	SDL_AtomicInt is_playing{.value = 0};
	SDL_AudioStream *recording_stream{nullptr};

	static float bytes_to_seconds(Size size) {
		return static_cast<float>(size << 2) / static_cast<float>(FREQUENCY);
	}
	static float ticks_diff_to_seconds(uint64_t init, uint64_t now) {
		return static_cast<float>(now - init) / 1000.f;
	}
};
