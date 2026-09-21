#pragma once

#include "base/arena.h"
#include "base/str_view.h"
#include "domain/word.h"
#include "ui/translations/langs.h"

enum class SearchMode : uint8_t {
	All = 0,
	TranslationsOnly = 1,
};

inline bool word_matches_query(Arena &a, const Word &word, StrView query);
inline char ascii_to_lower(char ch);
inline bool str_contains_ci(StrView haystack, StrView needle);
inline bool match_translation_query_cs(StrView translations_raw, StrView query);

namespace Xapian {
struct WritableDatabase;
struct Database;
struct MSet;
}; // namespace Xapian

struct WordStore {
	Xapian::WritableDatabase *db{nullptr};
	Xapian::Database *sub0{nullptr};
	Xapian::Database *sub1{nullptr};

	~WordStore();

	bool open(StrView path);
	bool open_sub0(StrView path);
	bool open_sub1(StrView path);
	void close();
	bool is_open() const { return db != nullptr; }

	Size word_count() const;
	Size matching_word_count(Arena &scratch, StrView query,
	                         SearchMode mode) const;

	/*
	 * Ensure the word exists in Xapian. If it already exists, word_id is filled
	 * from the stored copy.
	 */
	bool find_word(Arena &scratch, Word &word) const;

	bool get_by_id(Arena &scratch, WordId word_id, Word &word) const;
	bool get_by_id_for_lang(Arena &scratch, WordId word_id, Word &word,
	                        int8_t lang_id, uint8_t app_lang_id) const;

	bool get_by_id_for_lang(Arena &scratch, WordId word_id, Word &word,
	                        Lang lang, Lang app_lang) const {
		return get_by_id_for_lang(scratch, word_id, word,
		                          static_cast<int8_t>(lang),
		                          static_cast<int8_t>(app_lang));
	}

	// NOTE: used for db gen
	void save_direct(Arena &scratch, const Word &word,
	                 bool is_mark_dirty_or_new = false);

	// NOTE: not used yet: is_mark_dirty_or_new if non-user layer changed or it
	// is a new word
	void save(Arena &scratch, Word &word, bool is_mark_dirty_or_new = false);
	void set_was_learned(Arena &scratch, Word &word);
	bool remove_by_id(Arena &scratch, WordId word_id);

	// =========================================================================
	// NOTE: not used yet: server sync
	// =========================================================================

	// NOTE: not used yet: clearing dirty flag
	bool mark_synced(Arena &scratch, WordId word_id);

	// NOTE: not used yet: changing local id to the server's one
	bool rekey_word(Arena &scratch, WordId old_id, WordId new_id);

	bool search_mset(Arena &scratch, StrView query, Size start, Size count,
	                 Xapian::MSet *mset, SearchMode mode) const;

	Size get_smart_suggestions(Arena &scratch, Arena &out_arena, Size count,
	                           DynArr<Word> &out, uint64_t *rng_state) const;

  private:
	Size get_random_unlearned_words(Arena &scratch, Arena &out_arena,
	                                Size count, DynArr<Word> &out,
	                                uint64_t *rng_state) const;
};

inline bool word_matches_query(Arena &a, const Word &word, StrView query) {
	query = query.mut_trim().utf8_to_lowercase(a);
	if (!query) {
		return true;
	}
	if (match_translation_query_cs(word.translations_raw, query)) {
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

inline char ascii_to_lower(char ch) {
	if (ch >= 'A' && ch <= 'Z') {
		return static_cast<char>(ch - 'A' + 'a');
	}
	return ch;
}

inline bool str_contains_ci(StrView haystack, StrView needle) {
	if (!needle) {
		return true;
	}
	if (!haystack || needle.size > haystack.size) {
		return false;
	}
	for (Size i = 0; i <= haystack.size - needle.size; ++i) {
		bool match = true;
		for (Size j = 0; j < needle.size; ++j) {
			if (ascii_to_lower(haystack[i + j]) != ascii_to_lower(needle[j])) {
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

inline bool match_translation_query_cs(StrView translations_raw,
                                       StrView query) {
	for (; translations_raw;) {
		auto tr = translations_raw.mut_split_by(';').trim();
		if (!tr) {
			continue;
		}
		if (tr.is_contains_substr(query)) {
			return true;
		}
	}
	return false;
}
