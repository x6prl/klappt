#pragma once

#include "base/arena.h"
#include "base/str_view.h"

struct Noun;
struct Verb;

namespace grammar {

bool is_plural_only(const Noun &n);
bool is_singular_only(const Noun &n);

/*
 * NOTE: all results MAY be in scratch arena
 */

StrView noun_singular_with_article(Arena &scratch, const Noun &n);
StrView noun_plural_without_article(Arena &scratch, const Noun &n);
StrView noun_plural_with_article(Arena &scratch, const Noun &n);

/*
 * NOTE: all results MAY be in scratch arena
 */

StrView verb_past_participle(Arena &scratch, const Verb &v);
StrView verb_perfect_full(Arena &scratch, const Verb &v);
StrView verb_praeteritum_full(Arena &scratch, const Verb &v);
StrView verb_third_person_full(Arena &scratch, const Verb &v);

} // namespace grammar
