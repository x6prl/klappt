#pragma once

#include <SDL3/SDL_init.h>

#include "base/fixed_set.h"
#include "base/stack.h"
#include "base/str_view.h"

struct AppContext;

struct AppStatus {
	struct Notification {
		constexpr static uint64_t DEFAULT_TTL_MS = 5 * 1000; // ms
		StrView title{};
		StrView body{};
		uint64_t deadline{0};
		void (*on_click_cb)(AppContext *ctx){nullptr};
		bool operator==(const Notification &other) const = default;
	};
	SDL_AppResult app_quit{SDL_AppResult::SDL_APP_CONTINUE};
	FixedSet<Notification, 4> notifications;
	Stack<StrView, 4> error_msgs;

	void push_notification(Notification n) {
		for (auto &it : notifications) {
			if (n.body == it.body && n.title == it.body) {
				return;
			}
		}
		if (!notifications.push_one(n)) {
			push_error("Too many notifications"_v);
		}
	}
	void push_error(StrView emsg) { error_msgs.push(emsg); }

	void set_exit_normal() { app_quit = SDL_AppResult::SDL_APP_SUCCESS; }
	void set_exit_with_error(StrView emsg) {
		app_quit = SDL_AppResult::SDL_APP_FAILURE;
		push_error(emsg);
	}
};
