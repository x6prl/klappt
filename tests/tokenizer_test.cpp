#include <cassert>
#include <cstring>
#include <initializer_list>
#include <string_view>

#include "domain/tokenizer.h"

// TODO: rewrite
static void expect(StrView actual, std::string_view expected) {
	assert(actual.size == static_cast<Size>(expected.size()));
	assert(std::memcmp(actual.data, expected.data(), expected.size()) == 0);
}

static void expect_chunks(const DynArr<StrView> &chunks,
                          std::initializer_list<std::string_view> expected) {
	assert(chunks.size == static_cast<Size>(expected.size()));
	Size i = 0;
	for (const auto &part : expected) {
		expect(chunks[i++], part);
	}
}

static bool starts_at_utf8_boundary(StrView part) {
	return part.size == 0 ||
	       (static_cast<unsigned char>(part.data[0]) & 0xC0u) != 0x80u;
}

static bool is_utf8_continuation(unsigned char ch) {
	return (ch & 0xC0u) == 0x80u;
}

static Size utf8_codepoint_bytes(unsigned char lead) {
	if ((lead & 0x80u) == 0u) {
		return 1;
	}
	if ((lead & 0xE0u) == 0xC0u) {
		return 2;
	}
	if ((lead & 0xF0u) == 0xE0u) {
		return 3;
	}
	if ((lead & 0xF8u) == 0xF0u) {
		return 4;
	}
	return 1;
}

static bool is_valid_utf8(StrView part) {
	for (Size i = 0; i < part.size;) {
		const auto lead = static_cast<unsigned char>(part.data[i]);
		const Size count = utf8_codepoint_bytes(lead);
		if (count <= 0 || i + count > part.size) {
			return false;
		}
		for (Size j = 1; j < count; ++j) {
			if (!is_utf8_continuation(
			          static_cast<unsigned char>(part.data[i + j]))) {
				return false;
			}
		}
		i += count;
	}
	return true;
}

static bool starts_with_capital_ascii(StrView part) {
	return part.size > 0 && part.data[0] >= 'A' && part.data[0] <= 'Z';
}

static bool all_items_have_at_least_n_codepoints(const DynArr<StrView> &items,
                                                 Size min_codepoints) {
	for (const auto &item : items) {
		Size count = 0;
		for (Size i = 0; i < item.size;) {
			const auto lead = static_cast<unsigned char>(item.data[i]);
			i += utf8_codepoint_bytes(lead);
			++count;
		}
		if (count < min_codepoints) {
			return false;
		}
	}
	return true;
}

static void test_noun_tokenizer() {
	Arena arena;
	expect_chunks(
	      Tokenizer::to_chunks(arena, "Arbeitsblatt"_v, Tokenizer::Kind::Noun),
	      {"A", "r", "b", "ei", "t", "s", "b", "l", "a", "tt"});

	Arena arena2;
	expect_chunks(
	      Tokenizer::to_chunks(arena2, "Nachbarin"_v, Tokenizer::Kind::Noun),
	      {"N", "a", "ch", "b", "a", "r", "in"});

	Arena arena3;
	expect_chunks(Tokenizer::to_chunks(arena3, "Freundlichkeit"_v,
	                                   Tokenizer::Kind::Noun),
	              {"F", "r", "eu", "n", "d", "lich", "keit"});

	Arena arena4;
	expect_chunks(Tokenizer::to_chunks(arena4, "Postleitzahl"_v,
	                                   Tokenizer::Kind::Noun),
	              {"P", "o", "st", "l", "ei", "tz", "ah", "l"});

	Arena arena5;
	expect_chunks(Tokenizer::to_chunks(arena5, "Nachbar"_v,
	                                   Tokenizer::Kind::Noun),
	              {"N", "a", "ch", "b", "a", "r"});

	Arena arena6;
	expect_chunks(Tokenizer::to_chunks(arena6, "Buch"_v,
	                                   Tokenizer::Kind::Noun),
	              {"B", "u", "ch"});

	Arena arena7;
	expect_chunks(Tokenizer::to_chunks(arena7, "Freund"_v,
	                                   Tokenizer::Kind::Noun),
	              {"F", "r", "eu", "n", "d"});

	Arena arena8;
	expect_chunks(Tokenizer::to_chunks(arena8, "Ehemann"_v,
	                                   Tokenizer::Kind::Noun),
	              {"Eh", "e", "m", "a", "n", "n"});

	Arena arena9;
	expect_chunks(
	      Tokenizer::to_chunks(arena9, "Mai"_v, Tokenizer::Kind::Noun),
	      {"M", "ai"});

	Arena arena10;
	expect_chunks(
	      Tokenizer::to_chunks(arena10, "Boot"_v, Tokenizer::Kind::Noun),
	      {"B", "oo", "t"});

	Arena arena11;
	expect_chunks(
	      Tokenizer::to_chunks(arena11, "Bank"_v, Tokenizer::Kind::Noun),
	      {"B", "a", "nk"});

	Arena arena12;
	expect_chunks(
	      Tokenizer::to_chunks(arena12, "Psalm"_v, Tokenizer::Kind::Noun),
	      {"Ps", "a", "l", "m"});

	Arena arena13;
	expect_chunks(
	      Tokenizer::to_chunks(arena13, "Bäckerei"_v, Tokenizer::Kind::Noun),
	      {"B", "ä", "ck", "er", "ei"});

	Arena arena14;
	expect_chunks(
	      Tokenizer::to_chunks(arena14, "Stadt"_v, Tokenizer::Kind::Noun),
	      {"St", "a", "dt"});

	Arena arena15;
	expect_chunks(
	      Tokenizer::to_chunks(arena15, "Ruine"_v, Tokenizer::Kind::Noun),
	      {"R", "ui", "n", "e"});

	Arena arena16;
	expect_chunks(
	      Tokenizer::to_chunks(arena16, "Finanzierung"_v,
	                           Tokenizer::Kind::Noun),
	      {"F", "i", "n", "anz", "ierung"});

	Arena arena17;
	expect_chunks(
	      Tokenizer::to_chunks(arena17, "Universität"_v,
	                           Tokenizer::Kind::Noun),
	      {"U", "n", "i", "v", "e", "r", "s", "ität"});

	Arena arena18;
	expect_chunks(
	      Tokenizer::to_chunks(arena18, "Professor"_v, Tokenizer::Kind::Noun),
	      {"P", "r", "o", "f", "e", "s", "s", "or"});

	Arena arena19;
	expect_chunks(
	      Tokenizer::to_chunks(arena19, "Garage"_v, Tokenizer::Kind::Noun),
	      {"G", "a", "r", "age"});

	Arena arena20;
	expect_chunks(
	      Tokenizer::to_chunks(arena20, "Mission"_v, Tokenizer::Kind::Noun),
	      {"M", "i", "s", "sion"});

	Arena arena21;
	expect_chunks(
	      Tokenizer::to_chunks(arena21, "Konsequenz"_v, Tokenizer::Kind::Noun),
	      {"Kon", "se", "qu", "enz"});

	Arena arena22;
	expect_chunks(
	      Tokenizer::to_chunks(arena22, "Tendenz"_v, Tokenizer::Kind::Noun),
	      {"Tend", "enz"});
}

