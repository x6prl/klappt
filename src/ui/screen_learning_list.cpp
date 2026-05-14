#include "screen_helpers.h"
#include "ui/components/fast_list.h"
#include "ui/components/text_input.h"
#include "ui/components/word_card.h"
#include "ui/dpi.h"

void screen_learning_list_go(AppContext *ctx) { ctx->go(Screen::LearningList); }

void screen_learning_list_draw(AppContext *ctx) {
	const auto padding = udpi(6.f);
	const auto search_height = dpi(mobile_text_input_style_default().height);
	CLAY(CLAY_ID("WordsListShell"),
	     {.layout = {
				.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)},
				.padding = CLAY_PADDING_ALL(padding),
				.childGap = udpi(6.f),
				.layoutDirection = CLAY_TOP_TO_BOTTOM,
		  }}) {
		CLAY(CLAY_ID("WordsSearchSlot"),
		     {.layout = {.sizing = {CLAY_SIZING_GROW(0),
		                            CLAY_SIZING_FIXED(search_height)}}}) {
			auto search =
				  mobile_text_input(ctx, CLAY_ID("WordsSearch"),
			                        &ctx->learning_search, "Search words"_v);
			if (search.changed || search.submitted || search.blurred) {
				ctx->anim();
			}
		}
		auto now = time(nullptr);
		const auto query = ctx->learning_search.view();
		const auto total_words =
			  matching_learning_word_count(*ctx->words, query);
		CLAY(CLAY_ID("WordsListSlot"),
		     {.layout = {
					.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)}}}) {
			fast_list(
				  ctx, CLAY_ID("WordsList"), total_words,
				  dpi(WORD_CARD_ROW_HEIGHT), [&](FastListWindow window) {
					  for_each_matching_learning_word_range(
							*ctx->words, query, window.first,
							window.last - window.first,
							[&](Size index, const Word &w) {
								CLAY(CLAY_IDI("WordRow", index),
					                 {.layout = {
											.sizing = {
												  CLAY_SIZING_GROW(0),
												  CLAY_SIZING_FIXED(dpi(
														WORD_CARD_ROW_HEIGHT))}}}) {
									Engine::State state{};
									auto [is_success, is_present] =
										  ctx->states.get(w.word_id, state);
									// SDL_Log("%s %s",
						            //                  is_success ? "SUC" :
						            //                  "FAIL", is_present ?
						            //                  "PRES" : "NOTFOUND");
									Engine::Timestamp due = state.due;
									int due_mark{0};
									if (due >= 0) {
										due_mark =
											  due < now ? -1 * static_cast<int>(
																	 now - due)
														: static_cast<int>(due -
							                                               now);
									}
									bool is_tapped = word_card_with_due(
										  ctx, CLAY_IDI("Word", index), w,
										  due_mark);

									if (is_tapped) {
										screen_word_push(ctx, w.word_id);
									}
								}
								return true;
							});
					  return true;
				  });
		}
	}
}
