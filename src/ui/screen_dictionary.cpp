#include <SDL3/SDL_log.h>

#include "app/app_context.h"
#include "app/words_init.h"
#include "base/profiler.h"
#include "base/str_view.h"
#include "screen_helpers.h"
#include "ui/components/button.h"
#include "ui/components/lists.h"
#include "ui/components/text_input.h"
#include "ui/components/word_card.h"
#include "ui/dpi.h"
#include "ui/tslt.h"

void screen_dictionary_go(AppContext *ctx) {
	ctx->mobile_text_input.activate_text_input = true;
	ctx->go(Screen::Dictionary);
	ctx->push_one_frame();
}

void screen_dictionary_draw(AppContext *ctx) {
	KLAPPT_PROFILE_SCOPE();
	auto floating_button_clear_id = CLAY_ID("FloatingButtonClear");
	{ // floating buttons
		CLAY(floating_button_clear_id,
		     {.floating = {
					.offset = {dpi(8.f), 0.f},
					.attachPoints = {.element = CLAY_ATTACH_POINT_LEFT_TOP,
		                             .parent = CLAY_ATTACH_POINT_LEFT_CENTER},
					.attachTo = CLAY_ATTACH_TO_PARENT,
			  }}) {
			auto b = mobile_icon_button<false>(ctx, CLAY_ID("FLOATING"),
			                                   Icons::ROTATE);
			if (Clay_Hovered() && b.activated()) {
				ctx->dictionary_search.clear();
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
				  ctx, CLAY_ID("WordsSearch"), &ctx->dictionary_search,
				  "Search words"_v, mobile_text_input_style_default(),
				  floating_button_clear_id);
			if (search.changed || search.submitted || search.blurred) {
				ctx->push_one_frame();
			}
		}
		// NOTE: at least 2 chars to start searching
		if (ctx->dictionary_search.size >= 2) {
			KLAPPT_PROFILE_SCOPE_N("screen_words_list_draw::word_search");
			const auto query = ctx->dictionary_search.view();
			const auto total_words = ctx->word_store.matching_word_count(query);
			CLAY(CLAY_ID("WordsListSlot"),
			     {.layout = {.sizing = {CLAY_SIZING_GROW(0),
			                            CLAY_SIZING_GROW(0)}}}) {
				list::vertical_uniform_w(
					  ctx, CLAY_ID("WordsList"), total_words,
					  dpi(WORD_CARD_ROW_HEIGHT),
					  [&](AppContext *ctx, list::ItemsRange window) {
						  ctx->word_store.for_each_matching_word_range(
								ctx->arena_frame, query, window.first,
								window.last_exclusive - window.first,
								[&](Size index, Word &w) {
									CLAY(CLAY_IDI("WordRow", index),
						                 {.layout = {
												.sizing = {
													  CLAY_SIZING_GROW(0),
													  CLAY_SIZING_FIXED(dpi(
															WORD_CARD_ROW_HEIGHT))}}}) {
										auto tap_state =
											  word_card_for_words_list(
													ctx,
													CLAY_IDI("Word", index), w);
										if (tap_state ==
							                TapSwipeLongTap::LongTap) {
											// SDL_Log(StrView_Fmt,
											//                  StrView_Arg(
											// 			  w.translations_raw));
											toggle_word_from_learning_list_and_save_words_dat(
												  ctx, ctx->arena_frame, &w);
										} else if (tap_state ==
							                       TapSwipeLongTap::Tap) {
											screen_word_view_push(ctx,
								                                  w.word_id);
											ctx->anim();
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
