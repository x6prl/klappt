#include "worker.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_events.h>
#include <SDL3/SDL_log.h>
#include <SDL3/SDL_timer.h>

#include "app/app_context.h"
#include "app/audio_context.h"
#include "app/event_codes.h"
#if NEURO
#include "app/neuro_context.h"
#endif
#include "base/atomic.h"
#ifndef __EMSCRIPTEN__
#include "base/dyn_arr.h"
#endif

thread_local ThreadContext *_tctx{};
ThreadContext *tctx() { return _tctx; }

namespace {
// char *transcribe_audio(SoundContext *sc, const float *samples, int
// num_samples,
//                        int sample_rate) {
// 	auto &s = sc->sherpa_ctx;
// 	if (!s.asr)
// 		return nullptr;
// 	// TODO: !!!!!!!!! think harder
// 	Measure cofs{"ONNX: CreateOfflineStream"};
// 	if (!s.offline_stream)
// 		s.offline_stream = SherpaOnnxCreateOfflineStream(s.asr);
// 	if (!s.offline_stream)
// 		return nullptr;
// 	cofs.lap().printus();
//
// 	Measure awo{"ONNX: AcceptWaveformOffline"};
// 	SherpaOnnxAcceptWaveformOffline(s.offline_stream, sample_rate, samples,
// 	                                num_samples);
// 	awo.lap().printus();
//
// 	Measure dos{"ONNX: DecodeOfflineStream"};
// 	SherpaOnnxDecodeOfflineStream(s.asr, s.offline_stream);
// 	dos.lap().printus();
//
// 	Measure gr{"ONNX: GetResult"};
// 	const SherpaOnnxOfflineRecognizerResult *result =
// 		  SherpaOnnxGetOfflineStreamResult(s.offline_stream);
// 	gr.lap().printus();
//
// 	Measure onx{"ONNX: finish"};
// 	char *text = nullptr;
// 	if (result && result->text) {
// 		text = strdup(result->text);
// 	}
// 	onx.lap().printus("strdup");
//
// 	if (result)
// 		SherpaOnnxDestroyOfflineRecognizerResult(result);
// 	onx.lap().printus("destroy result");
// 	SherpaOnnxDestroyOfflineStream(s.offline_stream);
// 	s.offline_stream = nullptr;
// 	onx.lap().printus("destroy stream");
//
// 	return text;
// }

// char *record_and_transcribe(float duration_seconds) {
// 	SDL_AudioSpec spec;
// 	spec.channels = 1;
// 	spec.format = SDL_AUDIO_F32;
// 	spec.freq = SoundContext::FREQUENCY;
//
// 	SDL_AudioStream *stream = SDL_OpenAudioDeviceStream(
// 		  SDL_AUDIO_DEVICE_DEFAULT_RECORDING, &spec, NULL, NULL);
//
// 	if (!stream) {
// 		SDL_Log("Couldn't create recording stream: %s", SDL_GetError());
// 		return nullptr;
// 	}
//
// 	int total_samples = (int)(spec.freq * duration_seconds);
// 	int total_bytes = total_samples * sizeof(float);
// 	float *audio_buffer = (float *)malloc(total_bytes);
//
// 	if (!audio_buffer) {
// 		SDL_DestroyAudioStream(stream);
// 		return nullptr;
// 	}
//
// 	int bytes_recorded = 0;
//
// 	SDL_ResumeAudioStreamDevice(stream);
// 	SDL_Log("Recording for %.1f seconds...", duration_seconds);
//
// 	while (bytes_recorded < total_bytes) {
// 		int available = SDL_GetAudioStreamAvailable(stream);
// 		if (available > 0) {
// 			int to_read = available;
// 			if (bytes_recorded + to_read > total_bytes) {
// 				to_read = total_bytes - bytes_recorded;
// 			}
//
// 			int read = SDL_GetAudioStreamData(
// 				  stream, (Uint8 *)audio_buffer + bytes_recorded, to_read);
// 			if (read > 0) {
// 				bytes_recorded += read;
// 			}
// 		} else {
// 			SDL_Delay(10);
// 		}
// 	}
//
// 	SDL_PauseAudioStreamDevice(stream);
// 	SDL_DestroyAudioStream(stream);
// 	SDL_Log("Recording finished. Transcribing...");
//
// 	float max_amplitude = 0.0f;
// 	for (int i = 0; i < total_samples; i++) {
// 		float abs_val = fabsf(audio_buffer[i]);
// 		if (abs_val > max_amplitude)
// 			max_amplitude = abs_val;
// 	}
//
// 	if (max_amplitude < 0.01f) {
// 		SDL_Log("Audio too quiet, likely just silence. Skipping ASR.");
// 		free(audio_buffer);
// 		return nullptr;
// 	}
//
// 	char *text = transcribe_audio(audio_buffer, total_samples, spec.freq);
// 	free(audio_buffer);
// 	return text;
// }
template <class JobType, class GetQueue>
int worker_job_push_generic(AppContext *ctx, JobType job, GetQueue get_queue) {
	static AtomicInt job_id_counter{1};
	if (job.id < 0) {
		job.id = Atomic::inc(&job_id_counter);
	}
	auto queue = get_queue(ctx);

	SDL_LockMutex(queue->mutex);
	queue->queue.push(job);
	SDL_SignalCondition(queue->cond);
	SDL_UnlockMutex(queue->mutex);
	return job.id;
}

template <class JobType, class TCTXInitAndGetQueue, class FuncLauncher>
void worker_thread_generic(void *userdata,
                           TCTXInitAndGetQueue init_and_get_queue,
                           FuncLauncher launch_job_func) {
	auto app_ctx = static_cast<AppContext *>(userdata);

	ThreadContext tctx_var = {
		  .app_ctx = app_ctx,
	};
	_tctx = &tctx_var;

	auto job_queue = init_and_get_queue(app_ctx);

	for (;;) {
		SDL_LockMutex(job_queue->mutex);

		while (job_queue->queue.empty()
		       /* && !job_queue->quit*/
		) {
			SDL_WaitCondition(job_queue->cond, job_queue->mutex);
		}

		if (/*
		       job_queue->quit &&
		        */
		    job_queue->queue.empty()) {
			SDL_UnlockMutex(job_queue->mutex);
			break;
		}

		JobType current_job = job_queue->queue.front();
		job_queue->queue.pop();

		SDL_UnlockMutex(job_queue->mutex);

		SDL_Log("Thread %s: Processing Job %d", tctx()->thread_name,
		        current_job.id);

		launch_job_func(current_job);

		SDL_Log("Thread %s: Processing Job %d Finished", tctx()->thread_name,
		        current_job.id);
	}
}

} // namespace

