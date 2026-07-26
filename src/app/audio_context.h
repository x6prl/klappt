#pragma once

#include "SDL3/SDL_audio.h"
#include "base/arena.h"
#include "base/atomic.h"
#include <cstdint>

struct SherpaOnnxGeneratedAudio;

struct PlayGeneratedAudioDataPayload {};

struct PlaybackPayload {
	bool is_used{false};
	Size pp_index{};
	SDL_AudioSpec spec{.format = SDL_AUDIO_F32, .channels = 1};
	const void *audio_data{}; // NOTE: ptr to generated_audio->samples
	Size audio_data_size{};
	union {
		const SherpaOnnxGeneratedAudio *generated_audio{
			  nullptr}; // owns data, ptr to be freed after usage
	};
};

struct AudioContext {
	constexpr static auto FREQUENCY = 16000; // default for sherpa onnx

	struct {
		void *data{nullptr};
		AtomicInt size_bytes{0};
		const Size capacity_bytes{0};
	} rec_audio_buffer;

	// TODO: save and handle is_recording state in AudioThread too
	// SDL_AtomicInt is_recording{.value = 0};

	// SDL_AtomicInt is_asr_in_progress{.value = 0};
	// SDL_AtomicInt is_playing{.value = 0};

	SDL_AudioStream *recording_stream{nullptr};
	SDL_AudioStream *playback_stream{nullptr};

	// TODO: move such a data to MainThread, don't touch AudioThread
	static float bytes_to_seconds(Size size) {
		return static_cast<float>(size ) / (static_cast<float>(FREQUENCY) * sizeof(float));
	}
};

struct UI_Audio_ASR_TTS {
	// struct {
	// 	void *data{nullptr};
	// 	Size size_bytes{0};
	// 	const Size capacity_bytes{0};
	// } audio;
	// struct {
	// 	const SherpaOnnxOfflineTts *tts{};
	// 	const SherpaOnnxOfflineStream *offline_stream{};
	// 	const SherpaOnnxOfflineRecognizer *asr{};
	// } sherpa_ctx;
	/*
	 * =====================================
	 * ui thread controlled variables
	 */

	bool is_asr_initialized{false};
	bool is_asr_in_progress{false};

	bool is_tts_initialized{false};

	bool is_recording_initialized{false};
	bool is_recording{false};
	bool is_recording_button_pressed{false};
	uint64_t recording_start_ticks_ms{0};

	// bool is_play_initialized{false};
	// bool is_playing{false};

	/*
	 * =====================================
	 * dedicated thread controlled variables
	 */
	// SDL_AtomicInt is_initialized{.value = 0};
	// SDL_AtomicInt is_recording{.value = 0};
	// SDL_AtomicInt is_asr_in_progress{.value = 0};
	// SDL_AtomicInt is_playing{.value = 0};
	// SDL_AudioStream *recording_stream{nullptr};

	static float ticks_diff_to_seconds(uint64_t init, uint64_t now) {
		return static_cast<float>(now - init) / 1000.f;
	}
};
