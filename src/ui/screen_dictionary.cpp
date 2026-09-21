#include "domain/word_store_helpers.h"
#include "screen_helpers.h"

#include "app/app_context.h"
#include "base/profiler.h"
#include "base/str_view.h"

#include "ui/components/button.h"
#include "ui/components/lists.h"
#include "ui/components/text_input.h"
#include "ui/components/word_card.h"

#include "ui/translations/langs.h"
#include "ui/trs.h"

void screen_dictionary_go(AppContext *ctx) {
	ctx->mobile_text_input.activate_text_input = true;
	ctx->go(Screen::Dictionary);
	ctx->push_one_frame();
}

void screen_dictionary_draw(AppContext *ctx) {
	KLAPPT_PROFILE_SCOPE();
	auto floating_button_clear_id = CLAY_ID("FloatingButtonClear");
	{ // floating buttons

		// 38dp compact circle
		const float btn_size = 38.0f * sizes()->scale;

		CLAY(floating_button_clear_id,
		     {.layout =
		            {
						  .childGap = sizes()->space.sm,
						  .layoutDirection = CLAY_TOP_TO_BOTTOM,
					},
		      .floating = {
					.offset = {-static_cast<float>(sizes()->space.sm), 0.0f},
					.zIndex = 10,
					.attachPoints = {.element = CLAY_ATTACH_POINT_RIGHT_CENTER,
		                             .parent = CLAY_ATTACH_POINT_RIGHT_CENTER},
					.attachTo = CLAY_ATTACH_TO_PARENT,
			  }}) {

			MobileButtonStyle btn_style;
			btn_style.height = btn_size;
			btn_style.min_width = btn_size;
			btn_style.padding_x = 0;
			btn_style.padding_y = 0;
			btn_style.corner_radius = sizes()->radius.full.topLeft;
			btn_style.border_width = 0;
			btn_style.border = {};
			btn_style.border_pressed = {};
			btn_style.font_id = FontID::ICONS;
			btn_style.font_size = static_cast<uint16_t>(sizes()->dim.icon_sm);

			Clay_Color bg = theme()->surfaceContainerHigh;
			bg.a = static_cast<uint8_t>(bg.a * 0.82f);
			btn_style.background = bg;
			btn_style.background_pressed = theme()->primary;
			btn_style.text = theme()->primary;
			btn_style.text_pressed = theme()->onPrimary;

			if (ctx->settings
			          .is_show_dictionary_search_only_translations_button) {
				MobileButtonStyle mode_style = btn_style;
				mode_style.font_id = FontID::MAIN;
				if (ctx->is_dictionary_search_translations_only) {
					mode_style.background = theme()->primary;
					mode_style.text = theme()->onPrimary;
				}
				auto mode_btn = mobile_button(
					  ctx, CLAY_ID("FLOATING_MODE"),
					  lang_code(ctx->settings.tr_language), mode_style);
				if (mode_btn.activated()) {
					ctx->is_dictionary_search_translations_only =
						  !ctx->is_dictionary_search_translations_only;
					ctx->push_one_frame();
				}
			}

			auto b = mobile_button(ctx, CLAY_ID("FLOATING_RESET"),
			                       Icons::ROTATE, btn_style);

			if (b.activated()) {
				ctx->dictionary_search.clear();
				ctx->mobile_text_input.activate_text_input = true;
			}
		}
	}

	const auto search_height = sizes()->dim.min_touch_target;
	CLAY(CLAY_ID("WordsListShell"),
	     {
			   .layout =
					 {
						   .sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)},
						   .padding = sizes()->pad.screen,
						   .childGap = sizes()->space.sm,
						   .layoutDirection = CLAY_TOP_TO_BOTTOM,
					 },
		 }) {
		CLAY(CLAY_ID("WordsSearchSlot"),
		     {.layout = {.sizing = {CLAY_SIZING_GROW(0),
		                            CLAY_SIZING_FIXED(search_height)}}}) {
			auto search = mobile_text_input(
				  ctx, CLAY_ID("WordsSearch"), &ctx->dictionary_search,
				  tr()->screen_dictionary_search_words,
				  mobile_text_input_style_default(), floating_button_clear_id);
			if (search.changed || search.submitted || search.blurred) {
				ctx->push_one_frame();
			}
		}
		// NOTE: at least 2 chars to start searching
		if (ctx->dictionary_search.view().utf8_length() >= 2) {
			KLAPPT_PROFILE_SCOPE_N("screen_words_list_draw::word_search");
			const auto query = ctx->dictionary_search.view();
			const auto mode = ctx->is_dictionary_search_translations_only
			                        ? SearchMode::TranslationsOnly
			                        : SearchMode::All;

			const auto total_words = ctx->word_store.matching_word_count(
				  ctx->arena_frame, query, mode);
			CLAY(CLAY_ID("WordsListSlot"),
			     {.layout = {.sizing = {CLAY_SIZING_GROW(0),
			                            CLAY_SIZING_GROW(0)}}}) {
				const float card_gap = static_cast<float>(sizes()->space.xs);
				const float row_slot_height =
					  sizes()->dim.word_card_height + card_gap;
				list::vertical_uniform_w(
					  ctx, CLAY_ID("WordsList"), total_words, row_slot_height,
					  [&](AppContext *ctx, list::ItemsRange window) {
						  WordStoreHelper::for_each_matching_word_range(
								ctx->arena_frame, ctx->word_store, query,
								window.first,
								window.last_exclusive - window.first, mode,
								[&](Size index, Word &w) {
									CLAY(CLAY_IDI("WordRow", index),
						                 {.layout = {
												.sizing =
													  {CLAY_SIZING_GROW(0),
						                               CLAY_SIZING_FIXED(
															 row_slot_height)},
												.padding = {
													  .bottom = static_cast<
															uint16_t>(
															card_gap)}}}) {
										auto tap_state = word_card_dictionary(
											  ctx, CLAY_IDI("Word", index), w);
										if (tap_state ==
							                TapSwipeLongTap::LongTap) {
											// SDL_Log(StrView_Fmt,
								            //                  StrView_Arg(
								            // 			  w.translations_raw));
											toggle_word_from_learning_list_and_save_words_dat(
												  ctx, &w);
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
