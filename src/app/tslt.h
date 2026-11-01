#pragma once

#include "SDL3/SDL_events.h"
#include "SDL3/SDL_touch.h"
#include <clay/clay.h>

struct TapScrollLongTap {
	enum State { FingerUp, FingerDown, Tap, Scroll, LongTap };

	static constexpr float SCROLL_THRESHOLD_BASE{5.0f};          // px
	static constexpr float MULTITAP_RANGE_THRESHOLD_BASE{12.0f}; // px
	static constexpr int LONGTAP_THRESHOLD{300};                 // ms
	static constexpr int MULTITAP_TIME_THRESHOLD{350};           // ms

	State state{FingerUp};
	uint64_t key_down_time{0};
	uint64_t prev_tap_time{0};
	float x{};
	float y{};
	float motion_x{};
	float motion_y{};
	float prev_tap_x{};
	float prev_tap_y{};
	float scroll_threshold_px{SCROLL_THRESHOLD_BASE};
	float multitap_range_threshold_px{MULTITAP_RANGE_THRESHOLD_BASE};
	uint8_t tap_count{0}; // 1 = Single, 2 = Double, 3 = Triple
	bool is_delayed_reset{false};
	SDL_FingerID active_finger_id{0};
	bool has_active_finger{false};

	void on_display_scale_changed(float scale) {
		scroll_threshold_px = SCROLL_THRESHOLD_BASE * scale;
		multitap_range_threshold_px = MULTITAP_RANGE_THRESHOLD_BASE * scale;
	}

	// === Gesture Queries ===
	[[nodiscard]]
	bool is_tap() const {
		return state == Tap;
	}
	[[nodiscard]]
	bool is_single_tap() const {
		return state == Tap && tap_count == 1;
	}
	[[nodiscard]]
	bool is_double_tap() const {
		return state == Tap && tap_count == 2;
	}
	[[nodiscard]]
	bool is_triple_tap() const {
		return state == Tap && tap_count == 3;
	}
	[[nodiscard]]
	bool is_longtap() const {
		return state == LongTap;
	}
	[[nodiscard]]
	bool is_tap_or_longtap() const {
		return is_tap() || is_longtap();
	}
	[[nodiscard]]
	bool is_scroll() const {
		return state == Scroll;
	}
	[[nodiscard]]
	bool is_tap_or_longtap_or_scroll() const {
		return is_tap() || is_longtap() || is_scroll();
	}

	// === Bounding Box Queries ===
	[[nodiscard]]
	bool is_started_inside(const Clay_BoundingBox &rect) const {
		return point_in_rect(x, y, rect);
	}
	[[nodiscard]]
	bool is_scroll_started_inside(const Clay_BoundingBox &rect) const {
		return is_scroll() && is_started_inside(rect);
	}
	[[nodiscard]]
	bool is_started_and_ended_inside(const Clay_BoundingBox &rect) const {
		return point_in_rect(x, y, rect) &&
		       point_in_rect(motion_x, motion_y, rect);
	}
	[[nodiscard]]
	bool is_tap_started_and_ended_inside(const Clay_BoundingBox &rect) const {
		return is_tap() && is_started_and_ended_inside(rect);
	}
	[[nodiscard]]
	bool
	is_single_tap_started_and_ended_inside(const Clay_BoundingBox &rect) const {
		return is_single_tap() && is_started_and_ended_inside(rect);
	}
	[[nodiscard]]
	bool
	is_double_tap_started_and_ended_inside(const Clay_BoundingBox &rect) const {
		return is_double_tap() && is_started_and_ended_inside(rect);
	}
	[[nodiscard]]
	bool
	is_triple_tap_started_and_ended_inside(const Clay_BoundingBox &rect) const {
		return is_triple_tap() && is_started_and_ended_inside(rect);
	}
	[[nodiscard]]
	bool
	is_longtap_started_and_ended_inside(const Clay_BoundingBox &rect) const {
		return is_longtap() && is_started_and_ended_inside(rect);
	}

	// === Motion Offsets ===
	[[nodiscard]]
	float motion_offset_x() const {
		return motion_x - x;
	}
	[[nodiscard]]
	float motion_offset_y() const {
		return motion_y - y;
	}

	// === Event Consumption ===
	[[nodiscard]]
	bool consume_tap() {
		if (state == Tap) {
			reset_tap_state();
			return true;
		}
		return false;
	}
	[[nodiscard]]
	bool consume_single_tap() {
		if (is_single_tap()) {
			reset_tap_state();
			return true;
		}
		return false;
	}
	[[nodiscard]]
	bool consume_double_tap() {
		if (is_double_tap()) {
			reset_tap_state();
			return true;
		}
		return false;
	}
	[[nodiscard]]
	bool consume_triple_tap() {
		if (is_triple_tap()) {
			reset_tap_state();
			return true;
		}
		return false;
	}
	[[nodiscard]]
	bool consume_longtap() {
		if (state == LongTap) {
			state = FingerUp;
			is_delayed_reset = false;
			return true;
		}
		return false;
	}

	void flush_delayed_reset() {
		if (is_delayed_reset) {
			is_delayed_reset = false;
			state = FingerUp;
		}
	}

