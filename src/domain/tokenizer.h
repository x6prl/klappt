#pragma once

#include <cstdint>

#include "base/dyn_arr.h"
#include "base/str_view.h"

// TODO: rewrite
namespace Tokenizer {
enum class Kind : uint8_t { Noun = 0, Verb, Adjective };

Kind guess_kind(StrView word);
DynArr<StrView> to_chunks(Arena &a, StrView str, Kind kind = Kind::Verb);
DynArr<StrView> to_letters(Arena &a, StrView str);
DynArr<StrView> get_4_distractors_for_a_chunk(StrView token,
                                              uint32_t seed = 0);
}
