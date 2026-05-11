#pragma once

#include "SDL3/SDL_events.h"

struct AppContext;

void clay_handle_event(SDL_Event *event);
void clay_update_scroll(float delta_time_seconds);
void clay_init(AppContext *ctx);
