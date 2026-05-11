#include "app/words_init.h"
#include "screen_helpers.h"
#include "ui/components/fast_list.h"
#include "ui/components/text_input.h"
#include "ui/components/word_card.h"
#include "ui/dpi.h"

void screen_words_list_go(AppContext *ctx) {
	ctx->mobile_text_input.activate_text_input = true;
	ctx->go(Screen::WordsList);
	ctx->anim();
}

void screen_words_list_draw(AppContext *ctx) {
	const auto padding = udpi(6.f);
	const auto search_height = dpi(mobile_text_input_style_default().height);
	CLAY(CLAY_ID("WordsListShell"),
	     {
			   .layout =
					 {
						   .sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)},
						   .padding = CLAY_PADDING_ALL(padding),
						   .childGap = udpi(6.f),
						   .layoutDirection = CLAY_TOP_TO_BOTTOM,
					 },
		 }) {
		CLAY(CLAY_ID("WordsSearchSlot"),
		     {.layout = {.sizing = {CLAY_SIZING_GROW(0),
		                            CLAY_SIZING_FIXED(search_height)}}}) {
			auto search =
				  mobile_text_input(ctx, CLAY_ID("WordsSearch"),
			                        &ctx->words_search, "Search words"_v);
			if (search.changed || search.submitted || search.blurred) {
				ctx->anim();
			}
		}
		const auto query = ctx->words_search.view();
		const auto total_words =
			  ctx->word_store.matching_word_count(query);
		CLAY(CLAY_ID("WordsListSlot"),
		     {.layout = {
					.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)}}}) {
			fast_list(
				  ctx, CLAY_ID("WordsList"), total_words,
				  dpi(WORD_CARD_ROW_HEIGHT), [&](FastListWindow window) {
					  ctx->word_store.for_each_matching_word_range(
							ctx->tmparena, query, window.first,
							window.last - window.first,
							[&](Size index, Word &w) {
								CLAY(CLAY_IDI("WordRow", index),
					                 {.layout = {
											.sizing = {
												  CLAY_SIZING_GROW(0),
												  CLAY_SIZING_FIXED(dpi(
														WORD_CARD_ROW_HEIGHT))}}}) {
									auto switched = word_card_longtap(
										  ctx, CLAY_IDI("Word", index), w);
									if (switched) {
										SDL_Log(
											  StrView_Fmt,
											  StrView_Arg(w.translations_raw));
										w.in_learning_list =
											  w.in_learning_list ^ 1u;
										auto word = clone_word(ctx->arena, w);
										if (0 != w.in_learning_list) {
											add_word_to_learning_list(
												  ctx->tmparena, &word,
												  ctx->words, &ctx->word_store,
												  &ctx->states);
										} else {
											remove_word_from_learning_list(
												  ctx->tmparena, &word,
												  ctx->words, &ctx->word_store);
										}
										save_words_dat(ctx->tmparena,
							                           ctx->settings,
							                           *ctx->words);
									}
								}
								return true;
							});
					  return true;
				  });
		}
	}
}
