#include "base/dyn_arr.h"
#include "base/pair.h"
#include "base/profiler.h"
#include "base/str_builder.h"
#include "base/str_view.h"
#include "domain/exercises.h"
#include "domain/word.h"
#include "app/app_context.h"

#include "ui/components/button.h"
#include "ui/dpi.h"
#include "ui/themes.h"

#include "screen_helpers.h"

void screen_trainer_go(AppContext *ctx) { ctx->go(Screen::Trainer); }

namespace {

// [due_count, total_learning]
static inline Pair<Size, Size> collect_trainer_stats(AppContext *ctx) {
	KLAPPT_PROFILE_SCOPE_N("collect_trainer_stats");
	if (!ctx->words || ctx->words->size == 0) {
		return {0, 0};
	}

	const auto now = std::time(nullptr);
	const Size total_learning = ctx->words->size;
	Size due_count = 0;

	DynArr<WordId> due_ids{}; // TODO: we cannot use total_learning as a limit;
	                          // think about performance
	if (ctx->states.collect_due(ctx->arena_frame, now, due_ids) &&
	    due_ids.size > 0) {
		for (auto ref = ctx->words->begin(); ref < ctx->words->end();
		     ref.advance(ctx->words)) {
			if (due_ids.is_contains((*ctx->words)[ref].word_id)) {
				++due_count;
			}
		}
	}

	return {due_count, total_learning};
}

} // namespace

