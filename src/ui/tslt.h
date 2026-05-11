#pragma once

#include <cmath>
#include <cstdint>

#include "SDL3/SDL_events.h"
#include "SDL3/SDL_touch.h"
// #include "SDL3/SDL_log.h"

struct TapSwipeLongTap {
	enum State { KeyUp, KeyDown, Tap, Swipe, LongTap };

	static constexpr float MOUSE_SWIPE_THRESHOLD{5.0f};   // px
	static constexpr float TOUCH_SWIPE_THRESHOLD{18.0f};  // px
	static constexpr int LONGTAP_THRESHOLD{300}; // ms

	State state{KeyUp};
	uint64_t t{0};
	float x{};
	float y{};
	float swipe_threshold_px{MOUSE_SWIPE_THRESHOLD};
	bool delayed_reset{};

	bool is_tap() const { return state == Tap; }
	bool is_longtap() const { return state == LongTap; }
	bool is_swipe() const { return state == Swipe; }

	void flush_delayed_reset() {
		if (delayed_reset) {
			delayed_reset = false;
			state = KeyUp;
		}
	}

	void handle_event(uint64_t ticks, SDL_Event *event) {
		switch (event->type) {
		case SDL_EVENT_MOUSE_MOTION:
			if (event->motion.which == SDL_TOUCH_MOUSEID) {
				break;
			}
			handle_motion(ticks, event->motion.x, event->motion.y,
			              MOUSE_SWIPE_THRESHOLD);
			break;
		case SDL_EVENT_MOUSE_BUTTON_DOWN:
			if (event->button.which == SDL_TOUCH_MOUSEID) {
				break;
			}
			handle_key_down(ticks, event->button.x, event->button.y,
			                MOUSE_SWIPE_THRESHOLD);
			break;
		case SDL_EVENT_MOUSE_BUTTON_UP:
			if (event->button.which == SDL_TOUCH_MOUSEID) {
				break;
			}
			handle_key_up(ticks, event->button.x, event->button.y);
			break;
		default:
			handle_other(ticks);
		}
	}

	void handle_touch_event(uint64_t ticks, SDL_Event *event, float width,
	                        float height) {
		const auto px = event->tfinger.x * width;
		const auto py = event->tfinger.y * height;
		switch (event->type) {
		case SDL_EVENT_FINGER_MOTION:
			handle_motion(ticks, px, py, TOUCH_SWIPE_THRESHOLD);
			break;
		case SDL_EVENT_FINGER_DOWN:
			handle_key_down(ticks, px, py, TOUCH_SWIPE_THRESHOLD);
			break;
		case SDL_EVENT_FINGER_UP:
		case SDL_EVENT_FINGER_CANCELED:
			handle_key_up(ticks, px, py);
			break;
		default:
			handle_other(ticks);
		}
	}

	void handle_other(uint64_t ticks) {
		switch (state) {
		case KeyDown:
			if (ticks - t > LONGTAP_THRESHOLD) {
				// SDL_Log("LONG TAP { %f %f}", x, y);
				state = LongTap;
				delayed_reset = true;
			}
			break;
		default:
			break;
		}
	}

	void handle_motion(uint64_t ticks, float _x, float _y,
	                   float threshold_px) {
		switch (state) {
		case KeyDown: {
			swipe_threshold_px = threshold_px;
			const float dx = x - _x;
			const float dy = y - _y;
			if ((dx * dx) + (dy * dy) >
			    (swipe_threshold_px * swipe_threshold_px)) {
				// SDL_Log("SWIPE");
				state = Swipe;
			} else if (ticks - t > LONGTAP_THRESHOLD) {
				// SDL_Log("LONG TAP { %f %f}", x, y);
				state = LongTap;
				delayed_reset = true;
			}
			break;
		}
		default:
			break;
		}
	}

	void handle_key_down(uint64_t ticks, float _x, float _y,
	                     float threshold_px) {
		t = ticks;
		x = _x;
		y = _y;
		swipe_threshold_px = threshold_px;
		state = KeyDown;
		delayed_reset = false;
	}

	void handle_key_up(uint64_t ticks, float _x, float _y) {
		(void)ticks;
		x = _x;
		y = _y;
		switch (state) {
		case KeyDown:
			state = Tap;
			delayed_reset = true;
			// SDL_Log("TAP { %f %f}", x, y);
			break;
		default:
			state = KeyUp;
			// SDL_Log("<RESET>");
			break;
			// 	SDL_LogError(SDL_LOG_CATEGORY_ERROR, "%s error",
			// 	             __PRETTY_FUNCTION__);
		}
	}
};
