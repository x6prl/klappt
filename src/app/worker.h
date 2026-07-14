#pragma once

#include "SDL3/SDL_mutex.h"
#include "base/str_view.h"
#include <queue>

struct Job {
	enum class Type {
		TTS,
		ASR,
		ASR_DATA_FREE,
		ASR_RECORD_INIT,
		ASR_RECORD_DEINIT,
		ASR_RECORD_START,
		ASR_RECORD_STOP,
		ASR_RECORD_PLAY,
		// RECORD_AND_ASR,
		INIT,
	};
	int id{-1};
	Type type{Type::TTS};
	union {
		StrView tts_text{};
	};
};

template<class T>
struct JobQueue {
	std::queue<T> queue{};
	SDL_Mutex *mutex{};
	SDL_Condition *cond{};
	bool quit{};
};

struct AppContext;
void worker_job_push(AppContext *ctx, Job job);

int SDLCALL WorkerThread(void *userdata);