void screen_trainer_draw(AppContext *ctx) {
	const auto [due_count, total_learning] = collect_trainer_stats(ctx);
	const bool has_due = (due_count > 0);
	const auto padding = udpi(20.0f);

	CLAY(CLAY_ID("ScreenTrainerRoot"),
	     {
			   .layout =
					 {
						   .sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)},
						   .padding = CLAY_PADDING_ALL(padding),
						   .childGap = udpi(16.0f),
						   .childAlignment = {CLAY_ALIGN_X_CENTER,
	                                          CLAY_ALIGN_Y_CENTER},
						   .layoutDirection = CLAY_TOP_TO_BOTTOM,
					 },
			   .backgroundColor = theme()->surface,
		 }) {

		CLAY(CLAY_ID("TrainerHeroCard"),
		     {
				   .layout =
						 {
							   .sizing = {CLAY_SIZING_GROW(0),
		                                  CLAY_SIZING_FIT(0)},
							   .padding =
									 {
										   .left = udpi(20.f),
										   .right = udpi(20.f),
										   .top = udpi(28.f),
										   .bottom = udpi(28.f),
									 },
							   .childGap = udpi(14.0f),
							   .childAlignment = {CLAY_ALIGN_X_CENTER,
		                                          CLAY_ALIGN_Y_CENTER},
							   .layoutDirection = CLAY_TOP_TO_BOTTOM,
						 },
				   .backgroundColor = theme()->surfaceContainerLow,
				   .cornerRadius = CLAY_CORNER_RADIUS(dpi(24.f)),
			 }) {

			if (has_due) {
				draw_text("Bereit zum Lernen?"_v, theme()->onSurface,
				          static_cast<uint16_t>(udpi(24.f)), FontID::MAIN,
				          CLAY_TEXT_WRAP_WORDS, CLAY_TEXT_ALIGN_CENTER);

				StrBuilder count_str{};
				count_str.push(
					  ctx->arena_frame,
					  StrView::from_number(ctx->arena_frame, due_count));
				draw_text(count_str.join(ctx->arena_frame), theme()->primary,
				          static_cast<uint16_t>(udpi(64.f)), FontID::MAIN,
				          CLAY_TEXT_WRAP_WORDS, CLAY_TEXT_ALIGN_CENTER);

				draw_text("words ready for review"_v,
				          theme()->onSurfaceContainer,
				          static_cast<uint16_t>(udpi(15.f)), FontID::MAIN,
				          CLAY_TEXT_WRAP_WORDS, CLAY_TEXT_ALIGN_CENTER);

				// CLAY(CLAY_ID("TotalLearningBadge"),
				//      {
				// 		   .layout = {.padding = {udpi(10.f), udpi(10.f),
				// udpi(4.f), udpi(4.f)}}, 		   .backgroundColor =
				// theme()->surfaceContainerHigh, 		   .cornerRadius =
				// CLAY_CORNER_RADIUS(dpi(8.f)),
				// 	 }) {
				// 	StrBuilder t_str{};
				// 	t_str.push(ctx->arena_frame, "In learning: "_v);
				// 	t_str.push(ctx->arena_frame,
				// StrView::from_number(ctx->arena_frame, total_learning));
				// 	draw_text(t_str.join(ctx->arena_frame),
				// theme()->onSurfaceContainerHigh,
				// 	          static_cast<uint16_t>(udpi(12.f)));
				// }

			} else if (total_learning > 0) {
				draw_text("Alles erledigt!"_v, theme()->onSurface,
				          static_cast<uint16_t>(udpi(24.f)), FontID::MAIN,
				          CLAY_TEXT_WRAP_WORDS, CLAY_TEXT_ALIGN_CENTER);

				CLAY(CLAY_ID("AllDoneBadge"),
				     {
						   .layout = {.padding = {udpi(12.f), udpi(12.f),
				                                  udpi(6.f), udpi(6.f)}},
						   .backgroundColor = theme()->rightContainer,
						   .cornerRadius = CLAY_CORNER_RADIUS(dpi(10.f)),
					 }) {
					draw_text("✓ All words reviewed for now"_v,
					          theme()->onRightContainer,
					          static_cast<uint16_t>(udpi(14.f)));
				}

				CLAY(CLAY_ID("TotalLearningBadge"),
				     {
						   .layout = {.padding = {udpi(10.f), udpi(10.f),
				                                  udpi(4.f), udpi(4.f)}},
						   .backgroundColor = theme()->surfaceContainerHigh,
						   .cornerRadius = CLAY_CORNER_RADIUS(dpi(8.f)),
					 }) {
					StrBuilder t_str{};
					t_str.push(ctx->arena_frame, "In learning: "_v);
					t_str.push(ctx->arena_frame,
					           StrView::from_number(ctx->arena_frame,
					                                total_learning));
					draw_text(t_str.join(ctx->arena_frame),
					          theme()->onSurfaceContainerHigh,
					          static_cast<uint16_t>(udpi(12.f)));
				}
			} else {
				draw_text("Keine Wörter im Training"_v, theme()->onSurface,
				          static_cast<uint16_t>(udpi(22.f)), FontID::MAIN,
				          CLAY_TEXT_WRAP_WORDS, CLAY_TEXT_ALIGN_CENTER);

				draw_text("Add words from dictionary to start practicing"_v,
				          theme()->onSurfaceContainer,
				          static_cast<uint16_t>(udpi(14.f)), FontID::MAIN,
				          CLAY_TEXT_WRAP_WORDS, CLAY_TEXT_ALIGN_CENTER);
			}
		}

		CLAY(CLAY_ID("TrainerActions"),
		     {
				   .layout =
						 {
							   .sizing = {CLAY_SIZING_GROW(0),
		                                  CLAY_SIZING_FIT(0)},
							   .childAlignment = {CLAY_ALIGN_X_CENTER,
		                                          CLAY_ALIGN_Y_CENTER},
							   .layoutDirection = CLAY_TOP_TO_BOTTOM,
						 },
			 }) {

			if (has_due) {
				auto start_btn = mobile_button(ctx, CLAY_ID("TrainStartBtn"),
				                               "Start Training"_v,
				                               mobile_button_style_primary());
				if (start_btn.activated()) {
					screen_exercise_go(ctx, false);
				}
			} else {
				if (ctx->settings.is_using_suggestions) {
					auto explore_btn = mobile_button(
						  ctx, CLAY_ID("AddWordsBtn"), "Find new words"_v,
						  mobile_button_style_primary());
					if (explore_btn.activated()) {
						screen_word_suggestions_go(ctx);
					}
				}
			}
		}
	}
}
