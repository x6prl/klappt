#include "worker.h"

#include "SDL3/SDL_events.h"
#include "app/app_context.h"
#include <SDL3/SDL.h>
#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_log.h>
#include <SDL3/SDL_timer.h>

#include "app/audio_context.h"
#include "app/event_codes.h"
#include "app/neuro_context.h"
#include "base/atomic.h"
#include "base/dyn_arr.h"

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
	static AtomicInt job_id_counter{0};
	if (job.id < 0) {
		job.id = Atomic::inc(&job_id_counter);
	}
	SDL_Log("Main Thread: Pushing Job %d to the worker queue.", job.id);
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
void Worker::neuro_job_push(AppContext *ctx, NeuroJob job) {
	int id = worker_job_push_generic(
		  ctx, job,
		  [](AppContext *ctx) -> decltype(ctx->neuro_worker_job_queue) * {
			  return &ctx->neuro_worker_job_queue;
		  });
	SDL_Log("Main Thread: Pushing Neuro Job %d to the worker queue.", id);
}
void Worker::job_push(AppContext *ctx, Job job) {
	int id = worker_job_push_generic(
		  ctx, job, [](AppContext *ctx) -> decltype(ctx->worker_job_queue) * {
			  return &ctx->worker_job_queue;
		  });
	SDL_Log("Main Thread: Pushing Generic Job %d to the worker queue.", id);
}

