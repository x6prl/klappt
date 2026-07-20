#pragma once

#include "app/audio_context.h"
#include "base/dyn_arr.h"
#include <sherpa-onnx/c-api/c-api.h>

struct SherpaOnnxOfflineTts;
struct SherpaOnnxOfflineStream;
struct SherpaOnnxOfflineRecognizer;

struct NeuroContext {

	struct {
		const SherpaOnnxOfflineTts *offline_tts{};
		const SherpaOnnxOfflineStream *offline_stream{};
		const SherpaOnnxOfflineRecognizer *offline_recognizer{};
	} sherpa_ctx;

	DynArr<PlaybackPayload> playback_payload_pool{};

	Size get_unused_payload() {
		for (Size i = 0; i < playback_payload_pool.size; ++i) {
			if (!playback_payload_pool[i].is_used) {
				playback_payload_pool[i].is_used = true;
				SDL_Log("pool idx %d", i);
				return i;
			}
		}
		SDL_LogError(SDL_LOG_CATEGORY_ERROR, "playback payload pool was full!");
		return -1;
	}

	void release_payload_and_free_generated_audio_if_needed(Size idx) {
		auto &pp = playback_payload_pool[idx];
		if (pp.generated_audio) {
			SherpaOnnxDestroyOfflineTtsGeneratedAudio(pp.generated_audio);
			pp.generated_audio = nullptr;
		}
		pp.audio_data = nullptr;
		pp.audio_data_size = 0;
		pp.is_used = false;
	}
};
