#pragma once

#include "app/app_context.h"
#include "SDL3/SDL_events.h"
#include "SDL3/SDL_init.h"

// function name, arguments, return type
#define UI_ENTRY_FUNCTIONS                                                     \
	X(ui_event, (AppContext * ctx, SDL_Event * event), SDL_AppResult)          \
	X(ui_iterate, (AppContext * ctx), SDL_AppResult)                           \
	X(ui_clay_init, (AppContext * ctx), void)                                  \
	X(ui_settings_init, (AppContext * ctx), void)

#ifndef HOTRELOAD
#define X(NAME, ARGS, RETURN_TYPE) extern "C" RETURN_TYPE NAME ARGS;
UI_ENTRY_FUNCTIONS
#undef X
#endif
