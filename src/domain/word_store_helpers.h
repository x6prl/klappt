#pragma once

#include "base/arena.h"
#include "base/profiler.h"
#include "domain/word_store.h"
#include "domain/words_codec.h"

#include <xapian.h>

namespace WordStoreHelper {
// NOTE: not used yet: travers localy changed words
template <typename F>
bool for_each_dirty_word(Arena &scratch, const WordStore &ws, F &&visitor) {
	KLAPPT_PROFILE_SCOPE_N("WordStore::for_each_dirty_word");
	if (!ws.is_open())
		return false;

	const char *dirty_terms[] = {"SDIRTY", "SNEW"};
	for (const char *term : dirty_terms) {
		try {
			for (auto it = ws.db->postlist_begin(term);
			     it != ws.db->postlist_end(term); ++it) {
				auto guard = scratch.guard();
				const auto doc = ws.db->get_document(*it);
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
bool for_each_matching_word_range(Arena &scratch, const WordStore &ws,
                                  StrView query, Size start, Size count,
                                  SearchMode mode, F &&visitor) {
	KLAPPT_PROFILE_SCOPE_N("WordStore::for_each_matching_word_range");
	query.mut_trim();
	if (!ws.is_open() || !query || count <= 0) {
		return false;
	}
	if (start < 0) {
		start = 0;
	}

	try {
		auto guard = scratch.guard();
		Xapian::MSet mset;
		if (!ws.search_mset(scratch, query, start, count, &mset, mode)) {
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
				SDL_LogError(SDL_LOG_CATEGORY_ERROR,
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
bool for_each_word(Arena &scratch, const WordStore &ws, F &&visitor) {
	KLAPPT_PROFILE_SCOPE_N("WordStore::for_each_word");
	if (!ws.is_open())
		return false;

	try {
		for (auto it = ws.db->allterms_begin("Q");
		     it != ws.db->allterms_end("Q"); ++it) {
			auto guard = scratch.guard();
			auto postings = ws.db->postlist_begin(*it);
			if (postings == ws.db->postlist_end(*it)) {
				continue;
			}

			Word word{};
			const auto doc = ws.db->get_document(*postings);
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
} // namespace WordStoreHelper
