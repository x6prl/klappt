#pragma once

#include "base/arena.h"

template <class T, Size N> struct Stack {
	T data[N]{};
	Size size{0};

	// skip insert on full stack
	constexpr void push(T val) {
		if (size < capacity()) {
			data[size] = val;
			++size;
		}
	}
	constexpr void pop() {
		if (size > 0) {
			--size;
		}
	}
	constexpr T top_value() const {
		return data[size-1];
	}
	constexpr bool is_empty() const {
		return size == 0;
	}
	constexpr bool is_full() const {
		return size == capacity();
	}

	constexpr Size capacity() const { return N; }
	constexpr T &operator[](Size i) { return data[i]; }
	constexpr const T &operator[](Size i) const { return data[i]; }

	constexpr T *begin() { return data; }
	constexpr T *end() { return data + size; }
	constexpr const T *begin() const { return data; }
	constexpr const T *end() const { return data + size; }

	bool is_contains(T val) const {
		for (const auto &x : *this) {
			if (x == val)
				return true;
		}
		return false;
	}
};
