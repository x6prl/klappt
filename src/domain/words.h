#pragma once

#include "SDL3/SDL_log.h"

#include "word.h"

struct Words;

/*
 * As for now, can be used as a bad iterator
 */
struct WordRef {
	int32_t value{};

	constexpr bool operator==(const WordRef &) const = default;
	constexpr bool operator<(const WordRef other) const {
		return value < other.value;
	}
	constexpr bool operator<(const int32_t other) const {
		return value < other;
	}

	WordRef &advance(const Words *words);
};

struct Words {
	static constexpr Size MAX_WORDS = 1 << 11;
	bool used[MAX_WORDS]{};
	Word words[MAX_WORDS]{};
	Size next_free = 1;
	Size size = 0;

	constexpr WordRef begin() const { return {1}; }
	constexpr WordRef end() const { return {next_free}; }

	WordRef add() {
		for (Size ref = 1; ref < next_free; ++ref) {
			if (!used[ref]) {
				used[ref] = true;
				++size;
				return {ref};
			}
		}
		if (next_free < MAX_WORDS) {
			used[next_free] = true;
			++size;
			++next_free;
			return {next_free - 1};
		} else {
			// TODO: handle the error? controll this somewhere else?
			SDL_LogError(SDL_LOG_CATEGORY_ERROR, "%s:\n\tNO SPACE",
			             __PRETTY_FUNCTION__);
			return null_index();
		}
	}

	void remove_by_ref(WordRef ref) {
		--size;
		used[ref.value] = false;
		words[ref.value] = {};
		if (ref.value + 1 == next_free) {
			for (; next_free > 1 && !used[next_free - 1];) {
				--next_free;
			}
		}
	}
	void remove_by_id(WordId word_id) {
		for (auto ref = begin(); ref < end(); ref.advance(this)) {
			if ((*this)[ref].word_id == word_id) {
				remove_by_ref(ref);
			}
		}
	}

	Word &operator[](WordRef ref) { return words[ref.value]; }
	const Word &operator[](WordRef ref) const { return words[ref.value]; }

	static WordRef null_index() { return {0}; }
};

inline WordRef &WordRef::advance(const Words *words) {
	for (++value; value < words->next_free && !words->used[value]; ++value) {
	}
	return *this;
}
