#pragma once

#include <cstdint>
#include <cstring>

#include "base/arena.h"
#include "base/arr.h"
#include "base/dyn_arr.h"
#include "base/pair.h"
#include "base/str_view.h"
#include "base/str_view_list.h"
#include "word_id.h"

enum class WordType : int32_t { Nil = 0, Noun, Verb, Adj, Phrase };
enum class Gender : int32_t { unknown = -1, none = 0, m, f, n };

inline Gender str_to_gender(const char *str) {
	// TODO: maybe some day............
	if (0 == memcmp("der", str, 3)) {
		return Gender::m;
	} else if (0 == memcmp("die", str, 3)) {
		return Gender::f;
	} else if (0 == memcmp("das", str, 3)) {
		return Gender::n;
	} else if (0 == memcmp("(-)", str, 3)) {
		return Gender::none;
	} else {
		return Gender::unknown;
	}
}
// inline char gender_to_char(Gender g) {
// 	switch (g) {
// 	case Gender::unknown:
// 		return ' ';
// 	case Gender::none:
// 		return '-';
// 	case Gender::m:
// 		return 'm';
// 	case Gender::f:
// 		return 'f';
// 	case Gender::n:
// 		return 'n';
// 	}
// 	return ' ';
// }

inline StrView gender_to_char_strview(Gender g) {
	switch (g) {
	case Gender::unknown:
		return " "_v;
	case Gender::none:
		return "-"_v;
	case Gender::m:
		return "m"_v;
	case Gender::f:
		return "f"_v;
	case Gender::n:
		return "n"_v;
	}
	return " "_v;
}
inline StrView gender_to_article_nominative_strview(Gender g) {
	switch (g) {
	case Gender::unknown:
		return " "_v;
	case Gender::none:
		return " — "_v;
	case Gender::m:
		return "der"_v;
	case Gender::f:
		return "die"_v;
	case Gender::n:
		return "das"_v;
	}
	return " "_v;
}

struct Translation {
	enum Cue {
		Present3rdPerson,
		Past,
		ParticipleII,
		Aux,
		Comparative,
		Superlative
	};

	static constexpr Size MAX_CUES = 4;

	StrView base{};
	Arr<Pair<Cue, StrView>, MAX_CUES> cues{};
	Size cue_count{}; // TODO: drop it?

	static bool cue_from_tag(StrView tag, Cue &cue) {
		tag.mut_trim();
		if (tag == "prs"_v) {
			cue = Present3rdPerson;
			return true;
		}
		if (tag == "pst"_v) {
			cue = Past;
			return true;
		}
		if (tag == "par"_v) {
			cue = ParticipleII;
			return true;
		}
		if (tag == "aux"_v) {
			cue = Aux;
			return true;
		}
		if (tag == "cmp"_v) {
			cue = Comparative;
			return true;
		}
		if (tag == "sup"_v) {
			cue = Superlative;
			return true;
		}
		return false;
	}

	Translation() = default;
	explicit Translation(StrView raw) {
		raw.mut_trim();
		if (!raw) {
			return;
		}
		base = raw;
		if (raw.last() != '}') {
			return;
		}

		Size brace_index = -1;
		for (Size i = 0; i < raw.size; ++i) {
			if (raw[i] == '{') {
				brace_index = i;
				break;
			}
		}
		if (brace_index < 0) {
			return;
		}

		base = raw.slice(0, brace_index);
		base.mut_trimr();
		auto cue_items = raw.slice(brace_index + 1, raw.size - 1);
		cue_items.mut_trim();
		for (; cue_items && cue_count < MAX_CUES;) {
			auto cue_item = cue_items.mut_split_by(',').trim();
			if (!cue_item) {
				continue;
			}
			auto tag = cue_item.mut_split_by('=').trim();
			auto value = cue_item.trim();
			Cue cue{};
			if (!value || !cue_from_tag(tag, cue)) {
				continue;
			}
			cues[cue_count++] = {cue, value};
		}
	}

	StrView get_cue(Cue cue) const {
		for (Size i = 0; i < cue_count; ++i) {
			const auto &c = cues[i];
			if (c.first == cue) {
				return c.second;
			}
		}
		return {};
	}
};

inline DynArr<Translation> translations_from_raw(Arena &a,
                                                 StrView translations_raw) {
	DynArr<Translation> ret{};
	for (; translations_raw;) {
		auto item = translations_raw.mut_split_by(';').trim();
		if (!item) {
			continue;
		}
		ret.push(a, Translation{item});
	}
	return ret;
}

inline char word_matches_ascii_lower(char ch) {
	if (ch >= 'A' && ch <= 'Z') {
		return static_cast<char>(ch - 'A' + 'a');
	}
	return ch;
}

inline bool word_matches_contains_ci(StrView haystack, StrView needle) {
	if (!needle) {
		return true;
	}
	if (!haystack || needle.size > haystack.size) {
		return false;
	}
	for (Size i = 0; i <= haystack.size - needle.size; ++i) {
		bool match = true;
		for (Size j = 0; j < needle.size; ++j) {
			if (word_matches_ascii_lower(haystack[i + j]) !=
			    word_matches_ascii_lower(needle[j])) {
				match = false;
				break;
			}
		}
		if (match) {
			return true;
		}
	}
	return false;
}

