#pragma once

#include "base/arena.h"
#include "base/str_view.h"

struct Noun;
struct Verb;

namespace grammar {

[[nodiscard]]
bool is_plural_only(const Noun &n);
[[nodiscard]]
bool is_singular_only(const Noun &n);
[[nodiscard]]
bool is_aux_sein(const Verb &v);
[[nodiscard]]
bool is_regular(const Verb &v);

/*
 * NOTE: all results MAY be in scratch arena
 */

[[nodiscard]]
StrView noun_singular_with_article(Arena &scratch, const Noun &n);
[[nodiscard]]
StrView noun_plural_without_article(Arena &scratch, const Noun &n);
[[nodiscard]]
StrView noun_plural_with_article(Arena &scratch, const Noun &n);

/*
 * NOTE: all results MAY be in scratch arena
 */
[[nodiscard]]
bool verb_is_separable_prefix(const Verb &v);
[[nodiscard]]
StrView verb_stem(const Verb &v);
[[nodiscard]]
StrView verb_separable_prefix(const Verb &v);
[[nodiscard]]
StrView verb_infinitive_without_separable_prefix(const Verb &v);
[[nodiscard]]
StrView verb_past_participle(Arena &scratch, const Verb &v);
[[nodiscard]]
StrView verb_perfect_full(Arena &scratch, const Verb &v);
[[nodiscard]]
StrView verb_praeteritum_full(Arena &scratch, const Verb &v);
[[nodiscard]]
StrView verb_third_person_full(Arena &scratch, const Verb &v);

} // namespace grammar
