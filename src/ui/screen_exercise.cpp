#include "domain/exercises.h"

#include <SDL3/SDL_log.h>

#include "base/measure.h"
#include "base/profiler.h"
#include "base/str_view.h"
#include "domain/word.h"
#include "app/app_context.h"

#include "ui/components/button.h"
#include "ui/components/list_island.h"
#include "ui/dpi.h"
#include "ui/themes.h"

#include "screen_helpers.h"

namespace {

static inline float calculate_exercise_internal_progress(const ExerciseState &ex) {
	Size total_substages = 0;
	Size passed_substages = 0;

	for (Size i = 0; i < ex.stages.size; ++i) {
		const auto &st = ex.stages[i];
		total_substages += st.substages.size;
		if (i < ex.current_stage) {
			passed_substages += st.substages.size;
		} else if (i == ex.current_stage) {
			passed_substages += st.current_substage;
		}
	}

	if (total_substages <= 0) return 0.0f;
	return static_cast<float>(passed_substages) / static_cast<float>(total_substages);
}

static inline void draw_exercise_progress_bar(AppContext *ctx, const ExerciseState &ex) {
	const float progress = calculate_exercise_internal_progress(ex);

	CLAY(CLAY_ID("ExerciseProgressBarTrack"),
	     {
			   .layout =
					 {
						   .sizing = {CLAY_SIZING_GROW(0),
	                                  CLAY_SIZING_FIXED(dpi(3.0f))},
						   .layoutDirection = CLAY_LEFT_TO_RIGHT,
					 },
			   .backgroundColor = theme()->surfaceContainerHigh,
		 }) {
		if (progress > 0.001f) {
			CLAY(CLAY_ID("ExerciseProgressBarFill"),
			     {
					   .layout =
							 {
								   .sizing = {CLAY_SIZING_PERCENT(progress),
			                                  CLAY_SIZING_GROW(0)},
							 },
					   .backgroundColor = theme()->secondary,
				 }) {}
		}
	}
}

} // namespace

void screen_exercise_go(AppContext *ctx, bool reset_stack) {
	KLAPPT_PROFILE_SCOPE_N("screen_exercise_go");
	Measure m{__FUNCTION__};
	auto generate_at_most = ctx->settings.exercise_round_size;
	Size due_count{};
	{
		KLAPPT_PROFILE_SCOPE_N("generate_new_exercises");
		due_count = ctx->exercises.generate_new_exercises(ctx, generate_at_most);
	}
	m.lap().printus();
	if (reset_stack) {
		ctx->go(Screen::Exercice);
	} else {
		ctx->push(Screen::Exercice);
	}
}

