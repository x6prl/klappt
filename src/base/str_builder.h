#pragma once

#include <cstring>

#include "dyn_arr.h"
#include "str_view.h"

struct StrBuilder {
	static constexpr Size MAX_SIZE = 64;

	DynArr<StrView> data{};
	Size total_lenght{0};

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
		auto dst = a.pushN<char>(total_lenght);
		StrView ret = {dst, total_lenght};

		for (auto &str : data) {
			if (str.size > 0) {
				memcpy(dst, str.data, str.size);
				dst += str.size;
			}
		}
		return ret;
	}
	StrView join(Arena &a, char delim) {
		auto delim_count = (data.size - 1);
		auto result_lenght = total_lenght + delim_count;
		auto dst = a.pushN<char>(result_lenght);
		StrView ret = {dst, result_lenght};

		for (Size i{0}; i < data.size; ++i) {
			if (i) {
				dst[-1] = delim;
			}
			auto &str = data[i];
			if (str.size > 0) {
				memcpy(dst, str.data, str.size);
				// if (i != data.size - 1) {
				// 	dst[str.size] = delim;
				// }
				dst += str.size + 1;
			}
		}
		return ret;
	}
	StrView join(Arena &a, StrView insert) {
		auto delim_count = (data.size - 1);
		auto result_lenght = total_lenght + delim_count * insert.size;
		auto dst = a.pushN<char>(result_lenght);
		StrView ret = {dst, result_lenght};

		for (Size i{0}; i < data.size; ++i) {
			if (i) {
				memcpy(dst - data.size, insert.data, insert.size);
			}
			auto &str = data[i];
			if (str.size > 0) {
				memcpy(dst, str.data, str.size);
				// if (i != data.size - 1) {
				// 	dst[str.size] = delim;
				// }
				dst += str.size + data.size;
			}
		}
		return ret;
	}
};
