#pragma once

#include "app/worker.h"

struct AppContext;
struct SherpaOnnxGeneratedAudio;

void record_init(AppContext *ctx);
void record_start(AppContext *ctx);
void record_stop_then_do_nothing(AppContext *ctx);
void record_stop_then_run_asr(AppContext *ctx);
void record_deinit(AppContext *ctx);
void record_play(AppContext *ctx);

void playback_play_and_run_on_finished(
	  AppContext *ctx, PlaybackPayload *playback_payload_ptr,
	  void (*func_on_finished)(AudioContext *actx, AudioJob::Payload *payload));
