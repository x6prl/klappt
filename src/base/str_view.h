#pragma once

#include "arena.h"
#include "base/dyn_arr.h"
#include "base/pair.h"
#include <clay/clay.h>

#include <cstddef>
#include <cstdint>
#include <cstring>

#define StrView_Fmt "%.*s"
#define StrView_Arg(str_view) (int)((str_view).size), ((str_view).data)

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

	[[nodiscard]]
	char first() const;
	[[nodiscard]]
	char last() const;

	Size utf8_length() const;
	[[nodiscard]]
	StrView utf8_to_lowercase_german(Arena &a) const;
	[[nodiscard]]
	StrView utf8_to_lowercase(Arena &a) const;
	[[nodiscard]]
	StrView utf8_remove_punctuation(Arena &a) const;

	[[nodiscard]]
	StrView copy(Arena &a) const;

	[[nodiscard]]
	static StrView concat(Arena &arena, const StrView left,
	                      const StrView right);
	[[nodiscard]]
	static StrView concat_with(Arena &arena, const StrView left,
	                           const StrView right, char delimiter);

	// Mutate this view by removing leading, trailing, or both-side
	// whitespace.
	StrView &mut_triml();
	StrView &mut_trimr();
	StrView &mut_trim();

	StrView &mut_triml_by(int (*handler)(int ch));
	StrView &mut_trimr_by(int (*handler)(int ch));
	StrView &mut_trim_by(int (*handler)(int ch));

	StrView &mut_chopl();
	StrView &mut_chopr();

	// Non-mutating trim variants. Return a trimmed copy of this view.
	[[nodiscard]]
	StrView triml() const;
	[[nodiscard]]
	StrView trimr() const;
	[[nodiscard]]
	StrView trim() const;

	// Return the head before the delimiter and advance this view to the
	// tail. If the delimiter is missing, return the whole view and clear
	// this view.
	StrView mut_split_by(char delimiter);
	StrView mut_split_by(int (*handler)(int ch));
	StrView mut_split();

	// Non-mutating split variants. Return {head, tail}, where tail is what
	// the corresponding mut_split* call would leave in this view.
	[[nodiscard]]
	Pair<StrView, StrView> split_by(char delimiter) const;
	[[nodiscard]]
	Pair<StrView, StrView> split_by(int (*handler)(int ch)) const;
	[[nodiscard]]
	Pair<StrView, StrView> split() const;

	[[nodiscard]]
	DynArr<StrView> split_all_by(Arena &a, char delimiter) const;

	[[nodiscard]]
	StrView slice(Size from = 0, Size to = -1) const;

	bool is_contains(char ch) const;
	bool is_contains_substr(StrView substr) const;
	bool is_contains_punctuation() const;
	bool is_contains_punctuation_unicode() const;
	bool is_starts_with(char ch) const;
	bool is_starts_with(StrView pref) const;
	const char *find(char ch) const;

	Clay_String to_clay_string() const;
	// allocates size+1
	const char *to_cstr(Arena &a) const;

	static StrView from_number(Arena &a, uint64_t val);
	static StrView from_number(Arena &a, uint32_t val);
	static StrView from_number(Arena &a, uint16_t val);
	static StrView from_number(Arena &a, int64_t val);
	static StrView from_number(Arena &a, int32_t val);
	static StrView from_number(Arena &a, int16_t val);
	static StrView from_number(Arena &a, float val);
	static StrView from_number(Arena &a, double val);
	static StrView from_number(Arena &a, float val, int precision);
	static StrView from_number(Arena &a, double val, int precision);

	static StrView from_number_hex(Arena &a, uint64_t val);

	static StrView from_chars(Arena &a, const char *data, int size);
	static StrView from_chars(Arena &a, const char *data);

	const char *begin() const;
	const char *end() const;
};

constexpr StrView operator""_v(const char *str, std::size_t size) noexcept {
	return {str, static_cast<Size>(size)};
}
