#pragma once

#include "SDL3/SDL_mutex.h"
#include "base/str_view.h"
// #include <barrier>
#include <cstdint>
#include <queue>

// struct LaneContext {
// 	using Barrier = std::barrier<>;
// 	int lane_idx{0};
// 	int lane_count{1};
// 	Barrier *_barrier;
//
// 	void sync_barrier() { _barrier->arrive_and_wait(); };
// };

struct AppContext;
using MainThreadCallback = void (*)(AppContext *);
using MainThreadCallbackWithPayload = void (*)(AppContext *, void *payload);
// struct MainThreadCallback {
// 	void (*func)(AppContext*ctx);
// };

struct NetContext;

struct AudioContext;
struct PlaybackPayload;

struct NeuroContext;

struct ThreadContext {
	Arena a{16 << 10};
	AppContext *app_ctx{nullptr};
	union {
		NetContext *net{nullptr};
		AudioContext *audio;
		NeuroContext *neuro;
	};
	// int32_t thread_index{-1};
	char thread_name[16]{};
	// LaneContext *lctx{nullptr};
};

struct Job {
	using JobFunction = void (*)(void);
	// enum class Type {
	// 	SINGLE_THREADED,
	// 	// MULTI_THREADED,
	// };
	int id{-1};
	// Type type{Type::SINGLE_THREADED};
	// Size thread_count{2};
	union {
		JobFunction func{nullptr};
		// void (*func_mt)(AppContext *app_ctx, LaneContext *lane_ctx);
	};
	// union {
	// 	StrView tts_text;
	// };
};

struct AudioJob {
	struct Payload {
		union {
			PlaybackPayload *playback_payload_ptr{nullptr};
		};
	};
	using JobFunction = void (*)(AudioContext *, Payload *);

	int id{-1};
	JobFunction func{nullptr};
	JobFunction func_on_finished{nullptr};
	Payload payload;
};

struct NeuroJob {
	struct Payload {
		union {
			StrView tts_text{};
			void *data_ptr;
			int32_t int32;
		};
	};
	using JobFunction = void (*)(NeuroContext *, Payload *);

	int id{-1};
	JobFunction func{nullptr};
	Payload payload;
};

template <class T> struct JobQueue {
	std::queue<T> queue{};
	SDL_Mutex *mutex{SDL_CreateMutex()};
	SDL_Condition *cond{SDL_CreateCondition()};
	// bool quit{false}; // NOTE: was useless
};

ThreadContext *tctx();
// inline LaneContext *lctx() { return tctx()->lctx; }
// inline LaneContext *lctx_set(LaneContext *new_lctx) {
// 	auto tctx_ = tctx();
// 	auto old_lctx = tctx_->lctx;
// 	tctx_->lctx = new_lctx;
// 	return old_lctx;
// }

// NOTE: commands to perform on main thread
namespace MT {
void run(MainThreadCallback func);
void run_with_payload(void *payload,
                                  MainThreadCallbackWithPayload func);
void touch_ui();
} // namespace MT
// NOTE: commands to perform on worker threads

namespace Worker {
void job_push(AppContext *ctx, Job job);
void audio_job_push(AppContext *ctx, AudioJob job);
#if NEURO
void neuro_job_push(AppContext *ctx, NeuroJob job);
#endif
} // namespace Worker

int SDLCALL WorkerThread(void *userdata);
int SDLCALL AudioWorkerThread(void *userdata);
#if NEURO
int SDLCALL NeuroWorkerThread(void *userdata);
#endif
