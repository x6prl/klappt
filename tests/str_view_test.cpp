#include <cassert>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <string_view>

#include "SDL3/SDL_log.h"
#include "base/str_builder.h"
#include "base/str_view.h"

static void expect(StrView view, std::string_view expected) {
	assert(view.size == static_cast<Size>(expected.size()));
	assert(std::memcmp(view.data, expected.data(), expected.size()) == 0);
}

static void test_literal_and_indexing() {
	constexpr auto udl = "hello"_v;
	static_assert(udl.size == 5);
	static_assert(udl.data[1] == 'e');

	constexpr auto prefix = "hello"_v;
	static_assert(prefix.size == 5);
	static_assert(prefix.data[4] == 'o');

	auto lit = StrView::lit("hello");
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

static void test_utf8_length() {
	assert(""_v.utf8_length() == 0);
	assert("abc"_v.utf8_length() == 3);
	assert("ä"_v.utf8_length() == 1);
	assert("Prät."_v.utf8_length() == 5);
	assert("прыгать"_v.utf8_length() == 7);
}

static void test_utf8_to_lowercase() {
	Arena a;

	assert(""_v.utf8_to_lowercase(a) == ""_v);

	// ASCII
	assert("ABC"_v.utf8_to_lowercase(a) == "abc"_v);
	assert("Hello, World!"_v.utf8_to_lowercase(a) == "hello, world!"_v);
	assert("123 ABC xyz"_v.utf8_to_lowercase(a) == "123 abc xyz"_v);

	// German
	assert("Ä"_v.utf8_to_lowercase(a) == "ä"_v);
	assert("Ö"_v.utf8_to_lowercase(a) == "ö"_v);
	assert("Ü"_v.utf8_to_lowercase(a) == "ü"_v);
	assert("ÄÖÜ"_v.utf8_to_lowercase(a) == "äöü"_v);

	assert("Straße"_v.utf8_to_lowercase(a) == "straße"_v);
	assert("FÜR ÄPFEL"_v.utf8_to_lowercase(a) == "für äpfel"_v);
	assert("PrÄT"_v.utf8_to_lowercase(a) == "prät"_v);

	// Russian
	assert("АБВ"_v.utf8_to_lowercase(a) == "абв"_v);
	assert("ПРИВЕТ"_v.utf8_to_lowercase(a) == "привет"_v);
	assert("ПрИвЕт"_v.utf8_to_lowercase(a) == "привет"_v);
	assert("Ёж"_v.utf8_to_lowercase(a) == "ёж"_v);

	// Turkish
	assert("Ğ"_v.utf8_to_lowercase(a) == "ğ"_v);
	assert("Ş"_v.utf8_to_lowercase(a) == "ş"_v);
	assert("İ"_v.utf8_to_lowercase(a) == "i"_v);

	// Mixed
	assert("ÄБĞ"_v.utf8_to_lowercase(a) == "äбğ"_v);
	assert("Hello ПРИВЕТ ÄÖÜ"_v.utf8_to_lowercase(a) == "hello привет äöü"_v);

	// Already lowercase
	assert("äöü"_v.utf8_to_lowercase(a) == "äöü"_v);
	assert("привет"_v.utf8_to_lowercase(a) == "привет"_v);
	assert("ğşi"_v.utf8_to_lowercase(a) == "ğşi"_v);

	// Unsupported characters stay unchanged
	assert("€"_v.utf8_to_lowercase(a) == "€"_v);
	assert("😀"_v.utf8_to_lowercase(a) == "😀"_v);
}

static void test_utf8_to_lowercase_german() {
	Arena a;

	assert(""_v.utf8_to_lowercase_german(a) == ""_v);

	assert("ABC"_v.utf8_to_lowercase_german(a) == "abc"_v);

	assert("Ä"_v.utf8_to_lowercase_german(a) == "ä"_v);
	assert("Ö"_v.utf8_to_lowercase_german(a) == "ö"_v);
	assert("Ü"_v.utf8_to_lowercase_german(a) == "ü"_v);

	assert("FÜR ÄPFEL"_v.utf8_to_lowercase_german(a) == "für äpfel"_v);

	// ß should stay unchanged
	assert("Straße"_v.utf8_to_lowercase_german(a) == "straße"_v);

	// Non-German UTF-8 should not change
	assert("ПРИВЕТ"_v.utf8_to_lowercase_german(a) == "ПРИВЕТ"_v);
	assert("ĞŞİ"_v.utf8_to_lowercase_german(a) == "ĞŞİ"_v);
}

static void test_str_builder_join() {
	Arena a;
	StrBuilder b;

	assert(b.join(a) == ""_v);

	b.push(a, "abc"_v);
	assert(b.join(a) == "abc"_v);

	b.push(a, "def"_v);
	assert(b.join(a) == "abcdef"_v);

	b.push(a, ""_v);
	assert(b.join(a) == "abcdef"_v);
}

static void test_str_builder_join_char() {
	Arena a;
	StrBuilder b;

	assert(b.join(a, ',') == ""_v);

	b.push(a, "a"_v);
	assert(b.join(a, ',') == "a"_v);

	b.push(a, "b"_v);
	assert(b.join(a, ',') == "a,b"_v);

	b.push(a, "ccc"_v);
	assert(b.join(a, ',') == "a,b,ccc"_v);

	b.push(a, ""_v);
	assert(b.join(a, ',') == "a,b,ccc,"_v);
}

static void test_str_builder_join_str() {
	Arena a;
	StrBuilder b;

	assert(b.join(a, "--"_v) == ""_v);

	b.push(a, "a"_v);
	assert(b.join(a, "--"_v) == "a"_v);

	b.push(a, "bb"_v);
	assert(b.join(a, "--"_v) == "a--bb"_v);

	b.push(a, "ccc"_v);
	assert(b.join(a, "--"_v) == "a--bb--ccc"_v);

	b.push(a, ""_v);
	assert(b.join(a, "--"_v) == "a--bb--ccc--"_v);
}

static void test_str_builder_numbers() {
	Arena a;
	StrBuilder b;

	b.push(a, 123);
	b.push(a, " "_v);
	b.push(a, -45);

	assert(b.join(a) == "123 -45"_v);
}

static void test_str_builder_append() {
	Arena a;

	StrBuilder a1;
	a1.push(a, "abc"_v);
	a1.push(a, "def"_v);

	StrBuilder a2;
	a2.push(a, "123"_v);
	a2.push(a, "456"_v);

	a1.append(a, a2);

	assert(a1.join(a) == "abcdef123456"_v);
}

static void test_str_builder_formatted_bytes() {
	Arena a;

	{
		StrBuilder b;
		b.push_formatted_bytes(a, 0);
		assert(b.join(a) == "0B"_v);
	}

	{
		StrBuilder b;
		b.push_formatted_bytes(a, 1);
		assert(b.join(a) == "1B"_v);
	}

	{
		StrBuilder b;
		b.push_formatted_bytes(a, 1023);
		assert(b.join(a) == "1023B"_v);
	}

	{
		StrBuilder b;
		b.push_formatted_bytes(a, 1024);
		assert(b.join(a) == "1.0KB"_v);
	}

	{
		StrBuilder b;
		b.push_formatted_bytes(a, 1536);
		assert(b.join(a) == "1.5KB"_v);
	}

	{
		StrBuilder b;
		b.push_formatted_bytes(a, 1024 * 1024);
		assert(b.join(a) == "1.0MB"_v);
	}

	{
		StrBuilder b;
		b.push_formatted_bytes(a, 5 * 1024 * 1024);
		assert(b.join(a) == "5.0MB"_v);
	}
}

static void test_str_builder_empty_strings() {
	Arena a;
	StrBuilder b;

	b.push(a, ""_v);
	b.push(a, ""_v);
	b.push(a, ""_v);

	assert(b.join(a) == ""_v);
	assert(b.join(a, ',') == ",,"_v);
	assert(b.join(a, "--"_v) == "----"_v);
}

static void test_str_builder_total_length() {
	Arena a;
	StrBuilder b;

	assert(b.total_lenght == 0);

	b.push(a, "abc"_v);
	assert(b.total_lenght == 3);

	b.push(a, ""_v);
	assert(b.total_lenght == 3);

	b.push(a, "de"_v);
	assert(b.total_lenght == 5);
}

int main() {
	test_literal_and_indexing();
	test_trim_variants();
	test_split_by_char();
	test_split_by_predicate();
	test_copy_and_concat();
	test_slice_and_from_chars();
	test_utf8_length();
	test_utf8_to_lowercase();
	test_utf8_to_lowercase_german();
	test_str_builder_total_length();
	test_str_builder_append();
	test_str_builder_empty_strings();
	test_str_builder_formatted_bytes();
	test_str_builder_numbers();
	test_str_builder_join();
	test_str_builder_join_char();
	test_str_builder_join_str();
}
