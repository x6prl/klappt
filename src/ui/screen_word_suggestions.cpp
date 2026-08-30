#include <SDL3/SDL_log.h>

#include "base/arena.h"
#include "base/measure.h"
#include "base/str_view.h"
#include "app/words_init.h"
#include "domain/word.h"

#include "ui/components/button.h"
#include "ui/components/word_card.h"
#include "ui/dpi.h"

#include "screen_helpers.h"

namespace {
constexpr Size SUGGESTIONS_COUNT = 10;
} // namespace

void screen_word_suggestions_go(AppContext *ctx) {
	Measure m{__FUNCTION__};

	auto &suggestions_arena = ctx->arena_screen();
	auto &suggestions_list = ctx->suggestions_list;

	if (suggestions_list.is_empty()) {
		suggestions_list = DynArr<Word>::filled_zero_or_default(
			  suggestions_arena, SUGGESTIONS_COUNT);
	}
	suggestions_list.size = 0;

	uint64_t rng_state = ctx->ticks;

	ctx->word_store.get_smart_suggestions(
		  ctx->arena_frame, suggestions_arena, SUGGESTIONS_COUNT,
		  suggestions_list, &rng_state);

	m.lap().printus();

	for (Size i = 0; i < suggestions_list.size; ++i) {
		SDL_Log("Suggestion [%d]: " StrView_Fmt, static_cast<int>(i),
		        StrView_Arg(word_tts_full(ctx->arena_frame, suggestions_list[i])));
	}

	SDL_Log("Picked %d suggestions", static_cast<int>(suggestions_list.size));
	ctx->go(Screen::WordSuggestions);
}

void screen_word_suggestions_draw(AppContext *ctx) {
	CLAY(CLAY_ID("WordSuggestionsScreen"),
	     {
			   .layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)},
	                      .padding = CLAY_PADDING_ALL(udpi(16.0f)),
	                      .childGap = udpi(12.0f),
	                      .childAlignment = {CLAY_ALIGN_X_CENTER,
	                                         CLAY_ALIGN_Y_TOP},
	                      .layoutDirection = CLAY_TOP_TO_BOTTOM},
		 }) {
		draw_text("Click a word to select"_v, theme()->onSurface, udpi(24));
		for (auto &word : ctx->suggestions_list) {
			if (word_card_tap(ctx, CLAY_IDI("Suggestion", word.word_id.value),
			                  word)) {
				word.in_learning_list = word.in_learning_list + 1;
				word.in_learning_list %= 2;
				SDL_Log("clicked");
			}
		}
		CLAY(CLAY_ID("AddSelectedContainer"),
		     {
				   .layout =
						 {
							   .sizing =
									 {
										   CLAY_SIZING_GROW(0),
										   CLAY_SIZING_GROW(0),
									 },
							   .childGap = udpi(12.f),
							   .childAlignment = {CLAY_ALIGN_X_CENTER,
		                                          CLAY_ALIGN_Y_CENTER},
						 },
			 }) {
			auto add_all_btn_style = mobile_button_style_primary();
			add_all_btn_style.background = theme()->secondary;
			auto regenerate_res = mobile_button(ctx, CLAY_ID("Regenerate"),
			                                    "Other!"_v, add_all_btn_style);
			auto add_all_res = mobile_button(ctx, CLAY_ID("AddAll"),
			                                 "Add all"_v, add_all_btn_style);
			auto add_selected_res =
				  mobile_button(ctx, CLAY_ID("AddSelected"), "Add selected"_v,
			                    mobile_button_style_primary());
			if (regenerate_res.activated()) {
				screen_word_suggestions_go(ctx);
			}
			if (add_selected_res.activated()) {
				Measure m{"adding new words"};
				for (auto &word : ctx->suggestions_list) {
					if (!word.in_learning_list) {
						continue;
					}
					add_word_to_learning_list(ctx->arena_frame, &word,
					                          ctx->words, &ctx->word_store,
					                          &ctx->states, &ctx->app_status);
				}
				m.lap().printus("xapian and states");
				save_words_dat(ctx->arena_frame, ctx->settings, *ctx->words);
				m.lap().printus("words.dat");
				screen_exercise_go(ctx, true);
			} else if (add_all_res.activated()) {
				Measure m{"adding new words"};
				for (auto &word : ctx->suggestions_list) {
					add_word_to_learning_list(ctx->arena_frame, &word,
					                          ctx->words, &ctx->word_store,
					                          &ctx->states, &ctx->app_status);
				}
				m.lap().printus("xapian and states");
				save_words_dat(ctx->arena_frame, ctx->settings, *ctx->words);
				m.lap().printus("words.dat");
				screen_exercise_go(ctx, true);
			}
		}
	}
}