void MT::touch_ui() {
	SDL_Event event{};
	event.type = SDL_EVENT_USER;
	event.user.code = NOTIFY_UI_GENERAL_CODE;
	event.user.data1 = nullptr;

	SDL_PushEvent(&event);
}
void MT::run(MainThreadCallback func) {
	SDL_Event event{};
	event.type = SDL_EVENT_USER;
	event.user.code = MAIN_THREAD_RUN_FUNC_CODE;
	{
		// NOTE: works well on x86_64, arm64 and wasm
		event.user.data1 = reinterpret_cast<void *>(func);
	}

	SDL_PushEvent(&event);
}
void MT::run_with_payload(void *payload, MainThreadCallbackWithPayload func) {
	SDL_Event event{};
	event.type = SDL_EVENT_USER;
	event.user.code = MAIN_THREAD_RUN_FUNC_WITH_PAYLOAD_CODE;
	{
		// NOTE: works well on x86_64, arm64 and wasm
		event.user.data1 = reinterpret_cast<void *>(func);
	}
	event.user.data2 = payload;

	SDL_PushEvent(&event);
}

void Worker::audio_job_push(AppContext *ctx, AudioJob job) {
	int id = worker_job_push_generic(
		  ctx, job,
		  [](AppContext *ctx) -> decltype(ctx->audio_worker_job_queue) * {
			  return &ctx->audio_worker_job_queue;
		  });
	SDL_Log("Main Thread: Pushing Audio Job %d to the worker queue.", id);
}
#if NEURO
void Worker::neuro_job_push(AppContext *ctx, NeuroJob job) {
	int id = worker_job_push_generic(
		  ctx, job,
		  [](AppContext *ctx) -> decltype(ctx->neuro_worker_job_queue) * {
			  return &ctx->neuro_worker_job_queue;
		  });
	SDL_Log("Main Thread: Pushing Neuro Job %d to the worker queue.", id);
}
#endif // NEURO
void Worker::job_push(AppContext *ctx, Job job) {
	int id = worker_job_push_generic(
		  ctx, job, [](AppContext *ctx) -> decltype(ctx->worker_job_queue) * {
			  return &ctx->worker_job_queue;
		  });
	SDL_Log("Main Thread: Pushing Generic Job %d to the worker queue.", id);
}

