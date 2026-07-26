#include "audio.h"

#include "SDL3/SDL_log.h"
#include "app/app_context.h"
#include "app/audio_context.h"
#include "app/worker.h"
#include "base/measure.h"

#include "platform/neuro.h"

namespace {

void playback_play_(AudioContext *actx, SDL_AudioSpec spec,
                    const void *data_ptr, const Size data_size) {
	SDL_Log(" %s: start", __FUNCTION__);
	SDL_Log("freq=%d format=%x channels=%d", spec.freq, spec.format,
	        spec.channels);
	// init
	if (!actx->playback_stream) {
		SDL_Log(" %s: pb stream create", __FUNCTION__);
		actx->playback_stream = SDL_OpenAudioDeviceStream(
			  SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, NULL, NULL);
		SDL_Log("Got device: freq=%d format=%x channels=%d", spec.freq,
		        spec.format, spec.channels);
	} else {
		// Update format if it has changed since the last call
		SDL_SetAudioStreamFormat(actx->playback_stream, &spec, NULL);
		SDL_Log("Got device: freq=%d format=%x channels=%d", spec.freq,
		        spec.format, spec.channels);
	}

	if (!actx->playback_stream) {
		SDL_LogError(SDL_LOG_CATEGORY_ERROR, "Couldn't create audio stream: %s",
		             SDL_GetError());
		return;
	}

	// SDL_ClearAudioStream(actx->playback_stream);

	if (!SDL_PutAudioStreamData(actx->playback_stream, data_ptr, data_size)) {
		SDL_Log("Failed to put audio data: %s", SDL_GetError());
		return;
	}

	// flush remaining buffered samples
	if (!SDL_FlushAudioStream(actx->playback_stream)) {
		SDL_Log("Failed to flush audio stream: %s", SDL_GetError());
		return;
	}

	// start playing
	SDL_ResumeAudioStreamDevice(actx->playback_stream);

	// block the thread
	while (SDL_GetAudioStreamQueued(actx->playback_stream) > 0) {
		SDL_Delay(10);
	}

	// hardware is still playing the last chunk
	SDL_Delay(100);
	{ // deinit
	  // NOTE: we don't do this
	  // SDL_DestroyAudioStream(actx->playback_stream);
	}
}

template <bool RUN_ASR_ON_FINISH> void push_rec_stop_job(AppContext *ctx) {
	// worker_job_push(ctx, {.type = Job::Type::ASR_RECORD_STOP});
	Worker::audio_job_push(
		  ctx,
		  AudioJob{.func = [](AudioContext *actx, AudioJob::Payload *payload) {
			  (void)payload;
			  Measure m{"JOB: REC STOP"};
			  if constexpr (RUN_ASR_ON_FINISH) {
				  m.name = "JOB: REC STOP AND RUN ASR";
			  }
			  // auto &ctx = *sound_ctx;
			  SDL_PauseAudioStreamDevice(actx->recording_stream);

			  { // notify UI
			    // SDL_SetAtomicInt(&ctx.is_recording, SoundContext::FALSE);
			    // push_event(ASR_NOTIFY_UI_RECORDING_STOP_CODE, nullptr);
				  MT::run([](AppContext *ctx) {
					  ctx->audio_asr_tts_status.is_recording = false;
				  });
			  }
			  m.lap().printms();

			  Size bytes_recorded = 0;
			  { // copy audio to buffer
				  Atomic::set(&actx->rec_audio_buffer.size_bytes, 0);

				  for (;;) {
					  // TODO: drain audio stream over buffer cap
					  int available =
							SDL_GetAudioStreamAvailable(actx->recording_stream);
					  if (bytes_recorded <
				                actx->rec_audio_buffer.capacity_bytes &&
				          available > 0) {
						  int to_read = available;
						  if (bytes_recorded + to_read >
					          actx->rec_audio_buffer.capacity_bytes) {
							  to_read = actx->rec_audio_buffer.capacity_bytes -
						                bytes_recorded;
						  }

						  int read = SDL_GetAudioStreamData(
								actx->recording_stream,
								static_cast<Uint8 *>(
									  actx->rec_audio_buffer.data) +
									  bytes_recorded,
								to_read);
						  SDL_Log("run_asr read: %d", read);
						  if (read > 0) {
							  bytes_recorded += read;
						  } else if (read < 0) {
							  SDL_LogError(SDL_LOG_CATEGORY_ERROR,
						                   "SDL_GetAudioStreamData failed: %s",
						                   SDL_GetError());
							  break;
						  } else if (read == 0) {
							  break;
						  }
					  } else {
						  break;
					  }
				  }

				  Atomic::set(&actx->rec_audio_buffer.size_bytes,
			                  bytes_recorded);
				  SDL_Log("run_asr buffer usage: %d of %d", bytes_recorded,
			              actx->rec_audio_buffer.capacity_bytes);
			  }
			  constexpr auto _50ms = AudioContext::FREQUENCY * 4 / 20;
			  if (bytes_recorded < _50ms) {
				  SDL_Log("< 50 ms");
				  return;
			  } else {
				  SDL_Log("Recorded %f seconds",
			              AudioContext::bytes_to_seconds(bytes_recorded));
				  if constexpr (RUN_ASR_ON_FINISH) {
					  run_asr(tctx()->app_ctx);
				  }
			  }
			  m.lap().printms();
		  }});
}

void job_rec_init(AudioContext *actx, AudioJob::Payload *payload) {
	(void)payload;
	Measure m{"JOB: REC INIT"};
	SDL_AudioSpec spec;
	spec.channels = 1;
	spec.format = SDL_AUDIO_F32;
	spec.freq = AudioContext::FREQUENCY;

	int count{0};
	SDL_AudioDeviceID *ids = SDL_GetAudioRecordingDevices(&count);
	for (int i = 0; i < count; ++i) {
		SDL_Log("Recording device: %s", SDL_GetAudioDeviceName(ids[i]));
	}

	SDL_Log("Opening rec device...");
	SDL_Log("freq=%d format=%x channels=%d", spec.freq, spec.format,
	        spec.channels);
	SDL_AudioStream *stream = SDL_OpenAudioDeviceStream(
		  SDL_AUDIO_DEVICE_DEFAULT_RECORDING, &spec, NULL, NULL);
	SDL_Log("Got device: freq=%d format=%x channels=%d", spec.freq, spec.format,
	        spec.channels);

	if (!stream) {
		SDL_LogError(SDL_LOG_CATEGORY_ERROR,
		             "Couldn't create recording stream: %s", SDL_GetError());
		return;
	}
	actx->recording_stream = stream;
	{ // notify ui
	  // SDL_SetAtomicInt(&ctx.is_initialized,
	  // SoundContext::TRUE); worker_touch_ui();
		MT::run([](AppContext *ctx) {
			ctx->audio_asr_tts_status.is_recording_initialized = true;
		});
	}
	m.lap().printms();
}
void job_rec_start(AudioContext *actx, AudioJob::Payload *payload) {
	(void)payload;
	Measure m{"JOB: REC START"};
	// auto &ctx = *sound_ctx;
	if (!actx->recording_stream) {
		SDL_LogError(SDL_LOG_CATEGORY_ERROR,
		             "Couldn't record audio: recording stream is not "
		             "initialized");
		return;
	}
	SDL_ResumeAudioStreamDevice(actx->recording_stream);
	{ // notify ui that recording started
	  // SDL_SetAtomicInt(&ctx.is_recording,
	  // SoundContext::TRUE);
	  // push_event(ASR_NOTIFY_UI_RECORDING_START_CODE,
	  // nullptr);
		MT::run([](AppContext *ctx) {
			ctx->audio_asr_tts_status.is_recording = true;
		});
	}
	m.lap().printms();
}

} // namespace