int SDLCALL WorkerThread(void *userdata) {
	worker_thread_generic<Job>(
		  userdata,
		  [](AppContext *ctx) -> decltype(ctx->worker_job_queue) * {
			  strncpy(tctx()->thread_name, "Worker0", 15);
			  return &ctx->worker_job_queue;
		  },
		  [](Job &job) { job.func(); });
	// Arena a{};
	//
	// auto _ctx = static_cast<AppContext *>(userdata);
	// auto &job_queue = _ctx->worker_job_queue;
	// auto *sound_ctx = _ctx->sound_ctx;
	//
	// while (true) {
	// 	SDL_LockMutex(job_queue.mutex);
	//
	// 	while (job_queue.queue.empty() && !job_queue.quit) {
	// 		SDL_WaitCondition(job_queue.cond, job_queue.mutex);
	// 	}
	//
	// 	if (job_queue.quit && job_queue.queue.empty()) {
	// 		SDL_UnlockMutex(job_queue.mutex);
	// 		break;
	// 	}
	//
	// 	Job current_job = job_queue.queue.front();
	// 	job_queue.queue.pop();
	//
	// 	SDL_UnlockMutex(job_queue.mutex);

	// SDL_Log("Worker Thread: Processing Job %d (Payload: " StrView_Fmt ")",
	//         current_job.id, StrView_Arg(current_job.tts_text));

	// switch (current_job.type) {
	// case Job::Type::INIT: {
	// 	// ot_test(a);
	// 	if (init_sherpa_engines(sound_ctx) != 0) {
	// 		cleanup_sherpa_engines(sound_ctx);
	// 		SDL_LogError(SDL_LOG_CATEGORY_ERROR, "SHERPA ENGINES ERROR");
	// 	} else {
	// 		push_event(ASR_NOTIFY_UI_GENERAL_CODE, nullptr);
	// 	}
	// } break;
	// case Job::Type::TTS: {
	// 	Measure m{"JOB: TTS"};
	// 	SherpaOnnxGenerationConfig gen_config;
	// 	memset(&gen_config, 0, sizeof(gen_config));
	//
	// 	{ // set speech speed
	// 		constexpr auto SPEED_NORMAL = 1.f;
	// 		constexpr auto SPEED_SLOW = .55f;
	// 		static Hash prev_str_hash;
	// 		Hash this_str_hash = hash_str_view(current_job.tts_text);
	// 		bool is_same_hashes = prev_str_hash == this_str_hash;
	//
	// 		if (is_same_hashes) {
	// 			gen_config.speed = SPEED_SLOW;
	// 			prev_str_hash = {};
	// 		} else {
	// 			prev_str_hash = this_str_hash;
	// 			gen_config.speed = SPEED_NORMAL;
	// 		}
	// 	}
	//
	// 	char text_buf[1024]{};
	// 	auto &str = current_job.tts_text;
	// 	memcpy(text_buf, str.data, (str.size < 1024 ? str.size : 1023));
	//
	// 	const SherpaOnnxGeneratedAudio *audio =
	// 		  SherpaOnnxOfflineTtsGenerateWithConfig(
	// 				sound_ctx->sherpa_ctx.tts, text_buf, &gen_config,
	// 				nullptr, nullptr);
	//
	// 	if (!audio) {
	// 		SDL_LogError(SDL_LOG_CATEGORY_ERROR,
	// 		             "[Sherpa TTS] Generation failed.");
	// 	}
	//
	// 	int num_samples = audio->n;
	// 	int sample_rate = audio->sample_rate;
	// 	auto audio_data = audio->samples;
	//
	// 	SDL_Log("Synthesized %d samples at %d Hz", num_samples,
	// 	        sample_rate);
	// 	SDL_AudioSpec spec;
	//
	// 	spec.channels = 1;
	// 	spec.format = SDL_AUDIO_F32;
	// 	spec.freq = sample_rate;
	//
	// 	SDL_AudioStream *stream = SDL_OpenAudioDeviceStream(
	// 		  SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, NULL, NULL);
	// 	if (!stream) {
	// 		SDL_Log("Couldn't create audio stream: %s", SDL_GetError());
	// 		return SDL_APP_FAILURE;
	// 	}
	//
	// 	if (!SDL_PutAudioStreamData(stream, audio_data,
	// 	                            num_samples * sizeof(float))) {
	// 		SDL_Log("Failed to put audio data: %s", SDL_GetError());
	// 	}
	//
	// 	SDL_ResumeAudioStreamDevice(stream);
	//
	// 	while (SDL_GetAudioStreamAvailable(stream) > 0) {
	// 		SDL_Delay(10);
	// 	}
	//
	// 	SDL_Delay(50);
	//
	// 	SDL_DestroyAudioStream(stream);
	// 	SherpaOnnxDestroyOfflineTtsGeneratedAudio(audio);
	// 	m.lap().printms();
	// } break;
	// case Job::Type::ASR: {
	// 	Measure m{"JOB: ASR"};
	// 	auto &ctx = *sound_ctx;
	// 	{ // notify ui
	// 		SDL_SetAtomicInt(&ctx.is_asr_in_progress, SoundContext::TRUE);
	// 		push_event(ASR_NOTIFY_UI_GENERAL_CODE, nullptr);
	// 	}
	// 	m.lap().printus("ui notified");
	// 	char *spoken_text{nullptr};
	// 	const SherpaOnnxOfflineRecognizerResult *sherpa_result{nullptr};
	// 	// transcribe_audio(
	// 	// 	  sound_ctx, (float *)ctx.audio.data,
	// 	// 	  ctx.audio.size_bytes / sizeof(float),
	// 	// 	  SoundContext::FREQUENCY);
	// 	{
	// 		auto &s = ctx.sherpa_ctx;
	// 		if (s.asr) {
	// 			// TODO: !!!!!!!!! think harder
	// 			Measure cofs{"ONNX: CreateOfflineStream"};
	// 			if (!s.offline_stream)
	// 				s.offline_stream = SherpaOnnxCreateOfflineStream(s.asr);
	// 			if (s.offline_stream) {
	// 				cofs.lap().printus();
	//
	// 				Measure awo{"ONNX: AcceptWaveformOffline"};
	// 				auto samples_count =
	// 					  ctx.audio.size_bytes / sizeof(float);
	// 				SherpaOnnxAcceptWaveformOffline(
	// 					  s.offline_stream, SoundContext::FREQUENCY,
	// 					  static_cast<float *>(ctx.audio.data),
	// 					  samples_count);
	// 				awo.lap().printus();
	//
	// 				Measure dos{"ONNX: DecodeOfflineStream"};
	// 				SherpaOnnxDecodeOfflineStream(s.asr, s.offline_stream);
	// 				dos.lap().printus();
	//
	// 				Measure gr{"ONNX: GetResult"};
	// 				sherpa_result = SherpaOnnxGetOfflineStreamResult(
	// 					  s.offline_stream);
	// 				gr.lap().printus();
	//
	// 				Measure onx{"ONNX: finish"};
	// 				char *text = nullptr;
	// 				if (sherpa_result && sherpa_result->text) {
	// 					text = strdup(sherpa_result->text);
	// 				}
	// 				onx.lap().printus("strdup");
	//
	// 				s.offline_stream = nullptr;
	// 				onx.lap().printus("destroy stream");
	//
	// 				spoken_text = text;
	// 			} else {
	// 				SDL_LogError(SDL_LOG_CATEGORY_ERROR,
	// 				             "SherpaOnnxCreateOfflineStream failed");
	// 			}
	// 		}
	// 	}
	// 	m.lap().printus("audio transcribed");
	// 	{ // notify ui
	// 		if (spoken_text) {
	// 			// NOTE: we have a result and asr is not in progress anymore
	// 			SDL_Log("Whisper heard: '%s'", spoken_text);
	// 			SDL_SetAtomicInt(&ctx.is_asr_in_progress,
	// 			                 SoundContext::FALSE);
	// 			push_event(ASR_FINISHED_EVENT_CODE, spoken_text);
	// 			// NOTE: freed after copying in ui via the other job
	// 			// free(spoken_text);
	// 		} else {
	// 			// NOTE: we have a result and asr is not in progress anymore
	// 			SDL_Log("Transcription failed or returned empty.");
	// 			SDL_SetAtomicInt(&ctx.is_asr_in_progress,
	// 			                 SoundContext::FALSE);
	// 			push_event(ASR_NOTIFY_UI_GENERAL_CODE, nullptr);
	// 		}
	// 	}
	// 	m.lap().printus("ui notified");
	// 	if (sherpa_result) {
	// 		SherpaOnnxDestroyOfflineRecognizerResult(sherpa_result);
	// 		m.lap().printus("sherpa result destroyed");
	// 	}
	// 	m.lap().printms();
	// } break;
	// case Job::Type::ASR_RECORD_INIT: {
	// 	Measure m{"JOB: ASR REC INIT"};
	// 	auto &ctx = *sound_ctx;
	// 	SDL_AudioSpec spec;
	// 	spec.channels = 1;
	// 	spec.format = SDL_AUDIO_F32;
	// 	spec.freq = SoundContext::FREQUENCY;
	//
	// 	SDL_AudioStream *stream = SDL_OpenAudioDeviceStream(
	// 		  SDL_AUDIO_DEVICE_DEFAULT_RECORDING, &spec, NULL, NULL);
	//
	// 	int count{0};
	// 	SDL_AudioDeviceID *ids = SDL_GetAudioRecordingDevices(&count);
	// 	for (int i = 0; i < count; ++i) {
	// 		SDL_Log("Recording device: %s", SDL_GetAudioDeviceName(ids[i]));
	// 	}
	//
	// 	if (!stream) {
	// 		SDL_LogError(SDL_LOG_CATEGORY_ERROR,
	// 		             "Couldn't create recording stream: %s",
	// 		             SDL_GetError());
	// 		break;
	// 	}
	// 	ctx.recording_stream = stream;
	// 	{ // notify ui
	// 		SDL_SetAtomicInt(&ctx.is_initialized, SoundContext::TRUE);
	// 		push_event(ASR_NOTIFY_UI_GENERAL_CODE, nullptr);
	// 	}
	// 	m.lap().printms();
	// } break;
	// case Job::Type::ASR_RECORD_DEINIT: {
	// 	Measure m{"JOB: ASR REC DEINIT"};
	// 	auto &ctx = *sound_ctx;
	// 	if (ctx.recording_stream) {
	// 		SDL_DestroyAudioStream(ctx.recording_stream);
	// 	}
	// 	SDL_SetAtomicInt(&ctx.is_initialized, SoundContext::FALSE);
	// 	m.lap().printms();
	// } break;
	// case Job::Type::ASR_RECORD_START: {
	// 	Measure m{"JOB: ASR REC START"};
	// 	auto &ctx = *sound_ctx;
	// 	if (!ctx.recording_stream) {
	// 		SDL_LogError(SDL_LOG_CATEGORY_ERROR,
	// 		             "Couldn't record audio: recording stream is not "
	// 		             "initialized");
	// 		break;
	// 	}
	// 	SDL_ResumeAudioStreamDevice(ctx.recording_stream);
	// 	{ // notify ui that recording started
	// 		SDL_SetAtomicInt(&ctx.is_recording, SoundContext::TRUE);
	// 		push_event(ASR_NOTIFY_UI_RECORDING_START_CODE, nullptr);
	// 	}
	// 	m.lap().printms();
	// } break;
	// case Job::Type::ASR_RECORD_STOP: {
	// 	Measure m{"JOB: ASR REC STOP"};
	// 	auto &ctx = *sound_ctx;
	// 	SDL_PauseAudioStreamDevice(ctx.recording_stream);
	//
	// 	{ // notify UI
	// 		SDL_SetAtomicInt(&ctx.is_recording, SoundContext::FALSE);
	// 		push_event(ASR_NOTIFY_UI_RECORDING_STOP_CODE, nullptr);
	// 	}
	// 	m.lap().printms();
	//
	// 	{ // copy audio to buffer
	// 		ctx.audio.size_bytes = 0;
	// 		Size bytes_recorded = 0;
	//
	// 		for (int i{0};; ++i) {
	// 			SDL_Log("run_asr iter %d", i);
	// 			int available =
	// 				  SDL_GetAudioStreamAvailable(ctx.recording_stream);
	// 			SDL_Log("run_asr avail: %d", available);
	// 			SDL_Log("run_asr recorded: %d", bytes_recorded);
	// 			if (bytes_recorded < ctx.audio.capacity_bytes &&
	// 			    available > 0) {
	// 				int to_read = available;
	// 				if (bytes_recorded + to_read >
	// 				    ctx.audio.capacity_bytes) {
	// 					to_read = ctx.audio.capacity_bytes - bytes_recorded;
	// 				}
	//
	// 				int read = SDL_GetAudioStreamData(
	// 					  ctx.recording_stream,
	// 					  (Uint8 *)ctx.audio.data + bytes_recorded,
	// 					  to_read);
	// 				SDL_Log("run_asr read: %d", read);
	// 				if (read > 0) {
	// 					bytes_recorded += read;
	// 				}
	// 			} else {
	// 				break;
	// 			}
	// 		}
	// 		ctx.audio.size_bytes = bytes_recorded;
	// 	}
	// 	constexpr auto _50ms = SoundContext::FREQUENCY * 4 / 20;
	// 	if (ctx.audio.size_bytes < _50ms) {
	// 		SDL_Log("< 50 ms");
	// 		break;
	// 	}
	// 	m.lap().printms();
	// } break;
	// case Job::Type::ASR_RECORD_PLAY: {
	// 	Measure m{"JOB: ASR REC PLAY"};
	// 	auto &ctx = *sound_ctx;
	// 	SDL_AudioSpec spec;
	//
	// 	spec.channels = 1;
	// 	spec.format = SDL_AUDIO_F32;
	// 	spec.freq = SoundContext::FREQUENCY;
	//
	// 	SDL_AudioStream *output_stream = SDL_OpenAudioDeviceStream(
	// 		  SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, NULL, NULL);
	// 	if (!output_stream) {
	// 		SDL_Log("Couldn't create audio stream: %s", SDL_GetError());
	// 		return SDL_APP_FAILURE;
	// 	}
	//
	// 	if (!SDL_PutAudioStreamData(output_stream, ctx.audio.data,
	// 	                            ctx.audio.size_bytes)) {
	// 		SDL_Log("Failed to put audio data: %s", SDL_GetError());
	// 	}
	//
	// 	SDL_ResumeAudioStreamDevice(output_stream);
	//
	// 	while (SDL_GetAudioStreamAvailable(output_stream) > 0) {
	// 		SDL_Delay(10);
	// 	}
	//
	// 	SDL_Delay(100);
	//
	// 	SDL_DestroyAudioStream(output_stream);
	// 	m.lap().printms();
	// } break;
	// case Job::Type::ASR_DATA_FREE: {
	// 	Measure m{"JOB: ASR DATA FREE"};
	// 	// NOTE: same ptr as spoken_text in Type::ASR
	// 	// TODO: refactor
	// 	free((void *)current_job.tts_text.data);
	// 	m.lap().printms();
	// } break;
	// 	// case Job::Type::RECORD_AND_ASR: {
	// 	// 	char *spoken_text = record_and_transcribe(3.0f);
	// 	//
	// 	// 	if (spoken_text) {
	// 	// 		SDL_Log("Whisper heard: '%s'", spoken_text);
	// 	// 		SDL_Event event{};
	// 	// 		event.type = SDL_EVENT_USER;
	// 	// 		event.user.code = ASR_FINISHED_EVENT_CODE;
	// 	// 		event.user.data1 = spoken_text;
	// 	//
	// 	// 		SDL_PushEvent(&event);
	// 	//
	// 	// 		// free(spoken_text);
	// 	// 	} else {
	// 	// 		SDL_Log("Transcription failed or returned empty.");
	// 	// 	}
	// 	// } break;
	// }
	// }

	SDL_Log("Worker Thread: Exiting");
	return 0;
}

int SDLCALL AudioWorkerThread(void *userdata) {
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
			  // NOTE: writing to another thread directly
			  ctx->audio = audio_ctx;
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

int SDLCALL NeuroWorkerThread(void *userdata) {
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
			  // TODO: should be more careful?
		      // NOTE: writing to another thread directly
			  ctx->neuro = neuro_ctx;
			  return &ctx->neuro_worker_job_queue;
		  },
		  [](NeuroJob &job) { job.func(tctx()->neuro, &job.payload); });
	SDL_Log("Neuro Worker Thread: Exiting");
	return 0;
}
