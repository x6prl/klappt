#pragma once

#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <clay/clay.h>

#include "base/arr.h"
#include "base/dyn_arr.h"
#include "base/hash.h"
#include "base/pair.h"
#include "base/str_view.h"

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
	using Size = uint32_t;

	constexpr static uint16_t PAGE_NONE = 0xFFFFu;
	constexpr static uint16_t ATLAS_SIZE = 2048u;
	constexpr static uint16_t FAST_FONT_MAX = 1u << 7; // up to size 128
	constexpr static uint32_t FAST_ASCII_MAX = 128u;

	// -------------------------------------------------------------------------
	// Glyph Representation
	// -------------------------------------------------------------------------
	struct GlyphEntry {
		uint32_t codepoint{0};
		uint16_t font_id{0};
		uint16_t font_size{0};
		uint16_t page{PAGE_NONE};
		uint16_t rx{0}, ry{0}; // Position in texture atlas
		uint16_t w{0}, h{0};   // Size in pixels
		int16_t minx{0};       // Left bearing offset from pen_x
		int16_t maxy{0};       // Top offset relative to baseline
		int16_t advance{0};    // Horizontal pen advance
		bool loaded{false};
	};

	// -------------------------------------------------------------------------
	// Atlas Shelf Allocator
	// -------------------------------------------------------------------------
	struct AtlasPage {
		constexpr static uint32_t SHELF_MAX = 128u;
		SDL_Texture *tex{nullptr};
		struct Shelf {
			uint16_t y{0}, h{0}, x{0};
		};
		Shelf shelves[SHELF_MAX]{};
		uint32_t shelf_count{0};
		uint16_t current_y{0};
	};

	uint64_t current_ticks{0};
	uint64_t frame_index{0};

	void set_time(uint64_t ticks_ms) {
		current_ticks = ticks_ms;
		++frame_index;
	}

	constexpr static uint32_t GLYPH_HASH_SIZE = 1u << 13;
	constexpr static uint32_t GLYPH_POOL_MAX = GLYPH_HASH_SIZE / 2;

	GlyphEntry glyph_pool[GLYPH_POOL_MAX]{};
	uint32_t glyph_pool_count{1}; // Index 0 reserved for null

	struct HashSlot {
		uint32_t codepoint{0};
		uint16_t font_id{0};
		uint16_t font_size{0};
		uint32_t pool_index{0}; // 0 = empty
	};
	HashSlot glyph_hash_table[GLYPH_HASH_SIZE]{};

	const GlyphEntry
		  *fast_ascii[FontID::COUNT][FAST_FONT_MAX][FAST_ASCII_MAX]{};

	// -------------------------------------------------------------------------
	// Font Cache
	// -------------------------------------------------------------------------
	struct FontKey {
		uint16_t font_id;
		uint16_t font_size;
	};
	using FontEntry = Pair<FontKey, TTF_Font *>;
	TTF_Font *base_fonts[FontID::COUNT]{};
	Arr<FontEntry, 96> fonts{};
	TTF_Font *fast_fonts[FontID::COUNT][FAST_FONT_MAX]{};

	// -------------------------------------------------------------------------
	// State
	// -------------------------------------------------------------------------
	AtlasPage atlas{};
	SDL_BlendMode atlas_blend{SDL_BLENDMODE_BLEND};

	// -------------------------------------------------------------------------
	// API
	// -------------------------------------------------------------------------
	void atlas_init(SDL_Renderer *r);
	void prewarm(DynArr<uint16_t> sizes);

	TTF_Font *get_font(uint16_t font_id, uint16_t font_size);
	int get_font_ascent(uint16_t font_id, uint16_t font_size);
	int get_font_height(uint16_t font_id, uint16_t font_size);

	const GlyphEntry *get_glyph(uint32_t codepoint, uint16_t font_id,
	                            uint16_t font_size);

	Clay_Dimensions measure_text(Clay_StringSlice slice,
	                             Clay_TextElementConfig *config);
	void measure_string(StrView str, uint16_t font_id, uint16_t font_size,
	                    int *out_w, int *out_h);

	// Immediate string draw
	void draw_string(SDL_Renderer *r, StrView str, uint16_t font_id,
	                 uint16_t font_size, float x, float y, SDL_Color color);

	// -------------------------------------------------------------------------
	// Internals
	// -------------------------------------------------------------------------
	bool atlas_alloc(uint16_t w, uint16_t h, uint16_t &rx, uint16_t &ry);
	const GlyphEntry *load_glyph(uint32_t codepoint, uint16_t font_id,
	                             uint16_t font_size);
};
