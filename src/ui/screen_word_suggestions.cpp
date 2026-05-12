#include "app/words_init.h"
#include "base/measure.h"
#include "screen_helpers.h"
#include "ui/components/button.h"
#include "ui/components/word_card.h"
#include "ui/dpi.h"

void screen_word_suggestions_go(AppContext *ctx) {
	Measure m{__FUNCTION__};
	static Arena suggestions_arena(20 << 10); // 20KB
	constexpr Size SUGGESTIONS_COUNT = 10;
	auto &suggestions_list = ctx->suggestions_list;
	if (suggestions_list.empty()) {
		suggestions_list =
			  DynArr<Word>::filled_zero_or_default(suggestions_arena, SUGGESTIONS_COUNT);
	}
	suggestions_list.size = 0;

	DynArr<WordId> candidates{};
	ctx->word_store.for_each_word(
		  ctx->tmparena,
		  [a = &ctx->tmparena, list = &candidates](const Word &w) {
			  if (0 == w.in_learning_list && 0 == w.was_learned &&
		          WordType::Phrase != w.type) {
				  list->push(*a, w.word_id);
			  }
			  return true;
		  });

	uint64_t rng_state = ctx->ticks;
	auto &store = ctx->word_store;
	DynArr<Size> candidates_used_indices{};
	auto list_size = std::min(SUGGESTIONS_COUNT, candidates.size);
	for (; suggestions_list.size < list_size;) {
		auto rindex = random_num(0, candidates.size, &rng_state);
		if (candidates_used_indices.is_contains(rindex)) {
			continue;
		}
		candidates_used_indices.push(ctx->tmparena, rindex);
		Word tmpword;
		store.get_by_id(ctx->tmparena, candidates[rindex], tmpword);
		suggestions_list[suggestions_list.size] =
			  clone_word(suggestions_arena, tmpword);
		suggestions_list.size += 1;
	}

	m.lap().printus();
	SDL_Log("found %d candidates", candidates.size);
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
					add_word_to_learning_list(ctx->tmparena, &word, ctx->words,
					                          &ctx->word_store, &ctx->states, &ctx->app_status);
				}
				m.lap().printus("xapian and states");
				save_words_dat(ctx->tmparena, ctx->settings, *ctx->words);
				m.lap().printus("words.dat");
				screen_exercise_go(ctx, true);
			} else if (add_all_res.activated()) {
				Measure m{"adding new words"};
				for (auto &word : ctx->suggestions_list) {
					add_word_to_learning_list(ctx->tmparena, &word, ctx->words,
					                          &ctx->word_store, &ctx->states, &ctx->app_status);
				}
				m.lap().printus("xapian and states");
				save_words_dat(ctx->tmparena, ctx->settings, *ctx->words);
				m.lap().printus("words.dat");
				screen_exercise_go(ctx, true);
			}
		}
	}
}
