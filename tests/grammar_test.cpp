#include <cstdio>
#include <cstdlib>

#if __has_include("domain/grammar.h")
#include "domain/grammar.h"
#elif __has_include("grammar/grammar.h")
#include "grammar/grammar.h"
#else
#include "grammar.h"
#endif

#if __has_include("domain/word.h")
#include "domain/word.h"
#else
#include "word.h"
#endif

#include "base/arena.h"
#include "base/str_view.h"

// -----------------------------------------------------------------------------
// Minimal Test Harness
// -----------------------------------------------------------------------------

static int g_tests_run = 0;
static int g_tests_failed = 0;

#define TEST_CHECK(expr)                                                       \
	do {                                                                       \
		++g_tests_run;                                                         \
		if (!(expr)) {                                                         \
			++g_tests_failed;                                                  \
			std::fprintf(stderr, "[FAIL] %s:%d: Assertion failed: (%s)\n",     \
			             __FILE__, __LINE__, #expr);                           \
		}                                                                      \
	} while (0)

#define TEST_CHECK_STR_EQ(actual, expected)                                    \
	do {                                                                       \
		++g_tests_run;                                                         \
		StrView _act = (actual);                                               \
		StrView _exp = (expected);                                             \
		if (_act != _exp) {                                                    \
			++g_tests_failed;                                                  \
			std::fprintf(stderr,                                               \
			             "[FAIL] %s:%d: Expected \"" StrView_Fmt               \
			             "\", got \"" StrView_Fmt "\"\n",                      \
			             __FILE__, __LINE__, StrView_Arg(_exp),                \
			             StrView_Arg(_act));                                   \
		}                                                                      \
	} while (0)

// -----------------------------------------------------------------------------
// Noun Tests
// -----------------------------------------------------------------------------

static void test_noun_classification() {
	// Plural-only nouns (Pluraliatantum)
	{
		Noun eltern{.lemma = "Eltern"_v,
		            .plural_suffix = "(pl.)"_v,
		            .gender = Gender::none};

		TEST_CHECK(grammar::is_plural_only(eltern));
		TEST_CHECK(!grammar::is_singular_only(eltern));
	}

	// Singular-only nouns (Singulariatantum)
	{
		Noun milch{.lemma = "Milch"_v,
		           .plural_suffix = "(sg.)"_v,
		           .gender = Gender::f};

		TEST_CHECK(grammar::is_singular_only(milch));
		TEST_CHECK(!grammar::is_plural_only(milch));
	}

	// Regular countable nouns
	{
		Noun tisch{
			  .lemma = "Tisch"_v, .plural_suffix = "-e"_v, .gender = Gender::m};
		Noun katze{
			  .lemma = "Katze"_v, .plural_suffix = "-n"_v, .gender = Gender::f};
		Noun buch{
			  .lemma = "Buch"_v, .plural_suffix = "-er"_v, .gender = Gender::n};

		TEST_CHECK(!grammar::is_plural_only(tisch));
		TEST_CHECK(!grammar::is_singular_only(tisch));
		TEST_CHECK(!grammar::is_plural_only(katze));
		TEST_CHECK(!grammar::is_singular_only(katze));
		TEST_CHECK(!grammar::is_plural_only(buch));
		TEST_CHECK(!grammar::is_singular_only(buch));
	}
}

static void test_noun_singular_with_article(Arena &scratch) {
	// Masculine: der
	{
		Noun n{.lemma = "Hund"_v, .plural_suffix = "-e"_v, .gender = Gender::m};
		TEST_CHECK_STR_EQ(grammar::noun_singular_with_article(scratch, n),
		                  "der Hund"_v);
	}

	// Feminine: die
	{
		Noun n{
			  .lemma = "Katze"_v, .plural_suffix = "-n"_v, .gender = Gender::f};
		TEST_CHECK_STR_EQ(grammar::noun_singular_with_article(scratch, n),
		                  "die Katze"_v);
	}

	// Neuter: das
	{
		Noun n{
			  .lemma = "Haus"_v, .plural_suffix = "-er"_v, .gender = Gender::n};
		TEST_CHECK_STR_EQ(grammar::noun_singular_with_article(scratch, n),
		                  "das Haus"_v);
	}

	// Plural-only: no singular form
	{
		Noun n{.lemma = "Eltern"_v,
		       .plural_suffix = "(pl.)"_v,
		       .gender = Gender::none};
		TEST_CHECK(grammar::noun_singular_with_article(scratch, n).size == 0);
	}
}

static void test_noun_plural_forms(Arena &scratch) {
	// Suffix -e
	{
		Noun n{
			  .lemma = "Tisch"_v, .plural_suffix = "-e"_v, .gender = Gender::m};
		TEST_CHECK_STR_EQ(grammar::noun_plural_without_article(scratch, n),
		                  "Tische"_v);
		TEST_CHECK_STR_EQ(grammar::noun_plural_with_article(scratch, n),
		                  "die Tische"_v);
	}

	// Suffix -en
	{
		Noun n{
			  .lemma = "Frau"_v, .plural_suffix = "-en"_v, .gender = Gender::f};
		TEST_CHECK_STR_EQ(grammar::noun_plural_without_article(scratch, n),
		                  "Frauen"_v);
		TEST_CHECK_STR_EQ(grammar::noun_plural_with_article(scratch, n),
		                  "die Frauen"_v);
	}

	// Suffix -s
	{
		Noun n{.lemma = "Auto"_v, .plural_suffix = "-s"_v, .gender = Gender::n};
		TEST_CHECK_STR_EQ(grammar::noun_plural_without_article(scratch, n),
		                  "Autos"_v);
		TEST_CHECK_STR_EQ(grammar::noun_plural_with_article(scratch, n),
		                  "die Autos"_v);
	}

	// Plural only (lemma is already the plural form)
	{
		Noun n{.lemma = "Leute"_v,
		       .plural_suffix = "(pl.)"_v,
		       .gender = Gender::none};
		TEST_CHECK_STR_EQ(grammar::noun_plural_without_article(scratch, n),
		                  "Leute"_v);
		TEST_CHECK_STR_EQ(grammar::noun_plural_with_article(scratch, n),
		                  "die Leute"_v);
	}

	// Singular only (has no plural)
	{
		Noun n{.lemma = "Milch"_v,
		       .plural_suffix = "(sg.)"_v,
		       .gender = Gender::f};
		TEST_CHECK(grammar::noun_plural_without_article(scratch, n).size == 0);
		TEST_CHECK(grammar::noun_plural_with_article(scratch, n).size == 0);
	}
}

// -----------------------------------------------------------------------------
// Verb Tests
// -----------------------------------------------------------------------------

static void test_verb_separable_prefix() {
	// Separable: aufstehen (prefix: "auf", size 3)
	{
		Verb v{.infinitive = "aufstehen"_v,
		       .third_person = "steht auf"_v,
		       .praeteritum = "stand auf"_v,
		       .auxv_and_past_participle = "ist aufgestanden"_v,
		       .separable_prefix_size = 3};
		TEST_CHECK(grammar::verb_is_separable_prefix(v));
		TEST_CHECK_STR_EQ(grammar::verb_separable_prefix(v), "auf"_v);
	}

	// Separable: abfahren (prefix: "ab", size 2)
	{
		Verb v{.infinitive = "abfahren"_v,
		       .third_person = "fährt ab"_v,
		       .praeteritum = "fuhr ab"_v,
		       .auxv_and_past_participle = "ist abgefahren"_v,
		       .separable_prefix_size = 2};
		TEST_CHECK(grammar::verb_is_separable_prefix(v));
		TEST_CHECK_STR_EQ(grammar::verb_separable_prefix(v), "ab"_v);
	}

	// Inseparable / prefix size 0: machen
	{
		Verb v{.infinitive = "machen"_v,
		       .third_person = "macht"_v,
		       .praeteritum = "machte"_v,
		       .auxv_and_past_participle = "hat gemacht"_v,
		       .separable_prefix_size = 0};
		TEST_CHECK(!grammar::verb_is_separable_prefix(v));
		TEST_CHECK_STR_EQ(grammar::verb_separable_prefix(v), ""_v);
	}
}

static void test_verb_stem() {
	// Standard regular verb: machen -> mach
	{
		Verb v{.infinitive = "machen"_v, .separable_prefix_size = 0};
		TEST_CHECK_STR_EQ(grammar::verb_stem(v), "mach"_v);
	}

	// Regular verb with -d/-t root: arbeiten -> arbeit
	{
		Verb v{.infinitive = "arbeiten"_v, .separable_prefix_size = 0};
		TEST_CHECK_STR_EQ(grammar::verb_stem(v), "arbeit"_v);
	}

	// Verb with -ern: wandern -> wander
	{
		Verb v{.infinitive = "wandern"_v, .separable_prefix_size = 0};
		TEST_CHECK_STR_EQ(grammar::verb_stem(v), "wander"_v);
	}

	// Separable verb: aufmachen -> mach
	{
		Verb v{.infinitive = "aufmachen"_v, .separable_prefix_size = 3};
		TEST_CHECK_STR_EQ(grammar::verb_stem(v), "mach"_v);
	}
}

static void test_verb_auxiliary() {
	// Auxiliary "sein"
	{
		Verb gehen{.infinitive = "gehen"_v,
		           .auxv_and_past_participle = "ist gegangen"_v};
		Verb bleiben{.infinitive = "bleiben"_v,
		             .auxv_and_past_participle = "ist geblieben"_v};

		TEST_CHECK(grammar::is_aux_sein(gehen));
		TEST_CHECK(grammar::is_aux_sein(bleiben));
	}

	// Auxiliary "haben"
	{
		Verb machen{.infinitive = "machen"_v,
		            .auxv_and_past_participle = "hat gemacht"_v};
		Verb lernen{.infinitive = "lernen"_v,
		            .auxv_and_past_participle = "hat gelernt"_v};

		TEST_CHECK(!grammar::is_aux_sein(machen));
		TEST_CHECK(!grammar::is_aux_sein(lernen));
	}
}

static void test_verb_regularity() {
	// Regular (weak) verbs
	{
		// NOTE: we expect 3p, past and pp fields to be empty
		Verb machen{
			  .infinitive = "machen"_v,
			  // .third_person = "macht"_v,
		      // .praeteritum = "machte"_v,
			  .auxv_and_past_participle = "hat"_v
			  // .auxv_and_past_participle = "hat gemacht"_v
		};
		Verb lernen{
			  .infinitive = "lernen"_v,
			  // .third_person = "lernt"_v,
		      // .praeteritum = "lernte"_v,
			  .auxv_and_past_participle = "hat"_v
			  // .auxv_and_past_participle = "hat gelernt"_v
		};

		TEST_CHECK(grammar::is_regular(machen));
		TEST_CHECK(grammar::is_regular(lernen));
	}

	// Irregular (strong) verbs
	{
		Verb gehen{.infinitive = "gehen"_v,
		           .third_person = "geht"_v,
		           .praeteritum = "ging"_v,
		           .auxv_and_past_participle = "ist gegangen"_v};
		Verb sehen{.infinitive = "sehen"_v,
		           .third_person = "sieht"_v,
		           .praeteritum = "sah"_v,
		           .auxv_and_past_participle = "hat gesehen"_v};

		TEST_CHECK(!grammar::is_regular(gehen));
		TEST_CHECK(!grammar::is_regular(sehen));
	}
}

static void test_verb_tenses_and_forms(Arena &scratch) {
	{
		Verb v{.infinitive = "machen"_v,
		       .third_person = ""_v,
		       .praeteritum = ""_v,
		       .auxv_and_past_participle = "hat"_v,
		       .separable_prefix_size = 0};

		TEST_CHECK_STR_EQ(grammar::verb_past_participle(scratch, v),
		                  "gemacht"_v);
		TEST_CHECK_STR_EQ(grammar::verb_perfect_full(scratch, v),
		                  "hat gemacht"_v);
		TEST_CHECK_STR_EQ(grammar::verb_praeteritum_full(scratch, v),
		                  "machte"_v);
		TEST_CHECK_STR_EQ(grammar::verb_third_person_full(scratch, v),
		                  "macht"_v);
	}
	{
		Verb v{.infinitive = "betteln"_v,
		       .third_person = ""_v,
		       .praeteritum = ""_v,
		       .auxv_and_past_participle = "hat"_v,
		       .separable_prefix_size = 0};

		TEST_CHECK_STR_EQ(grammar::verb_past_participle(scratch, v),
		                  "gebettelt"_v);
		TEST_CHECK_STR_EQ(grammar::verb_perfect_full(scratch, v),
		                  "hat gebettelt"_v);
	}
	{
		Verb v{.infinitive = "ernten"_v,
		       .third_person = ""_v,
		       .praeteritum = ""_v,
		       .auxv_and_past_participle = "hat"_v,
		       .separable_prefix_size = 0};

		TEST_CHECK_STR_EQ(grammar::verb_past_participle(scratch, v),
		                  "geerntet"_v);
		TEST_CHECK_STR_EQ(grammar::verb_perfect_full(scratch, v),
		                  "hat geerntet"_v);
	}
	{
		Verb v{.infinitive = "bechern"_v,
		       .third_person = ""_v,
		       .praeteritum = ""_v,
		       .auxv_and_past_participle = "hat"_v,
		       .separable_prefix_size = 0};

		TEST_CHECK_STR_EQ(grammar::verb_past_participle(scratch, v),
		                  "gebechert"_v);
		TEST_CHECK_STR_EQ(grammar::verb_perfect_full(scratch, v),
		                  "hat gebechert"_v);
	}
	{
		Verb v{.infinitive = "beisln"_v,
		       .third_person = ""_v,
		       .praeteritum = ""_v,
		       .auxv_and_past_participle = "hat"_v,
		       .separable_prefix_size = 0};

		TEST_CHECK_STR_EQ(grammar::verb_past_participle(scratch, v),
		                  "gebeislt"_v);
		TEST_CHECK_STR_EQ(grammar::verb_perfect_full(scratch, v),
		                  "hat gebeislt"_v);
	}
	{
		Verb v{.infinitive = "beichten"_v,
		       .third_person = ""_v,
		       .praeteritum = ""_v,
		       .auxv_and_past_participle = "hat"_v,
		       .separable_prefix_size = 0};

		TEST_CHECK_STR_EQ(grammar::verb_past_participle(scratch, v),
		                  "gebeichtet"_v);
		TEST_CHECK_STR_EQ(grammar::verb_perfect_full(scratch, v),
		                  "hat gebeichtet"_v);
	}
	{
		Verb v{.infinitive = "geistern"_v,
		       .third_person = ""_v,
		       .praeteritum = ""_v,
		       .auxv_and_past_participle = "hat"_v,
		       .separable_prefix_size = 0};

		TEST_CHECK_STR_EQ(grammar::verb_past_participle(scratch, v),
		                  "gegeistert"_v);
		TEST_CHECK_STR_EQ(grammar::verb_perfect_full(scratch, v),
		                  "hat gegeistert"_v);
	}
	{
		Verb v{.infinitive = "entern"_v,
		       .third_person = ""_v,
		       .praeteritum = ""_v,
		       .auxv_and_past_participle = "hat"_v,
		       .separable_prefix_size = 0};

		TEST_CHECK_STR_EQ(grammar::verb_past_participle(scratch, v),
		                  "geentert"_v);
		TEST_CHECK_STR_EQ(grammar::verb_perfect_full(scratch, v),
		                  "hat geentert"_v);
	}
	{
		Verb v{.infinitive = "erörtern"_v,
		       .third_person = ""_v,
		       .praeteritum = ""_v,
		       .auxv_and_past_participle = "hat"_v,
		       .separable_prefix_size = 0};

		TEST_CHECK_STR_EQ(grammar::verb_past_participle(scratch, v),
		                  "erörtert"_v);
		TEST_CHECK_STR_EQ(grammar::verb_perfect_full(scratch, v),
		                  "hat erörtert"_v);
	}
	{
		Verb v{.infinitive = "entäußern"_v,
		       .third_person = ""_v,
		       .praeteritum = ""_v,
		       .auxv_and_past_participle = "hat"_v,
		       .separable_prefix_size = 0};

		TEST_CHECK_STR_EQ(grammar::verb_past_participle(scratch, v),
		                  "entäußert"_v);
		TEST_CHECK_STR_EQ(grammar::verb_perfect_full(scratch, v),
		                  "hat entäußert"_v);
	}
	{
		Verb v{.infinitive = "erben"_v,
		       .third_person = ""_v,
		       .praeteritum = ""_v,
		       .auxv_and_past_participle = "hat"_v,
		       .separable_prefix_size = 0};

		TEST_CHECK_STR_EQ(grammar::verb_past_participle(scratch, v),
		                  "geerbt"_v);
		TEST_CHECK_STR_EQ(grammar::verb_perfect_full(scratch, v),
		                  "hat geerbt"_v);
	}
	{
		Verb v{.infinitive = "beuteln"_v,
		       .third_person = ""_v,
		       .praeteritum = ""_v,
		       .auxv_and_past_participle = "hat"_v,
		       .separable_prefix_size = 0};

		TEST_CHECK_STR_EQ(grammar::verb_past_participle(scratch, v),
		                  "gebeutelt"_v);
		TEST_CHECK_STR_EQ(grammar::verb_perfect_full(scratch, v),
		                  "hat gebeutelt"_v);
	}
	{
		Verb v{.infinitive = "beiern"_v,
		       .third_person = ""_v,
		       .praeteritum = ""_v,
		       .auxv_and_past_participle = "hat"_v,
		       .separable_prefix_size = 0};

		TEST_CHECK_STR_EQ(grammar::verb_past_participle(scratch, v),
		                  "gebeiert"_v);
		TEST_CHECK_STR_EQ(grammar::verb_perfect_full(scratch, v),
		                  "hat gebeiert"_v);
	}
	{
		Verb v{.infinitive = "geiern"_v,
		       .third_person = ""_v,
		       .praeteritum = ""_v,
		       .auxv_and_past_participle = "hat"_v,
		       .separable_prefix_size = 0};

		TEST_CHECK_STR_EQ(grammar::verb_past_participle(scratch, v),
		                  "gegeiert"_v);
		TEST_CHECK_STR_EQ(grammar::verb_perfect_full(scratch, v),
		                  "hat gegeiert"_v);
	}
	{
		Verb v{.infinitive = "geifern"_v,
		       .third_person = ""_v,
		       .praeteritum = ""_v,
		       .auxv_and_past_participle = "hat"_v,
		       .separable_prefix_size = 0};

		TEST_CHECK_STR_EQ(grammar::verb_past_participle(scratch, v),
		                  "gegeifert"_v);
		TEST_CHECK_STR_EQ(grammar::verb_perfect_full(scratch, v),
		                  "hat gegeifert"_v);
	}
	{
		Verb v{.infinitive = "telefonieren"_v,
		       .third_person = ""_v,
		       .praeteritum = ""_v,
		       .auxv_and_past_participle = "hat"_v,
		       .separable_prefix_size = 0};

		TEST_CHECK_STR_EQ(grammar::verb_past_participle(scratch, v),
		                  "telefoniert"_v);
		TEST_CHECK_STR_EQ(grammar::verb_perfect_full(scratch, v),
		                  "hat telefoniert"_v);
	}
	{
		Verb v{.infinitive = "kasteien"_v,
		       .third_person = ""_v,
		       .praeteritum = ""_v,
		       .auxv_and_past_participle = "hat"_v,
		       .separable_prefix_size = 0};

		TEST_CHECK_STR_EQ(grammar::verb_past_participle(scratch, v),
		                  "kasteit"_v);
		TEST_CHECK_STR_EQ(grammar::verb_perfect_full(scratch, v),
		                  "hat kasteit"_v);
	}
	{
		Verb v{.infinitive = "prophezeien"_v,
		       .third_person = ""_v,
		       .praeteritum = ""_v,
		       .auxv_and_past_participle = "hat"_v,
		       .separable_prefix_size = 0};

		TEST_CHECK_STR_EQ(grammar::verb_past_participle(scratch, v),
		                  "prophezeit"_v);
		TEST_CHECK_STR_EQ(grammar::verb_perfect_full(scratch, v),
		                  "hat prophezeit"_v);
	}
	{
		Verb v{.infinitive = "schmieren"_v,
		       .third_person = ""_v,
		       .praeteritum = ""_v,
		       .auxv_and_past_participle = "hat"_v,
		       .separable_prefix_size = 0};

		TEST_CHECK_STR_EQ(grammar::verb_past_participle(scratch, v),
		                  "geschmiert"_v);
		TEST_CHECK_STR_EQ(grammar::verb_perfect_full(scratch, v),
		                  "hat geschmiert"_v);
	}
	{
		Verb v{.infinitive = "bleien"_v,
		       .third_person = ""_v,
		       .praeteritum = ""_v,
		       .auxv_and_past_participle = "hat"_v,
		       .separable_prefix_size = 0};

		TEST_CHECK_STR_EQ(grammar::verb_past_participle(scratch, v),
		                  "gebleit"_v);
		TEST_CHECK_STR_EQ(grammar::verb_perfect_full(scratch, v),
		                  "hat gebleit"_v);
	}
	{
		Verb v{.infinitive = "schneien"_v,
		       .third_person = ""_v,
		       .praeteritum = ""_v,
		       .auxv_and_past_participle = "hat"_v,
		       .separable_prefix_size = 0};

		TEST_CHECK_STR_EQ(grammar::verb_past_participle(scratch, v),
		                  "geschneit"_v);
		TEST_CHECK_STR_EQ(grammar::verb_perfect_full(scratch, v),
		                  "hat geschneit"_v);
	}
	{
		Verb v{.infinitive = "verjagen"_v,
		       .third_person = ""_v,
		       .praeteritum = ""_v,
		       .auxv_and_past_participle = "hat"_v,
		       .separable_prefix_size = 0};

		TEST_CHECK_STR_EQ(grammar::verb_past_participle(scratch, v),
		                  "verjagt"_v);
		TEST_CHECK_STR_EQ(grammar::verb_perfect_full(scratch, v),
		                  "hat verjagt"_v);
	}
	{
		Verb v{.infinitive = "benoten"_v,
		       .third_person = ""_v,
		       .praeteritum = ""_v,
		       .auxv_and_past_participle = "hat"_v,
		       .separable_prefix_size = 0};

		TEST_CHECK_STR_EQ(grammar::verb_past_participle(scratch, v),
		                  "benotet"_v);
		TEST_CHECK_STR_EQ(grammar::verb_perfect_full(scratch, v),
		                  "hat benotet"_v);
	}
	{
		Verb v{.infinitive = "drucken"_v,
		       .third_person = ""_v,
		       .praeteritum = ""_v,
		       .auxv_and_past_participle = "hat"_v,
		       .separable_prefix_size = 0};

		TEST_CHECK_STR_EQ(grammar::verb_past_participle(scratch, v),
		                  "gedruckt"_v);
		TEST_CHECK_STR_EQ(grammar::verb_perfect_full(scratch, v),
		                  "hat gedruckt"_v);
		TEST_CHECK_STR_EQ(grammar::verb_praeteritum_full(scratch, v),
		                  "druckte"_v);
		TEST_CHECK_STR_EQ(grammar::verb_third_person_full(scratch, v),
		                  "druckt"_v);
	}

	// Strong verbs with auxiliary "sein"
	{
		Verb v{.infinitive = "gehen"_v,
		       .third_person = "geht"_v,
		       .praeteritum = "ging"_v,
		       .auxv_and_past_participle = "ist gegangen"_v,
		       .separable_prefix_size = 0};

		TEST_CHECK_STR_EQ(grammar::verb_past_participle(scratch, v),
		                  "gegangen"_v);
		TEST_CHECK_STR_EQ(grammar::verb_perfect_full(scratch, v),
		                  "ist gegangen"_v);
		TEST_CHECK_STR_EQ(grammar::verb_praeteritum_full(scratch, v), "ging"_v);
		TEST_CHECK_STR_EQ(grammar::verb_third_person_full(scratch, v),
		                  "geht"_v);
	}

	// Separable verbs
	{
		// NOTE: we do not store separatable prefix in p3 and past fields, but
		// do provide complete pp
		Verb v{.infinitive = "aufstehen"_v,
		       .third_person = "steht"_v,
		       .praeteritum = "stand"_v,
		       .auxv_and_past_participle = "ist aufgestanden"_v,
		       .separable_prefix_size = 3};

		TEST_CHECK_STR_EQ(grammar::verb_past_participle(scratch, v),
		                  "aufgestanden"_v);
		TEST_CHECK_STR_EQ(grammar::verb_perfect_full(scratch, v),
		                  "ist aufgestanden"_v);
		TEST_CHECK_STR_EQ(grammar::verb_praeteritum_full(scratch, v),
		                  "stand auf"_v);
		TEST_CHECK_STR_EQ(grammar::verb_third_person_full(scratch, v),
		                  "steht auf"_v);
	}
}

// -----------------------------------------------------------------------------
// Arena Scratch TempGuard Safety Test
// -----------------------------------------------------------------------------

static void test_arena_scratch_safety(Arena &scratch) {
	const Size initial_offset = scratch.offset;

	{
		auto guard = scratch.guard();
		Noun n{.lemma = "Fenster"_v,
		       .plural_suffix = "-"_v,
		       .gender = Gender::n};
		StrView s = grammar::noun_plural_with_article(scratch, n);
		(void)s;
		// Allocations occurred within the guard scope
		TEST_CHECK(scratch.offset >= initial_offset);
	}

	// On guard destruction, scratch offset must be restored
	TEST_CHECK(scratch.offset == initial_offset);
}

// -----------------------------------------------------------------------------
// Main Runner
// -----------------------------------------------------------------------------

int main() {
	Arena scratch(1 << 20); // 1 MiB

	test_noun_classification();
	test_noun_singular_with_article(scratch);
	test_noun_plural_forms(scratch);
	test_verb_separable_prefix();
	test_verb_stem();
	test_verb_auxiliary();
	test_verb_regularity();
	test_verb_tenses_and_forms(scratch);
	test_arena_scratch_safety(scratch);

	std::printf("======================================\n");
	std::printf("Grammar Tests: %d run, %d failed\n", g_tests_run,
	            g_tests_failed);
	std::printf("======================================\n");

	return g_tests_failed == 0 ? 0 : 1;
}
