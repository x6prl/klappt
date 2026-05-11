#include <cassert>
#include <cctype>
#include <cstring>
#include <string_view>

#include "base/str_view.h"

static void expect(StrView view, std::string_view expected) {
	assert(view.size == static_cast<Size>(expected.size()));
	assert(std::memcmp(view.data, expected.data(), expected.size()) == 0);
}

static void test_literal_and_indexing() {
	constexpr auto lit = StrView::lit("hello");
	static_assert(lit.size == 5);
	static_assert(lit.data[0] == 'h');

	constexpr auto udl = "hello"_v;
	static_assert(udl.size == 5);
	static_assert(udl.data[1] == 'e');

	constexpr auto prefix = "hello"_v;
	static_assert(prefix.size == 5);
	static_assert(prefix.data[4] == 'o');

	StrView mutable_view = lit;
	assert(mutable_view);
	assert(mutable_view.first() == 'h');
	assert(mutable_view.last() == 'o');
	assert(mutable_view[1] == 'e');
	assert(mutable_view == "hello");
	assert(!(mutable_view == "hell"));
	assert(!(mutable_view == "hello world"));
	expect(udl, "hello");
	expect(prefix, "hello");
}

static void test_trim_variants() {
	auto left = StrView::lit("  hello");
	expect(left.mut_triml(), "hello");

	auto right = StrView::lit("hello  ");
	expect(right.mut_trimr(), "hello");

	auto both = StrView::lit(" \t hello \n");
	expect(both.mut_trim(), "hello");

	auto only_space = StrView::lit(" \n\t ");
	assert(!only_space.mut_trim());
	assert(only_space.size == 0);

	auto unchanged = StrView::lit("  hello  ");
	expect(unchanged.triml(), "hello  ");
	expect(unchanged.trimr(), "  hello");
	expect(unchanged.trim(), "hello");
	expect(unchanged, "  hello  ");
}

static void test_split_by_char() {
	auto csv = StrView::lit("alpha,beta,gamma");
	auto head = csv.mut_split_by(',');
	expect(head, "alpha");
	expect(csv, "beta,gamma");

	auto tail = csv.mut_split_by(',');
	expect(tail, "beta");
	expect(csv, "gamma");

	auto missing = csv.mut_split_by(',');
	expect(missing, "gamma");
	assert(!csv);
}

static void test_split_by_predicate() {
	auto words = StrView::lit("eins zwei");
	auto first = words.mut_split();
	expect(first, "eins");
	expect(words, "zwei");

	auto lines = StrView::lit("row1\nrow2");
	auto line = lines.mut_split_by(&std::isspace);
	expect(line, "row1");
	expect(lines, "row2");

	auto plain = StrView::lit("token");
	auto missing = plain.mut_split_by(&std::isspace);
	expect(missing, "token");
	assert(!plain);
}

static void test_copy_and_concat() {
	Arena arena;
	auto left = StrView::lit("guten");
	auto right = StrView::lit("tag");

	auto copy = left.copy(arena);
	expect(copy, "guten");
	assert(copy.data != left.data);

	auto joined = StrView::concat(arena, left, right);
	expect(joined, "gutentag");

	auto delimited = StrView::concat_with(arena, left, right, ' ');
	expect(delimited, "guten tag");
}

static void test_slice_and_from_chars() {
	auto word = StrView::lit("prefix");
	expect(word.slice(3), "fix");
	expect(word.slice(1, 4), "ref");
	expect(word.slice(4, 2), "");

	Arena arena;
	auto created = StrView::from_chars(arena, "hallo");
	expect(created, "hallo");
	assert(created.data[created.size] == '\0');

	const char raw[] = {'a', 'b', 'c', 'd', 'e', 'f'};
	auto partial = StrView::from_chars(arena, raw, 3);
	expect(partial, "abc");
	assert(partial.data[partial.size] == '\0');
}

int main() {
	test_literal_and_indexing();
	test_trim_variants();
	test_split_by_char();
	test_split_by_predicate();
	test_copy_and_concat();
	test_slice_and_from_chars();
}
