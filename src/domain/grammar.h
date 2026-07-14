#pragma once
#include "base/arr.h"
#include "base/str_view.h"
#include "base/str_view_list.h"
#include <cctype>

namespace grammar {
static constexpr Arr<StrView, 35> separable_prefixes = {
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
	  "teil"_v,
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
		bool is_long_enough = pref.size >= pref.size + size_addon;
		if (is_long_enough && pref == str.slice(0, pref.size)) {
			return pref;
		}
	}
	return {};
}

template <Size N> StrView ends_with_one_of(StrView str, Arr<StrView, N> arr) {
	for (auto &suf : arr) {
		if (suf == str.slice(str.size - suf.size)) {
			return suf;
		}
	}
	return {};
}

/*
 * is_separable to treat dual suffixes as separable
 */
inline StrView verb_form_pp(Arena &tmp, Arena &a, StrView inf,
                            bool is_separable) {
	auto is_consonant = [](char ch) {
		char lower_ch = std::tolower(static_cast<unsigned char>(ch));

		if (!std::isalpha(static_cast<unsigned char>(ch))) {
			return false;
		}

		return !(lower_ch == 'a' || lower_ch == 'e' || lower_ch == 'i' ||
		         lower_ch == 'o' || lower_ch == 'u');
	};

	auto pref = grammar::starts_with_one_of(inf, grammar::separable_prefixes);
	{
		if (!pref && is_separable) {
			pref = grammar::starts_with_one_of(inf, grammar::dual_prefixes);
		}
		inf = inf.slice(pref.size);
	}

	bool is_ending_en = (inf[inf.size - 2] == 'e');
	auto base = inf.slice(0, is_ending_en ? inf.size - 2 : inf.size - 1);
	StrViewArray builder{};

	if (pref) {
		builder.push(tmp, pref);
	}

	auto inseparable_prefix = starts_with_one_of(inf, inseparable_prefixes);
	bool do_not_add_ge = ends_with_one_of(inf, no_ge_suffixes) ||
	                     (inf.size > 6 && inseparable_prefix);
	if (!do_not_add_ge) {
		builder.push(tmp, "ge"_v);
	}

	builder.push(tmp, base);

	bool is_d_or_t = base.last() == 'd' || base.last() == 't';
	bool is_m_or_n_and_cons_before_them =
		  (base.last() == 'm' || base.last() == 'n') &&
		  (base[base.size - 2] != 'l' || base[base.size - 2] != 'r') &&
		  is_consonant(base[base.size - 2]);
	if (                                    //
		  is_d_or_t ||                      //
		  is_m_or_n_and_cons_before_them || //
		  false                             //
	) {
		// should add _e_
		builder.push(tmp, "e"_v);
	}
	builder.push(tmp, "t"_v);
	return builder.join(a);
}

/*
 * is_separable to treat dual suffixes as separable
 */
inline StrView verb_form_with_ending(Arena &tmp, Arena &a, StrView inf,
                                     StrView ending, bool is_separable) {
	auto is_consonant = [](char ch) {
		char lower_ch = std::tolower(static_cast<unsigned char>(ch));

		if (!std::isalpha(static_cast<unsigned char>(ch))) {
			return false;
		}

		return !(lower_ch == 'a' || lower_ch == 'e' || lower_ch == 'i' ||
		         lower_ch == 'o' || lower_ch == 'u');
	};

	auto pref = grammar::starts_with_one_of(inf, grammar::separable_prefixes);
	{
		if (!pref && is_separable) {
			pref = grammar::starts_with_one_of(inf, grammar::dual_prefixes);
		}
		inf = inf.slice(pref.size);
	}

	bool is_ending_en = (inf[inf.size - 2] == 'e');
	auto base = inf.slice(0, is_ending_en ? inf.size - 2 : inf.size - 1);
	StrViewArray builder{};
	builder.push(tmp, base);

	bool is_d_or_t = base.last() == 'd' || base.last() == 't';
	bool is_m_or_n_and_cons_before_them =
		  (base.last() == 'm' || base.last() == 'n') &&
		  (base[base.size - 2] != 'l' || base[base.size - 2] != 'r') &&
		  is_consonant(base[base.size - 2]);
	if (                                    //
		  is_d_or_t ||                      //
		  is_m_or_n_and_cons_before_them || //
		  false                             //
	) {
		// should add _e_
		builder.push(tmp, "e"_v);
	}
	builder.push(tmp, ending);
	if (pref) {
		builder.push(tmp, " "_v);
		builder.push(tmp, pref);
	}
	return builder.join(a);
}

} // namespace grammar
