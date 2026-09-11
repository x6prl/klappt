#include "grammar.h"

#include <cctype>

#include "base/arr.h"
#include "base/profiler.h"
#include "base/str_builder.h"
#include "base/str_view.h"
#include "word.h"

namespace {

static constexpr Arr<StrView, 8> inseparable_prefixes = {
	  "be"_v, "emp"_v, "ent"_v, "er"_v, "ge"_v, "miss"_v, "ver"_v, "zer"_v};
static constexpr Arr<StrView, 2> no_ge_suffixes = {"ieren"_v, "eien"_v};
static constexpr Arr<StrView, 8> stem_parts_exceptions = {
	  "icht"_v, "ister"_v, "ter"_v,  "ier"_v,
	  "utel"_v, "ifer"_v,  "cher"_v, "isl"_v};

template <Size N>
bool stem_starts_with_one_of_and_stem_looks_valid(StrView stem,
                                                  Arr<StrView, N> arr) {
	constexpr Size VALID_STEM_LENGTH_MIN = 3;
	for (auto &pref : arr) {
		if (stem.is_starts_with(pref)) {
			// there are words like bessern and betten, ernten and erben
			// also beichten, geistern, entern
			// these should take 'ge',
			// and sser, tt, nt, b, icht, ister, ter, etc. — are not valid stems
			if (stem.utf8_length() >=
			    pref.utf8_length() + VALID_STEM_LENGTH_MIN) {
				auto without_pref = stem.slice(pref.size);

				// NOTE: if first stem symbol is  ß — it is
				// automatically makes impossible 'without_pref' to be a valid
				// stem
				constexpr auto eszett = "ß"_v;
				auto is_starts_with_eszett =
					  0 == memcmp(without_pref.data, eszett.data, eszett.size);
				// NOTE: cases like 'bessern' and 'betten'
				bool has_double_letter =
					  without_pref[1] == without_pref.first();
				// NOTE: also, a valid stem should contain a german vowel
				if (!is_starts_with_eszett && !has_double_letter &&
				    without_pref.is_contains_vowels_german()) [[unlikely]] {
					if (stem_parts_exceptions.is_contains(without_pref))
						  [[unlikely]] {
						return false;
					}
					return true;
				}
			}
		}
	}
	return false;
}

template <Size N>
bool infinitiv_ends_with_one_of_and_infinitiv_looks_valid(StrView infinitiv,
                                                          Arr<StrView, N> arr) {
	for (auto &suff : arr) {
		if (infinitiv.is_ends_with(suff)) {
			// there are words like schneien
			// these should take 'ge',
			// and schn and others — are not valid start for such an infinitiv
			if (infinitiv.utf8_length() >= suff.utf8_length() + 2) {
				// SDL_Log(StrView_Fmt, StrView_Arg(infinitiv));
				auto without_suff =
					  infinitiv.slice(0, infinitiv.size - suff.size);
				if (without_suff.is_contains_vowels_german()) {
					return true;
				}
			}
		}
	}
	return false;
}

inline bool needs_intercalary_e(StrView base) {
	KLAPPT_PROFILE_SCOPE_N("grammar::needs_intercalary_e");
	if (base.size < 2)
		return false;
	char last = base.last();
	char prev = base[base.size - 2];

	auto is_consonant = [](char ch) {
		char lower = std::tolower(static_cast<unsigned char>(ch));
		return std::isalpha(static_cast<unsigned char>(ch)) &&
		       !(lower == 'a' || lower == 'e' || lower == 'i' || lower == 'o' ||
		         lower == 'u');
	};

	bool is_d_or_t = (last == 'd' || last == 't');
	bool is_hard_nasal = (last == 'm' || last == 'n') && is_consonant(prev) &&
	                     (prev != 'l' && prev != 'r' && prev != 'm' &&
	                      prev != 'n' && prev != 'h');

	return is_d_or_t || is_hard_nasal;
}

StrView verb_form_pp(Arena &scratch, const Verb &v) {
	KLAPPT_PROFILE_SCOPE_N("grammar::verb_form_pp");
	StrView pref = grammar::verb_separable_prefix(v);

	auto [stem, inf_without_pref] =
		  grammar::verb_stem_and_infinitive_without_separable_prefix(
				v); // no prefix and -n/-en

	StrBuilder builder{};
	if (pref) {
		builder.push(scratch, pref);
	}

	bool do_not_add_ge = infinitiv_ends_with_one_of_and_infinitiv_looks_valid(
							   inf_without_pref, no_ge_suffixes) ||
	                     (stem_starts_with_one_of_and_stem_looks_valid(
							   stem, inseparable_prefixes));
	if (!do_not_add_ge) {
		builder.push(scratch, "ge"_v);
	}

	builder.push(scratch, stem);
	if (needs_intercalary_e(stem)) {
		builder.push(scratch, "e"_v);
	}
	builder.push(scratch, "t"_v);

	return builder.join(scratch);
}

StrView verb_form_with_ending(Arena &scratch, const Verb &v, StrView ending) {
	KLAPPT_PROFILE_SCOPE_N("grammar::verb_form_with_ending");
	StrView pref = grammar::verb_separable_prefix(v);
	StrView stem = grammar::verb_stem(v);

	StrBuilder builder{};
	builder.push(scratch, stem);

	if (needs_intercalary_e(stem)) {
		builder.push(scratch, "e"_v);
	}
	builder.push(scratch, ending);

	if (pref) {
		builder.push(scratch, " "_v);
		builder.push(scratch, pref);
	}

	return builder.join(scratch);
}

} // namespace

