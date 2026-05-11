#include "tokenizer.h"

#include "base/arr.h"

#include <type_traits>

// TODO: rewrite
template <class T, class... U>
	requires(std::is_same_v<T, U> && ...)
Arr(T, U...) -> Arr<T, 1 + sizeof...(U)>;

namespace {

// Hard cap for temporary UTF-8 indexing. Tokenization only needs short words.
constexpr Size MAX_UTF8_CODEPOINTS = 96;

struct Utf8Index {
	// Byte offsets for each decoded codepoint plus one trailing end offset.
	Arr<Size, MAX_UTF8_CODEPOINTS + 1> starts{};
	// Lowercased/code-folded codepoints for cheap vowel and compare checks.
	Arr<uint32_t, MAX_UTF8_CODEPOINTS> lower{};
	Size count{};
};

// Prefix and suffix peeling share the same machinery.
enum class AffixSide : uint8_t { Prefix = 0, Suffix };

struct AtomTables {
	static constexpr Arr clusters4 = {"dsch"_v};
	static constexpr Arr clusters3 = {"chs"_v, "sch"_v, "tsch"_v};
	static constexpr Arr clusters2 = {
		  "aa"_v, "ah"_v, "ai"_v, "au"_v, "ch"_v, "ck"_v, "dt"_v, "ee"_v,
		  "eh"_v, "ei"_v, "eu"_v, "ie"_v, "ih"_v, "ks"_v, "ll"_v, "mm"_v,
		  "ng"_v, "nk"_v, "nn"_v, "oo"_v, "oh"_v, "pf"_v, "ph"_v, "ps"_v,
		  "qu"_v, "rr"_v, "sh"_v, "sp"_v, "ss"_v, "st"_v, "th"_v, "tt"_v,
		  "tz"_v, "uh"_v, "ui"_v, "äh"_v, "äu"_v, "öh"_v, "üh"_v};
};

struct NounTables {
	static constexpr Arr deriv_suffixes = {
		  "age"_v,   "ant"_v,  "anz"_v,  "chen"_v,   "ei"_v,   "ent"_v,
		  "enz"_v,   "erin"_v, "heit"_v, "ierung"_v, "in"_v,   "innen"_v,
		  "ismus"_v, "ist"_v,  "ität"_v, "keit"_v,   "lein"_v, "ling"_v,
		  "ment"_v,  "nis"_v,  "or"_v,   "schaft"_v, "sion"_v, "ssion"_v,
		  "tion"_v,  "tum"_v,  "ung"_v,  "ur"_v};
	static constexpr Arr inflection_suffixes = {
		  "e"_v, "en"_v, "er"_v, "ern"_v, "es"_v, "n"_v, "nen"_v, "s"_v};
};

struct VerbTables {
	static constexpr Arr prefixes = {
		  "ab"_v,     "an"_v,     "auf"_v,    "aus"_v,    "be"_v,
		  "bei"_v,    "dar"_v,    "durch"_v,  "ein"_v,    "emp"_v,
		  "ent"_v,    "er"_v,     "fest"_v,   "fort"_v,   "ge"_v,
		  "her"_v,    "heraus"_v, "herein"_v, "hin"_v,    "hinter"_v,
		  "los"_v,    "miss"_v,   "mit"_v,    "nach"_v,   "raus"_v,
		  "rein"_v,   "rüber"_v,  "teil"_v,   "um"_v,     "unter"_v,
		  "ver"_v,    "vor"_v,    "weg"_v,    "weiter"_v, "wider"_v,
		  "wieder"_v, "zer"_v,    "zu"_v,     "zurück"_v, "zusammen"_v,
		  "über"_v};
	static constexpr Arr infinitive_suffixes = {"eln"_v, "en"_v, "ern"_v,
	                                            "ieren"_v, "n"_v};
	static constexpr Arr finite_suffixes = {"e"_v,    "end"_v,  "est"_v, "et"_v,
	                                        "iert"_v, "st"_v,   "t"_v,   "te"_v,
	                                        "ten"_v,  "test"_v, "tet"_v};
};

struct AdjectiveTables {
	static constexpr Arr prefixes = {"un"_v, "ur"_v};
	static constexpr Arr deriv_suffixes = {
		  "arm"_v,  "bar"_v, "frei"_v,  "haft"_v, "ig"_v,  "isch"_v,
		  "lich"_v, "los"_v, "reich"_v, "sam"_v,  "voll"_v};
	static constexpr Arr outer_suffixes = {
		  "e"_v,    "em"_v,   "en"_v,   "er"_v,  "ere"_v, "erem"_v,
		  "eren"_v, "erer"_v, "eres"_v, "es"_v,  "st"_v,  "ste"_v,
		  "stem"_v, "sten"_v, "ster"_v, "stes"_v};
};

struct FallbackDistractors {
	static constexpr Arr letters = {
		  "a"_v, "ä"_v, "b"_v, "c"_v, "d"_v, "e"_v, "f"_v, "g"_v, "h"_v, "i"_v,
		  "j"_v, "k"_v, "l"_v, "m"_v, "n"_v, "o"_v, "ö"_v, "p"_v, "q"_v, "r"_v,
		  "s"_v, "ß"_v, "t"_v, "u"_v, "ü"_v, "v"_v, "w"_v, "x"_v, "y"_v, "z"_v};
	static constexpr Arr short_chunks = {"ab"_v,  "an"_v,  "auf"_v, "aus"_v,
	                                     "bei"_v, "ein"_v, "los"_v, "vor"_v};
	static constexpr Arr suffix_chunks = {"bar"_v,  "en"_v,   "ig"_v,  "in"_v,
	                                      "keit"_v, "lich"_v, "los"_v, "sam"_v};
	static constexpr Arr stem_chunks = {"arbeit"_v, "blatt"_v,   "freund"_v,
	                                    "komm"_v,   "schreib"_v, "sprech"_v,
	                                    "wort"_v,   "zahl"_v};
};

// True for UTF-8 continuation bytes (10xxxxxx).
inline bool utf8_is_continuation(unsigned char ch) {
	return (ch & 0xC0u) == 0x80u;
}

// Byte width of a UTF-8 codepoint based on its lead byte.
inline Size utf8_codepoint_bytes(unsigned char lead) {
	if ((lead & 0x80u) == 0u) {
		return 1;
	}
	if ((lead & 0xE0u) == 0xC0u) {
		return 2;
	}
	if ((lead & 0xF0u) == 0xE0u) {
		return 3;
	}
	if ((lead & 0xF8u) == 0xF0u) {
		return 4;
	}
	return 1;
}

// Decodes one codepoint and reports how many bytes were consumed.
// Malformed sequences degrade to a single-byte interpretation.
uint32_t decode_utf8(const char *ptr, Size available, Size *used_bytes) {
	if (available <= 0) {
		*used_bytes = 0;
		return 0;
	}

	const unsigned char lead = static_cast<unsigned char>(ptr[0]);
	const Size count = utf8_codepoint_bytes(lead);
	if (count <= 1 || count > available) {
		*used_bytes = 1;
		return lead;
	}

	uint32_t cp = lead & ((1u << (8 - count - 1)) - 1u);
	for (Size i = 1; i < count; ++i) {
		const unsigned char ch = static_cast<unsigned char>(ptr[i]);
		if (!utf8_is_continuation(ch)) {
			*used_bytes = 1;
			return lead;
		}
		cp = (cp << 6) | (ch & 0x3Fu);
	}
	*used_bytes = count;
	return cp;
}

// German-aware lowercase folding used for table lookup and vowel checks.
uint32_t fold_german(uint32_t cp) {
	if (cp >= 'A' && cp <= 'Z') {
		return cp - 'A' + 'a';
	}
	switch (cp) {
	case 0x00C4:
		return 0x00E4;
	case 0x00D6:
		return 0x00F6;
	case 0x00DC:
		return 0x00FC;
	default:
		return cp;
	}
}

// Small uppercase predicate for first-letter case matching.
bool is_upper_german(uint32_t cp) {
	if (cp >= 'A' && cp <= 'Z') {
		return true;
	}
	switch (cp) {
	case 0x00C4:
	case 0x00D6:
	case 0x00DC:
	case 0x1E9E:
		return true;
	default:
		return false;
	}
}

// Uppercases a single initial codepoint for distractor case normalization.
uint32_t uppercase_german(uint32_t cp) {
	if (cp >= 'a' && cp <= 'z') {
		return cp - 'a' + 'A';
	}
	switch (cp) {
	case 0x00E4:
		return 0x00C4;
	case 0x00F6:
		return 0x00D6;
	case 0x00FC:
		return 0x00DC;
	case 0x00DF:
		return 0x1E9E;
	default:
		return cp;
	}
}

// Encodes one Unicode codepoint back into UTF-8 bytes.
Size encode_utf8(uint32_t cp, char *dst) {
	if (cp <= 0x7Fu) {
		dst[0] = static_cast<char>(cp);
		return 1;
	}
	if (cp <= 0x7FFu) {
		dst[0] = static_cast<char>(0xC0u | (cp >> 6));
		dst[1] = static_cast<char>(0x80u | (cp & 0x3Fu));
		return 2;
	}
	if (cp <= 0xFFFFu) {
		dst[0] = static_cast<char>(0xE0u | (cp >> 12));
		dst[1] = static_cast<char>(0x80u | ((cp >> 6) & 0x3Fu));
		dst[2] = static_cast<char>(0x80u | (cp & 0x3Fu));
		return 3;
	}
	dst[0] = static_cast<char>(0xF0u | (cp >> 18));
	dst[1] = static_cast<char>(0x80u | ((cp >> 12) & 0x3Fu));
	dst[2] = static_cast<char>(0x80u | ((cp >> 6) & 0x3Fu));
	dst[3] = static_cast<char>(0x80u | (cp & 0x3Fu));
	return 4;
}

// Builds a UTF-8-safe view over the word so later helpers can slice by
// codepoint without copying the original bytes.
bool build_utf8_index(StrView word, Utf8Index *index) {
	index->count = 0;
	for (Size i = 0; i < word.size;) {
		if (index->count >= MAX_UTF8_CODEPOINTS) {
			return false;
		}
		index->starts[index->count] = i;
		Size used = 0;
		index->lower[index->count] =
			  fold_german(decode_utf8(word.data + i, word.size - i, &used));
		if (used <= 0) {
			return false;
		}
		i += used;
		++index->count;
	}
	index->starts[index->count] = word.size;
	return true;
}

// Slices a StrView by codepoint range using Utf8Index byte offsets.
StrView slice_cp(StrView word, const Utf8Index &index, Size from_cp,
                 Size to_cp) {
	return word.slice(index.starts[from_cp], index.starts[to_cp]);
}

// Returns true if the codepoint range contains any vowel.
bool has_vowel(const Utf8Index &index, Size from_cp = 0, Size to_cp = -1) {
	if (to_cp < 0 || to_cp > index.count) {
		to_cp = index.count;
	}
	for (Size i = from_cp; i < to_cp; ++i) {
		switch (index.lower[i]) {
		case 'a':
		case 'e':
		case 'i':
		case 'o':
		case 'u':
		case 'y':
		case 0x00E4:
		case 0x00F6:
		case 0x00FC:
			return true;
		default:
			break;
		}
	}
	return false;
}

// Single-codepoint vowel predicate for atomic chunking decisions.
// TODO: distractors for vowels should be mostly vowels too
/*
bool is_vowel(uint32_t cp) {
    switch (fold_german(cp)) {
    case 'a':
    case 'e':
    case 'i':
    case 'o':
    case 'u':
    case 'y':
    case 0x00E4:
    case 0x00F6:
    case 0x00FC:
        return true;
    default:
        return false;
    }
}
*/

// German-aware comparison used by binary search over sorted token tables.
int compare_folded(StrView lhs, StrView rhs) {
	Size li = 0;
	Size ri = 0;
	while (li < lhs.size && ri < rhs.size) {
		Size lbytes = 0;
		Size rbytes = 0;
		const auto lcp =
			  fold_german(decode_utf8(lhs.data + li, lhs.size - li, &lbytes));
		const auto rcp =
			  fold_german(decode_utf8(rhs.data + ri, rhs.size - ri, &rbytes));
		if (lcp < rcp) {
			return -1;
		}
		if (lcp > rcp) {
			return 1;
		}
		li += lbytes;
		ri += rbytes;
	}
	if (li == lhs.size && ri == rhs.size) {
		return 0;
	}
	return li == lhs.size ? -1 : 1;
}

// Small deterministic hash so distractor order varies by token and caller seed.
uint32_t token_seed(StrView token, uint32_t seed) {
	uint32_t hash = 2166136261u ^ seed;
	for (Size i = 0; i < token.size;) {
		Size used = 0;
		const auto cp =
			  fold_german(decode_utf8(token.data + i, token.size - i, &used));
		hash ^= cp;
		hash *= 16777619u;
		i += used ? used : 1;
	}
	return hash;
}

// Lower-bound for sorted StrView tables.
template <Size N>
Size lower_bound(const Arr<StrView, N> &table, StrView value) {
	Size lo = 0;
	Size hi = N;
	while (lo < hi) {
		const Size mid = lo + (hi - lo) / 2;
		if (compare_folded(table[mid], value) < 0) {
			lo = mid + 1;
		} else {
			hi = mid;
		}
	}
	return lo;
}

// Membership test over sorted StrView tables.
template <Size N> bool contains(const Arr<StrView, N> &table, StrView value) {
	const Size pos = lower_bound(table, value);
	return pos < N && compare_folded(table[pos], value) == 0;
}

template <Size N>
// Finds the longest matching affix on the selected side while keeping enough
// codepoints in the remaining core.
StrView longest_affix(StrView word, const Utf8Index &index,
                      const Arr<StrView, N> &table, Size min_remaining_cp,
                      AffixSide side) {
	if (side == AffixSide::Prefix) {
		for (Size end_cp = index.count - min_remaining_cp; end_cp > 0;
		     --end_cp) {
			const auto candidate = slice_cp(word, index, 0, end_cp);
			if (contains(table, candidate)) {
				return candidate;
			}
		}
		return {};
	}

	for (Size start_cp = min_remaining_cp; start_cp < index.count; ++start_cp) {
		const auto candidate = slice_cp(word, index, start_cp, index.count);
		if (contains(table, candidate)) {
			return candidate;
		}
	}
	return {};
}

// Right-side affixes are peeled in reverse order, so restore left-to-right
// order.
void push_reversed(Arena &a, DynArr<StrView> &dst, const DynArr<StrView> &src) {
	for (Size i = src.size - 1; i >= 0; --i) {
		dst.push(a, src[i]);
	}
}

// Checks whether a token begins with an uppercase codepoint.
bool starts_uppercase(StrView token) {
	if (!token) {
		return false;
	}
	Size used = 0;
	return is_upper_german(decode_utf8(token.data, token.size, &used));
}

bool is_ascii_outer_punctuation(char ch) {
	switch (ch) {
	case '"':
	case '\'':
	case '(':
	case ')':
	case '[':
	case ']':
	case '{':
	case '}':
	case '<':
	case '>':
	case '.':
	case ',':
	case ';':
	case ':':
	case '!':
	case '?':
		return true;
	default:
		return false;
	}
}

StrView trim_word_punctuation(StrView word) {
	word.mut_trim();
	while (word.size > 0 && is_ascii_outer_punctuation(word.first())) {
		word = word.slice(1);
	}
	while (word.size > 0 && is_ascii_outer_punctuation(word.last())) {
		word = word.slice(0, word.size - 1);
	}
	return word;
}

Size codepoint_count(StrView token) {
	Utf8Index index{};
	if (!token || !build_utf8_index(token, &index)) {
		return token.size;
	}
	return index.count;
}

// Uppercases the first codepoint and reuses the rest of the original bytes
// unchanged. Falls back to the original view if no change is needed or the temp
// buffer is too small.
StrView uppercase_first(StrView src, char *dst, Size capacity) {
	if (!src) {
		return src;
	}
	Size used = 0;
	const auto cp = decode_utf8(src.data, src.size, &used);
	const auto upper = uppercase_german(cp);
	if (upper == cp) {
		return src;
	}

	const Size head_size = encode_utf8(upper, dst);
	if (head_size + src.size - used > capacity) {
		return src;
	}
	for (Size i = 0; i < src.size - used; ++i) {
		dst[head_size + i] = src.data[used + i];
	}
	return {dst, head_size + src.size - used};
}

// Makes distractors follow the same initial capitalization as the source token.
void match_distractor_case(StrView token, Arr<StrView, 4> &distractors) {
	if (!starts_uppercase(token)) {
		return;
	}

	thread_local char capitalized[4][64]{};
	for (Size i = 0; i < 4; ++i) {
		distractors[i] = uppercase_first(distractors[i], capitalized[i],
		                                 sizeof(capitalized[i]));
	}
}

// Final stem splitter: emits small German-friendly chunks such as eu, ch, ng.
void append_atomic_chunks(Arena &a, StrView stem, DynArr<StrView> &out) {
	Utf8Index index{};
	if (!stem || !build_utf8_index(stem, &index) || index.count <= 1) {
		out.push(a, stem);
		return;
	}

	for (Size start_cp = 0; start_cp < index.count;) {
		Size take = 1;
		if (start_cp + 4 <= index.count &&
		    contains(AtomTables::clusters4,
		             slice_cp(stem, index, start_cp, start_cp + 4))) {
			take = 4;
		} else if (start_cp + 3 <= index.count &&
		           contains(AtomTables::clusters3,
		                    slice_cp(stem, index, start_cp, start_cp + 3))) {
			take = 3;
		} else if (start_cp + 2 <= index.count &&
		           contains(AtomTables::clusters2,
		                    slice_cp(stem, index, start_cp, start_cp + 2))) {
			take = 2;
		}

		out.push(a, slice_cp(stem, index, start_cp, start_cp + take));
		start_cp += take;
	}
}

struct StemUnit {
	Size from_cp{};
	Size to_cp{};
	bool nucleus{};
};

bool unit_is_nucleus(StrView unit) {
	if (compare_folded(unit, "qu"_v) == 0) {
		return false;
	}

	Utf8Index index{};
	if (!build_utf8_index(unit, &index)) {
		return false;
	}
	return has_vowel(index);
}

void build_atomic_units(Arena &a, StrView stem, const Utf8Index &index,
                        DynArr<StemUnit> &units) {
	for (Size start_cp = 0; start_cp < index.count;) {
		Size take = 1;
		if (start_cp + 4 <= index.count &&
		    contains(AtomTables::clusters4,
		             slice_cp(stem, index, start_cp, start_cp + 4))) {
			take = 4;
		} else if (start_cp + 3 <= index.count &&
		           contains(AtomTables::clusters3,
		                    slice_cp(stem, index, start_cp, start_cp + 3))) {
			take = 3;
		} else if (start_cp + 2 <= index.count &&
		           contains(AtomTables::clusters2,
		                    slice_cp(stem, index, start_cp, start_cp + 2))) {
			take = 2;
		}

		const auto unit = slice_cp(stem, index, start_cp, start_cp + take);
		units.push(a, {start_cp, start_cp + take, unit_is_nucleus(unit)});
		start_cp += take;
	}
}

void push_unit_range(Arena &a, StrView stem, const Utf8Index &index,
                     const DynArr<StemUnit> &units, Size from_unit,
                     Size to_unit, DynArr<StrView> &out) {
	if (from_unit >= to_unit) {
		return;
	}
	out.push(a, slice_cp(stem, index, units[from_unit].from_cp,
	                     units[to_unit - 1].to_cp));
}

void append_large_noun_chunks(Arena &a, StrView stem, DynArr<StrView> &out) {
	Utf8Index index{};
	if (!stem || !build_utf8_index(stem, &index) || index.count <= 3) {
		append_atomic_chunks(a, stem, out);
		return;
	}

	DynArr<StemUnit> units{};
	build_atomic_units(a, stem, index, units);
	if (units.size <= 1) {
		out.push(a, stem);
		return;
	}

	for (Size start = 0; start < units.size;) {
		if (units.size - start <= 1) {
			push_unit_range(a, stem, index, units, start, units.size, out);
			break;
		}

		Size vowel = start;
		while (vowel < units.size && !units[vowel].nucleus) {
			++vowel;
		}
		if (vowel >= units.size) {
			push_unit_range(a, stem, index, units, start, units.size, out);
			break;
		}

		Size next_vowel = vowel + 1;
		while (next_vowel < units.size && !units[next_vowel].nucleus) {
			++next_vowel;
		}

		Size chunk_end = units.size;
		if (next_vowel < units.size) {
			const Size consonants_between = next_vowel - (vowel + 1);
			if (consonants_between <= 1) {
				chunk_end = next_vowel;
			} else {
				chunk_end = next_vowel - 1;
			}
		} else {
			const Size trailing = units.size - (vowel + 1);
			if (trailing == 0) {
				chunk_end = units.size;
			} else if (trailing == 1 && vowel > start &&
			           units[vowel + 1].to_cp - units[vowel + 1].from_cp > 1) {
				chunk_end = vowel + 1;
			} else {
				chunk_end = units.size;
			}
		}

		if (chunk_end <= start) {
			chunk_end = start + 1;
		}
		push_unit_range(a, stem, index, units, start, chunk_end, out);
		start = chunk_end;
	}
}

template <Size N>
// Removes matching affixes from one side of the word and stores them in the
// order they should later appear in the final token stream.
void peel_affixes(Arena &a, StrView *core, DynArr<StrView> &parts,
                  const Arr<StrView, N> &table, Size min_remaining_cp,
                  AffixSide side, Size max_rounds = 1) {
	for (Size i = 0; i < max_rounds; ++i) {
		Utf8Index index{};
		if (!build_utf8_index(*core, &index) ||
		    index.count <= min_remaining_cp + 1) {
			return;
		}
		const auto affix =
			  longest_affix(*core, index, table, min_remaining_cp, side);
		if (!affix) {
			return;
		}

		const auto next_core =
			  side == AffixSide::Prefix
					? (*core).slice(affix.size)
					: (*core).slice(0, (*core).size - affix.size);
		Utf8Index next_index{};
		if (!build_utf8_index(next_core, &next_index) ||
		    next_index.count < min_remaining_cp || !has_vowel(next_index)) {
			return;
		}

		parts.push(a, affix);
		*core = next_core;
	}
}

// Reassembles left affixes, atomized core, and right affixes into one token
// list.
DynArr<StrView> finish_word(Arena &a, StrView core, const DynArr<StrView> &left,
                            const DynArr<StrView> &right,
                            bool prefer_large_noun_chunks = false) {
	DynArr<StrView> out{};
	for (const auto &part : left) {
		out.push(a, part);
	}

	if (!core) {
		push_reversed(a, out, right);
		return out;
	}

	if (prefer_large_noun_chunks) {
		append_large_noun_chunks(a, core, out);
	} else {
		append_atomic_chunks(a, core, out);
	}

	push_reversed(a, out, right);
	return out;
}

// Nouns mainly peel derivational and inflectional suffixes, then atomize the
// stem.
DynArr<StrView> tokenize_noun(Arena &a, StrView word) {
	DynArr<StrView> left{};
	DynArr<StrView> right{};
	StrView core = word.trim();
	if (!core) {
		return {};
	}

	peel_affixes(a, &core, right, NounTables::deriv_suffixes, 3,
	             AffixSide::Suffix, 2);
	peel_affixes(a, &core, right, NounTables::inflection_suffixes, 3,
	             AffixSide::Suffix);
	peel_affixes(a, &core, right, NounTables::deriv_suffixes, 3,
	             AffixSide::Suffix);
	if (right.is_contains("heit"_v) || right.is_contains("keit"_v)) {
		peel_affixes(a, &core, right, AdjectiveTables::deriv_suffixes, 3,
		             AffixSide::Suffix);
	}
	const bool prefer_large_chunks =
		  right.is_contains("anz"_v) || right.is_contains("enz"_v);
	return finish_word(a, core, left, right, prefer_large_chunks);
}

// Verbs peel suffixes first, then known prefixes, then atomize the remaining
// stem.
DynArr<StrView> tokenize_verb(Arena &a, StrView word) {
	DynArr<StrView> left{};
	DynArr<StrView> right{};
	StrView core = word.trim();
	if (!core) {
		return {};
	}

	peel_affixes(a, &core, right, VerbTables::infinitive_suffixes, 3,
	             AffixSide::Suffix);
	if (right.empty()) {
		peel_affixes(a, &core, right, VerbTables::finite_suffixes, 3,
		             AffixSide::Suffix);
	}
	peel_affixes(a, &core, left, VerbTables::prefixes, 3, AffixSide::Prefix, 3);
	return finish_word(a, core, left, right);
}

// Adjectives peel inflectional/derivational suffixes plus optional negative
// prefixes.
DynArr<StrView> tokenize_adjective(Arena &a, StrView word) {
	DynArr<StrView> left{};
	DynArr<StrView> right{};
	StrView core = word.trim();
	if (!core) {
		return {};
	}

	peel_affixes(a, &core, right, AdjectiveTables::outer_suffixes, 3,
	             AffixSide::Suffix);
	peel_affixes(a, &core, right, AdjectiveTables::deriv_suffixes, 3,
	             AffixSide::Suffix, 2);
	peel_affixes(a, &core, left, AdjectiveTables::prefixes, 3,
	             AffixSide::Prefix, 3);
	return finish_word(a, core, left, right);
}

template <Size N>
// Builds up to four nearby alternatives from the same sorted affix table.
void fill_neighbors(Arr<StrView, 4> &dst, StrView token, uint32_t seed,
                    const Arr<StrView, N> &table) {
	const Size pos = lower_bound(table, token);
	const uint32_t mixed = token_seed(token, seed);
	const bool prefer_right = (mixed & 1u) != 0;
	const Size start_offset = 1 + static_cast<Size>((mixed >> 1) % 2u);
	Size out = 0;
	auto try_push = [&](Size idx) {
		if (idx < 0 || idx >= N || table[idx] == token || out >= 4) {
			return;
		}
		for (Size i = 0; i < out; ++i) {
			if (dst[i] == table[idx]) {
				return;
			}
		}
		dst[out++] = table[idx];
	};
	for (Size delta = 0; out < 4 && delta < N; ++delta) {
		const Size offset = start_offset + delta;
		if (prefer_right) {
			try_push(pos + offset);
			try_push(pos - offset);
		} else {
			try_push(pos - offset);
			try_push(pos + offset);
		}
	}
	while (out < 4) {
		dst[out] = FallbackDistractors::short_chunks[out];
		++out;
	}
}

template <Size N>
// Uses a table only when the token itself is part of that table.
bool try_fill_neighbors(Arr<StrView, 4> &dst, StrView token, uint32_t seed,
                        const Arr<StrView, N> &table) {
	if (!contains(table, token)) {
		return false;
	}
	fill_neighbors(dst, token, seed, table);
	return true;
}

template <Size N>
// Fallback helper that keeps distractors in the same rough size class as the
// token.
void fill_first_distinct(Arr<StrView, 4> &dst, StrView token, uint32_t seed,
                         const Arr<StrView, N> &table) {
	const Size start = static_cast<Size>(token_seed(token, seed) % N);
	Size out = 0;
	for (Size step = 0; step < N; ++step) {
		const auto &candidate = table[(start + step) % N];
		if (candidate == token) {
			continue;
		}
		dst[out++] = candidate;
		if (out == 4) {
			return;
		}
	}
	while (out < 4) {
		dst[out] = table[(start + out) % N];
		++out;
	}
}

template <Size N>
bool has_known_prefix(StrView word, const Arr<StrView, N> &table,
                      Size min_remaining_cp = 2) {
	Utf8Index index{};
	if (!word || !build_utf8_index(word, &index) ||
	    index.count <= min_remaining_cp) {
		return false;
	}
	return longest_affix(word, index, table, min_remaining_cp,
	                     AffixSide::Prefix);
}

template <Size N>
bool has_known_suffix(StrView word, const Arr<StrView, N> &table,
                      Size min_remaining_cp = 2) {
	Utf8Index index{};
	if (!word || !build_utf8_index(word, &index) ||
	    index.count <= min_remaining_cp) {
		return false;
	}
	return longest_affix(word, index, table, min_remaining_cp,
	                     AffixSide::Suffix);
}

} // namespace