inline bool word_matches_translation_query(StrView translations_raw,
                                           StrView query) {
	for (; translations_raw;) {
		auto item = translations_raw.mut_split_by(';').trim();
		if (!item) {
			continue;
		}
		Translation translation{item};
		if (word_matches_contains_ci(translation.base, query)) {
			return true;
		}
		for (Size i = 0; i < translation.cue_count; ++i) {
			if (word_matches_contains_ci(translation.cues[i].second, query)) {
				return true;
			}
		}
	}
	return false;
}

struct Noun {
	StrView lemma;
	StrView plural_suffix;
	Gender gender;
};

struct Verb {
	StrView infinitive;
	StrView third_person;
	StrView praeteritum;
	StrView auxv_and_past_participle;
};

struct Adj {
	StrView lemma;
	StrView comparative;
	StrView superlative;
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
	int8_t in_learning_list{0};
	int8_t was_learned{0};
	int8_t padding[2]{0};
	union {
		Noun n;
		Verb v;
		Adj a;
		Phrase p;
		uint8_t _d[sizeof(Verb)]{0};
	};
	StrView translations_raw{};
	StrView grammar{};
};

inline StrView most_meaningfull_lemma(const Word &word) {
	switch (word.type) {
	case WordType::Noun:
		return word.n.lemma;
	case WordType::Verb:
		return word.v.infinitive;
	case WordType::Adj:
		return word.a.lemma;
	case WordType::Phrase:
		return word.p.text;
	default:
		return {};
	}
}

inline bool word_store_matches_query(const Word &word, StrView query) {
	query.mut_trim();
	if (!query) {
		return true;
	}
	if (word_matches_translation_query(word.translations_raw, query) ||
	    word_matches_contains_ci(word.grammar, query)) {
		return true;
	}
	switch (word.type) {
	case WordType::Noun:
		return word_matches_contains_ci(word.n.lemma, query) ||
		       word_matches_contains_ci(word.n.plural_suffix, query);
	case WordType::Verb:
		return word_matches_contains_ci(word.v.infinitive, query) ||
		       word_matches_contains_ci(word.v.third_person, query) ||
		       word_matches_contains_ci(word.v.praeteritum, query) ||
		       word_matches_contains_ci(word.v.auxv_and_past_participle, query);
	case WordType::Adj:
		return word_matches_contains_ci(word.a.lemma, query) ||
		       word_matches_contains_ci(word.a.comparative, query) ||
		       word_matches_contains_ci(word.a.superlative, query);
	case WordType::Phrase:
		return word_matches_contains_ci(word.p.text, query);
	case WordType::Nil:
		return false;
	}
	return false;
}

// Lexeme identity is the learner-relevant German side only.
// The active store is scoped to a single target language, so translations may
// vary within that language and can still be merged for duplicate lexemes.
inline bool same_lexeme(const Word &lhs, const Word &rhs) {
	if (lhs.type != rhs.type || lhs.grammar != rhs.grammar) {
		return false;
	}

	switch (lhs.type) {
	case WordType::Nil:
		return true;
	case WordType::Noun:
		return lhs.n.gender == rhs.n.gender && lhs.n.lemma == rhs.n.lemma &&
		       lhs.n.plural_suffix == rhs.n.plural_suffix;
	case WordType::Verb:
		return lhs.v.infinitive == rhs.v.infinitive &&
		       lhs.v.third_person == rhs.v.third_person &&
		       lhs.v.praeteritum == rhs.v.praeteritum &&
		       lhs.v.auxv_and_past_participle == rhs.v.auxv_and_past_participle;
	case WordType::Adj:
		return lhs.a.lemma == rhs.a.lemma &&
		       lhs.a.comparative == rhs.a.comparative &&
		       lhs.a.superlative == rhs.a.superlative &&
		       lhs.a.is_indeclinable == rhs.a.is_indeclinable;
	case WordType::Phrase:
		return lhs.p.text == rhs.p.text;
	}

	return false;
}

// Full payload equality keeps the stricter comparison for callers which care
// about annotations too.
inline bool same_payload(const Word &lhs, const Word &rhs) {
	return same_lexeme(lhs, rhs) &&
	       lhs.translations_raw == rhs.translations_raw;
}

inline bool words_equal_ignoring_id(const Word &lhs, const Word &rhs) {
	return same_payload(lhs, rhs);
}

inline Word clone_word(Arena &a, const Word &src) {
	Word dst{};
	dst.word_id = src.word_id;
	dst.type = src.type;
	dst.in_learning_list = src.in_learning_list;
	dst.was_learned = src.was_learned;
	dst.translations_raw = src.translations_raw.copy(a);
	dst.grammar = src.grammar.copy(a);

	switch (src.type) {
	case WordType::Nil:
		break;
	case WordType::Noun:
		dst.n.lemma = src.n.lemma.copy(a);
		dst.n.plural_suffix = src.n.plural_suffix.copy(a);
		dst.n.gender = src.n.gender;
		break;
	case WordType::Verb:
		dst.v.infinitive = src.v.infinitive.copy(a);
		dst.v.third_person = src.v.third_person.copy(a);
		dst.v.praeteritum = src.v.praeteritum.copy(a);
		dst.v.auxv_and_past_participle = src.v.auxv_and_past_participle.copy(a);
		break;
	case WordType::Adj:
		dst.a.lemma = src.a.lemma.copy(a);
		dst.a.comparative = src.a.comparative.copy(a);
		dst.a.superlative = src.a.superlative.copy(a);
		dst.a.is_indeclinable = src.a.is_indeclinable;
		break;
	case WordType::Phrase:
		dst.p.text = src.p.text.copy(a);
		break;
	}

	return dst;
}

