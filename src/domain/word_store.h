#pragma once

#include <SDL3/SDL_log.h>
#include <xapian.h>

#include "base/arena.h"
#include "base/profiler.h"
#include "base/str_view.h"
#include "domain/word.h"
#include "ui/translations/langs.h"
#include "words_codec.h"

inline bool word_matches_query(Arena &a, const Word &word, StrView query);
inline char ascii_to_lower(char ch);
inline bool str_contains_ci(StrView haystack, StrView needle);
inline bool match_translation_query_cs(StrView translations_raw, StrView query);

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
	Size matching_word_count(StrView query) const;

	/*
	 * Ensure the word exists in Xapian. If it already exists, word_id is filled
	 * from the stored copy.
	 */
	bool find_and_fill_word_id(Arena &scratch, Word &word);

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

	// NOTE: not used yet: travers localy changed words
	template <typename F>
	bool for_each_dirty_word(Arena &scratch, F &&visitor) const {
		KLAPPT_PROFILE_SCOPE_N("WordStore::for_each_dirty_word");
		if (!db)
			return false;

		const char *dirty_terms[] = {"SDIRTY", "SNEW"};
		for (const char *term : dirty_terms) {
			try {
				for (auto it = db->postlist_begin(term);
				     it != db->postlist_end(term); ++it) {
					auto guard = scratch.guard();
					const auto doc = db->get_document(*it);
					const auto data = doc.get_data();

					Word word{};
					if (!WordsCodec::word_decode(scratch, data.data(),
					                             static_cast<Size>(data.size()),
					                             word)) {
						continue;
					}

					const bool is_new = (term[1] == 'N'); // 'SNEW'
					if (!visitor(word, is_new)) {
						return false;
					}
				}
			} catch (const Xapian::Error &e) {
				SDL_LogError(SDL_LOG_CATEGORY_ERROR,
				             "Iterating dirty words failed: %s",
				             e.get_description().c_str());
				return false;
			}
		}
		return true;
	}

	// =========================================================================
	// Search and pagination
	// =========================================================================

	template <typename F>
	bool for_each_matching_word_range(Arena &scratch, StrView query, Size start,
	                                  Size count, F &&visitor) const {
		KLAPPT_PROFILE_SCOPE_N("WordStore::for_each_matching_word_range");
		query.mut_trim();
		if (!db || !query || count <= 0) {
			return false;
		}
		if (start < 0) {
			start = 0;
		}

		try {
			auto guard = scratch.guard();
			Xapian::MSet mset;
			if (!search_mset(query, start, count, mset)) {
				return false;
			}

			mset.fetch();
			auto item_guard = scratch.guard();
			for (auto it = mset.begin(); it != mset.end(); ++it) {
				Word word{};
				const auto doc = it.get_document();
				const auto data = doc.get_data();

				if (!WordsCodec::word_decode(scratch, data.data(),
				                             static_cast<Size>(data.size()),
				                             word)) {
					SDL_LogError(
						  SDL_LOG_CATEGORY_ERROR,
						  "Decoding Xapian matched word document failed");
					continue;
				}

				if (!visitor(static_cast<Size>(it.get_rank()), word)) {
					break;
				}
			}
			return true;
		} catch (const Xapian::Error &e) {
			SDL_LogError(SDL_LOG_CATEGORY_ERROR,
			             "Iterating matching Xapian words failed: %s",
			             e.get_description().c_str());
			return false;
		}
	}

	// full db traverse
	template <typename F>
	bool for_each_word(Arena &scratch, F &&visitor) const {
		KLAPPT_PROFILE_SCOPE_N("WordStore::for_each_word");
		if (!db)
			return false;

		try {
			for (auto it = db->allterms_begin("Q"); it != db->allterms_end("Q");
			     ++it) {
				auto guard = scratch.guard();
				auto postings = db->postlist_begin(*it);
				if (postings == db->postlist_end(*it)) {
					continue;
				}

				Word word{};
				const auto doc = db->get_document(*postings);
				const auto data = doc.get_data();
				if (!WordsCodec::word_decode(scratch, data.data(),
				                             static_cast<Size>(data.size()),
				                             word)) {
					continue;
				}

				if (!visitor(word)) {
					break;
				}
			}
			return true;
		} catch (const Xapian::Error &e) {
			SDL_LogError(SDL_LOG_CATEGORY_ERROR,
			             "Iterating all Xapian words failed: %s",
			             e.get_description().c_str());
			return false;
		}
	}

	Size get_smart_suggestions(Arena &scratch, Arena &out_arena, Size count,
	                           DynArr<Word> &out, uint64_t *rng_state) const;

  private:
	Size get_random_unlearned_words(Arena &scratch, Arena &out_arena,
	                                Size count, DynArr<Word> &out,
	                                uint64_t *rng_state) const;

  private:
	bool search_mset(StrView query, Size start, Size count,
	                 Xapian::MSet &mset) const;
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
