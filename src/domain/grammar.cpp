#include "grammar.h"

#include <cctype>

#include "base/arr.h"
#include "base/str_builder.h"
#include "word.h"

namespace {
static constexpr Arr<StrView, 34> separable_prefixes = {
	  "ab"_v,
	  "an"_v,
	  "auf"_v,
	  "aus"_v,
	  "bei"_v,
	  "dar"_v,
	  "ein"_v,
	  "empor"_v,
	  "fest"_v,
	  "fort"_v,
	  "her"_v,
	  "heraus"_v,
	  "herein"_v,
	  "hin"_v,
	  "los"_v,
	  "nieder"_v,
	  "mit"_v,
	  "nach"_v,
	  "raus"_v,
	  "rein"_v,
	  "rüber"_v,
	  "teil"_v,
	  "vor"_v,
	  "weg"_v,
	  "weiter"_v,
	  "zu"_v,
	  "zurück"_v,
	  "zusammen"_v,

	  // separable parts, but not prefixes
	  "statt"_v,
	  "frei"_v,
	  "bloß"_v,
	  "gut"_v,
	  "tot"_v,
	  "fern"_v,
};
static constexpr Arr<StrView, 7> dual_prefixes = {
	  "durch"_v, "hinter"_v, "um"_v,  "unter"_v,
	  "wider"_v, "wieder"_v, "über"_v};
static constexpr Arr<StrView, 8> inseparable_prefixes = {
	  "be"_v, "emp"_v, "ent"_v, "er"_v, "ge"_v, "miss"_v, "ver"_v, "zer"_v};
static constexpr Arr<StrView, 2> no_ge_suffixes = {"ieren"_v, "eien"_v};
// ====================================

template <Size N>
StrView starts_with_one_of(StrView str, Arr<StrView, N> arr,
                           Size size_addon = 4) {
	for (auto &pref : arr) {
		bool is_long_enough = str.size >= pref.size + size_addon;
		if (is_long_enough && pref == str.slice(0, pref.size)) {
			return pref;
		}
	}
	return {};
}

template <Size N> StrView ends_with_one_of(StrView str, Arr<StrView, N> arr) {
	for (auto &suf : arr) {
		if (str.size >= suf.size && suf == str.slice(str.size - suf.size)) {
			return suf;
		}
	}
	return {};
}

/*
 * is_separable to treat dual suffixes as separable
 *  NOTE: result may be in scratch arena
 */
StrView verb_form_pp(Arena &scratch, StrView inf, bool is_separable) {
	auto is_consonant = [](char ch) {
		char lower_ch = std::tolower(static_cast<unsigned char>(ch));

		if (!std::isalpha(static_cast<unsigned char>(ch))) {
			return false;
		}

		return !(lower_ch == 'a' || lower_ch == 'e' || lower_ch == 'i' ||
		         lower_ch == 'o' || lower_ch == 'u');
	};

	auto pref = starts_with_one_of(inf, separable_prefixes);
	{
		if (!pref && is_separable) {
			pref = starts_with_one_of(inf, dual_prefixes);
		}
		inf = inf.slice(pref.size);
	}

	bool is_ending_en = (inf[inf.size - 2] == 'e');
	auto base = inf.slice(0, is_ending_en ? inf.size - 2 : inf.size - 1);
	StrBuilder builder{};

	if (pref) {
		builder.push(scratch, pref);
	}

	auto inseparable_prefix = starts_with_one_of(inf, inseparable_prefixes);
	bool do_not_add_ge = ends_with_one_of(inf, no_ge_suffixes) ||
	                     (inf.size > 6 && inseparable_prefix);
	if (!do_not_add_ge) {
		builder.push(scratch, "ge"_v);
	}

	builder.push(scratch, base);

	if (base.size >= 2) {
		char last = base.last();
		char prev = base[base.size - 2];

		bool is_d_or_t = (last == 'd' || last == 't');

		// we need -e- for -tm, -dm, -fn, -chn, -gn, -kn, etc.
		// and not for -mm, -nn, -lm, -rm, -hm:
		bool is_m_or_n_with_hard_cons =
			  (last == 'm' || last == 'n') && is_consonant(prev) &&
			  (prev != 'l' && prev != 'r' && prev != 'm' && prev != 'n' &&
		       prev != 'h');

		if (is_d_or_t || is_m_or_n_with_hard_cons) {
			builder.push(scratch, "e"_v);
		}
	}
	builder.push(scratch, "t"_v);
	return builder.join(scratch);
}

/*
 * is_separable to treat dual suffixes as separable
 *  NOTE: result may be in scratch arena
 */
StrView verb_form_with_ending(Arena &scratch, StrView inf, StrView ending,
                              bool is_separable) {
	auto is_consonant = [](char ch) {
		char lower_ch = std::tolower(static_cast<unsigned char>(ch));

		if (!std::isalpha(static_cast<unsigned char>(ch))) {
			return false;
		}

		return !(lower_ch == 'a' || lower_ch == 'e' || lower_ch == 'i' ||
		         lower_ch == 'o' || lower_ch == 'u');
	};

	auto pref = starts_with_one_of(inf, separable_prefixes);
	{
		if (!pref && is_separable) {
			pref = starts_with_one_of(inf, dual_prefixes);
		}
		inf = inf.slice(pref.size);
	}

	bool is_ending_en = (inf[inf.size - 2] == 'e');
	auto base = inf.slice(0, is_ending_en ? inf.size - 2 : inf.size - 1);
	StrBuilder builder{};
	builder.push(scratch, base);

	if (base.size >= 2) {
		char last = base.last();
		char prev = base[base.size - 2];

		bool is_d_or_t = (last == 'd' || last == 't');

		// we need -e- for -tm, -dm, -fn, -chn, -gn, -kn, etc.
		// and not for -mm, -nn, -lm, -rm, -hm:
		bool is_m_or_n_with_hard_cons =
			  (last == 'm' || last == 'n') && is_consonant(prev) &&
			  (prev != 'l' && prev != 'r' && prev != 'm' && prev != 'n' &&
		       prev != 'h');

		if (is_d_or_t || is_m_or_n_with_hard_cons) {
			builder.push(scratch, "e"_v);
		}
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

bool is_plural_only(const Noun &n) { return n.plural_suffix == "(pl.)"; }
bool is_singular_only(const Noun &n) { return n.plural_suffix == "(sg.)"; }

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

StrView verb_past_participle(Arena &scratch, const Verb &v) {
	auto [aux, pp] = v.auxv_and_past_participle.split();
	if (aux && pp) {
		return pp;
	}
	return verb_form_pp(scratch, v.infinitive, true);
}

StrView verb_perfect_full(Arena &scratch, const Verb &v) {
	auto [aux, pp] = v.auxv_and_past_participle.split();
	if (aux && pp) {
		return v.auxv_and_past_participle;
	}

	StrBuilder builder{};
	builder.push(scratch, aux ? aux : "hat"_v);
	builder.push(scratch, pp ? pp : verb_form_pp(scratch, v.infinitive, true));
	return builder.join(scratch, ' ');
}

StrView verb_praeteritum_full(Arena &scratch, const Verb &v) {
	if (v.praeteritum) {
		return v.praeteritum;
	} else {
		return verb_form_with_ending(scratch, v.infinitive, "te"_v, true);
	}
}

StrView verb_third_person_full(Arena &scratch, const Verb &v) {
	if (v.third_person) {
		return v.third_person;
	} else {
		return verb_form_with_ending(scratch, v.infinitive, "t"_v, true);
	}
}

} // namespace grammar