void record_init(AppContext *ctx) {
	// worker_job_push(ctx, {.type = Job::Type::ASR_RECORD_INIT});
	Worker::audio_job_push(ctx, AudioJob{.func = job_rec_init});
}

void record_start(AppContext *ctx) {
	ctx->asr_result = {};

	Worker::audio_job_push(ctx, AudioJob{.func = job_rec_start});
	// worker_job_push(ctx, {.type = Job::Type::ASR_RECORD_START});
}

void record_stop_then_do_nothing(AppContext *ctx) {
	return push_rec_stop_job<false>(ctx);
}

void record_stop_then_run_asr(AppContext *ctx) {
	// worker_job_push(ctx, {.type = Job::Type::ASR_RECORD_STOP});
	return push_rec_stop_job<true>(ctx);
}

void record_deinit(AppContext *ctx) {
	// worker_job_push(ctx, {.type = Job::Type::ASR_RECORD_DEINIT});
	Worker::audio_job_push(
		  ctx,
		  AudioJob{.func = [](AudioContext *actx, AudioJob::Payload *payload) {
			  (void)payload;
			  Measure m{"JOB: REC DEINIT"};
			  if (actx->recording_stream) {
				  SDL_DestroyAudioStream(actx->recording_stream);
				  actx->recording_stream = nullptr;
			  }
			  { // notify UI
				  MT::run([](AppContext *ctx) {
					  // SDL_SetAtomicInt(&ctx.is_initialized,
				      // SoundContext::FALSE);
					  ctx->audio_asr_tts_status.is_recording_initialized = true;
				  });
			  }
			  m.lap().printms();
		  }});
}
void record_play(AppContext *ctx) {
	// worker_job_push(ctx, {.type = Job::Type::ASR_RECORD_PLAY});
	Worker::audio_job_push(
		  ctx,
		  AudioJob{.func = [](AudioContext *actx, AudioJob::Payload *payload) {
			  (void)payload;
			  Measure m{"JOB: REC PLAY"};
			  SDL_AudioSpec spec;

			  spec.channels = 1;
			  spec.format = SDL_AUDIO_F32;
			  spec.freq = AudioContext::FREQUENCY;

			  auto size_bytes = Atomic::get(&actx->rec_audio_buffer.size_bytes);
			  playback_play_(actx, spec, actx->rec_audio_buffer.data,
		                     size_bytes);
			  m.lap().printms();
		  }});
}

void playback_play_and_run_on_finished(
	  AppContext *ctx, PlaybackPayload *playback_payload_ptr,
	  void (*func_on_finished)(AudioContext *actx,
                               AudioJob::Payload *payload)) {
	SDL_Log(">>>> %s function call", __FUNCTION__);
	AudioJob::Payload j_payload = {.playback_payload_ptr =
	                                     playback_payload_ptr};
	// 	  .generated_audio_ptr = PlayGeneratedAudioDataPayload(payload)};

	Worker::audio_job_push(
		  ctx, {.func =
	                  [](AudioContext *actx, AudioJob::Payload *payload) {
						  Measure m{"JOB: PLAYBACK PLAY PAYLOAD"};
						  auto playback_payload = payload->playback_payload_ptr;
						  playback_play_(actx, playback_payload->spec,
		                                 playback_payload->audio_data,
		                                 playback_payload->audio_data_size);
					  },
	            .func_on_finished = func_on_finished,
	            .payload = j_payload});
}
