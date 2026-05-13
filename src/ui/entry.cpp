#include "entry.h"
#include "screen_helpers.h"

#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#endif

#include "SDL3/SDL_events.h"
#include "SDL3/SDL_log.h"
#include "SDL3/SDL_render.h"
#include "SDL3/SDL_timer.h"

#include "app/event_codes.h"
#include "base/pair.h"
#include "base/profiler.h"
#include "base/str_view.h"
#include "base/str_view_list.h"
#include "clay_support.h"
#include "components/button.h"
#include "components/keypad_island.h"
#include "components/list_island.h"
#include "components/text_input.h"
#include "dpi.h"
#include "sdlcr.h"
#include "textcache.h"
#include "themes.h"
#include "tslt.h"

void screen_start_go(AppContext *ctx);
void screen_start_draw(AppContext *ctx);

void screen_exercise_go(AppContext *ctx, bool reset_stack);
void screen_exercise_draw(AppContext *ctx);

void screen_exercise_summary_go(AppContext *ctx);
void screen_exercise_summary_draw(AppContext *ctx);

void screen_exercise_review_push(AppContext *ctx);
void screen_exercise_review_draw(AppContext *ctx);

void screen_words_list_go(AppContext *ctx);
void screen_words_list_draw(AppContext *ctx);

void screen_learning_list_go(AppContext *ctx);
void screen_learning_list_draw(AppContext *ctx);

void screen_word_suggestions_go(AppContext *ctx);
void screen_word_suggestions_draw(AppContext *ctx);

void screen_settings_push(AppContext *ctx);
void screen_settings_draw(AppContext *ctx);

void screen_word_push(AppContext *ctx, WordId word_id);
void screen_word_draw(AppContext *ctx);

void screen_word_edit_push(AppContext *ctx, WordId word_id);
void screen_word_edit_draw(AppContext *ctx);

void screen_onboarding_go(AppContext *ctx);
void screen_onboarding_draw(AppContext *ctx);

