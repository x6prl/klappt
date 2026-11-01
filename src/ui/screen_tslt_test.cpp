#include "app/app_context.h"
#include "base/str_builder.h"
#include "base/str_view.h"
#include "screen_helpers.h"
#include "ui/components/button.h"
#include <algorithm>
#include <cmath>
#include <cstring>

namespace {

struct TsltTestState {
	int single_tap_count{0};
	int double_tap_count{0};
	int triple_tap_count{0};
	int longtap_count{0};
	int scroll_count{0};
	bool was_scrolling{false};

	struct LogEntry {
		char buf[80]{};
		uint32_t len{0};
		Clay_Color color{};

		StrView view() const { return StrView{buf, len}; }
	};

	static constexpr size_t MAX_LOGS = 6;
	LogEntry logs[MAX_LOGS]{};
	size_t log_count{0};

	void add_log(StrView text, Clay_Color color) {
		if (log_count >= MAX_LOGS) {
			for (size_t i = 1; i < MAX_LOGS; ++i) {
				logs[i - 1] = logs[i];
			}
			log_count = MAX_LOGS - 1;
		}

		auto &entry = logs[log_count++];
		entry.len = static_cast<uint32_t>(
			  std::min(text.size, (long)sizeof(entry.buf) - 1));
		std::memcpy(entry.buf, text.data, entry.len);
		entry.buf[entry.len] = '\0';
		entry.color = color;
	}

	void reset() {
		single_tap_count = 0;
		double_tap_count = 0;
		triple_tap_count = 0;
		longtap_count = 0;
		scroll_count = 0;
		was_scrolling = false;
		log_count = 0;
	}
};

static TsltTestState s_test{};

static StrView state_to_str(TapScrollLongTap::State s) {
	switch (s) {
	case TapScrollLongTap::FingerUp:
		return "FingerUp"_v;
	case TapScrollLongTap::FingerDown:
		return "FingerDown"_v;
	case TapScrollLongTap::Tap:
		return "Tap"_v;
	case TapScrollLongTap::Scroll:
		return "Scroll"_v;
	case TapScrollLongTap::LongTap:
		return "LongTap"_v;
	default:
		return "Unknown"_v;
	}
}

static Clay_Color state_to_bg_color(AppContext *ctx,
                                    TapScrollLongTap::State s) {
	switch (s) {
	case TapScrollLongTap::FingerUp:
		return theme()->surfaceContainerHigh;
	case TapScrollLongTap::FingerDown:
		return theme()->secondary;
	case TapScrollLongTap::Tap:
		return theme()->primary;
	case TapScrollLongTap::Scroll:
		return theme()->surfaceContainerLow;
	case TapScrollLongTap::LongTap:
		return theme()->wrongContainer;
	default:
		return theme()->outline;
	}
}

static Clay_Color state_to_fg_color(AppContext *ctx,
                                    TapScrollLongTap::State s) {
	switch (s) {
	case TapScrollLongTap::FingerUp:
		return theme()->onSurfaceContainerHigh;
	case TapScrollLongTap::FingerDown:
		return theme()->onSecondary;
	case TapScrollLongTap::Tap:
		return theme()->onPrimary;
	case TapScrollLongTap::Scroll:
		return theme()->onSurface;
	case TapScrollLongTap::LongTap:
		return theme()->onWrongContainer;
	default:
		return theme()->onSurface;
	}
}

static StrView swipe_direction_str(float dx, float dy) {
	if (std::abs(dx) > std::abs(dy)) {
		return dx > 0 ? "Right ->"_v : "<- Left"_v;
	} else {
		return dy > 0 ? "Down v"_v : "^ Up"_v;
	}
}

} // namespace

void screen_tslt_test_push(AppContext *ctx) {
	s_test.reset();
	ctx->push(Screen::TsltTest);
}