int SDLCALL WorkerThread(void *userdata) {
	KLAPPT_PROFILE_THREAD("worker");
	worker_thread_generic<Job>(
		  userdata,
		  [](AppContext *ctx) -> decltype(ctx->worker_job_queue) * {
			  strncpy(tctx()->thread_name, "Worker0", 15);
			  return &ctx->worker_job_queue;
		  },
		  [](Job &job) { job.func(); });
	SDL_Log("Worker Thread: Exiting");
	return 0;
}

int SDLCALL AudioWorkerThread(void *userdata) {
	KLAPPT_PROFILE_THREAD("audio");
	worker_thread_generic<AudioJob>(
		  userdata,
		  [](AppContext *ctx) -> JobQueue<AudioJob> * {
			  strncpy(tctx()->thread_name, "Audio", 15);
			  constexpr Size ASR_BUFFER_CAPACITY_BYTES =
					sizeof(float) * 16000 *
					10; // 4 bytes * 16000kHz * 10 seconds
			  auto audio_ctx = new AudioContext{
					.rec_audio_buffer = {
						  .data = (float *)malloc(ASR_BUFFER_CAPACITY_BYTES),
						  .capacity_bytes = ASR_BUFFER_CAPACITY_BYTES}};
			  tctx()->audio = audio_ctx;
			  MT::run_with_payload(
					tctx()->audio, [](AppContext *ctx, void *ptr) {
						ctx->audio = static_cast<AudioContext *>(ptr);
					});
			  return &ctx->audio_worker_job_queue;
		  },
		  [](AudioJob &job) {
			  job.func(tctx()->audio, &job.payload);
			  if (job.func_on_finished) {
				  job.func_on_finished(tctx()->audio, &job.payload);
			  }
		  });
	SDL_Log("Audio Worker Thread: Exiting");
	return 0;
}
#if NEURO
int SDLCALL NeuroWorkerThread(void *userdata) {
	KLAPPT_PROFILE_THREAD("neuro");
	worker_thread_generic<NeuroJob>(
		  userdata,
		  [](AppContext *ctx) -> JobQueue<NeuroJob> * {
			  strncpy(tctx()->thread_name, "Neuro", 15);
			  constexpr Size pp_pool_size = 20;
			  auto pp_pool = DynArr<PlaybackPayload>::filled_zero_or_default(
					tctx()->a, pp_pool_size);
			  auto neuro_ctx = new NeuroContext{
					.playback_payload_pool = pp_pool,
			  };
			  tctx()->neuro = neuro_ctx;
			  for (Size i{0}; i < pp_pool_size; ++i) {
				  pp_pool[i].pp_index = i;
			  }
			  MT::run_with_payload(
					tctx()->neuro, [](AppContext *ctx, void *ptr) {
						ctx->neuro = static_cast<NeuroContext *>(ptr);
					});
			  return &ctx->neuro_worker_job_queue;
		  },
		  [](NeuroJob &job) { job.func(tctx()->neuro, &job.payload); });
	SDL_Log("Neuro Worker Thread: Exiting");
	return 0;
}
#endif // NEURO