namespace Tokenizer {
Kind guess_kind(StrView word) {
	word = trim_word_punctuation(word);
	if (!word) {
		return Kind::Verb;
	}

	int noun_score = 0;
	int verb_score = 0;
	int adjective_score = 0;

	const bool uppercase = starts_uppercase(word);
	if (uppercase) {
		noun_score += 4;
	} else {
		verb_score += 1;
		adjective_score += 1;
	}

	const bool noun_deriv =
		  has_known_suffix(word, NounTables::deriv_suffixes, 3);
	const bool noun_inflect =
		  has_known_suffix(word, NounTables::inflection_suffixes, 3);
	const bool verb_infinitive =
		  has_known_suffix(word, VerbTables::infinitive_suffixes, 3);
	const bool verb_finite =
		  has_known_suffix(word, VerbTables::finite_suffixes, 3);
	const bool verb_prefix = has_known_prefix(word, VerbTables::prefixes, 3);
	const bool adjective_deriv =
		  has_known_suffix(word, AdjectiveTables::deriv_suffixes, 3);
	const bool adjective_outer =
		  has_known_suffix(word, AdjectiveTables::outer_suffixes, 3);
	const bool adjective_prefix =
		  has_known_prefix(word, AdjectiveTables::prefixes, 3);

	if (noun_deriv) {
		noun_score += 3;
	}
	if (noun_inflect) {
		noun_score += uppercase ? 2 : 1;
	}
	if (verb_infinitive) {
		verb_score += 4;
	}
	if (verb_finite) {
		verb_score += 3;
	}
	if (verb_prefix) {
		verb_score += (verb_infinitive || verb_finite) ? 2 : 1;
	}
	if (adjective_deriv) {
		adjective_score += 3;
	}
	if (adjective_outer) {
		adjective_score += 2;
	}
	if (adjective_prefix) {
		adjective_score += 1;
	}

	if (noun_score >= verb_score && noun_score >= adjective_score) {
		return Kind::Noun;
	}
	if (adjective_score > verb_score) {
		return Kind::Adjective;
	}
	return Kind::Verb;
}

// Main public entry point. Verb remains the default because existing exercise
// code calls tokenization without specifying a part of speech.
DynArr<StrView> to_chunks(Arena &a, StrView str, Kind kind) {
	switch (kind) {
	case Kind::Noun:
		return tokenize_noun(a, str);
	case Kind::Adjective:
		return tokenize_adjective(a, str);
	case Kind::Verb:
	default:
		return tokenize_verb(a, str);
	}
}

DynArr<StrView> to_letters(Arena &a, StrView str) {
	DynArr<StrView> ret;
	for (auto &ch : str) {
		if (utf8_is_continuation(ch)) {
			ret.last().size += 1;
		} else {
			ret.push(a, {&ch, 1});
		}
	}
	return ret;
}

// Returns four distractors for one chunk. Prefer same-family affixes when
// possible, otherwise fall back to a small static pool.
DynArr<StrView> get_4_distractors_for_a_chunk(StrView token, uint32_t seed) {
	thread_local Arr<StrView, 4> out{};
	const Size cp_count = codepoint_count(token);

	if (cp_count <= 1) {
		fill_first_distinct(out, token, seed, FallbackDistractors::letters);
		match_distractor_case(token, out);
		return DynArr<StrView>::from(out.data, 4);
	}

	if (try_fill_neighbors(out, token, seed, VerbTables::prefixes) ||
	    try_fill_neighbors(out, token, seed, VerbTables::infinitive_suffixes) ||
	    try_fill_neighbors(out, token, seed, VerbTables::finite_suffixes) ||
	    try_fill_neighbors(out, token, seed, AdjectiveTables::deriv_suffixes) ||
	    try_fill_neighbors(out, token, seed, AdjectiveTables::outer_suffixes) ||
	    try_fill_neighbors(out, token, seed, NounTables::deriv_suffixes) ||
	    try_fill_neighbors(out, token, seed, NounTables::inflection_suffixes) ||
	    try_fill_neighbors(out, token, seed, AtomTables::clusters2) ||
	    try_fill_neighbors(out, token, seed, AtomTables::clusters3) ||
	    try_fill_neighbors(out, token, seed, AtomTables::clusters4)) {
		match_distractor_case(token, out);
		return DynArr<StrView>::from(out.data, 4);
	}

	const auto *fallback = &FallbackDistractors::stem_chunks;
	if (token.size <= 6) {
		fallback = &FallbackDistractors::short_chunks;
	} else if (token.size <= 12) {
		fallback = &FallbackDistractors::suffix_chunks;
	}

	fill_first_distinct(out, token, seed, *fallback);
	match_distractor_case(token, out);
	return DynArr<StrView>::from(out.data, 4);
}
} // namespace Tokenizer
