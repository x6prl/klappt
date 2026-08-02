#include "app/words_init.h"
#include "base/arena.h"
#include "base/measure.h"
#include "base/str_view.h"
#include "domain/word.h"
#include "screen_helpers.h"
#include "ui/components/button.h"
#include "ui/components/word_card.h"
#include "ui/dpi.h"
#include <SDL3/SDL_log.h>
namespace {
constexpr Size SUGGESTIONS_COUNT = 10;
constexpr Size WORDS_CHECK_MAX = 1000;
} // namespace

void screen_word_suggestions_go(AppContext *ctx) {
	Measure m{__FUNCTION__};
	// TODO: review
	auto &suggestions_arena = ctx->arena_screen();
	auto &suggestions_list = ctx->suggestions_list;
	if (suggestions_list.is_empty()) {
		suggestions_list = DynArr<Word>::filled_zero_or_default(
			  suggestions_arena, SUGGESTIONS_COUNT);
	}
	suggestions_list.size = 0;

	uint64_t rng_state = ctx->ticks;

	DynArr<WordId> candidates{};

	auto word_count = ctx->word_store.word_count();
	// TODO: make uniform
	auto range_start =
		  random_num(0,
	                 WORDS_CHECK_MAX < word_count ? word_count - WORDS_CHECK_MAX
	                                              : word_count,
	                 &rng_state);
	auto range_count = WORDS_CHECK_MAX;
	SDL_Log("st=%d, c=%d", range_start, range_count);

	ctx->word_store.for_each_word_range(
		  ctx->arena_frame, range_start, range_count,
		  [a = &suggestions_arena, list = &candidates](Size, const Word &w) {
			  if (0 == w.in_learning_list && 0 == w.was_learned &&
		          WordType::Phrase != w.type) {
				  list->push(*a, w.word_id);
				  SDL_Log(StrView_Fmt, StrView_Arg(word_tts_full(*a, *a, w)));
			  }
			  return true;
		  });

	auto &store = ctx->word_store;
	{
		auto g = ctx->arena_frame.guard();
		DynArr<Size> candidates_used_indices{};
		auto list_size = std::min(SUGGESTIONS_COUNT, candidates.size);
		for (; suggestions_list.size < list_size;) {
			auto rindex = random_num(0, candidates.size, &rng_state);
			if (candidates_used_indices.is_contains(rindex)) {
				continue;
			}
			candidates_used_indices.push(ctx->arena_frame, rindex);
			Word tmpword;
			store.get_by_id(ctx->arena_frame, candidates[rindex], tmpword);
			suggestions_list[suggestions_list.size] =
				  word_clone(suggestions_arena, tmpword);
			suggestions_list.size += 1;
		}
	}

	m.lap().printus();
	for (auto &c : candidates) {
		Word tmpword;
		store.get_by_id(ctx->arena_frame, c, tmpword);
		SDL_Log(StrView_Fmt,
		        StrView_Arg(word_tts_full(ctx->arena_frame, ctx->arena_frame,
		                                  tmpword)));
	}
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