static void test_verb_tokenizer() {
	Arena arena;
	expect_chunks(Tokenizer::to_chunks(arena, "verstehen"_v,
	                                   Tokenizer::Kind::Verb),
	              {"ver", "st", "eh", "en"});

	Arena arena2;
	expect_chunks(Tokenizer::to_chunks(arena2, "zurückkommen"_v,
	                                   Tokenizer::Kind::Verb),
	              {"zurück", "k", "o", "mm", "en"});

	Arena arena3;
	expect_chunks(Tokenizer::to_chunks(arena3, "arbeiten"_v),
	              {"a", "r", "b", "ei", "t", "en"});

	Arena arena4;
	expect_chunks(
	      Tokenizer::to_chunks(arena4, "gemacht"_v, Tokenizer::Kind::Verb),
	      {"ge", "m", "a", "ch", "t"});

	Arena arena5;
	expect_chunks(
	      Tokenizer::to_chunks(arena5, "studieren"_v, Tokenizer::Kind::Verb),
	      {"st", "u", "d", "ieren"});

	Arena arena6;
	expect_chunks(
	      Tokenizer::to_chunks(arena6, "studiert"_v, Tokenizer::Kind::Verb),
	      {"st", "u", "d", "iert"});

	Arena arena7;
	expect_chunks(
	      Tokenizer::to_chunks(arena7, "durchsuchen"_v,
	                           Tokenizer::Kind::Verb),
	      {"durch", "s", "u", "ch", "en"});

	Arena arena8;
	expect_chunks(
	      Tokenizer::to_chunks(arena8, "weitergehen"_v,
	                           Tokenizer::Kind::Verb),
	      {"weiter", "g", "eh", "en"});

	Arena arena9;
	expect_chunks(
	      Tokenizer::to_chunks(arena9, "herausfinden"_v,
	                           Tokenizer::Kind::Verb),
	      {"heraus", "f", "i", "n", "d", "en"});

	Arena arena10;
	expect_chunks(
	      Tokenizer::to_chunks(arena10, "überarbeiten"_v,
	                           Tokenizer::Kind::Verb),
	      {"über", "a", "r", "b", "ei", "t", "en"});
}

