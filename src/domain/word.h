#pragma once

#include "base/arena.h"
#include "base/dyn_arr.h"
#include "base/str_view.h"

#include "domain/word_id.h"
#include <cstdint>

enum class WordType : int8_t { Nil = 0, Noun, Verb, Adj, Phrase };
enum class Gender : int8_t { unknown = -1, none = 0, m, f, n };

Gender str_to_gender(const char *str);
StrView gender_to_char_strview(Gender g);
StrView gender_to_article_nominative_strview(Gender g);

struct Noun {
	StrView lemma{};
	StrView plural_suffix{}; // for empty — '-' is used; // TODO: think of excluding '-'
	Gender gender{};
};

struct Verb {
	StrView infinitive{};
	StrView third_person{};
	StrView praeteritum{};
	StrView auxv_and_past_participle{};
	uint8_t separable_prefix_size{0};
};

struct Adj {
	StrView lemma{};
	StrView comparative{};
	StrView superlative{};
	bool is_indeclinable{false};
};

struct Phrase {
	StrView text;
};

static_assert(sizeof(Verb) >= sizeof(Noun));
static_assert(sizeof(Verb) >= sizeof(Adj));
static_assert(sizeof(Verb) >= sizeof(Phrase));

struct Word {
	WordId word_id{0};
	WordType type{WordType::Nil};
	bool in_learning_list{0};
	bool was_learned{0};
	uint8_t popularity{0};
	int8_t lang_id{0}; // -1 for `de`, others — like in Lang

	union {
		Verb v;
		Noun n;
		Adj a;
		Phrase p;
		uint8_t _d[sizeof(Verb)]{0};
	};

	StrView translations_raw{};
	StrView json_payload{};
};

DynArr<StrView> word_translations_split(Arena &a, StrView translations_raw);
DynArr<StrView> word_translations_discrete(Arena &a, StrView translations_raw);

// Lexeme identity is the learner-relevant German side only.
// The active store is scoped to a single target language, so translations
// may vary within that language and can still be merged for duplicate
// lexemes.
bool word_has_same_lexeme(const Word &lhs, const Word &rhs);
bool word_has_same_lexeme_and_payload(const Word &lhs, const Word &rhs);

StrView word_primary_lemma(const Word &word);

Word word_clone(Arena &a, const Word &src);
StrView word_tts_full(Arena &a, const Word &word);

[[maybe_unused]]
StrView word_to_lexeme_str(Arena &scratch, Arena &a, const Word &w);