	void handle_sdl_event(float window_width, float window_height,
	                      uint64_t ticks, SDL_Event *event) {
		switch (event->type) {
		// --- TOUCH EVENTS
		case SDL_EVENT_FINGER_UP: {
			if (has_active_finger &&
			    event->tfinger.fingerID == active_finger_id) {
				handle_key_up(ticks, event->tfinger.x * window_width,
				              event->tfinger.y * window_height);
				has_active_finger = false;
			}
		} break;
		case SDL_EVENT_FINGER_DOWN: {
			if (!has_active_finger) {
				has_active_finger = true;
				active_finger_id = event->tfinger.fingerID;
				handle_key_down(ticks, event->tfinger.x * window_width,
				                event->tfinger.y * window_height);
			}
		} break;
		case SDL_EVENT_FINGER_MOTION: {
			if (has_active_finger &&
			    event->tfinger.fingerID == active_finger_id) {
				handle_motion(ticks, event->tfinger.x * window_width,
				              event->tfinger.y * window_height);
			}
		} break;
		case SDL_EVENT_FINGER_CANCELED: {
			if (has_active_finger &&
			    event->tfinger.fingerID == active_finger_id) {
				has_active_finger = false;
				state = FingerUp;
				is_delayed_reset = false;
				prev_tap_time = 0;
				tap_count = 0;
			}
		} break;
		// --- MOUSE EVENTS
		case SDL_EVENT_MOUSE_MOTION: {
			if (event->motion.which != SDL_TOUCH_MOUSEID && has_active_finger) {
				handle_motion(ticks, event->motion.x, event->motion.y);
			}
		} break;
		case SDL_EVENT_MOUSE_BUTTON_DOWN: {
			if (event->button.which != SDL_TOUCH_MOUSEID &&
			    event->button.button == SDL_BUTTON_LEFT) {
				if (!has_active_finger) {
					has_active_finger = true;
					handle_key_down(ticks, event->button.x, event->button.y);
				}
			}
		} break;
		case SDL_EVENT_MOUSE_BUTTON_UP: {
			if (event->button.which != SDL_TOUCH_MOUSEID &&
			    event->button.button == SDL_BUTTON_LEFT) {
				if (has_active_finger) {
					handle_key_up(ticks, event->button.x, event->button.y);
					has_active_finger = false;
				}
			}
		} break;
		default:
			break;
		}

		switch (state) {
		case FingerDown:
			if (ticks - key_down_time > LONGTAP_THRESHOLD) {
				state = LongTap;
				is_delayed_reset = true;
				prev_tap_time = 0;
				tap_count = 0;
			}
			break;
		default:
			break;
		}
	}

	void handle_motion(uint64_t ticks, float new_x, float new_y) {
		if (state != FingerUp) {
			motion_x = new_x;
			motion_y = new_y;
		}
		switch (state) {
		case FingerDown: {
			const float dx = x - motion_x;
			const float dy = y - motion_y;
			if ((dx * dx) + (dy * dy) >
			    (scroll_threshold_px * scroll_threshold_px)) {
				state = Scroll;
				prev_tap_time = 0;
				tap_count = 0;
			} else if (ticks - key_down_time > LONGTAP_THRESHOLD) {
				state = LongTap;
				is_delayed_reset = true;
				prev_tap_time = 0;
				tap_count = 0;
			}
			break;
		}
		default:
			break;
		}
	}

	void handle_key_down(uint64_t ticks, float new_x, float new_y) {
		key_down_time = ticks;
		x = new_x;
		y = new_y;
		motion_x = new_x;
		motion_y = new_y;
		state = FingerDown;
		is_delayed_reset = false;
	}
	void handle_key_up(uint64_t ticks, float new_x, float new_y) {
		motion_x = new_x;
		motion_y = new_y;
		switch (state) {
		case FingerDown:
			if (ticks - key_down_time >= LONGTAP_THRESHOLD) {
				state = LongTap;
				prev_tap_time = 0;
				tap_count = 0;
			} else {
				const float dx = prev_tap_x - new_x;
				const float dy = prev_tap_y - new_y;
				const bool is_in_range =
					  (dx * dx) + (dy * dy) < (multitap_range_threshold_px *
				                               multitap_range_threshold_px);

				if ((ticks - prev_tap_time <= MULTITAP_TIME_THRESHOLD) &&
				    is_in_range) {
					tap_count++;
				} else {
					tap_count = 1;
				}

				if (tap_count >= 3) {
					tap_count = 3;
					prev_tap_time = 0;
				} else {
					prev_tap_time = ticks;
				}

				prev_tap_x = new_x;
				prev_tap_y = new_y;
				state = Tap;
			}
			is_delayed_reset = true;
			break;
		default:
			state = FingerUp;
			prev_tap_time = 0;
			tap_count = 0;
			break;
		}
	}

	void reset_tap_state() {
		state = FingerUp;
		is_delayed_reset = false;
	}

	// NOTE: left/top inclusive, right/bottom exclusive
	static inline bool point_in_rect(float px, float py,
	                                 const Clay_BoundingBox &r) {
		return px >= r.x && px < r.x + r.width && py >= r.y &&
		       py < r.y + r.height;
	}
};