namespace grammar {

bool is_plural_only(const Noun &n) { return n.plural_suffix == "(pl.)"_v; }
bool is_singular_only(const Noun &n) { return n.plural_suffix == "(sg.)"_v; }
bool is_aux_sein(const Verb &v) {
	return v.auxv_and_past_participle.is_starts_with("ist"_v);
}

bool is_irrregular(const Verb &v) {
	return v.third_person || v.praeteritum ||  //
	       v.auxv_and_past_participle.size > 3 // !just 'hat' and !just 'ist'
		  ;
}
bool is_regular(const Verb &v) { return !is_irrregular(v); }

StrView noun_singular_with_article(Arena &scratch, const Noun &n) {
	if (is_plural_only(n)) {
		return {};
	}
	switch (n.gender) {
	case Gender::f:
	case Gender::n:
	case Gender::m:
		return StrView::concat_with(
			  scratch, gender_to_article_nominative_strview(n.gender), n.lemma,
			  ' ');
	default:
		return {};
	}
}

StrView noun_plural_without_article(Arena &scratch, const Noun &n) {
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
		} else if (suf == "-es"_v) {
			// NOTE: Spillbaum -> Spillbaumes
			// https://en.wiktionary.org/wiki/Spillbaum
			lemma = StrView::concat(scratch, lemma, "es"_v);
		}
		// -------------------------------------------
		else {
			// ????
			SDL_LogError(SDL_LOG_CATEGORY_ERROR,
			             StrView_Fmt ": unknown suffix " StrView_Fmt ". %s: %d",
			             StrView_Arg(n.lemma), StrView_Arg(suf), __FILE_NAME__,
			             __LINE__);
		}
	} else if (lemma.size > 1 && lemma[lemma.size - 1] == 'a' &&
	           suf == "-en"_v) {
		// NOTE: case — ends with _a_ (Firma -> Firmen, Thema -> Themen)
		lemma =
			  StrView::concat(scratch, lemma.slice(0, lemma.size - 1), "en"_v);
	} else {
		suf.mut_split_by('-');
		lemma = StrView::concat(scratch, lemma, suf);
	}

	return lemma;
}

StrView noun_plural_with_article(Arena &scratch, const Noun &n) {
	constexpr auto DIE_ = "die "_v;
	auto ret = noun_plural_without_article(scratch, n);
	if (ret) {
		return StrView::concat(scratch, DIE_, ret);
	}
	return {};
}

bool verb_is_separable_prefix(const Verb &v) { return v.separable_prefix_size; }

StrView verb_separable_prefix(const Verb &v) {
	return v.infinitive.slice(0, v.separable_prefix_size);
}

StrView verb_infinitive_without_separable_prefix(const Verb &v) {
	auto ret = v.infinitive.slice(v.separable_prefix_size);
	return ret;
}

StrView verb_stem(const Verb &v) {
	return verb_stem_and_infinitive_without_separable_prefix(v).first;
}

Pair<StrView, StrView>
verb_stem_and_infinitive_without_separable_prefix(const Verb &v) {
	const StrView inf = verb_infinitive_without_separable_prefix(v);
	// '-en' case
	if ('e' == inf[inf.size - 2]) {
		return {inf.slice(0, inf.size - 2), inf};
	}
	// '-n' case
	return {{inf.data, inf.size - 1}, inf};
}

StrView verb_past_participle(Arena &scratch, const Verb &v) {
	auto [aux, pp] = v.auxv_and_past_participle.split();
	if (aux && pp) {
		return pp;
	}
	return verb_form_pp(scratch, v);
}

StrView verb_perfect_full(Arena &scratch, const Verb &v) {
	auto [aux, pp] = v.auxv_and_past_participle.split();
	if (aux && pp) {
		return v.auxv_and_past_participle;
	}

	StrBuilder builder{};
	builder.push(scratch, aux ? aux : "hat"_v);
	builder.push(scratch, pp ? pp : verb_form_pp(scratch, v));
	return builder.join(scratch, ' ');
}

StrView verb_praeteritum_full(Arena &scratch, const Verb &v) {
	if (v.praeteritum && v.praeteritum != "-"_v) {
		if (verb_is_separable_prefix(v)) {
			return StrView::concat_with(scratch, v.praeteritum,
			                            verb_separable_prefix(v), ' ');
		}
		return v.praeteritum;
	}
	return verb_form_with_ending(scratch, v, "te"_v);
}

StrView verb_third_person_full(Arena &scratch, const Verb &v) {
	if (v.third_person) {
		if (verb_is_separable_prefix(v)) {
			return StrView::concat_with(scratch, v.third_person,
			                            verb_separable_prefix(v), ' ');
		}
		return v.third_person;
	}
	return verb_form_with_ending(scratch, v, "t"_v);
}

} // namespace grammar