static void test_adjective_tokenizer() {
	Arena arena;
	expect_chunks(Tokenizer::to_chunks(arena, "freundlichsten"_v,
	                                   Tokenizer::Kind::Adjective),
	              {"f", "r", "eu", "n", "d", "lich", "sten"});

	Arena arena2;
	expect_chunks(Tokenizer::to_chunks(arena2, "glücklicheren"_v,
	                                   Tokenizer::Kind::Adjective),
	              {"g", "l", "ü", "ck", "lich", "eren"});

	Arena arena3;
	expect_chunks(Tokenizer::to_chunks(arena3, "unabhängig"_v,
	                                   Tokenizer::Kind::Adjective),
	              {"un", "a", "b", "h", "ä", "ng", "ig"});

	Arena arena4;
	expect_chunks(
	      Tokenizer::to_chunks(arena4, "hilfreich"_v,
	                           Tokenizer::Kind::Adjective),
	      {"h", "i", "l", "f", "reich"});

	Arena arena5;
	expect_chunks(
	      Tokenizer::to_chunks(arena5, "kostenfrei"_v,
	                           Tokenizer::Kind::Adjective),
	      {"k", "o", "st", "e", "n", "frei"});

	Arena arena6;
	expect_chunks(
	      Tokenizer::to_chunks(arena6, "dauerhaft"_v,
	                           Tokenizer::Kind::Adjective),
	      {"d", "au", "e", "r", "haft"});
}

static void test_utf8_boundaries() {
	Arena arena;
	auto chunks = Tokenizer::to_chunks(arena, "glücklicheren"_v,
	                                   Tokenizer::Kind::Adjective);
	for (const auto &chunk : chunks) {
		assert(starts_at_utf8_boundary(chunk));
		assert(is_valid_utf8(chunk));
	}
}

static void test_distractors() {
	auto distractors = Tokenizer::get_4_distractors_for_a_chunk("lich"_v);
	assert(distractors.size == 4);
	for (const auto &item : distractors) {
		assert(item != "lich"_v);
	}

	auto small = Tokenizer::get_4_distractors_for_a_chunk("e"_v);
	assert(small.size == 4);
	for (const auto &item : small) {
		assert(item.size <= 2);
		assert(item != "e"_v);
	}
	for (Size i = 0; i < small.size; ++i) {
		for (Size j = i + 1; j < small.size; ++j) {
			assert(small[i] != small[j]);
		}
	}

	auto other_small = Tokenizer::get_4_distractors_for_a_chunk("k"_v);
	assert(other_small.size == 4);
	for (const auto &item : other_small) {
		assert(item.size <= 2);
		assert(item != "k"_v);
	}
	for (Size i = 0; i < other_small.size; ++i) {
		for (Size j = i + 1; j < other_small.size; ++j) {
			assert(other_small[i] != other_small[j]);
		}
	}

	auto atom = Tokenizer::get_4_distractors_for_a_chunk("eh"_v);
	assert(atom.size == 4);
	assert(all_items_have_at_least_n_codepoints(atom, 2));
	for (const auto &item : atom) {
		assert(item != "eh"_v);
	}

	auto other_atom = Tokenizer::get_4_distractors_for_a_chunk("au"_v);
	assert(other_atom.size == 4);
	assert(all_items_have_at_least_n_codepoints(other_atom, 2));
	for (const auto &item : other_atom) {
		assert(item != "au"_v);
	}

	auto prefix = Tokenizer::get_4_distractors_for_a_chunk("ab"_v);
	assert(prefix.size == 4);
	for (const auto &item : prefix) {
		assert(item != "ab"_v);
		assert(item.size >= 2);
	}

	auto capitalized = Tokenizer::get_4_distractors_for_a_chunk("Freund"_v);
	assert(capitalized.size == 4);
	for (const auto &item : capitalized) {
		assert(starts_with_capital_ascii(item));
	}

	StrView seeded[4][4]{};
	for (Size i = 0; i < 4; ++i) {
		const auto sample =
		      Tokenizer::get_4_distractors_for_a_chunk("lich"_v, i + 1);
		for (Size j = 0; j < 4; ++j) {
			seeded[i][j] = sample[j];
		}
	}
	bool any_diff = false;
	for (Size i = 0; i < 4 && !any_diff; ++i) {
		for (Size j = i + 1; j < 4 && !any_diff; ++j) {
			for (Size k = 0; k < 4; ++k) {
				if (seeded[i][k] != seeded[j][k]) {
					any_diff = true;
					break;
				}
			}
		}
	}
	assert(any_diff);
}

static void test_guess_kind() {
	assert(Tokenizer::guess_kind("Haus"_v) == Tokenizer::Kind::Noun);
	assert(Tokenizer::guess_kind("Konsequenz"_v) == Tokenizer::Kind::Noun);
	assert(Tokenizer::guess_kind("verstehen"_v) == Tokenizer::Kind::Verb);
	assert(Tokenizer::guess_kind("zurückkommen"_v) == Tokenizer::Kind::Verb);
	assert(Tokenizer::guess_kind("freundlich"_v) == Tokenizer::Kind::Adjective);
	assert(Tokenizer::guess_kind("unabhängig"_v) ==
	       Tokenizer::Kind::Adjective);
	assert(Tokenizer::guess_kind("\"Nachbarin,\""_v) ==
	       Tokenizer::Kind::Noun);
}

int main() {
	test_noun_tokenizer();
	test_verb_tokenizer();
	test_adjective_tokenizer();
	test_utf8_boundaries();
	test_distractors();
	test_guess_kind();
}