void screen_exercise_draw(AppContext *ctx) {
	if (!ctx->exercises.is_initialized()) {
		CLAY(CLAY_ID("ExerciseEmpty"),
		     {
				   .layout =
						 {
							   .sizing = {CLAY_SIZING_GROW(0),
		                                  CLAY_SIZING_GROW(0)},
							   .childGap = udpi(24.f),
							   .childAlignment = {CLAY_ALIGN_X_CENTER,
		                                          CLAY_ALIGN_Y_CENTER},
							   .layoutDirection = CLAY_TOP_TO_BOTTOM,
						 },
				   .backgroundColor = theme()->surface,
			 }) {
			draw_text("No exercises due"_v, theme()->onSurface,
			          static_cast<uint16_t>(udpi(24.f)));

			auto gen = mobile_button(ctx, CLAY_ID("GenExercises"),
			                         "Add more words to the learning list"_v);
			if (gen.activated()) {
				screen_word_suggestions_go(ctx);
			}
		}
		return;
	}

	auto &es = ctx->exercises;
	const auto &current_ex = es.exercises[es.exercise_current_idx];
	const auto padding = udpi(20.f);

	const auto source_text = es.source();
	const Size text_length = source_text.utf8_length();
	const bool is_phrase = (current_ex.word_type == WordType::Phrase) || (text_length > 40);

	CLAY(CLAY_ID("ExerciseScreenRoot"),
	     {
			   .layout =
					 {
						   .sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)},
						   .layoutDirection = CLAY_TOP_TO_BOTTOM,
					 },
			   .backgroundColor = theme()->surface,
		 }) {

		draw_exercise_progress_bar(ctx, current_ex);

		const float source_font_size = is_phrase
		      ? get_font_size_based_on_str_size(ctx->display_width, ctx->scale, text_length, 16.f, 22.f)
		      : get_font_size_based_on_str_size(ctx->display_width, ctx->scale, text_length, 22.f, 36.f);

		const auto font_id = translation_font_id(ctx);

		CLAY(CLAY_ID("TaskContainer"),
		     {
				   .layout =
						 {
							   .sizing = {CLAY_SIZING_GROW(0),
		                                  CLAY_SIZING_PERCENT(is_phrase ? 0.20f : 0.18f)},
							   .padding =
									 {
										   .left = padding,
										   .right = padding,
										   .top = udpi(8.f),
										   .bottom = udpi(4.f),
									 },
							   .childGap = udpi(5.f),
							   .childAlignment = {CLAY_ALIGN_X_CENTER,
		                                          CLAY_ALIGN_Y_CENTER},
							   .layoutDirection = CLAY_TOP_TO_BOTTOM,
						 },
				   .backgroundColor = theme()->surface,
			 }) {

			draw_text(source_text, theme()->onSurface,
			          static_cast<uint16_t>(source_font_size), font_id,
			          CLAY_TEXT_WRAP_WORDS, CLAY_TEXT_ALIGN_CENTER);

			const auto form_badge_text = form_type_to_str(es.form_type());
			const auto valency_text = es.valency();
			const bool has_form = form_badge_text.size > 0;
			const bool has_valency = valency_text.size > 0;

			if (has_form || has_valency) {
				CLAY(CLAY_ID("TaskBadgesRow"),
				     {
						   .layout =
								 {
									   .sizing = {CLAY_SIZING_GROW(0),
				                                  CLAY_SIZING_FIT(0)},
									   .childGap = udpi(6.f),
									   .childAlignment = {CLAY_ALIGN_X_CENTER,
				                                          CLAY_ALIGN_Y_CENTER},
									   .layoutDirection = CLAY_LEFT_TO_RIGHT,
								 },
					 }) {
					if (has_form) {
						CLAY(CLAY_ID("FormBadge"),
						     {
								   .layout =
										 {
											   .padding = {udpi(7.f), udpi(7.f),
						                                   udpi(2.f),
						                                   udpi(2.f)},
										 },
								   .backgroundColor = theme()->secondary,
								   .cornerRadius = CLAY_CORNER_RADIUS(dpi(5.f)),
							 }) {
							draw_text(form_badge_text, theme()->onSecondary,
							          static_cast<uint16_t>(udpi(11.f)));
						}
					}
					if (has_valency) {
						CLAY(CLAY_ID("ValencyBadge"),
						     {
								   .layout =
										 {
											   .padding = {udpi(7.f), udpi(7.f),
						                                   udpi(2.f),
						                                   udpi(2.f)},
										 },
								   .backgroundColor =
										 theme()->surfaceContainerHigh,
								   .cornerRadius = CLAY_CORNER_RADIUS(dpi(5.f)),
							 }) {
							draw_text(valency_text,
							          theme()->onSurfaceContainerHigh,
							          static_cast<uint16_t>(udpi(11.f)));
						}
					}
				}
			}

			if (!is_phrase && es.source_sub0().size > 0) {
				auto sub_color = theme()->onSurface;
				sub_color.a = static_cast<uint8_t>(sub_color.a * 0.6f);
				draw_text(es.source_sub0(), sub_color,
				          static_cast<uint16_t>(udpi(12.f)), font_id,
				          CLAY_TEXT_WRAP_WORDS, CLAY_TEXT_ALIGN_CENTER);
			}
		}

		CLAY(CLAY_ID("AnswerContainer"),
		     {
				   .layout =
						 {
							   .sizing = {CLAY_SIZING_GROW(0),
		                                  CLAY_SIZING_PERCENT(is_phrase ? 0.30f : 0.32f)},
							   .padding =
									 {
										   .left = padding,
										   .right = padding,
										   .top = udpi(6.f),
										   .bottom = udpi(6.f),
									 },
							   .childAlignment = {CLAY_ALIGN_X_CENTER,
		                                          is_phrase ? CLAY_ALIGN_Y_TOP : CLAY_ALIGN_Y_CENTER},
							   .layoutDirection = CLAY_TOP_TO_BOTTOM,
						 },
			 }) {

			if (is_phrase) { // NOTE: special case
				auto scroll_id = CLAY_ID("AnswerScrollBox");
				auto scd = Clay_GetScrollContainerData(scroll_id);
				float autoscroll_y = 0.0f;
				if (scd.found && scd.contentDimensions.height > scd.scrollContainerDimensions.height) {
					autoscroll_y = -(scd.contentDimensions.height - scd.scrollContainerDimensions.height);
				}

				CLAY(scroll_id,
				     {
						   .layout =
								 {
									   .sizing = {CLAY_SIZING_GROW(0),
				                                  CLAY_SIZING_GROW(0)},
									   .padding =
											 {
												   .left = udpi(16.f),
												   .right = udpi(16.f),
												   .top = udpi(12.f),
												   .bottom = udpi(12.f),
											 },
									   .childAlignment = {CLAY_ALIGN_X_LEFT,
				                                          CLAY_ALIGN_Y_TOP},
									   .layoutDirection = CLAY_TOP_TO_BOTTOM,
								 },
						   .backgroundColor = theme()->surfaceContainer,
						   .cornerRadius = CLAY_CORNER_RADIUS(dpi(16.f)),
						   .clip = {
								.vertical = true,
								.childOffset = {0.0f, autoscroll_y}
						   },
					 }) {
					draw_text(es.response(), theme()->onSurfaceContainer,
					          static_cast<uint16_t>(udpi(17.f)),
					          FontID::MONOSPACE_REGULAR, CLAY_TEXT_WRAP_WORDS,
					          CLAY_TEXT_ALIGN_LEFT);
				}
			} else {
				CLAY(CLAY_ID("AnswerCompactBox"),
				     {
						   .layout =
								 {
									   .sizing = {CLAY_SIZING_GROW(0),
				                                  CLAY_SIZING_FIT(0)},
									   .padding =
											 {
												   .left = udpi(16.f),
												   .right = udpi(16.f),
												   .top = udpi(14.f),
												   .bottom = udpi(14.f),
											 },
									   .childAlignment = {CLAY_ALIGN_X_LEFT,
				                                          CLAY_ALIGN_Y_CENTER},
									   .layoutDirection = CLAY_TOP_TO_BOTTOM,
								 },
						   .backgroundColor = theme()->surfaceContainer,
						   .cornerRadius = CLAY_CORNER_RADIUS(dpi(16.f)),
					 }) {
					draw_text(es.response(), theme()->onSurfaceContainer,
					          static_cast<uint16_t>(udpi(18.f)),
					          FontID::MONOSPACE_REGULAR, CLAY_TEXT_WRAP_WORDS,
					          CLAY_TEXT_ALIGN_LEFT);
				}
			}
		}

		CLAY(CLAY_ID("InputContainer"),
		     {
				   .layout =
						 {
							   .sizing = {CLAY_SIZING_GROW(0),
		                                  CLAY_SIZING_GROW(0)},
							   .childAlignment = {CLAY_ALIGN_X_CENTER,
		                                          CLAY_ALIGN_Y_CENTER},
							   .layoutDirection = CLAY_TOP_TO_BOTTOM,
						 },
				   .backgroundColor = theme()->surface,
			 }) {

			IslandStyle style = {
				  .surface = theme()->surface,
				  .background = theme()->surfaceContainer,
				  .divider_color = theme()->shadow,
				  .shadow_color = theme()->shadow,
				  .text = theme()->onSurfaceContainer,
			};

			auto &ss = es.substage();
			const auto submit_tap = [](AppContext *ctx, int tapped) {
				ctx->exercises.submit_result(tapped);
				keypad_island_invalidate();
				list_island_invalidate();
				ctx->push_one_frame();
			};

			if (ss.is_keypad && ss.opts.size >= 3) {
				keypad_island(ctx, CLAY_ID("KeypadIsland"), ss.opts.data,
				              ss.opts.size, style, submit_tap);
			} else {
				list_island(ctx, CLAY_ID("ListIsland"), ss.opts.data,
				            ss.opts.size, style, submit_tap);
			}
		}
	}
}