void screen_tslt_test_draw(AppContext *ctx) {
	auto &tslt = ctx->tslt;
	Arena &frame = ctx->arena_frame;

	const float off_x = tslt.motion_offset_x();
	const float off_y = tslt.motion_offset_y();
	const float dist = std::sqrt(off_x * off_x + off_y * off_y);

	CLAY(CLAY_ID("TsltTestScreen"),
	     {
			   .layout =
					 {
						   .sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)},
						   .childAlignment = {CLAY_ALIGN_X_CENTER,
	                                          CLAY_ALIGN_Y_TOP},
						   .layoutDirection = CLAY_TOP_TO_BOTTOM,
					 },
			   .backgroundColor = theme()->surface,
		 }) {

		// ─── Top Telemetry & Diagnostics ─────────────────────────────────
		CLAY(CLAY_ID("TelemetryCard"),
		     {
				   .layout =
						 {
							   .sizing = {CLAY_SIZING_GROW(0),
		                                  CLAY_SIZING_FIT(0)},
							   .padding = sizes()->pad.card,
							   .childGap = sizes()->space.xs,
							   .layoutDirection = CLAY_TOP_TO_BOTTOM,
						 },
				   .backgroundColor = theme()->surfaceContainerLow,
				   .cornerRadius = sizes()->radius.md,
			 }) {

			// State badge row
			CLAY(CLAY_ID("StateRow"),
			     {
					   .layout =
							 {
								   .sizing = {CLAY_SIZING_GROW(0),
			                                  CLAY_SIZING_FIT(0)},
								   .childGap = sizes()->space.sm,
								   .childAlignment = {CLAY_ALIGN_X_LEFT,
			                                          CLAY_ALIGN_Y_CENTER},
								   .layoutDirection = CLAY_LEFT_TO_RIGHT,
							 },
				 }) {
				draw_text("State:"_v, theme()->onSurface,
				          sizes()->font.label_md);

				CLAY(CLAY_ID("StateBadge"),
				     {
						   .layout = {.padding = sizes()->pad.badge_compact},
						   .backgroundColor =
								 state_to_bg_color(ctx, tslt.state),
						   .cornerRadius = sizes()->radius.xs,
					 }) {
					draw_text(state_to_str(tslt.state),
					          state_to_fg_color(ctx, tslt.state),
					          sizes()->font.label_sm);
				}

				if (tslt.tap_count > 0) {
					CLAY(CLAY_ID("StreakBadge"),
					     {
							   .layout = {.padding =
					                            sizes()->pad.badge_compact},
							   .backgroundColor = theme()->surfaceContainerHigh,
							   .cornerRadius = sizes()->radius.xs,
						 }) {
						draw_text(
							  StrBuilder::concat(
									frame, "Streak: "_v,
									StrView::from_number(frame, tslt.tap_count)),
							  theme()->onSurface, sizes()->font.label_sm);
					}
				}

				if (tslt.is_delayed_reset) {
					CLAY(CLAY_ID("DelayedResetBadge"),
					     {
							   .layout = {.padding =
					                            sizes()->pad.badge_compact},
							   .backgroundColor = theme()->surfaceContainerHigh,
							   .cornerRadius = sizes()->radius.xs,
						 }) {
						draw_text("DelayedReset"_v, theme()->onSurface,
						          sizes()->font.label_sm);
					}
				}
			}

			// Pos & Delta readouts
			const auto pos_str = StrBuilder::concat(
				  frame, "Start: ("_v,
				  StrView::from_number(frame, static_cast<int>(tslt.x)), ", "_v,
				  StrView::from_number(frame, static_cast<int>(tslt.y)),
				  ")  Curr: ("_v,
				  StrView::from_number(frame, static_cast<int>(tslt.motion_x)),
				  ", "_v,
				  StrView::from_number(frame, static_cast<int>(tslt.motion_y)),
				  ")"_v);
			draw_text(pos_str, theme()->onSurfaceContainer,
			          sizes()->font.body_sm);

			const auto delta_str = StrBuilder::concat(
				  frame, "Delta: dx="_v,
				  StrView::from_number(frame, static_cast<int>(off_x)),
				  " dy="_v,
				  StrView::from_number(frame, static_cast<int>(off_y)),
				  " | Dist: "_v,
				  StrView::from_number(frame, static_cast<int>(dist)),
				  "px (Th: "_v,
				  StrView::from_number(
						frame, static_cast<int>(tslt.scroll_threshold_px)),
				  "px)"_v);
			draw_text(delta_str, theme()->outline, sizes()->font.label_sm);
		}

		// ─── Test Interactive Zones ──────────────────────────────────────
		CLAY(CLAY_ID("TestTargetsContainer"),
		     {
				   .layout =
						 {
							   .sizing = {CLAY_SIZING_GROW(0),
		                                  CLAY_SIZING_GROW(0)},
							   .padding = sizes()->pad.screen,
							   .childGap = sizes()->space.md,
							   .layoutDirection = CLAY_TOP_TO_BOTTOM,
						 },
			 }) {

			// ─── ZONE 1: Multi-Tap Target (Single / Double / Triple) ──────
			const auto tap_box_id = CLAY_ID("TapTestBox");
			const auto tap_rect = Clay_GetElementData(tap_box_id).boundingBox;

			const bool tap_hovered =
				  tslt.is_started_inside(tap_rect) &&
				  (tslt.state == TapScrollLongTap::FingerDown);

			if (tslt.is_triple_tap_started_and_ended_inside(tap_rect)) {
				s_test.triple_tap_count++;
				s_test.add_log(
					  StrBuilder::concat(
							frame, "TRIPLE-TAP #"_v,
							StrView::from_number(frame,
				                                 s_test.triple_tap_count),
							" inside!"_v),
					  theme()->primary);
				(void)tslt.consume_triple_tap();
			} else if (tslt.is_double_tap_started_and_ended_inside(tap_rect)) {
				s_test.double_tap_count++;
				s_test.add_log(
					  StrBuilder::concat(
							frame, "DOUBLE-TAP #"_v,
							StrView::from_number(frame,
				                                 s_test.double_tap_count),
							" inside"_v),
					  theme()->secondary);
				(void)tslt.consume_double_tap();
			} else if (tslt.is_single_tap_started_and_ended_inside(tap_rect)) {
				s_test.single_tap_count++;
				s_test.add_log(
					  StrBuilder::concat(
							frame, "Single Tap #"_v,
							StrView::from_number(frame,
				                                 s_test.single_tap_count),
							" inside"_v),
					  theme()->onSurface);
				(void)tslt.consume_single_tap();
			}

			CLAY(tap_box_id,
			     {
					   .layout =
							 {
								   .sizing = {CLAY_SIZING_GROW(0),
			                                  CLAY_SIZING_FIT(0)},
								   .padding = sizes()->pad.card_compact,
								   .childGap = sizes()->space.xs,
								   .childAlignment = {CLAY_ALIGN_X_CENTER,
			                                          CLAY_ALIGN_Y_CENTER},
								   .layoutDirection = CLAY_TOP_TO_BOTTOM,
							 },
					   .backgroundColor = tap_hovered
			                                    ? theme()->surfaceContainerHigh
			                                    : theme()->surfaceContainer,
					   .cornerRadius = sizes()->radius.md,
					   .border = {.color = theme()->outline,
			                      .width = {1, 1, 1, 1}},
				 }) {

				draw_text("MULTI-TAP TARGET"_v, theme()->onSurface,
				          sizes()->font.title_md);

				// Visual indicators for streak state 1, 2, 3
				CLAY(CLAY_ID("StreakRow"),
				     {
						   .layout =
								 {
									   .sizing = {CLAY_SIZING_GROW(0),
				                                  CLAY_SIZING_FIT(0)},
									   .childGap = sizes()->space.sm,
									   .childAlignment = {CLAY_ALIGN_X_CENTER,
				                                          CLAY_ALIGN_Y_CENTER},
									   .layoutDirection = CLAY_LEFT_TO_RIGHT,
								 },
					 }) {

					auto draw_badge = [&](int target_idx, StrView label,
					                      int count, Clay_Color active_color) {
						const bool is_active = (tslt.tap_count >= target_idx);
						CLAY(CLAY_IDI("StreakStep", target_idx),
						     {
								   .layout =
										 {
											   .padding =
													 sizes()->pad.badge_compact,
										 },
								   .backgroundColor =
										 is_active
											   ? active_color
											   : theme()->surfaceContainerHigh,
								   .cornerRadius = sizes()->radius.xs,
							 }) {
							draw_text(
								  StrBuilder::concat(
										frame, label, ": "_v,
										StrView::from_number(frame, count)),
								  is_active ? theme()->surface
											: theme()->onSurfaceContainer,
								  sizes()->font.label_sm);
						}
					};

					draw_badge(1, "1x"_v, s_test.single_tap_count,
					           theme()->onSurface);
					draw_badge(2, "2x"_v, s_test.double_tap_count,
					           theme()->secondary);
					draw_badge(3, "3x"_v, s_test.triple_tap_count,
					           theme()->primary);
				}
			}

			// ─── ZONE 2: Long-Tap Target ─────────────────────────────────
			const auto longtap_box_id = CLAY_ID("LongTapTestBox");
			const auto longtap_rect =
				  Clay_GetElementData(longtap_box_id).boundingBox;

			const bool holding_inside_longtap =
				  tslt.is_started_and_ended_inside(longtap_rect) &&
				  (tslt.state == TapScrollLongTap::FingerDown);

			if (tslt.is_longtap_started_and_ended_inside(longtap_rect)) {
				s_test.longtap_count++;
				s_test.add_log(
					  StrBuilder::concat(
							frame, "Long-Tap #"_v,
							StrView::from_number(frame, s_test.longtap_count),
							" inside (bounds ok)"_v),
					  theme()->secondary);
				(void)tslt.consume_longtap();
			}

			CLAY(longtap_box_id,
			     {
					   .layout =
							 {
								   .sizing = {CLAY_SIZING_GROW(0),
			                                  CLAY_SIZING_FIXED(72.f)},
								   .padding = sizes()->pad.card_compact,
								   .childAlignment = {CLAY_ALIGN_X_CENTER,
			                                          CLAY_ALIGN_Y_CENTER},
								   .layoutDirection = CLAY_TOP_TO_BOTTOM,
							 },
					   .backgroundColor = holding_inside_longtap
			                                    ? theme()->wrongContainer
			                                    : theme()->surfaceContainer,
					   .cornerRadius = sizes()->radius.md,
					   .border = {.color = holding_inside_longtap
			                                     ? theme()->secondary
			                                     : theme()->outline,
			                      .width = {2, 2, 2, 2}},
				 }) {
				draw_text(
					  StrBuilder::concat(
							frame, "HOLD FOR LONG-TAP (Count: "_v,
							StrView::from_number(frame, s_test.longtap_count),
							")"_v),
					  holding_inside_longtap ? theme()->onWrongContainer
											 : theme()->onSurface,
					  sizes()->font.title_md);
				if (holding_inside_longtap) {
					draw_text("Holding inside bounds... waiting threshold"_v,
					          theme()->onWrongContainer,
					          sizes()->font.label_sm);
				}
			}

			// ─── ZONE 3: Swipe / Drag Surface with Scroll Bound Overlay ───
			const auto swipe_surface_id = CLAY_ID("SwipeTestSurface");
			const auto swipe_rect =
				  Clay_GetElementData(swipe_surface_id).boundingBox;

			const bool is_touch_started_in_pad =
				  tslt.is_started_inside(swipe_rect);
			const bool is_active_touch_in_pad =
				  is_touch_started_in_pad &&
				  (tslt.state != TapScrollLongTap::FingerUp);
			const bool is_scrolling_from_pad =
				  tslt.is_scroll_started_inside(swipe_rect);

			if (is_scrolling_from_pad) {
				if (!s_test.was_scrolling) {
					s_test.was_scrolling = true;
					s_test.scroll_count++;
					s_test.add_log(
						  StrBuilder::concat(frame, "Scroll #"_v,
					                         StrView::from_number(
												   frame, s_test.scroll_count),
					                         " started inside ("_v,
					                         swipe_direction_str(off_x, off_y),
					                         ")"_v),
						  theme()->secondary);
				}
			} else if (!tslt.is_scroll()) {
				s_test.was_scrolling = false;
			}

			CLAY(swipe_surface_id,
			     {
					   .layout =
							 {
								   .sizing = {CLAY_SIZING_GROW(0),
			                                  CLAY_SIZING_GROW(0)},
								   .padding = sizes()->pad.card,
								   .childGap = sizes()->space.xs,
								   .childAlignment = {CLAY_ALIGN_X_CENTER,
			                                          CLAY_ALIGN_Y_CENTER},
								   .layoutDirection = CLAY_TOP_TO_BOTTOM,
							 },
					   .backgroundColor = is_scrolling_from_pad
			                                    ? theme()->surfaceContainer
			                                    : theme()->surfaceContainerLow,
					   .cornerRadius = sizes()->radius.md,
					   .border = {.color = is_scrolling_from_pad
			                                     ? theme()->primary
			                                     : theme()->outline,
			                      .width = {1, 1, 1, 1}},
				 }) {

				draw_text(
					  StrBuilder::concat(
							frame, "SWIPE / DRAG PAD (Swipes: "_v,
							StrView::from_number(frame, s_test.scroll_count),
							")"_v),
					  theme()->onSurfaceContainer, sizes()->font.body_md);

				if (is_scrolling_from_pad) {
					draw_text(swipe_direction_str(off_x, off_y),
					          theme()->primary, sizes()->font.title_lg);
				} else {
					draw_text("Drag finger here to test scroll threshold"_v,
					          theme()->outline, sizes()->font.label_sm);
				}

				// ─── Scroll Threshold Metric & Gauge Bar ─────────────────
				const float th = std::max(tslt.scroll_threshold_px, 1.0f);
				const float travel = is_active_touch_in_pad ? dist : 0.0f;
				const float progress_fraction =
					  std::clamp(travel / th, 0.0f, 1.0f);

				draw_text(
					  StrBuilder::concat(
							frame, "Scroll Bound: "_v,
							StrView::from_number(frame,
				                                 static_cast<int>(travel)),
							" / "_v,
							StrView::from_number(frame, static_cast<int>(th)),
							"px"_v),
					  is_scrolling_from_pad ? theme()->primary
											: theme()->onSurfaceContainer,
					  sizes()->font.label_sm);

				constexpr float GAUGE_WIDTH = 220.0f;
				CLAY(CLAY_ID("ScrollBoundTrack"),
				     {
						   .layout =
								 {
									   .sizing = {CLAY_SIZING_FIXED(
														GAUGE_WIDTH),
				                                  CLAY_SIZING_FIXED(6.f)},
									   .layoutDirection = CLAY_LEFT_TO_RIGHT,
								 },
						   .backgroundColor = theme()->surfaceContainerHigh,
						   .cornerRadius = sizes()->radius.full,
					 }) {
					const float fill_w = GAUGE_WIDTH * progress_fraction;
					if (fill_w > 0.0f) {
						CLAY(CLAY_ID("ScrollBoundFill"),
						     {
								   .layout =
										 {
											   .sizing = {CLAY_SIZING_FIXED(
																fill_w),
						                                  CLAY_SIZING_GROW(0)},
										 },
								   .backgroundColor =
										 is_scrolling_from_pad
											   ? theme()->primary
											   : theme()->secondary,
								   .cornerRadius = sizes()->radius.full,
							 }) {}
					}
				}

				// ─── Spatial Dead-Zone Circle & Tracking Dots ─────────────
				if (is_active_touch_in_pad) {
					const float radius = tslt.scroll_threshold_px;
					const float diam = radius * 2.0f;
					const float local_start_x = tslt.x - swipe_rect.x;
					const float local_start_y = tslt.y - swipe_rect.y;

					// 1. Scroll boundary ring (circle of radius scroll_threshold_px)
					CLAY(CLAY_ID("ScrollDeadZoneCircle"),
					     {
							   .layout =
									 {
										   .sizing = {CLAY_SIZING_FIXED(diam),
					                                  CLAY_SIZING_FIXED(diam)},
										   .childAlignment =
												 {CLAY_ALIGN_X_CENTER,
					                              CLAY_ALIGN_Y_CENTER},
									 },
							   .backgroundColor =
									 is_scrolling_from_pad
										   ? theme()->surfaceContainerHigh
										   : theme()->surfaceContainerLow,
							   .cornerRadius = sizes()->radius.full,
							   .floating =
									 {
										   .offset = {local_start_x - radius,
					                                  local_start_y - radius},
										   .zIndex = 1,
										   .attachTo = CLAY_ATTACH_TO_PARENT,
									 },
							   .border =
									 {
										   .color = is_scrolling_from_pad
					                                      ? theme()->primary
					                                      : theme()->secondary,
										   .width = {2, 2, 2, 2},
									 },
						 }) {
						// Touch Start Anchor point
						CLAY(CLAY_ID("ScrollStartAnchor"),
						     {
								   .layout =
										 {
											   .sizing =
													 {CLAY_SIZING_FIXED(4.f),
						                              CLAY_SIZING_FIXED(4.f)},
										 },
								   .backgroundColor = theme()->onSurface,
								   .cornerRadius = sizes()->radius.full,
							 }) {}
					}

					// 2. Current pointer location dot
					const float local_curr_x = tslt.motion_x - swipe_rect.x;
					const float local_curr_y = tslt.motion_y - swipe_rect.y;

					CLAY(CLAY_ID("ScrollCurrentFingerDot"),
					     {
							   .layout =
									 {
										   .sizing = {CLAY_SIZING_FIXED(10.f),
					                                  CLAY_SIZING_FIXED(10.f)},
									 },
							   .backgroundColor = is_scrolling_from_pad
					                                    ? theme()->primary
					                                    : theme()->outline,
							   .cornerRadius = sizes()->radius.full,
							   .floating =
									 {
										   .offset = {local_curr_x - 5.f,
					                                  local_curr_y - 5.f},
										   .zIndex = 2,
										   .attachTo = CLAY_ATTACH_TO_PARENT,
									 },
							   .border = {.color = theme()->surface,
					                      .width = {1, 1, 1, 1}},
						 }) {}
				}
			}

			// ─── Recent Gesture Event Log ─────────────────────────────────
			CLAY(CLAY_ID("LogContainer"),
			     {
					   .layout =
							 {
								   .sizing = {CLAY_SIZING_GROW(0),
			                                  CLAY_SIZING_FIT(0)},
								   .padding = sizes()->pad.card_compact,
								   .childGap = sizes()->space.xs,
								   .layoutDirection = CLAY_TOP_TO_BOTTOM,
							 },
					   .backgroundColor = theme()->surfaceContainer,
					   .cornerRadius = sizes()->radius.sm,
				 }) {
				draw_text("Event Log:"_v, theme()->outline,
				          sizes()->font.label_sm);

				if (s_test.log_count == 0) {
					draw_text("No events recorded yet."_v,
					          theme()->onSurfaceContainer,
					          sizes()->font.label_sm);
				} else {
					for (size_t i = 0; i < s_test.log_count; ++i) {
						draw_text(s_test.logs[i].view(), s_test.logs[i].color,
						          sizes()->font.label_sm);
					}
				}
			}
		}

		// ─── Bottom Action Bar ───────────────────────────────────────────
		CLAY(CLAY_ID("BottomBar"),
		     {
				   .layout =
						 {
							   .sizing = {CLAY_SIZING_GROW(0),
		                                  CLAY_SIZING_FIT(0)},
							   .padding = sizes()->pad.screen,
							   .childGap = sizes()->space.md,
							   .childAlignment = {CLAY_ALIGN_X_CENTER,
		                                          CLAY_ALIGN_Y_CENTER},
							   .layoutDirection = CLAY_LEFT_TO_RIGHT,
						 },
				   .backgroundColor = theme()->surface,
			 }) {

			auto back_btn = mobile_icon_button<false>(
				  ctx, CLAY_ID_LOCAL("BackButton"), Icons::BACK);
			if (back_btn.activated()) {
				on_back_button_pressed(ctx);
			}

			auto clear_btn = mobile_icon_button<false>(
				  ctx, CLAY_ID_LOCAL("ClearButton"), Icons::ROTATE);
			if (clear_btn.activated()) {
				s_test.reset();
			}
		}
	}
}
