#include "screen_helpers.h"

#include "app/app_context.h"
#include "base/dyn_arr.h"
#include "base/pair.h"
#include "base/profiler.h"
#include "base/str_builder.h"
#include "base/str_view.h"
#include "domain/exercises.h"
#include "domain/word.h"

#include "ui/components/button.h"

#include "ui/trs.h"

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

	CLAY(CLAY_ID("ScreenTrainerRoot"),
	     {
			   .layout =
					 {
						   .sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)},
						   .padding = sizes()->pad.screen,
						   .childGap = sizes()->space.xl,
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
										   .left = sizes()->space.lg,
										   .right = sizes()->space.lg,
										   .top = sizes()->space.xl,
										   .bottom = sizes()->space.xl,
									 },
							   .childGap = sizes()->space.sm,
							   .childAlignment = {CLAY_ALIGN_X_CENTER,
		                                          CLAY_ALIGN_Y_CENTER},
							   .layoutDirection = CLAY_TOP_TO_BOTTOM,
						 },
				   .backgroundColor = theme()->surfaceContainerLow,
				   .cornerRadius = sizes()->radius.lg,
			 }) {

			if (has_due) {
				draw_text(tr()->screen_trainer_hero_ready_title,
				          theme()->onSurface, sizes()->font.title_lg,
				          FontID::MAIN, CLAY_TEXT_WRAP_WORDS,
				          CLAY_TEXT_ALIGN_CENTER);

				StrBuilder count_str{};
				count_str.push(
					  ctx->arena_frame,
					  StrView::from_number(ctx->arena_frame, due_count));
				draw_text(count_str.join(ctx->arena_frame), theme()->primary,
				          sizes()->font.display, FontID::MAIN,
				          CLAY_TEXT_WRAP_WORDS, CLAY_TEXT_ALIGN_CENTER);

				draw_text(tr()->screen_trainer_words_ready_for_review,
				          theme()->onSurfaceContainer, sizes()->font.body_sm,
				          FontID::MAIN, CLAY_TEXT_WRAP_WORDS,
				          CLAY_TEXT_ALIGN_CENTER);

				// CLAY(CLAY_ID("TotalLearningBadge"),
				//      {
				// 		   .layout = {.padding = {udpi(10.f), udpi(10.f),
				// udpi(4.f), udpi(4.f)}}, 		   .backgroundColor =
				// theme()->surfaceContainerHigh, 		   .cornerRadius =
				// CLAY_CORNER_RADIUS(dpi(8.f)),
				// 	 }) {
				// 	StrBuilder t_str{};
				// 	t_str.push(ctx->arena_frame,
				// tr()->screen_trainer_in_learning);
				// 	t_str.push(ctx->arena_frame,
				// StrView::from_number(ctx->arena_frame, total_learning));
				// 	draw_text(t_str.join(ctx->arena_frame),
				// theme()->onSurfaceContainerHigh,
				// 	          static_cast<uint16_t>(udpi(12.f)));
				// }

			} else if (total_learning > 0) {
				draw_text(tr()->screen_trainer_hero_all_done_title,
				          theme()->onSurface, sizes()->font.title_lg,
				          FontID::MAIN, CLAY_TEXT_WRAP_WORDS,
				          CLAY_TEXT_ALIGN_CENTER);

				CLAY(CLAY_ID("AllDoneBadge"),
				     {
						   .layout = {.padding = sizes()->pad.badge},
						   .backgroundColor = theme()->rightContainer,
						   .cornerRadius = sizes()->radius.sm,
					 }) {
					draw_text(tr()->screen_trainer_all_words_reviewed,
					          theme()->onRightContainer, sizes()->font.body_md);
				}

				CLAY(CLAY_ID("TotalLearningBadge"),
				     {
						   .layout = {.padding = sizes()->pad.badge},
						   .backgroundColor = theme()->surfaceContainerHigh,
						   .cornerRadius = sizes()->radius.sm,
					 }) {
					StrBuilder t_str{};
					t_str.push(ctx->arena_frame,
					           tr()->screen_trainer_in_learning);
					t_str.push(ctx->arena_frame,
					           StrView::from_number(ctx->arena_frame,
					                                total_learning));
					draw_text(t_str.join(ctx->arena_frame),
					          theme()->onSurfaceContainerHigh,
					          sizes()->font.label_md);
				}
			} else {
				draw_text(tr()->screen_trainer_hero_empty_title,
				          theme()->onSurface, sizes()->font.title_lg,
				          FontID::MAIN, CLAY_TEXT_WRAP_WORDS,
				          CLAY_TEXT_ALIGN_CENTER);

				draw_text(tr()->screen_trainer_empty_hint,
				          theme()->onSurfaceContainer, sizes()->font.body_sm,
				          FontID::MAIN, CLAY_TEXT_WRAP_WORDS,
				          CLAY_TEXT_ALIGN_CENTER);
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
				auto start_btn =
					  mobile_button(ctx, CLAY_ID("TrainStartBtn"),
				                    tr()->screen_trainer_action_start_training,
				                    mobile_button_style_primary());
				if (start_btn.activated()) {
					screen_exercise_go(ctx, false);
				}
			} else {
				if (ctx->settings.is_using_suggestions) {
					auto explore_btn =
						  mobile_button(ctx, CLAY_ID("AddWordsBtn"),
					                    tr()->screen_trainer_action_find_words,
					                    mobile_button_style_primary());
					if (explore_btn.activated()) {
						screen_word_suggestions_go(ctx);
					}
				}
			}
		}
	}
}
