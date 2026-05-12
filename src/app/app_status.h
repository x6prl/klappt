#pragma once

#include <SDL3/SDL_init.h>

#include "base/stack.h"
#include "base/str_view.h"

struct AppStatus {
	SDL_AppResult app_quit{SDL_AppResult::SDL_APP_CONTINUE};
	Stack<StrView, 4> error_msgs;

	void set_exit_normal() { app_quit = SDL_AppResult::SDL_APP_SUCCESS; }
	void set_exit_with_error(StrView emsg) {
		app_quit = SDL_AppResult::SDL_APP_FAILURE;
		push_error(emsg);
	}
	void push_error(StrView emsg) { error_msgs.push(emsg); }
};
