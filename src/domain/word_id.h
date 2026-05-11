#pragma once

#include <cstddef>
#include <cstdint>

struct WordId {
	uint64_t value{};

	constexpr bool operator==(const WordId &) const = default;
	constexpr bool operator<(const WordId &other) const {
		return value < other.value;
	}
};

struct WordIdHash {
	size_t operator()(const WordId &word_id) const noexcept {
		return static_cast<size_t>(word_id.value);
	}
};
