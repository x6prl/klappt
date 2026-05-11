#pragma once

#include "arena.h"

template <class T, Size N> struct Arr {
	T data[N]{};

	constexpr Size size() const { return N; }
	constexpr T &operator[](Size i) { return data[i]; }
	constexpr const T &operator[](Size i) const { return data[i]; }

	constexpr T *begin() { return data; }
	constexpr T *end() { return data + size(); }
	constexpr const T *begin() const { return data; }
	constexpr const T *end() const { return data + size(); }

	bool is_contains(T val) const {
		for (const auto &x : *this) {
			if (x == val)
				return true;
		}
		return false;
	}
};