namespace {
constexpr Uint32 animation_timer_interval_ms = 4;
constexpr Uint64 interaction_animation_duration_ms = 700;

Uint32 SDLCALL animation_timer_cb(void *userdata, SDL_TimerID,
                                  Uint32 interval) {
	auto *ctx = static_cast<AppContext *>(userdata);
	SDL_Event event{};
	event.type = SDL_EVENT_USER;
	event.user.code = ANIMATION_EVENT_CODE;
	SDL_PushEvent(&event);

	return (ctx->ticks - ctx->animation_ticks_start <
	        interaction_animation_duration_ms)
	             ? interval
	             : 0;
}

void frame_end(AppContext *ctx) {
	if (ctx->animate) {
		ctx->animate = false;
		ctx->animation_ticks_start = ctx->ticks;
		if (ctx->animation_timer_id) {
			SDL_RemoveTimer(ctx->animation_timer_id);
			ctx->animation_timer_id = 0;
		}
		ctx->animation_timer_id = SDL_AddTimer(animation_timer_interval_ms,
		                                       animation_timer_cb, ctx);
	}
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"

#ifdef __EMSCRIPTEN__
extern "C" EMSCRIPTEN_KEEPALIVE void mobile_text_input_web_wakeup() {
	SDL_Event event{};
	event.type = SDL_EVENT_USER;
	event.user.code = TEXT_INPUT_WAKE_EVENT_CODE;
	SDL_PushEvent(&event);
}
#endif

Clay_Dimensions measure_text_sdl(Clay_StringSlice text,
                                 Clay_TextElementConfig *config,
                                 void *userData) {
	AppContext *ctx = static_cast<AppContext *>(userData);
	return ctx->text->measure_text(text, config);
}

static bool dm = false;

void app_bar_layout(AppContext *ctx, StrView title) {
	if (ctx->app_status.error_msgs.size) {
		StrViewArray strs{};
		strs.push(ctx->tmparena, title);
		strs.push(ctx->tmparena, "e:"_v);
		strs.push(ctx->tmparena,
		          StrView::from_integer(ctx->tmparena,
		                                ctx->app_status.error_msgs.size));
		title = strs.join(ctx->tmparena, ' ');
	}
	const auto app_bar_height = dpi(60.0f);
	const auto app_bar_button_style = mobile_button_style_app_bar();
	CLAY(CLAY_ID("AppBar"),
	     {
			   .layout =
					 {
						   .sizing =
								 {
									   .width = CLAY_SIZING_GROW(0),
									   .height =
											 CLAY_SIZING_FIXED(app_bar_height),
								 },
						   .childAlignment = {CLAY_ALIGN_X_LEFT,
	                                          CLAY_ALIGN_Y_BOTTOM},
					 },
			   .backgroundColor = theme()->surfaceContainerLow,
		 }) {

		CLAY(CLAY_ID("BackButtonSlot"),
		     {
				   .layout =
						 {
							   .sizing =
									 {
										   .width = CLAY_SIZING_FIXED(
												 app_bar_height),
										   .height = CLAY_SIZING_FIXED(
												 app_bar_height),
									 },
							   .childAlignment = {CLAY_ALIGN_X_CENTER,
		                                          CLAY_ALIGN_Y_CENTER},
						 },
			 }) {
			if (ctx->is_backable()) {
				auto back = mobile_button(ctx, CLAY_ID("BackButton"), ""_v,
				                          app_bar_button_style);
				if (back.tapped) {
					ctx->pop();
				}
			}
		}
		CLAY(CLAY_ID("Title"),
		     {
				   .layout =
						 {
							   .sizing = {CLAY_SIZING_GROW(0),
		                                  CLAY_SIZING_FIXED(app_bar_height)},
							   .childAlignment = {CLAY_ALIGN_X_CENTER,
		                                          CLAY_ALIGN_Y_CENTER},
						 },
			 }) {
			if (title) {
				draw_text(title, theme()->onSurfaceContainerLow, udpi(20.0f));
			}
		}
		CLAY(CLAY_ID("RightActionSlot"),
		     {
				   .layout =
						 {
							   .sizing =
									 {
										   .width = CLAY_SIZING_FIXED(
												 app_bar_height),
										   .height = CLAY_SIZING_FIXED(
												 app_bar_height),
									 },
							   .childAlignment = {CLAY_ALIGN_X_CENTER,
		                                          CLAY_ALIGN_Y_CENTER},
						 },
			 }) {
			if (ctx->screen() != Screen::Settings) {
				auto settings = mobile_button(ctx, CLAY_ID("Settings"), ""_v,
				                              app_bar_button_style);
				if (settings.activated()) {
					screen_settings_push(ctx);
				}
			}
		}
	}
}

void bottom_bar_layout(AppContext *ctx) {
	constexpr auto bottom_bar_size{60.f};
	constexpr Arr<Pair<StrView, Screen>, 3> menu{{
		  {""_v, Screen::Start},
		  {""_v, Screen::WordsList},
		  {""_v, Screen::LearningList},
	}};

	CLAY(CLAY_ID("BottomBar"),
	     {.layout =
	            {
					  .sizing =
							{
								  CLAY_SIZING_GROW(0),
								  CLAY_SIZING_FIXED(dpi(bottom_bar_size)),
							},
				},
	      .backgroundColor = theme()->surfaceContainerLow}) {

		const Clay_ElementDeclaration buttonParent{
			  .layout =
					{
						  .sizing = {.width = CLAY_SIZING_GROW(0),
		                             .height = CLAY_SIZING_FIXED(
										   dpi(bottom_bar_size))},
						  .childAlignment = {CLAY_ALIGN_X_CENTER,
		                                     CLAY_ALIGN_Y_CENTER},
					},
			  .backgroundColor = {}};

		auto style = mobile_button_style_surface_container_high();
		style.background = style.border = {};
		style.font_id = FontID::ICONS;
		style.font_size = bottom_bar_size / 1.8;
		style.height = bottom_bar_size;
		style.fill_width = true;
		style.corner_radius = 0.f;
		style.border_width = 0.f;

		for (Size i = 0; i < menu.size(); ++i) {
			CLAY(CLAY_IDI("ButtonParent", i), buttonParent) {
				auto b = mobile_button(ctx, CLAY_IDI("Button", i),
				                       menu[i].first, style);
				if (b.activated()) {
					if (menu[i].second == Screen::WordsList) {
						screen_words_list_go(ctx);
					} else if (menu[i].second == Screen::LearningList) {
						screen_learning_list_go(ctx);
					} else {
						screen_start_go(ctx);
					}
				}
			}
		}
	}
}

StrView str_view_x_of_n(Arena &a, Size x, Size n) {
	++x;
	StrViewArray strs{};

	strs.push(a, StrView::from_integer(a, x));
	strs.push(a, StrView::from_integer(a, n));
	return strs.join(a, '/');
}

StrView exercise_x_of_n(AppContext *ctx) {
	auto etotal = ctx->exercises.exercise_total();
	if (!etotal) {
		return {};
	}
	auto &a = ctx->tmparena;
	return str_view_x_of_n(a, ctx->exercises.exercise_current_idx, etotal);
}

StrView review_x_of_n(AppContext *ctx) {
	auto etotal_to_show = ctx->exercises.results.size;
	if (!etotal_to_show) {
		return {};
	}
	auto &a = ctx->tmparena;
	return str_view_x_of_n(a, ctx->exercises.exercise_current_idx,
	                       etotal_to_show);
}

} // namespace

extern "C" void ui_clay_init(AppContext *ctx) {
	KLAPPT_PROFILE_SCOPE_N("ui_clay_init");
	clay_init(ctx);
	Clay_SetMeasureTextFunction(measure_text_sdl, ctx);
}

extern "C" void ui_settings_init(AppContext *ctx) {
	KLAPPT_PROFILE_SCOPE_N("ui_settings_init");
	SDL_Log("SETTINGS INIT");
	theme_set(ctx->settings.theme_type);
}

extern "C" SDL_AppResult ui_event(AppContext *ctx, SDL_Event *event) {
	KLAPPT_PROFILE_SCOPE_N("ui_event");
	KLAPPT_PROFILE_NAME_F("ui_event:type=%u screen=%s", event->type,
	                      screen_name(ctx->screen()));
	const auto ticks = ctx->ticks;

	{
		KLAPPT_PROFILE_SCOPE_N("ui_event.pointer_state");
		switch (event->type) {
		case SDL_EVENT_FINGER_UP:
		case SDL_EVENT_FINGER_CANCELED:
		case SDL_EVENT_FINGER_DOWN:
		case SDL_EVENT_FINGER_MOTION: {
			int width = 0;
			int height = 0;
			SDL_GetWindowSizeInPixels(ctx->window, &width, &height);
			ctx->tslt.handle_touch_event(ticks, event,
			                             static_cast<float>(width),
			                             static_cast<float>(height));
			break;
		}
		default:
			ctx->tslt.handle_event(ticks, event);
			break;
		}
	}
	{
		KLAPPT_PROFILE_SCOPE_N("ui_event.clay_handle_event");
		clay_handle_event(event);
	}
	{
		KLAPPT_PROFILE_SCOPE_N("ui_event.mobile_text_input_handle_event");
		if (mobile_text_input_handle_event(ctx, event)) {
			ctx->anim();
			return SDL_APP_CONTINUE;
		}
	}

	{
		KLAPPT_PROFILE_SCOPE_N("ui_event.dispatch");
		switch (event->type) {
		case SDL_EVENT_QUIT:
			ctx->app_status.set_exit_normal();
			return SDL_APP_CONTINUE;

		case SDL_EVENT_FINGER_DOWN:
		case SDL_EVENT_FINGER_MOTION:
		case SDL_EVENT_MOUSE_WHEEL:
		case SDL_EVENT_FINGER_UP:
		case SDL_EVENT_FINGER_CANCELED:
			ctx->anim();
			break;

		case SDL_EVENT_KEY_DOWN:
			if (event->key.scancode == SDL_SCANCODE_ESCAPE ||
			    event->key.scancode == SDL_SCANCODE_AC_BACK ||
			    event->key.key == SDLK_AC_BACK) {
				if (ctx->screen() == Screen::Exercice) {
					if (ctx->exercises.handler_back_pressed(ctx)) {
						// NOTE: it wasn't the last substage of the current
						// exercise
						break;
					} else {
						// NOTE: it was, but we ignore it
						// TODO: think more
						break;
					}
				}
				if (!ctx->pop()) {
					// TODO: ask user whether should exit
				}
			}
			break;
		}
	}

	return SDL_APP_CONTINUE;
}

extern "C" SDL_AppResult ui_iterate(AppContext *ctx) {
	KLAPPT_PROFILE_SCOPE_N("ui_iterate");
	KLAPPT_PROFILE_NAME_F("ui_iterate:%s", screen_name(ctx->screen()));
	auto _tmp_arena_guard = ctx->tmparena.guard();
	auto g = ctx->tmparena.guard();
	static Uint64 frame_ticks_last = ctx->ticks;
	const Uint64 frame_ticks = ctx->ticks;
	const float frame_delta_time_seconds =
		  static_cast<float>(frame_ticks - frame_ticks_last) / 1000.0f;
	frame_ticks_last = frame_ticks;
	{
		KLAPPT_PROFILE_SCOPE_N("mobile_text_input_begin_frame");
		mobile_text_input_begin_frame(ctx);
	}

	Clay_SetDebugModeEnabled(dm);
	{
		KLAPPT_PROFILE_SCOPE_N("clay_update_scroll");
		clay_update_scroll(frame_delta_time_seconds);
	}

	{
		KLAPPT_PROFILE_SCOPE_N("Clay_BeginLayout");
		Clay_BeginLayout();
	}
	CLAY(CLAY_ID("OuterContainer"),
	     {.layout =
	            {
					  .sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)},
					  .layoutDirection = CLAY_TOP_TO_BOTTOM,
				},
	      .backgroundColor = theme()->surface}) {
		CLAY(CLAY_ID("Content"),
		     {
				   .layout =
						 {
							   .sizing = {CLAY_SIZING_GROW(0),
		                                  CLAY_SIZING_GROW(0)},
							   .layoutDirection = CLAY_TOP_TO_BOTTOM,
						 },
			 }) {
			switch (ctx->screen()) {
			case Screen::Start: {
				KLAPPT_PROFILE_SCOPE_N("render_screen.Start");
				app_bar_layout(ctx, ""_v);
				screen_start_draw(ctx);
				bottom_bar_layout(ctx);
				break;
			}
			case Screen::Exercice: {
				KLAPPT_PROFILE_SCOPE_N("render_screen.Exercise");
				app_bar_layout(ctx, exercise_x_of_n(ctx));
				screen_exercise_draw(ctx);
				break;
			}
			case Screen::ExerciceResultSummary: {
				KLAPPT_PROFILE_SCOPE_N("render_screen.ExerciseSummary");
				app_bar_layout(ctx, "Summary"_v);
				screen_exercise_summary_draw(ctx);
				break;
			}
			case Screen::ExerciseReview: {
				KLAPPT_PROFILE_SCOPE_N("render_screen.ExerciseReview");
				app_bar_layout(ctx, review_x_of_n(ctx));
				screen_exercise_review_draw(ctx);
				break;
			}
			case Screen::WordsList: {
				KLAPPT_PROFILE_SCOPE_N("render_screen.WordsList");
				app_bar_layout(ctx, "Words"_v);
				screen_words_list_draw(ctx);
				bottom_bar_layout(ctx);
				break;
			}
			case Screen::LearningList: {
				KLAPPT_PROFILE_SCOPE_N("render_screen.LearningList");
				app_bar_layout(ctx, "Learning"_v);
				screen_learning_list_draw(ctx);
				bottom_bar_layout(ctx);
				break;
			}
			case Screen::WordSuggestions: {
				KLAPPT_PROFILE_SCOPE_N("render_screen.WordSuggestions");
				app_bar_layout(ctx, "Suggestions"_v);
				screen_word_suggestions_draw(ctx);
				break;
			}
			case Screen::Settings: {
				KLAPPT_PROFILE_SCOPE_N("render_screen.Settings");
				app_bar_layout(ctx, "Settings"_v);
				screen_settings_draw(ctx);
				bottom_bar_layout(ctx);
				break;
			}
			case Screen::Word: {
				KLAPPT_PROFILE_SCOPE_N("render_screen.Word");
				app_bar_layout(ctx, "???"_v);
				screen_word_draw(ctx);
				bottom_bar_layout(ctx);
				break;
			}
			case Screen::WordEdit: {
				KLAPPT_PROFILE_SCOPE_N("render_screen.WordEdit");
				app_bar_layout(ctx, "Edit"_v);
				screen_word_edit_draw(ctx);
				bottom_bar_layout(ctx);
				break;
			}
			case Screen::Onboarding: {
				KLAPPT_PROFILE_SCOPE_N("render_screen.Onboarding");
				screen_onboarding_draw(ctx);
				break;
			}
			}
		}
	}

	Clay_RenderCommandArray render_commands;
	{
		KLAPPT_PROFILE_SCOPE_N("Clay_EndLayout");
		render_commands = Clay_EndLayout();
	}
	{
		KLAPPT_PROFILE_SCOPE_N("mobile_text_input_sync");
		mobile_text_input_sync(ctx);
	}
	Clay_SDL3RendererData renderer_data{
		  .renderer = ctx->renderer,
		  .text = ctx->text,
		  .ticks = ctx->ticks,
	};
	{
		KLAPPT_PROFILE_SCOPE_N("SDL_Clay_RenderClayCommands");
		SDL_Clay_RenderClayCommands(&renderer_data, &render_commands);
	}
	list_island_commit(ctx);
	keypad_island_commit(ctx);

	{
		KLAPPT_PROFILE_SCOPE_N("SDL_RenderPresent");
		SDL_RenderPresent(ctx->renderer);
	}

	Engine::Exercises::CommitResult exercise_commit_result;
	{
		KLAPPT_PROFILE_SCOPE_N("Exercises::commit");
		exercise_commit_result = ctx->exercises.commit(ctx);
	}
	switch (exercise_commit_result) {
	case Engine::Exercises::CommitResult::None:
		break;
	case Engine::Exercises::CommitResult::StartNextRound:
		screen_exercise_go(ctx, true);
		break;
	case Engine::Exercises::CommitResult::ShowSummary:
		screen_exercise_summary_go(ctx);
		break;
	}

	{
		KLAPPT_PROFILE_SCOPE_N("mobile_text_input_end_frame");
		mobile_text_input_end_frame(ctx);
	}
	{
		KLAPPT_PROFILE_SCOPE_N("tslt.flush_delayed_reset");
		ctx->tslt.flush_delayed_reset();
	}
	{
		KLAPPT_PROFILE_SCOPE_N("frame_end");
		frame_end(ctx);
	}

	return ctx->app_status.app_quit;
}

#pragma GCC diagnostic pop
