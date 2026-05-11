#include "hash.h"

#include "../../xxHash/xxh3.h"

Hash hash_str_view(StrView str) {
	constexpr Hash seed = 1337u;
	return XXH3_64bits_withSeed(str.data, str.size, seed);
}

Hash hash_text(StrView str, uint16_t font_id, uint16_t font_size,
               uint32_t color) {
	Hash seed = static_cast<uint64_t>(font_id) << 48 |
	            static_cast<uint64_t>(font_size) << 32 |
	            static_cast<uint64_t>(color);
	return XXH3_64bits_withSeed(str.data, str.size, seed);
}
