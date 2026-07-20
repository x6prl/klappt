#pragma once

#include "base/str_view.h"

struct AppContext;

void init_tts(AppContext *ctx);
void init_asr(AppContext *ctx);
void run_asr(AppContext *ctx);
void run_tts(AppContext *ctx, StrView tts_text);
