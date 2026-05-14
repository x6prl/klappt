#pragma once

#include <algorithm>

#include "app/app_context.h"
#include "app/app_status.h"
#include "base/shuffle.h"
#include "domain/exercises.h"
#include "domain/words.h"
#include "ui/components/button.h"
#include "ui/textcache.h"

void screen_start_go(AppContext *ctx);
void screen_start_draw(AppContext *ctx);

void screen_exercise_go(AppContext *ctx, bool reset_stack);
void screen_exercise_draw(AppContext *ctx);

void screen_exercise_summary_go(AppContext *ctx);
void screen_exercise_summary_draw(AppContext *ctx);

void screen_exercise_review_push(AppContext *ctx);
void screen_exercise_review_draw(AppContext *ctx);

void screen_words_list_go(AppContext *ctx);
void screen_words_list_draw(AppContext *ctx);

void screen_learning_list_go(AppContext *ctx);
void screen_learning_list_draw(AppContext *ctx);

void screen_word_suggestions_go(AppContext *ctx);
void screen_word_suggestions_draw(AppContext *ctx);

void screen_settings_push(AppContext *ctx);
void screen_settings_draw(AppContext *ctx);

void screen_word_view_push(AppContext *ctx, WordId word_id);
void screen_word_view_draw(AppContext *ctx);

void screen_word_edit_push(AppContext *ctx);
void screen_word_edit_draw(AppContext *ctx);

void screen_onboarding_go(AppContext *ctx);
void screen_onboarding_draw(AppContext *ctx);

inline void
draw_text(StrView text, Clay_Color color, uint16_t font_size = 16,
          uint16_t font_id = FontID::MAIN,
          Clay_TextElementConfigWrapMode wrap_mode = CLAY_TEXT_WRAP_WORDS,
          Clay_TextAlignment text_alignment = CLAY_TEXT_ALIGN_CENTER) {
	CLAY_TEXT(text.to_clay_string(),
	          CLAY_TEXT_CONFIG({
					.textColor = color,
					.fontId = font_id,
					.fontSize = static_cast<uint16_t>(font_size),
					.wrapMode = wrap_mode,
					.textAlignment = text_alignment,
			  }));
}

inline uint16_t translation_font_id(const AppContext *ctx) {
	return ctx->settings.tr_language == Settings::TranslationLanguage::Arabic
	             ? FontID::ARABIC_MAIN
	             : FontID::MAIN;
}

inline float get_font_size_based_on_str_size(float viewpoint_width, float scale,
                                             Size str_size,
                                             float min_font_size = 20.f,
                                             float max_font_size = 32.f) {
	float length_factor = std::clamp(str_size - 15.f, 0.f, 10.f) / 10.f;
	auto target_text_width = viewpoint_width * (0.8f + 0.15f * length_factor);
	auto font_size = std::clamp((target_text_width / str_size),
	                            scale * min_font_size, scale * max_font_size);
	return font_size;
}

inline void add_word_to_learning_list(Arena &tmparena, Word *word, Words *words,
                                      WordStore *word_store,
                                      Engine::States *states,
                                      AppStatus *app_status) {
	uint8_t LEARNING_LIST_ID = 1;
	word->in_learning_list = LEARNING_LIST_ID;

	auto word_ref = words->add();
	if (word_ref != Words::null_index()) {
		(*words)[word_ref] = *word;
		word_store->save(tmparena, *word);
		auto rollback_added_word = [&] {
			word->in_learning_list = 0;
			words->remove_by_ref(word_ref);
			word_store->save(tmparena, *word);
		};
		Engine::State state{};
		auto [success, was_found] = states->get(word->word_id, state);
		if (!success) {
			rollback_added_word();
			app_status->push_error("Cannot read word learning state"_v);
			return;
		}
		if (!was_found) {
			if (!states->set(word->word_id, state)) {
				rollback_added_word();
				app_status->push_error("Cannot create word learning state"_v);
			}
		}
	} else {
		app_status->push_error("Cannot add to the learning list"_v);
	}
}

inline void remove_word_from_learning_list(Arena &tmparena, Word *word,
                                           Words *words,
                                           WordStore *word_store) {
	word->in_learning_list = 0;
	words->remove_by_id(word->word_id);
	word_store->save(tmparena, *word);
}

template <class F>
bool for_each_matching_learning_word_range(const Words &words, StrView query,
                                           Size start, Size count,
                                           F &&visitor) {
	query.mut_trim();
	Size matched = 0;
	Size emitted = 0;
	for (auto ref = words.begin(); ref < words.end(); ref.advance(&words)) {
		const auto &word = words[ref];
		if (!word_store_matches_query(word, query)) {
			continue;
		}
		if (matched < start) {
			++matched;
			continue;
		}
		if (emitted >= count) {
			break;
		}
		if (!visitor(matched, word)) {
			return false;
		}
		++matched;
		++emitted;
	}
	return true;
}

inline Size matching_learning_word_count(const Words &words, StrView query) {
	query.mut_trim();
	Size count = 0;
	for (auto ref = words.begin(); ref < words.end(); ref.advance(&words)) {
		if (word_store_matches_query(words[ref], query)) {
			++count;
		}
	}
	return count;
}
