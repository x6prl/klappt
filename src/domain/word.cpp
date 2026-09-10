#include "word.h"
#include "base/str_builder.h"
#include "domain/grammar.h"

Gender str_to_gender(const char *str) {
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

StrView gender_to_char_strview(Gender g) {
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

StrView gender_to_article_nominative_strview(Gender g) {
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

DynArr<StrView> word_translations_split(Arena &a, StrView translations_raw) {
	DynArr<StrView> ret{};
	for (; translations_raw;) {
		auto tr_item = translations_raw.mut_split_by(';').trim();
		ret.push(a, tr_item);
	}
	return ret;
}

DynArr<StrView> word_translations_discrete(Arena &a, StrView translations_raw) {
	DynArr<StrView> ret{};
	for (; translations_raw;) {
		auto tr_item = translations_raw.mut_split_by(';').trim();
		for (; tr_item;) {
			auto tr = tr_item.mut_split_by(',').trim();
			if (!tr || ret.is_contains(tr)) {
				continue;
			}
			ret.push(a, tr);
			// SDL_Log(">>>>>>>>>>>>= " StrView_Fmt, StrView_Arg(tr));
		}
	}
	return ret;
}

StrView word_tts_full(Arena &a, const Word &word) {
	switch (word.type) {
	case WordType::Noun: {
		StrBuilder builder{};
		builder.push(a, gender_to_article_nominative_strview(word.n.gender));
		builder.push(a, word.n.lemma);
		builder.push(a, ".\n"_v);
		builder.push(a, grammar::noun_plural_with_article(a, word.n));
		return builder.join(a, ' ');
	}
	case WordType::Verb: {
		StrBuilder builder{};
		{ // present form
			builder.push(a, word.v.infinitive);

			// 3rd person
			builder.push(a, ".\n er"_v);
			builder.push(a, grammar::verb_third_person_full(a, word.v));
		}

		// past form
		builder.push(a, ".\n"_v);
		builder.push(a, grammar::verb_praeteritum_full(a, word.v));

		// perfect
		builder.push(a, ".\n"_v);
		builder.push(a, grammar::verb_perfect_full(a, word.v));

		return builder.join(a, ' ');
	}
	case WordType::Adj: {
		if (word.a.is_indeclinable) {
			return word.a.lemma;
		} else {
			StrBuilder builder{};
			builder.push(a, word.a.lemma);
			if (word.a.comparative) {
				builder.push(a, ".\n"_v);
				builder.push(a, word.a.comparative);
			}
			if (word.a.superlative) {
				builder.push(a, ".\n"_v);
				builder.push(a, word.a.superlative);
			}
			return builder.join(a, ' ');
		}
	}
	case WordType::Phrase:
		return word.p.text;
	default:
		return {};
	}
}

StrView word_primary_lemma(const Word &word) {
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

bool word_has_same_lexeme(const Word &lhs, const Word &rhs) {
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
		       lhs.v.separable_prefix_size == rhs.v.separable_prefix_size;
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

// Full payload equality keeps the stricter comparison for callers which
// care about annotations too.
bool word_has_same_lexeme_and_payload(const Word &lhs, const Word &rhs) {
	return word_has_same_lexeme(lhs, rhs) &&
	       lhs.translations_raw == rhs.translations_raw &&
	       lhs.json_payload == rhs.json_payload;
}

Word word_clone(Arena &a, const Word &src) {
	Word dst{};
	dst.word_id = src.word_id;
	dst.type = src.type;
	dst.in_learning_list = src.in_learning_list;
	dst.was_learned = src.was_learned;
	dst.popularity = src.popularity;
	dst.lang_id = src.lang_id;
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
		dst.v.separable_prefix_size = src.v.separable_prefix_size;
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

// NOTE: unused
StrView word_to_lexeme_str(Arena &scratch, Arena &a, const Word &w) {
	StrBuilder strs{};
	switch (w.type) {
	case WordType::Nil:
		return "<empty word>"_v;
		break;
	case WordType::Noun:
		SDL_Log("noun %d " StrView_Fmt " " StrView_Fmt, (int)w.n.gender,
		        StrView_Arg(w.n.lemma), StrView_Arg(w.n.plural_suffix));
		strs.push(scratch, gender_to_article_nominative_strview(w.n.gender));
		strs.push(scratch, w.n.lemma);
		strs.push(scratch, w.n.plural_suffix);
		break;
	case WordType::Verb:
		strs.push(scratch, w.v.infinitive);
		if (w.v.third_person) {
			strs.push(scratch, w.v.third_person);
		}
		if (w.v.praeteritum) {
			strs.push(scratch, w.v.praeteritum);
		}
		if (w.v.auxv_and_past_participle) {
			strs.push(scratch, w.v.auxv_and_past_participle);
		}
		if (w.v.third_person) {
			SDL_Log("verb " StrView_Fmt " / " StrView_Fmt " / " StrView_Fmt
			        " / " StrView_Fmt,
			        StrView_Arg(w.v.infinitive), StrView_Arg(w.v.third_person),
			        StrView_Arg(w.v.praeteritum),
			        StrView_Arg(w.v.auxv_and_past_participle));
		} else if (w.v.praeteritum || w.v.auxv_and_past_participle) {
			SDL_Log("verb " StrView_Fmt " / " StrView_Fmt " / " StrView_Fmt,
			        StrView_Arg(w.v.infinitive), StrView_Arg(w.v.praeteritum),
			        StrView_Arg(w.v.auxv_and_past_participle));
		} else {
			SDL_Log("verb " StrView_Fmt, StrView_Arg(w.v.infinitive));
		}
		break;
	case WordType::Adj:
		strs.push(scratch, w.a.lemma);
		if (w.a.is_indeclinable) {
			SDL_Log("adj " StrView_Fmt " (indecl.)", StrView_Arg(w.a.lemma));
			strs.push(scratch, "(indecl.)"_v);
		} else if (w.a.comparative || w.a.superlative) {
			SDL_Log("adj " StrView_Fmt " / " StrView_Fmt " / " StrView_Fmt,
			        StrView_Arg(w.a.lemma), StrView_Arg(w.a.comparative),
			        StrView_Arg(w.a.superlative));
			if (w.a.comparative) {
				strs.push(scratch, w.a.comparative);
			}
			if (w.a.superlative) {
				strs.push(scratch, w.a.superlative);
			}
		} else {
			SDL_Log("adj " StrView_Fmt, StrView_Arg(w.a.lemma));
		}
		break;
	case WordType::Phrase:
		strs.push(scratch, w.p.text);
		SDL_Log("phrase " StrView_Fmt, StrView_Arg(w.p.text));
		break;
	}

	return strs.join(a, ' ');
}
