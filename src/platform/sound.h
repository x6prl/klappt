#pragma once

struct AppContext;

void record_init(AppContext *ctx);
void record_start(AppContext *ctx);
void record_stop(AppContext *ctx);
void record_deinit(AppContext *ctx);
void record_play(AppContext *ctx);
void run_asr(AppContext *ctx);
