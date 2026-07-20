#pragma once

#include "base/arr.h"

template <class T, Size N> struct FixedSet {
	Arr<T, N> buffer{};
	Size size{0};
	Size capacity() const { return buffer.size(); }

	constexpr T &operator[](Size i) { return buffer[i]; }
	constexpr T *begin() { return buffer.begin(); }
	constexpr T *end() { return buffer.end(); }
	constexpr const T *begin() const { return buffer.begin(); }
	constexpr const T *end() const { return buffer.end(); }
	bool is_empty() { return !size; }

	bool push_one(T val) {
		if (size < buffer.size()) {
			buffer[size] = val;
			++size;
			return true;
		} else {
			return false;
		}
	}

	T top_value() {
		if (size) {
			return buffer[size - 1];
		} else {
			return {};
		}
	}

	bool pop_top() {
		if (size) {
			--size;
			return true;
		}
		return false;
	}

	// NOTE: goes from the beginning
	bool remove_by_val(T val) {
		for (Size i{0}; i < size; ++i) {
			if (val == buffer[i]) {
				if (i != size - 1) {
					// not the last element
					buffer[i] = buffer[size - 1];
				}
				--size;
				return true;
			}
		}
		return false;
	}
};
