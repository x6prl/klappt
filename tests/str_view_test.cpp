#include <cassert>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string_view>

#include "base/str_builder.h"
#include "base/str_view.h"

static void expect(StrView view, std::string_view expected) {
	if (view.size != static_cast<Size>(expected.size()) ||
	    (expected.size() > 0 &&
	     std::memcmp(view.data, expected.data(), expected.size()) != 0)) {
		std::fprintf(stderr,
		             "expect failed: got '%.*s' (size %d), expected '%.*s' "
		             "(size %d)\n",
		             (int)view.size, view.data ? view.data : "", (int)view.size,
		             (int)expected.size(), expected.data(),
		             (int)expected.size());
	}
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
	assert(mutable_view != "hell");
	assert(mutable_view != "hello world");
	assert(!(mutable_view != "hello"));

	assert(mutable_view == "hello"_v);
	assert(mutable_view != "world"_v);
	assert(!(mutable_view != "hello"_v));

	// Mutable indexing
	char buffer[] = "test";
	StrView mutable_buf{buffer, 4};
	mutable_buf[0] = 'b';
	assert(mutable_buf == "best"_v);

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

static void test_trim_by_and_chop() {
	auto left = StrView::lit("  hello");
	expect(left.mut_triml_by(&std::isspace), "hello");

	auto right = StrView::lit("hello  ");
	expect(right.mut_trimr_by(&std::isspace), "hello");

	auto both = StrView::lit(" \t hello \n");
	expect(both.mut_trim_by(&std::isspace), "hello");

	auto word = StrView::lit("abcdef");
	expect(word.mut_chopl(), "bcdef");
	expect(word.mut_chopr(), "bcde");

	word.mut_chopl();
	word.mut_chopl();
	word.mut_chopl();
	word.mut_chopr();
	assert(word.size == 0);
	assert(!word);
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

static void test_split_non_mutating() {
	auto csv = StrView::lit("alpha,beta,gamma");
	auto p1 = csv.split_by(',');
	expect(p1.first, "alpha");
	expect(p1.second, "beta,gamma");

	auto p2 = p1.second.split_by(',');
	expect(p2.first, "beta");
	expect(p2.second, "gamma");

	auto p3 = p2.second.split_by(',');
	expect(p3.first, "gamma");
	assert(!p3.second);
	assert(p3.second.size == 0);

	auto words = StrView::lit("eins zwei");
	auto sp = words.split();
	expect(sp.first, "eins");
	expect(sp.second, "zwei");

	auto lines = StrView::lit("row1\nrow2");
	auto sp_pred = lines.split_by(&std::isspace);
	expect(sp_pred.first, "row1");
	expect(sp_pred.second, "row2");

	auto single = StrView::lit("token");
	auto sp_single = single.split_by(&std::isspace);
	expect(sp_single.first, "token");
	assert(!sp_single.second);
}

static void test_split_all_by() {
	Arena arena;
	auto csv = StrView::lit("alpha,beta,gamma");
	auto parts = csv.split_all_by(arena, ',');
	assert(parts.size == 3);
	expect(parts[0], "alpha");
	expect(parts[1], "beta");
	expect(parts[2], "gamma");

	auto single = StrView::lit("single").split_all_by(arena, ',');
	assert(single.size == 1);
	expect(single[0], "single");
}

static void test_search_and_predicates() {
	auto text = StrView::lit("Hello, World!");

	// Char tests
	assert(text.is_starts_with('H'));
	assert(!text.is_starts_with('h'));
	assert(text.is_ends_with('!'));
	assert(!text.is_ends_with('?'));
	assert(text.is_contains('W'));
	assert(!text.is_contains('x'));

	// Substring tests
	assert(text.is_starts_with("Hello"_v));
	assert(!text.is_starts_with("World"_v));
	assert(text.is_ends_with("World!"_v));
	assert(!text.is_ends_with("Hello"_v));
	assert(text.is_contains_substr("lo, W"_v));
	assert(!text.is_contains_substr("Earth"_v));

	// Empty substring edge cases
	assert(text.is_starts_with(""_v));
	assert(text.is_ends_with(""_v));
	assert(text.is_contains_substr(""_v));

	// find()
	const char *found = text.find('W');
	assert(found != nullptr);
	assert(*found == 'W');
	assert(found == text.data + 7);
	assert(text.find('z') == nullptr);
}

static void test_punctuation() {
	Arena a;

	auto plain = StrView::lit("HelloWorld123");
	assert(!plain.is_contains_punctuation());
	assert(!plain.is_contains_punctuation_unicode());
	assert(plain.utf8_remove_punctuation(a) == "HelloWorld123"_v);

	auto ascii_punct = StrView::lit("Hello, World!");
	assert(ascii_punct.is_contains_punctuation());
	assert(ascii_punct.is_contains_punctuation_unicode());

	auto removed_ascii = ascii_punct.utf8_remove_punctuation(a);
	assert(!removed_ascii.is_contains_punctuation());
	assert(removed_ascii == "Hello World"_v);

	auto german_punct = StrView::lit("Prät.");
	assert(german_punct.is_contains_punctuation());
	assert(german_punct.utf8_remove_punctuation(a) == "Prät"_v);

	assert(""_v.utf8_remove_punctuation(a) == ""_v);
}

static void test_conversions() {
	Arena a;
	auto sv = StrView::lit("clay_and_cstr");

	const char *cstr = sv.to_cstr(a);
	assert(cstr != sv.data);
	assert(std::strcmp(cstr, "clay_and_cstr") == 0);
	assert(cstr[sv.size] == '\0');

	Clay_String clay = sv.to_clay_string();
	assert(clay.length == static_cast<int32_t>(sv.size));
	assert(std::memcmp(clay.chars, sv.data, sv.size) == 0);
}

static void test_from_numbers() {
	Arena arena;

	// Integer formatting
	assert(StrView::from_number(
				 arena, static_cast<uint64_t>(18446744073709551615ULL)) ==
	       "18446744073709551615"_v);
	assert(StrView::from_number(arena,
	                            static_cast<int64_t>(-9223372036854775807LL)) ==
	       "-9223372036854775807"_v);
	assert(StrView::from_number(arena, static_cast<int32_t>(-42)) == "-42"_v);
	assert(StrView::from_number(arena, static_cast<uint32_t>(42)) == "42"_v);
	assert(StrView::from_number(arena, static_cast<int32_t>(0)) == "0"_v);

	// Float formatting with explicit precision
	assert(StrView::from_number(arena, 3.14159f, 2) == "3.14"_v);
	assert(StrView::from_number(arena, 2.71828, 3) == "2.718"_v);

	// Float formatting with default precision (2 decimal places, rounded)
	assert(StrView::from_number(arena, 1.25f) == "1.25"_v);
	assert(StrView::from_number(arena, 9.875) == "9.88"_v);

	// Hexadecimal
	assert(StrView::from_number_hex(arena, 0x1a) == "000000000000001a"_v);
}

static void test_iterators() {
	auto sv = StrView::lit("12345");
	assert(sv.begin() == sv.data);
	assert(sv.end() == sv.data + sv.size);

	char expected = '1';
	for (char ch : sv) {
		assert(ch == expected);
		expected++;
	}
	assert(expected == '6');
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
	test_trim_by_and_chop();
	test_split_by_char();
	test_split_by_predicate();
	test_split_non_mutating();
	test_split_all_by();
	test_search_and_predicates();
	test_punctuation();
	test_conversions();
	test_from_numbers();
	test_iterators();
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
