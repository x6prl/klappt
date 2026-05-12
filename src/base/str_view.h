#pragma once

#include "arena.h"
#include "base/pair.h"
#include <clay/clay.h>

#include <charconv>
#include <cstddef>
#include <cstring>

#define StrView_Fmt "%.*s"
#define StrView_Arg(str_view) (str_view).size, (str_view).data

struct StrView {
	const char *data{nullptr};
	Size size{0};

	template <size_t N> consteval static StrView lit(const char (&str)[N]) {
		return {str, static_cast<Size>(N - 1)};
	}
	static StrView lit(const char str[]) {
		return {str, static_cast<Size>(strlen(str))};
	}

	operator bool() const;
	char &operator[](Size i);
	const char &operator[](Size i) const;
	bool operator==(const char other[]) const;
	bool operator!=(const char other[]) const;
	bool operator==(const StrView &other) const;
	bool operator!=(const StrView &other) const;

	char first() const;
	char last() const;

	StrView copy(Arena &a) const;
	static StrView concat(Arena &arena, const StrView left,
	                      const StrView right);
	static StrView concat_with(Arena &arena, const StrView left,
	                           const StrView right, char delimiter);

	// Mutate this view by removing leading, trailing, or both-side whitespace.
	StrView &mut_triml();
	StrView &mut_trimr();
	StrView &mut_trim();

	// Non-mutating trim variants. Return a trimmed copy of this view.
	StrView triml() const;
	StrView trimr() const;
	StrView trim() const;

	// Return the head before the delimiter and advance this view to the tail.
	// If the delimiter is missing, return the whole view and clear this view.
	StrView mut_split_by(char delimiter);
	StrView mut_split_by(int (*handler)(int ch));
	StrView mut_split();

	// Non-mutating split variants. Return {head, tail}, where tail is what
	// the corresponding mut_split* call would leave in this view.
	Pair<StrView, StrView> split_by(char delimiter) const;
	Pair<StrView, StrView> split_by(int (*handler)(int ch)) const;
	Pair<StrView, StrView> split() const;

	StrView slice(Size from = 0, Size to = -1) const;

	bool is_contains(char ch) const;
	const char *find(char ch) const;

	Clay_String to_clay_string() const;

	static StrView from_integer(Arena &a, auto val) {
		constexpr Size BUF_SIZE = 16;
		auto strbuf = a.pushN<char>(BUF_SIZE);
		auto to_chars_res = std::to_chars(strbuf, strbuf + BUF_SIZE, val);
		return {strbuf, static_cast<Size>(to_chars_res.ptr - strbuf)};
	}
	static StrView from_chars(Arena &a, const char *data, int size);
	static StrView from_chars(Arena &a, const char *data);

	const char *begin() const;
	const char *end() const;
};

constexpr StrView operator""_v(const char *str, std::size_t size) noexcept {
	return {str, static_cast<Size>(size)};
}
