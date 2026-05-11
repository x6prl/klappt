#include "base/measure.h"
#include "base/profiler.h"
#include "screen_helpers.h"
#include "ui/components/button.h"
#include "ui/components/list_island.h"
#include "ui/dpi.h"

void screen_exercise_go(AppContext *ctx, bool reset_stack) {
	KLAPPT_PROFILE_SCOPE_N("screen_exercise_go");
	SDL_Log("%s ctx=%p exercises=%p words=%p round_size=%d",
	        __PRETTY_FUNCTION__, static_cast<void *>(ctx),
	        static_cast<void *>(&ctx->exercises),
	        static_cast<void *>(ctx->words), ctx->settings.exercise_round_size);
	Measure m{__FUNCTION__};
	auto generate_at_most = ctx->settings.exercise_round_size;
	Size due_count{};
	{
		KLAPPT_PROFILE_SCOPE_N("generate_new_exercises");
		due_count = ctx->exercises.generate_new_exercises(ctx, generate_at_most);
	}
	if (!due_count) {
		// exercises stays uninitialized
	}
	m.lap().printus();
	SDL_Log("due count: %d", due_count);
	if (reset_stack) {
		ctx->go(Screen::Exercice);
	} else {
		ctx->push(Screen::Exercice);
	}
}

void screen_exercise_draw(AppContext *ctx) {
	if (!ctx->exercises.is_initialized()) {
		CLAY(CLAY_ID("ExcerciseEmpty"),
		     {.layout = {
					.sizing =
						  {
								CLAY_SIZING_GROW(0),
								CLAY_SIZING_GROW(0),
						  },
					.childGap = udpi(100.f),
					.childAlignment = {CLAY_ALIGN_X_CENTER,
		                               CLAY_ALIGN_Y_CENTER},
					.layoutDirection = CLAY_TOP_TO_BOTTOM,
			  }}) {
			draw_text("No exercises due"_v, theme()->onSurface, udpi(28));
			auto gen = mobile_button(ctx, CLAY_ID("GenExercises"),
			                         "Add more words to the learning list"_v);
			if (gen.activated()) {
				screen_word_suggestions_go(ctx);
			}
		}
		return;
	}
	auto &es = ctx->exercises;
	const auto padding = udpi(32.f);
	CLAY(CLAY_ID("Excercise"),
	     {
			   .layout =
					 {
						   .sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)},
						   .layoutDirection = CLAY_TOP_TO_BOTTOM,
					 },

		 }) {
		CLAY(CLAY_ID("TaskContainer"),
		     {
				   .layout =
						 {
							   .sizing = {CLAY_SIZING_GROW(0),
		                                  CLAY_SIZING_PERCENT(0.18f)},
							   .padding = CLAY_PADDING_ALL(padding),
							   .childAlignment = {CLAY_ALIGN_X_CENTER,
		                                          CLAY_ALIGN_Y_CENTER},
							   .layoutDirection = CLAY_TOP_TO_BOTTOM,
						 },
				   .backgroundColor = theme()->surface,
			 }) {

			Size text_lenght = es.source().size; // TODO: count UTF8 length)))
			if (ctx->settings.tr_language ==
			          Settings::TranslationLanguage::Russian ||
			    ctx->settings.tr_language ==
			          Settings::TranslationLanguage::Arabic) {
				text_lenght /= 2;
			}
			float source_font_size = get_font_size_based_on_str_size(
				  ctx->display_width, ctx->scale, text_lenght, 20.f, 48.f);

			const auto source_font_id = translation_font_id(ctx);
			draw_text(es.source(), theme()->onSurface, source_font_size,
			          source_font_id);
			auto sub_color = theme()->onSurface;
			sub_color.a -= 90.f;
			if (es.source_sub0().size > 0)
				draw_text(es.source_sub0(), sub_color, udpi(12), source_font_id,
				          CLAY_TEXT_WRAP_NEWLINES);
			if (es.source_sub1().size > 0)
				draw_text(es.source_sub1(), sub_color, udpi(12),
				          source_font_id);
		}
		CLAY(CLAY_ID("AnswerContainer"),
		     {
				   .layout =
						 {
							   .sizing = {CLAY_SIZING_GROW(0),
		                                  CLAY_SIZING_PERCENT(0.37f)},
							   .padding = CLAY_PADDING_ALL(padding),
							   .childAlignment = {CLAY_ALIGN_X_CENTER,
		                                          CLAY_ALIGN_Y_TOP},
							   .layoutDirection = CLAY_TOP_TO_BOTTOM,
						 },
			 }) {
			CLAY(CLAY_ID("Answer"),
			     {
					   .layout =
							 {
								   .sizing = {CLAY_SIZING_GROW(0),
			                                  CLAY_SIZING_PERCENT(0.4f)},
								   .padding =
										 {
											   .left = udpi(16),
											   .right = udpi(16),
											   .top = udpi(8),
											   .bottom = udpi(8),
										 },
								   .childGap = udpi(8),
								   .childAlignment =
										 {CLAY_ALIGN_X_LEFT,
			                              CLAY_ALIGN_Y_CENTER},
								   .layoutDirection = CLAY_TOP_TO_BOTTOM,
							 },
					   .backgroundColor = theme()->surfaceContainer,
					   .cornerRadius = CLAY_CORNER_RADIUS(dpi(16)),
				 }) {
				draw_text(es.response(), theme()->onSurfaceContainer, udpi(18),
				          FontID::MONOSPACE_REGULAR);
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
				ctx->anim();
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
