#pragma once

#include <cstdint>

#include "str_view.h"

using Hash = uint64_t;

Hash hash_str_view(StrView str);

Hash hash_text(StrView str, uint16_t font_id, uint16_t font_size,
               uint32_t color);
