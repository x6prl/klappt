#pragma once

#include <SDL3/SDL_log.h>
#include <cstdint>
#include <cstring>

#include "base/arena.h"
#include "base/dyn_arr.h"
#include "base/str_builder.h"
#include "base/str_view.h"
#include "domain/grammar.h"
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

	StrView grammar{};
	StrView text{};

	Translation() = default;
	explicit Translation(StrView raw) {
		// NOTE: grammar should not contain spaces
		if ('[' == raw.first()) {
			auto [g, t] = raw.split();
			grammar = g;
			text = t;
		} else {
			text = raw;
		}
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

inline bool word_matches_translation_query_cs(StrView translations_raw,
                                              StrView query) {
	for (; translations_raw;) {
		auto item = translations_raw.mut_split_by(';').trim();
		if (!item) {
			continue;
		}
		Translation translation{item};
		if (translation.text.is_contains_substr(query)) {
			return true;
		}
	}
	return false;
}

struct Noun {
	StrView lemma{};
	StrView plural_suffix{};
	Gender gender{};
};

struct Verb {
	StrView infinitive{};
	StrView third_person{};
	StrView praeteritum{};
	StrView auxv_and_past_participle{};
	bool is_separable_prefix{false};
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
	int8_t in_learning_list{0};
	int8_t was_learned{0};
	int8_t padding[2]{0};
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

inline bool is_plural_only(const Noun &n) { return n.plural_suffix == "(pl.)"; }
inline bool is_singular_only(const Noun &n) {
	return n.plural_suffix == "(sg.)";
}

// NOTE: result may be in scratch arena
inline StrView word_noun_get_plural_without_artikel(Arena &scratch,
                                                    const Noun &n) {
	auto suf = n.plural_suffix;
	{ // NOTE: case — unchangable nouns
		if (is_singular_only(n)) {
			return {};
		}
		if (is_plural_only(n) || suf == "-") {
			return n.lemma;
		}
	}

	StrView lemma{};

	if ('"' == suf.first()) {
		suf = suf.slice(1); // skip " for the next handling stage
		// NOTE: should add umlaut
		auto base = n.lemma;
		StrView um = ">error:PLURAL_UMLAUT_ERROR<"_v;
		int umlaut_idx = -1;
		bool is_digraph = false;

		for (Size i = base.size - 1; i >= 0; --i) {
			char ch = base[i];

			// 1. digraphs (au -> äu, aa -> ä)
			if (ch == 'u' && i > 0 && base[i - 1] == 'a') {
				um = "äu"_v;
				umlaut_idx = i - 1;
				is_digraph = true;
				break;
			} else if (ch == 'U' && i > 0 && base[i - 1] == 'A') {
				um = "ÄU"_v;
				umlaut_idx = i - 1;
				is_digraph = true;
				break;
			} else if (ch == 'a' && i > 0 && base[i - 1] == 'a') {
				um = "ä"_v;
				umlaut_idx = i - 1;
				is_digraph = true;
				break;
			} else if (ch == 'A' && i > 0 && base[i - 1] == 'A') {
				um = "Ä"_v;
				umlaut_idx = i - 1;
				is_digraph = true;
				break;
			}

			// 2. single vowels
			else if ('a' == ch) {
				um = "ä"_v;
				umlaut_idx = i;
				break;
			} else if ('u' == ch) {
				um = "ü"_v;
				umlaut_idx = i;
				break;
			} else if ('o' == ch) {
				um = "ö"_v;
				umlaut_idx = i;
				break;
			} else if ('A' == ch) {
				um = "Ä"_v;
				umlaut_idx = i;
				break;
			} else if ('U' == ch) {
				um = "Ü"_v;
				umlaut_idx = i;
				break;
			} else if ('O' == ch) {
				um = "Ö"_v;
				umlaut_idx = i;
				break;
			}
		}

		if (umlaut_idx >= 0) {
			StrBuilder builder{};
			builder.push(scratch, base.slice(0, umlaut_idx));
			builder.push(scratch, um);
			int skip_chars = is_digraph ? 2 : 1;
			builder.push(scratch, base.slice(umlaut_idx + skip_chars));
			lemma = builder.join(scratch);
		} else {
			lemma = base.copy(scratch);
		}
	} else {
		lemma = n.lemma.copy(scratch);
	}

	if (lemma.size > 2 && lemma.slice(lemma.size - 2) == "um"_v) {
		// NOTE: case — ends with _um_
		if (suf == "-en"_v) {
			// NOTE: latin nouns with german plural suffix (e.g., Museum ->
			// Museen)
			lemma[lemma.size - 2] = 'e';
			lemma[lemma.size - 1] = 'n';
		} else if (suf == "-ien"_v) {
			// NOTE: -ium takes -ien (Außenministerium -> Außenministerien)
			if (lemma.size >= 3 && lemma.slice(lemma.size - 3) == "ium"_v) {
				lemma = StrView::concat(scratch, lemma.slice(0, lemma.size - 3),
				                        "ien"_v);
			} else {
				lemma = StrView::concat(scratch, lemma, "ien"_v);
			}
		} else if (suf == "-a"_v) {
			// NOTE: classic latin nouns (e.g., Faktum -> Fakta)
			lemma[lemma.size - 2] = 'a';
			lemma.size -= 1;
		} else if (suf == "-s"_v) {
			// NOTE: simple -s plural (Parfum -> Parfums, Warum -> Warums)
			lemma = StrView::concat(scratch, lemma, "s"_v);
		} else if (suf == "-e"_v) {
			// NOTE: -aum takes umlaut (Müllraum -> Müllräume)
			//       other -um words do not (Eigentum -> Eigentume)
			if (lemma.size >= 3 && lemma.slice(lemma.size - 3) == "aum"_v) {
				lemma = StrView::concat(scratch, lemma.slice(0, lemma.size - 3),
				                        "äume"_v);
			} else {
				lemma = StrView::concat(scratch, lemma, "e"_v);
			}
		} else if (suf == "-er"_v || suf == "-r"_v) {
			// NOTE: -tum takes umlaut and -er (Königtum -> Königtümer)
			//       (-r is treated as a common dictionary typo for -er)
			if (lemma.size >= 3 && lemma.slice(lemma.size - 3) == "tum"_v) {
				lemma = StrView::concat(scratch, lemma.slice(0, lemma.size - 3),
				                        "tümer"_v);
			} else {
				lemma = StrView::concat(scratch, lemma, "er"_v);
			}
		} else if (suf == "-n"_v) {
			// NOTE: -ium takes -ien (Außenministerium -> Außenministerien)
			if (lemma.size >= 3 && lemma.slice(lemma.size - 3) == "ium"_v) {
				lemma = StrView::concat(scratch, lemma.slice(0, lemma.size - 3),
				                        "ien"_v);
			} else {
				lemma = StrView::concat(scratch, lemma, "n"_v);
			}
		} else if (suf == "-iatantum"_v) {
			// NOTE: Pluraletantum -> Pluraliatantum
			if (lemma.size >= 7 && lemma.slice(lemma.size - 7) == "etantum"_v) {
				lemma = StrView::concat(scratch, lemma.slice(0, lemma.size - 7),
				                        "iatantum"_v);
			} else {
				lemma = StrView::concat(scratch, lemma.slice(0, lemma.size - 2),
				                        "iatantum"_v);
			}
		}
		// -------------------------------------------
		else {
			// ????
			SDL_LogError(SDL_LOG_CATEGORY_ERROR,
			             StrView_Fmt ": unknown suffix " StrView_Fmt ". %s: %d",
			             StrView_Arg(n.lemma), StrView_Arg(suf), __FILE_NAME__,
			             __LINE__);
		}
	} else {
		suf.mut_split_by('-');
		lemma = StrView::concat(scratch, lemma, suf);
	}

	return lemma;
}

// NOTE: result may be in scratch arena
inline StrView word_noun_get_plural_with_artikel(Arena &scratch,
                                                 const Noun &n) {
	constexpr auto DIE_ = "die "_v;
	auto ret = word_noun_get_plural_without_artikel(scratch, n);
	if (ret) {
		return StrView::concat(scratch, DIE_, ret);
	}
	return {};
}

// NOTE: result may be in scratch arena
inline StrView word_verb_get_perfect_only_participle(Arena &scratch,
                                                     const Verb &v) {
	auto [aux, pp] = v.auxv_and_past_participle.split();
	if (aux && pp) {
		return pp;
	}
	return grammar::verb_form_pp(scratch, v.infinitive, true);
}

// NOTE: result may be in scratch arena
inline StrView word_verb_get_perfect_full(Arena &scratch, const Verb &v) {
	auto [aux, pp] = v.auxv_and_past_participle.split();
	if (aux && pp) {
		return v.auxv_and_past_participle;
	}

	StrBuilder builder{};
	builder.push(scratch, aux ? aux : "hat"_v);
	builder.push(scratch,
	             pp ? pp : grammar::verb_form_pp(scratch, v.infinitive, true));
	return builder.join(scratch, ' ');
}

// NOTE: result may be in scratch arena
inline StrView word_verb_get_praeteritum_full(Arena &scratch, const Verb &v) {
	if (v.praeteritum) {
		return v.praeteritum;
	} else {
		return grammar::verb_form_with_ending(scratch, v.infinitive, "te"_v,
		                                      true);
	}
}

// NOTE: result may be in scratch arena
inline StrView word_verb_get_third_person_full(Arena &scratch, const Verb &v) {
	if (v.third_person) {
		return v.third_person;
	} else {
		return grammar::verb_form_with_ending(scratch, v.infinitive, "t"_v,
		                                      true);
	}
}

// NOTE: result may be in scratch arena
inline StrView word_tts_full(Arena &scratch, const Word &word) {
	switch (word.type) {
	case WordType::Noun: {
		StrBuilder builder{};
		builder.push(scratch,
		             gender_to_article_nominative_strview(word.n.gender));
		builder.push(scratch, word.n.lemma);
		builder.push(scratch, ".\n"_v);
		builder.push(scratch,
		             word_noun_get_plural_with_artikel(scratch, word.n));
		return builder.join(scratch, ' ');
	}
	case WordType::Verb: {
		StrBuilder builder{};
		{ // present form
			builder.push(scratch, word.v.infinitive);

			// 3rd person
			builder.push(scratch, ".\n er"_v);
			builder.push(scratch,
			             word_verb_get_third_person_full(scratch, word.v));
		}

		// past form
		builder.push(scratch, ".\n"_v);
		builder.push(scratch, word_verb_get_praeteritum_full(scratch, word.v));

		// perfect
		builder.push(scratch, ".\n"_v);
		builder.push(scratch, word_verb_get_perfect_full(scratch, word.v));

		return builder.join(scratch, ' ');
	}
	case WordType::Adj: {
		if (word.a.is_indeclinable) {
			return word.a.lemma;
		} else {
			StrBuilder builder{};
			builder.push(scratch, word.a.lemma);
			if (word.a.comparative) {
				builder.push(scratch, ".\n"_v);
				builder.push(scratch, word.a.comparative);
			}
			if (word.a.superlative) {
				builder.push(scratch, ".\n"_v);
				builder.push(scratch, word.a.superlative);
			}
			return builder.join(scratch, ' ');
		}
	}
	case WordType::Phrase:
		return word.p.text;
	default:
		return {};
	}
}

inline StrView word_most_meaningfull_lemma(const Word &word) {
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

inline bool word_store_matches_query(Arena &a, const Word &word,
                                     StrView query) {
	query = query.mut_trim().utf8_to_lowercase(a);
	if (!query) {
		return true;
	}
	if (word_matches_translation_query_cs(word.translations_raw, query)) {
		return true;
	}
	auto word_matches_contains_cs = [&a](StrView h, StrView n) {
		return h.utf8_to_lowercase(a).is_contains_substr(n);
	};
	switch (word.type) {
	case WordType::Noun:
		return word_matches_contains_cs(word.n.lemma, query) ||
		       word_matches_contains_cs(word.n.plural_suffix, query);
	case WordType::Verb:
		return word_matches_contains_cs(word.v.infinitive, query) ||
		       word_matches_contains_cs(word.v.third_person, query) ||
		       word_matches_contains_cs(word.v.praeteritum, query) ||
		       word_matches_contains_cs(word.v.auxv_and_past_participle, query);
	case WordType::Adj:
		return word_matches_contains_cs(word.a.lemma, query) ||
		       word_matches_contains_cs(word.a.comparative, query) ||
		       word_matches_contains_cs(word.a.superlative, query);
	case WordType::Phrase:
		return word_matches_contains_cs(word.p.text, query);
	case WordType::Nil:
		return false;
	}
	return false;
}

// Lexeme identity is the learner-relevant German side only.
// The active store is scoped to a single target language, so translations may
// vary within that language and can still be merged for duplicate lexemes.
inline bool word_has_same_lexeme(const Word &lhs, const Word &rhs) {
	if (lhs.type != rhs.type) {
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
		       lhs.v.auxv_and_past_participle ==
		             rhs.v.auxv_and_past_participle &&
		       lhs.v.is_separable_prefix == rhs.v.is_separable_prefix;
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
inline bool word_has_same_payload(const Word &lhs, const Word &rhs) {
	return word_has_same_lexeme(lhs, rhs) &&
	       lhs.translations_raw == rhs.translations_raw &&
	       lhs.json_payload == rhs.json_payload;
}

inline bool words_equal_ignoring_id(const Word &lhs, const Word &rhs) {
	return word_has_same_payload(lhs, rhs);
}

inline Word word_clone(Arena &a, const Word &src) {
	Word dst{};
	dst.word_id = src.word_id;
	dst.type = src.type;
	dst.in_learning_list = src.in_learning_list;
	dst.was_learned = src.was_learned;
	dst.translations_raw = src.translations_raw.copy(a);
	dst.json_payload = src.json_payload.copy(a);

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
		dst.v.is_separable_prefix = src.v.is_separable_prefix;
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
