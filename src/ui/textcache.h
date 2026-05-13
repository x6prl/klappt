#pragma once

#include <cstdint>

#include "SDL3/SDL_pixels.h"
#include "SDL3_ttf/SDL_ttf.h"

#include "base/arena.h"
#include "base/arr.h"
#include "base/hash.h"
#include "base/pair.h"
#include "base/str_view.h"
#include <clay/clay.h>

constexpr uint32_t rgba_u32(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
	return (uint32_t(r) << 24) | (uint32_t(g) << 16) | (uint32_t(b) << 8) |
	       uint32_t(a);
}
constexpr uint32_t clay_color_to_u32(Clay_Color c) {
	return (uint32_t(c.r) << 24) | (uint32_t(c.g) << 16) |
	       (uint32_t(c.b) << 8) | uint32_t(c.a);
}
constexpr Clay_Color clay_color_normalize(Clay_Color c) {
	return {
		  (float)c.r / 255.0f,
		  (float)c.g / 255.0f,
		  (float)c.b / 255.0f,
		  (float)c.a / 255.0f,
	};
}
constexpr SDL_FColor clay_color_to_SDL_FColor_norm(Clay_Color c) {
	return {
		  (float)c.r / 255.0f,
		  (float)c.g / 255.0f,
		  (float)c.b / 255.0f,
		  (float)c.a / 255.0f,
	};
}
#define U32_R(x) ((uint8_t)(((uint32_t)(x) >> 24) & 0xFF))
#define U32_G(x) ((uint8_t)(((uint32_t)(x) >> 16) & 0xFF))
#define U32_B(x) ((uint8_t)(((uint32_t)(x) >> 8) & 0xFF))
#define U32_A(x) ((uint8_t)((uint32_t)(x) & 0xFF))
#define U32_RGBA(x) U32_R(x), U32_G(x), U32_B(x), U32_A(x),

namespace FontID {
constexpr uint16_t MAIN{0u};
constexpr uint16_t ICONS{1u};
constexpr uint16_t MONOSPACE_REGULAR{2u};
constexpr uint16_t MONOSPACE_BOLD{3u};
constexpr uint16_t ARABIC_MAIN{4u};
constexpr uint16_t COUNT{5u};
} // namespace FontID

struct TextCache {
	using Idx = Hash;
	struct TimestampSec {
		uint32_t tss;
	};
	static TimestampSec tss_from_ticks(uint64_t t) {
		return {static_cast<uint32_t>(t / 1000)};
	}
	constexpr static Idx MAP_SIZE = 1u << 11;
	constexpr static TimestampSec TEXT_TTL = {8u};

	struct Data {
		Hash hash;
		Size text_size;
		uint16_t font_id;
		uint16_t font_size;
		uint32_t color;
		TimestampSec timestamp;
		TTF_Text *text;

		bool is_obsolete(TimestampSec t) const {
			if (t.tss <= TEXT_TTL.tss) {
				return false;
			}
			return timestamp.tss < (t.tss - TEXT_TTL.tss);
		}
	};

	TTF_TextEngine *engine;
	struct FontKey {
		uint16_t font_id;
		uint16_t font_size;
	};
	using Font = Pair<FontKey, TTF_Font *>;
	TTF_Font *base_fonts[FontID::COUNT]{};
	Arr<Font, 96> fonts{};
	Data data[MAP_SIZE]{};

	TTF_Font *get_font(uint16_t font_id, uint16_t font_size);

	Pair<Idx, Hash> htable_lookup(StrView str, uint16_t font_id,
	                              uint16_t font_size, uint32_t color);
	Idx htable_erase(Idx idx);
	Idx lp_find_free_slot(Hash h, TimestampSec t);
	void htable_swap(Idx a, Idx b);

	TTF_Text *get(StrView str, uint16_t font_id, uint16_t font_size,
	              Clay_Color color, uint64_t t);
	Clay_Dimensions measure_text(Clay_StringSlice slice,
	                             Clay_TextElementConfig *config);
};
