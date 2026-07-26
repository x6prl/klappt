#pragma once

#include "base/arena.h"
#include "base/arr.h"
#include "base/str_view.h"

template <Size N> struct FixedStr {
	Arr<char, N> buffer{};
	Size size{0};

	bool is_empty() const { return !size; }

	StrView view() { return {buffer.data, size}; };

	const char *mutable_to_cstr() {
		buffer[buffer.size() == size ? buffer.size() - 1 : size] = '\0';
		return buffer.data;
	}

	void copy_from(StrView str) {
		size = str.size <= buffer.size() ? str.size : buffer.size();
		memcpy(buffer.data, str.data, size);
	}
};

