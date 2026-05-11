#pragma once

#include "SDL3/SDL_log.h"

#include "word.h"

struct WordRef {
	int32_t value{};

	constexpr bool operator==(const WordRef &) const = default;
	constexpr bool operator<(const int32_t other) { return value < other; }
	constexpr WordRef &operator++() {
		++value;
		return *this;
	}
	constexpr WordRef operator++(int) {
		auto copy = *this;
		++(*this);
		return copy;
	}
};

struct Words {
	static constexpr Size MAX_WORDS = 2 * 1024;
	bool used[MAX_WORDS]{};
	Word words[MAX_WORDS]{};
	Size next_free = 1;
	Size size = 0;

	WordRef add() {
		if (next_free < MAX_WORDS) {
			auto ret = next_free++;
			used[ret] = true;
			size++;
			return {ret};
		} else {
			for (Size i = 1; i < MAX_WORDS; ++i) {
				if (!used[i]) {
					return {i};
					used[i] = true;
					return {i};
				}
			}
			SDL_LogError(SDL_LOG_CATEGORY_ERROR, "%s:\n\tNO SPACE",
			             __PRETTY_FUNCTION__);
			exit(666);
			return null_index();
		}
	}

	void remove_by_ref(WordRef ref) {
		size--;
		used[ref.value] = false;
		words[ref.value] = {};
	}
	void remove_by_id(WordId word_id) {
		for (auto i{0}; i < MAX_WORDS; ++i) {
			if (used[i] && words[i].word_id == word_id) {
				remove_by_ref({i});
			}
		}
	}

	Word &operator[](WordRef ref) { return words[ref.value]; }
	const Word &operator[](WordRef ref) const { return words[ref.value]; }

	bool is_used(WordRef ref) const {
		return ref.value > 0 && used[ref.value];
	}

	static WordRef null_index() { return {0}; }
};
