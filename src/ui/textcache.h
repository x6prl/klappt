#pragma once

#include "base/arr.h"
#include "base/hash.h"
#include "base/pair.h"
#include "base/str_view.h"
#include <cstdint>

struct SDL_Renderer;
struct TTF_Text;
struct TTF_TextEngine;
struct TTF_Font;

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

	// -------------------------------------------------------------------------
	// TTF Text Cache
	// -------------------------------------------------------------------------
	constexpr static Idx TEXT_CACHE_HASHMAP_SIZE = 1u << 11;
	constexpr static Idx MAX_OCCUPIED =
		  (TEXT_CACHE_HASHMAP_SIZE * 3) / 4; // 75% load cap
	constexpr static TimestampSec TEXT_TTL = {8u};

	struct TextEntry {
		Hash hash;              // 8B
		uint32_t text_size;     // 12B
		uint16_t font_id;       // 14B
		uint16_t font_size;     // 16B
		uint32_t color;         // 20B
		TimestampSec timestamp; // 24B
		TTF_Text *text;         // 32B

		bool is_obsolete(TimestampSec t) const {
			if (t.tss <= TEXT_TTL.tss) {
				return false;
			}
			return timestamp.tss < (t.tss - TEXT_TTL.tss);
		}
	};

	// -------------------------------------------------------------------------
	// Measurement Cache
	// -------------------------------------------------------------------------
	constexpr static Idx MEASURE_CACHE_SIZE = 1u << 10;
	constexpr static Idx MEASURE_CACHE_FAST_PROBE = 4u;
	// TODO: gather stats and maybe move out .text from MeasureEntry
	struct MeasureEntry {
		Hash hash{0};                     // 8B
		uint16_t font_id{0};              // 10B
		uint16_t font_size{0};            // 12B
		uint16_t text_size{0};            // 14B
		                                  // padding 2B
		Clay_Dimensions dims{0.0f, 0.0f}; // 24B
		char text[40]{};                  // 64
	};

	// -------------------------------------------------------------------------
	// State
	// -------------------------------------------------------------------------
	TTF_TextEngine *engine{nullptr};
	struct FontKey {
		uint16_t font_id;
		uint16_t font_size;
	};
	using FontEntry = Pair<FontKey, TTF_Font *>;
	TTF_Font *base_fonts[FontID::COUNT]{};
	Arr<FontEntry, 96> fonts{};

	// TODO: gather fontsize statistics and add a shift?
	constexpr static uint16_t FAST_FONT_MAX = 1u << 7;
	TTF_Font *fast_fonts[FontID::COUNT][FAST_FONT_MAX]{};

	TextEntry text_cache_data[TEXT_CACHE_HASHMAP_SIZE]{};
	MeasureEntry measure_cache_data[MEASURE_CACHE_SIZE]{};

	TimestampSec current_time{0};
	uint16_t active_count{0};

	void set_time(uint64_t ticks_ms) {
		current_time = tss_from_ticks(ticks_ms);
	}

	TTF_Font *get_font(uint16_t font_id, uint16_t font_size);

	Pair<Idx, Hash> htable_lookup(StrView str, uint16_t font_id,
	                              uint16_t font_size, uint32_t color);
	Idx htable_erase(Idx idx);
	Idx lp_find_free_slot(Hash h, TimestampSec t);
	void htable_swap(Idx a, Idx b);

	TTF_Text *get(StrView str, uint16_t font_id, uint16_t font_size,
	              Clay_Color color);
	Clay_Dimensions measure_text(Clay_StringSlice slice,
	                             Clay_TextElementConfig *config);
	void prewarm(float scale, SDL_Renderer *renderer);
};
