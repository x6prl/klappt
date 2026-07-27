#include "app/words_init.h"
#include "base/str_view.h"
#include "screen_helpers.h"
#include "ui/components/button.h"
#include "ui/components/fast_list.h"
#include "ui/components/text_input.h"
#include "ui/components/word_card.h"
#include "ui/dpi.h"
#include "ui/tslt.h"
#include <SDL3/SDL_log.h>

void screen_words_list_go(AppContext *ctx) {
	ctx->mobile_text_input.activate_text_input = true;
	ctx->go(Screen::WordsList);
	ctx->push_one_frame();
}

void screen_words_list_draw(AppContext *ctx) {
	auto floating_button_clear_id = CLAY_ID("FloatingButtonClear");
	{ // floating buttons
		if (ctx->words_search.size > 0) {
			CLAY(floating_button_clear_id,
			     {.floating = {
						.offset = {dpi(8.f), 0.f},
						.attachPoints = {.element = CLAY_ATTACH_POINT_LEFT_TOP,
			                             .parent =
			                                   CLAY_ATTACH_POINT_LEFT_CENTER},
						.attachTo = CLAY_ATTACH_TO_PARENT,
				  }}) {
				auto b = mobile_icon_button<false>(ctx, CLAY_ID("FLOATING"),
				                                   Icons::CROSSHAIRS);
				if (Clay_Hovered() && b.activated()) {
					ctx->words_search.clear();
					// screen_words_list_go(ctx);
					ctx->mobile_text_input.activate_text_input = true;
					// ctx->mobile_text_input.focused_value =
					// &ctx->words_search; ctx->mobile_text_input.focused_id =
					// CLAY_ID("WordsSearch").id;
					// ctx->mobile_text_input.changed_id =
					// CLAY_ID("WordsSearch").id;
				}
			}
		}
	}

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
			auto search = mobile_text_input(
				  ctx, CLAY_ID("WordsSearch"), &ctx->words_search,
				  "Search words"_v, mobile_text_input_style_default(),
				  floating_button_clear_id);
			if (search.changed || search.submitted || search.blurred) {
				ctx->push_one_frame();
			}
		}
		// NOTE: at least 2 chars to start searching
		if (ctx->words_search.size >= 2) {
			const auto query = ctx->words_search.view();
			const auto total_words = ctx->word_store.matching_word_count(query);
			CLAY(CLAY_ID("WordsListSlot"),
			     {.layout = {.sizing = {CLAY_SIZING_GROW(0),
			                            CLAY_SIZING_GROW(0)}}}) {
				fast_list(
					  ctx, CLAY_ID("WordsList"), total_words,
					  dpi(WORD_CARD_ROW_HEIGHT), [&](FastListWindow window) {
						  ctx->word_store.for_each_matching_word_range(
								ctx->arena_frame, query, window.first,
								window.last - window.first,
								[&](Size index, Word &w) {
									CLAY(CLAY_IDI("WordRow", index),
						                 {.layout = {
												.sizing = {
													  CLAY_SIZING_GROW(0),
													  CLAY_SIZING_FIXED(dpi(
															WORD_CARD_ROW_HEIGHT))}}}) {
										auto tap_state = word_card_words_list(
											  ctx, CLAY_IDI("Word", index), w);
										if (tap_state ==
							                TapSwipeLongTap::LongTap) {
											SDL_Log(StrView_Fmt,
								                    StrView_Arg(
														  w.translations_raw));
											w.in_learning_list =
												  w.in_learning_list ^ 1u;
											auto word =
												  word_clone(ctx->arena, w);
											if (0 != w.in_learning_list) {
												add_word_to_learning_list(
													  ctx->arena_frame, &word,
													  ctx->words,
													  &ctx->word_store,
													  &ctx->states,
													  &ctx->app_status);
											} else {
												remove_word_from_learning_list(
													  ctx->arena_frame, &word,
													  ctx->words,
													  &ctx->word_store);
											}
											save_words_dat(ctx->arena_frame,
								                           ctx->settings,
								                           *ctx->words);
										} else if (tap_state ==
							                       TapSwipeLongTap::Tap) {
											screen_word_view_push(ctx,
								                                  w.word_id);
										}
									}
									return true;
								});
						  return true;
					  });
			}
		}
	}
}
