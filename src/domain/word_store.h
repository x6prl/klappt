#pragma once

#include <memory>
#include <string>

#include "SDL3/SDL_log.h"
#include "xapian.h"

#include "base/arena.h"
#include "base/profiler.h"
#include "words_codec.h"

struct Arena;

// TODO: rewrite
struct WordStore {
	std::string path{};
	std::unique_ptr<Xapian::WritableDatabase> db{};
	mutable Size cached_word_count{};
	mutable bool has_cached_word_count{};

	~WordStore();

	bool open(StrView path = {});
	void close();
	Size word_count() const;
	Size matching_word_count(StrView query) const;

	/*
	 * Ensure the word exists in Xapian. If it already exists, word_id is filled
	 * from the stored copy. If it is new, a fresh immutable word_id is
	 * assigned.
	 */
	bool ensure_word(Arena &scratch, Word &word, bool *was_new = nullptr);
	bool get_by_id(Arena &scratch, WordId word_id, Word &word);
	void save(Arena &scratch, Word &word);
	void set_was_learned(Arena &scratch, Word &word);

	template <class F> bool for_each_word(Arena &scratch, F &&visitor) const {
		return for_each_word_range(
			  scratch, 0, word_count(),
			  [&](Size, const Word &word) { return visitor(word); });
	}

	template <class F>
	bool for_each_matching_word_range(Arena &scratch, StrView query, Size start,
	                                  Size count, F &&visitor) const {
		KLAPPT_PROFILE_SCOPE_N("WordStore::for_each_matching_word_range");
		query.mut_trim();
		if (!query) {
			return for_each_word_range(scratch, start, count, visitor);
		}
		if (!db || count <= 0) {
			return true;
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
			for (auto it = mset.begin(); it != mset.end(); ++it) {
				Word word{};
				const auto doc = it.get_document();
				const auto data = doc.get_data();
				if (!WordsCodec::decode_word(scratch, data.data(),
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

	template <class F>
	bool for_each_word_range(Arena &scratch, Size start, Size count,
	                         F &&visitor) const {
		KLAPPT_PROFILE_SCOPE_N("WordStore::for_each_word_range");
		if (!db || count <= 0) {
			return true;
		}
		if (start < 0) {
			start = 0;
		}
		try {
			auto guard = scratch.guard();
			Size index = 0;
			Size emitted = 0;
			for (auto it = db->allterms_begin("Q"); it != db->allterms_end("Q");
			     ++it) {
				if (index < start) {
					++index;
					continue;
				}
				if (emitted >= count) {
					break;
				}
				auto postings = db->postlist_begin(*it);
				if (postings == db->postlist_end(*it)) {
					++index;
					continue;
				}

				Word word{};
				const auto doc = db->get_document(*postings);
				const auto data = doc.get_data();
				if (!WordsCodec::decode_word(scratch, data.data(),
				                             static_cast<Size>(data.size()),
				                             word)) {
					SDL_LogError(
						  SDL_LOG_CATEGORY_ERROR,
						  "Decoding Xapian word document failed for term %s",
						  (*it).c_str());
					++index;
					continue;
				}
				if (!visitor(index, word)) {
					break;
				}
				++index;
				++emitted;
			}
			return true;
		} catch (const Xapian::Error &e) {
			SDL_LogError(SDL_LOG_CATEGORY_ERROR,
			             "Iterating Xapian words failed: %s",
			             e.get_description().c_str());
			return false;
		}
	}

  private:
	bool search_mset(StrView query, Size start, Size count,
	                 Xapian::MSet &mset) const;
};
