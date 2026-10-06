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
	using JobFunctionParam = void (*)(const Job &);
	enum class Type {
		SINGLE_THREADED,
		SINGLE_THREADED_PARAMETRIZED,
		// MULTI_THREADED,
	} type{Job::Type::SINGLE_THREADED};
	int id{-1};
	// Type type{Type::SINGLE_THREADED};
	// Size thread_count{2};
	union {
		JobFunction func{nullptr};
		JobFunctionParam func_param;
		// void (*func_mt)(AppContext *app_ctx, LaneContext *lane_ctx);
	};
	union {
		unsigned char u8_arr[16]{};
		char i8_arr[16];
		uint16_t u16_arr[8];
		int16_t i16_arr[8];
		uint32_t u32_arr[4];
		int32_t i32_arr[4];
		uint64_t u64_arr[2];
		int64_t i64_arr[2];

		StrView str;

		void *ptr;
		uint64_t u64_val;
		int64_t i64_val;
	};
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
			int64_t int64;
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
void run_with_payload(void *payload, MainThreadCallbackWithPayload func);
void touch_ui();
} // namespace MT
// NOTE: commands to perform on worker threads

namespace Worker {
void job_push(AppContext *ctx, Job::Type job_type, Job job);
void audio_job_push(AppContext *ctx, AudioJob job);
void send_haptic_feedback(AppContext *ctx); // TODO: forbid sending more than one per frame?..
#if NEURO
void neuro_job_push(AppContext *ctx, NeuroJob job);
#endif
} // namespace Worker

int SDLCALL WorkerThread(void *userdata);
int SDLCALL AudioWorkerThread(void *userdata);
#if NEURO
int SDLCALL NeuroWorkerThread(void *userdata);
#endif
