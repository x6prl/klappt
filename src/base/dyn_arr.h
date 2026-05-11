#pragma once

#include "arena.h"
#include <algorithm>
#include <cstring>
#include <type_traits>
#include <utility>

template <class T> struct DynArr {
	static_assert(std::is_trivially_copyable_v<T>);
	static_assert(std::is_trivially_move_constructible_v<T>);
	static_assert(std::is_trivially_destructible_v<T>);

	static constexpr Size INITIAL_SIZE = 5; // mostly we need 5 elements

	T *data{nullptr};
	Size size{};
	Size reserved{};

	bool empty() const { return !size; }
	void reset_size_reserved() { size = reserved = 0; }
	T &operator[](Size i) { return data[i]; }
	const T &operator[](Size i) const { return data[i]; }
	T *begin() { return data; }
	T *end() { return data + size; }
	const T *begin() const { return data; }
	const T *end() const { return data + size; }
	T &first() { return *data; }
	T &last() { return data[size - 1]; }
	const T &first() const { return *data; }
	const T &last() const { return data[size - 1]; }

	void push(Arena &a, T val) {
		if (size >= reserved) {
			reserved = reserved ? reserved * 2 : INITIAL_SIZE;
			auto newdata = a.pushN<T>(reserved);
			if (size) {
				memcpy(newdata, data, sizeof(T) * (size));
			}
			data = newdata;
		}
		data[size] = val;
		++size;
	}
	void append(Arena &a, DynArr<T> other) {
		if (size + other.size > reserved) {
			reserved = size + other.size;
			auto newdata = a.pushN<T>(reserved);
			if (data) {
				memcpy(newdata, data, sizeof(T) * (size));
			}
			data = newdata;
		}
		memcpy(data + size, other.data, sizeof(T) * other.size);
		size += other.size;
	}
	void pop(Size count = 1) { size = count >= size ? 0 : size - count; }
	bool is_contains(T val) const {
		for (const auto &x : *this) {
			if (x == val)
				return true;
		}
		return false;
	}
	// bool is_contains_by_ref(const T &val) const {
	// 	for (const auto &x : *this) {
	// 		if (x == val)
	// 			return true;
	// 	}
	// 	return false;
	// }
	static DynArr<T> concat(Arena &a, DynArr<T> x, DynArr<T> y) {
		if (!y.size)
			return x;
		if (!x.size)
			return y;
		auto total_size = x.size + y.size;
		auto data = a.pushN<T>(total_size);
		memcpy(data, x.data, x.size * sizeof(T));
		memcpy(data + x.size, y.data, y.size * sizeof(T));
		return {data, total_size, total_size};
	}
	template <class... Args> static DynArr<T> with(Arena &a, Args... args) {
		constexpr auto total_args = sizeof...(args);
		DynArr ret{a.pushN<T>(total_args), 0, total_args};
		(ret.push(a, std::forward<Args>(args)), ...);
		return ret;
	}
	template <size_t reserve_at_least, class... Args>
	static DynArr<T> with(Arena &a, Args... args) {
		constexpr auto total_args = sizeof...(args);
		constexpr auto reserve = std::max(reserve_at_least, total_args);
		DynArr ret{a.pushN<T>(reserve), 0, reserve};
		(ret.push(a, args), ...);
		return ret;
	}
	static DynArr<T> from(T *data, Size size) { return {data, size, size}; }
	static DynArr<T> filled(Arena &a, T val, Size count) {
		DynArr ret{a.pushN<T>(count), 0, count};
		for (auto &x : ret) {
			x = val;
		}
		return ret;
	}
	static DynArr<T> filled_zero_or_default(Arena &a, Size count) {
		DynArr ret{a.pushN<T>(count), 0, count};
		if constexpr (std::is_trivially_constructible_v<T>) {
			memset(ret.data, 0, count * sizeof(T));
		} else {
			for (Size i{0}; i < count; ++i) {
				ret.data[i] = T{};
			}
		}
		return ret;
	}
};
