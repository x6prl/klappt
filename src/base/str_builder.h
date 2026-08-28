#pragma once

#include <cstring>

#include "dyn_arr.h"
#include "str_view.h"

struct StrBuilder {
	static constexpr Size MAX_SIZE = 64;

	DynArr<StrView> data{};
	Size total_lenght{0};

	StrBuilder() = default;
	StrBuilder(DynArr<StrView> arr) {
		for (auto &s : arr) {
			total_lenght += s.size;
		}
		data = arr;
	}

	void push(Arena &a, StrView str) {
		total_lenght += str.size;
		data.push(a, str);
	}
	template <typename T,
	          std::enable_if_t<std::is_arithmetic_v<T>, bool> = true>
	void push(Arena &a, T val) {
		push(a, StrView::from_number(a, val));
	}
	void push(Arena &a, float val, int precision) {
		push(a, StrView::from_number(a, val, precision));
	}
	void push(Arena &a, double val, int precision) {
		push(a, StrView::from_number(a, val, precision));
	}
	void push_formatted_bytes(Arena &a, Size bytes) {
		if (bytes < 1024) {
			push(a, bytes);
			push(a, "B"_v);
		} else if (bytes < 1024 * 1024) {
			push(a, (float)bytes / 1024.0f, 1);
			push(a, "KB"_v);
		} else {
			push(a, (float)bytes / (1024.0f * 1024.0f), 1);
			push(a, "MB"_v);
		}
	}

	void append(Arena &a, StrBuilder str) {
		total_lenght += str.total_lenght;
		data.append(a, std::move(str.data));
	}

	StrView join(Arena &a) {
		if (data.is_empty()) {
			return {};
		}
		auto dst = a.pushN<char>(total_lenght);
		StrView ret{dst, total_lenght};

		for (auto &str : data) {
			memcpy(dst, str.data, str.size);
			dst += str.size;
		}
		return ret;
	}
	StrView join(Arena &a, char delim) {
		if (data.is_empty()) {
			return {};
		}
		auto delim_count = (data.size - 1);
		auto result_lenght = total_lenght + delim_count;
		auto dst = a.pushN<char>(result_lenght);
		StrView ret{dst, result_lenght};

		for (Size i{0}; i < data.size; ++i) {
			if (i) {
				*dst = delim;
				++dst;
			}
			auto &str = data[i];
			memcpy(dst, str.data, str.size);
			dst += str.size;
		}
		return ret;
	}
	StrView join(Arena &a, StrView insert) {
		if (data.is_empty()) {
			return {};
		}
		auto delim_count = (data.size - 1);
		auto result_lenght = total_lenght + delim_count * insert.size;
		auto dst = a.pushN<char>(result_lenght);
		StrView ret{dst, result_lenght};

		for (Size i{0}; i < data.size; ++i) {
			if (i) {
				memcpy(dst, insert.data, insert.size);
				dst += insert.size;
			}
			auto &str = data[i];
			memcpy(dst, str.data, str.size);
			dst += str.size;
		}
		return ret;
	}
	template <typename... Args>
	[[nodiscard]]
	static StrView concat(Arena &arena, const Args &...args);
};

template <typename... Args>
StrView StrBuilder::concat(Arena &arena, const Args &...args) {
	const Size new_size = (args.size + ...);
	char *new_mem = arena.pushN<char>(new_size);

	char *dst = new_mem;
	auto append = [&](const StrView str) {
		memcpy(dst, str.data, str.size);
		dst += str.size;
	};

	(append(args), ...);

	return {new_mem, new_size};
}
