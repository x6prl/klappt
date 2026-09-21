#include "textcache.h"

#include <SDL3/SDL_log.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <SDL3/SDL_surface.h>

#include <cassert>

#include <initializer_list>

#include "base/profiler.h"

namespace {
constexpr SDL_Color WHITE{255, 255, 255, 255};

// Murmur3
uint32_t hash_glyph_key(uint32_t cp, uint16_t font_id, uint16_t font_size) {
	uint32_t h = cp ^ (static_cast<uint32_t>(font_id) << 16) ^
	             (static_cast<uint32_t>(font_size) << 21);
	h ^= h >> 16;
	h *= 0x85ebca6b;
	h ^= h >> 13;
	h *= 0xc2b2ae35;
	h ^= h >> 16;

	return h;
}
} // namespace

void TextCache::atlas_init(SDL_Renderer *r) {
	assert(!atlas.tex);

	KLAPPT_PROFILE_SCOPE_N("TextCache::atlas_init");

	atlas_blend = SDL_BLENDMODE_BLEND;

	SDL_PropertiesID props = SDL_CreateProperties();
	SDL_SetNumberProperty(props, SDL_PROP_TEXTURE_CREATE_FORMAT_NUMBER,
	                      SDL_PIXELFORMAT_RGBA32);
	SDL_SetNumberProperty(props, SDL_PROP_TEXTURE_CREATE_ACCESS_NUMBER,
	                      SDL_TEXTUREACCESS_STATIC);
	SDL_SetNumberProperty(props, SDL_PROP_TEXTURE_CREATE_WIDTH_NUMBER,
	                      ATLAS_SIZE);
	SDL_SetNumberProperty(props, SDL_PROP_TEXTURE_CREATE_HEIGHT_NUMBER,
	                      ATLAS_SIZE);
	atlas.tex = SDL_CreateTextureWithProperties(r, props);
	SDL_DestroyProperties(props);

	if (atlas.tex) {
		SDL_SetTextureBlendMode(atlas.tex, atlas_blend);
		SDL_SetTextureScaleMode(atlas.tex, SDL_SCALEMODE_LINEAR);
	} else {
		SDL_LogError(SDL_LOG_CATEGORY_ERROR,
		             "Glyph atlas texture allocation failed: %s",
		             SDL_GetError());
		exit(-6);
	}
}

bool TextCache::atlas_alloc(uint16_t w, uint16_t h, uint16_t &rx,
                            uint16_t &ry) {
	// 1px border on each side
	const uint16_t pw = w + 2;
	const uint16_t ph = h + 2;

	if (pw > ATLAS_SIZE || ph > ATLAS_SIZE) {
		return false;
	}

	// 1. Pack horizontally
	for (uint32_t s = 0; s < atlas.shelf_count; ++s) {
		auto &sh = atlas.shelves[s];
		if (ph <= sh.h && (static_cast<uint32_t>(sh.x) + pw) <= ATLAS_SIZE) {
			rx = sh.x + 1;
			ry = sh.y + 1;
			sh.x += pw;
			return true;
		}
	}

	// 2. Open a new shelf if vertical room remains
	if (atlas.shelf_count < AtlasPage::SHELF_MAX &&
	    (static_cast<uint32_t>(atlas.current_y) + ph) <= ATLAS_SIZE) {
		auto &s = atlas.shelves[atlas.shelf_count++];
		s.y = atlas.current_y;
		s.h = ph;
		s.x = pw;
		rx = 1;
		ry = atlas.current_y + 1;
		atlas.current_y += ph;
		return true;
	}

	SDL_LogError(SDL_LOG_CATEGORY_ERROR, "FATAL: Glyph Atlas completely full!");
	return false;
}

TTF_Font *TextCache::get_font(uint16_t font_id, uint16_t font_size) {
	if (font_id < FontID::COUNT && font_size < FAST_FONT_MAX) {
		TTF_Font *cached = fast_fonts[font_id][font_size];
		if (cached) {
			return cached;
		}
	}

	for (auto &it : fonts) {
		if (it.second && it.first.font_size == font_size &&
		    it.first.font_id == font_id) {
			if (font_id < FontID::COUNT && font_size < FAST_FONT_MAX) {
				fast_fonts[font_id][font_size] = it.second;
			}
			return it.second;
		}
	}

	for (auto &it : fonts) {
		if (!it.second) {
			TTF_Font *font = TTF_CopyFont(base_fonts[font_id]);
			if (!font) {
				SDL_LogError(SDL_LOG_CATEGORY_ERROR,
				             "Cannot copy font id %u size %u: %s", font_id,
				             font_size, SDL_GetError());
				exit(-6);
			}
			TTF_SetFontSize(font, font_size);
			TTF_SetFontHinting(font, TTF_HINTING_NONE);

			it.first = {font_id, font_size};
			it.second = font;
			if (font_id < FontID::COUNT && font_size < FAST_FONT_MAX) {
				fast_fonts[font_id][font_size] = font;
			}
			return font;
		}
	}
	SDL_LogError(SDL_LOG_CATEGORY_ERROR, "Font cache capacity exhausted");
	exit(-6);
}

int TextCache::get_font_ascent(uint16_t font_id, uint16_t font_size) {
	TTF_Font *font = get_font(font_id, font_size);
	return TTF_GetFontAscent(font);
}

int TextCache::get_font_height(uint16_t font_id, uint16_t font_size) {
	TTF_Font *font = get_font(font_id, font_size);
	return TTF_GetFontHeight(font);
}

const TextCache::GlyphEntry *TextCache::load_glyph(uint32_t codepoint,
                                                   uint16_t font_id,
                                                   uint16_t font_size) {
	KLAPPT_PROFILE_SCOPE_N("TextCache::load_glyph");

	if (glyph_pool_count >= GLYPH_POOL_MAX) {
		SDL_LogError(SDL_LOG_CATEGORY_ERROR, "Glyph pool capacity exhausted!");
		return nullptr;
	}

	TTF_Font *font = get_font(font_id, font_size);

	int minx = 0, maxx = 0, miny = 0, maxy = 0, advance = 0;
	if (!TTF_GetGlyphMetrics(font, codepoint, &minx, &maxx, &miny, &maxy,
	                         &advance)) {
		advance = font_size / 2;
	}

	GlyphEntry g{};
	g.codepoint = codepoint;
	g.font_id = font_id;
	g.font_size = font_size;
	g.minx = static_cast<int16_t>(minx);
	g.maxy = static_cast<int16_t>(maxy);
	g.advance = static_cast<int16_t>(advance);
	g.loaded = true;

	// NOTE: space characters return null
	SDL_Surface *surf = TTF_RenderGlyph_Blended(font, codepoint, WHITE);
	if (surf && surf->w > 0 && surf->h > 0) {
		uint16_t rx = 0, ry = 0;
		if (atlas_alloc(static_cast<uint16_t>(surf->w),
		                static_cast<uint16_t>(surf->h), rx, ry)) {
			SDL_Rect upload{rx, ry, surf->w, surf->h};
			SDL_UpdateTexture(atlas.tex, &upload, surf->pixels, surf->pitch);

			g.page = 0;
			g.rx = rx;
			g.ry = ry;
			g.w = static_cast<uint16_t>(surf->w);
			g.h = static_cast<uint16_t>(surf->h);
		}
		SDL_DestroySurface(surf);
	}

	const uint32_t pool_idx = glyph_pool_count++;
	glyph_pool[pool_idx] = g;
	const GlyphEntry *stable_ptr = &glyph_pool[pool_idx];

	constexpr uint32_t mask = GLYPH_HASH_SIZE - 1;
	uint32_t slot = hash_glyph_key(codepoint, font_id, font_size) & mask;
	while (glyph_hash_table[slot].pool_index != 0) {
		slot = (slot + 1) & mask;
	}

	glyph_hash_table[slot] = {.codepoint = codepoint,
	                          .font_id = font_id,
	                          .font_size = font_size,
	                          .pool_index = pool_idx};

	if (codepoint < FAST_ASCII_MAX && font_id < FontID::COUNT &&
	    font_size < FAST_FONT_MAX) {
		fast_ascii[font_id][font_size][codepoint] = stable_ptr;
	}

	return stable_ptr;
}

const TextCache::GlyphEntry *
TextCache::get_glyph(uint32_t codepoint, uint16_t font_id, uint16_t font_size) {
	if (codepoint < FAST_ASCII_MAX && font_id < FontID::COUNT &&
	    font_size < FAST_FONT_MAX) {
		const GlyphEntry *g = fast_ascii[font_id][font_size][codepoint];
		if (g)
			return g;
		return load_glyph(codepoint, font_id, font_size);
	}

	// Non-ASCII path
	constexpr uint32_t mask = GLYPH_HASH_SIZE - 1;
	uint32_t slot = hash_glyph_key(codepoint, font_id, font_size) & mask;

	while (glyph_hash_table[slot].pool_index != 0) {
		const auto &entry = glyph_hash_table[slot];
		if (entry.codepoint == codepoint && entry.font_id == font_id &&
		    entry.font_size == font_size) {
			return &glyph_pool[entry.pool_index];
		}
		slot = (slot + 1) & mask;
	}

	return load_glyph(codepoint, font_id, font_size);
}

Clay_Dimensions TextCache::measure_text(Clay_StringSlice slice,
                                        Clay_TextElementConfig *config) {
	if (slice.length <= 0) [[unlikely]] {
		return {0.0f, static_cast<float>(config->fontSize)};
	}

	if (slice.length == 1 && slice.chars[0] == ' ') {
		const auto font_size = config->fontSize;
		const auto *g = get_glyph(' ', config->fontId, font_size);
		return {static_cast<float>(g ? g->advance : (font_size / 3.0f)),
		        static_cast<float>(font_size)};
	}

	KLAPPT_PROFILE_SCOPE_N("TextCache::measure_text");
	const auto font_size = config->fontSize;

	float width = 0.0f;
	const char *ptr = slice.chars;
	size_t remaining = static_cast<size_t>(slice.length);

	while (remaining > 0) {
		const Uint32 cp = SDL_StepUTF8(&ptr, &remaining);
		if (!cp) [[unlikely]] {
			break;
		}
		const auto *g = get_glyph(cp, config->fontId, font_size);
		width += g ? static_cast<float>(g->advance)
		           : (static_cast<float>(font_size) * 0.5f);
	}

	return {width,
	        static_cast<float>(get_font_height(config->fontId, font_size))};
}

void TextCache::measure_string(StrView str, uint16_t font_id,
                               uint16_t font_size, int *out_w, int *out_h) {
	Clay_StringSlice slice{static_cast<int32_t>(str.size), str.data};
	Clay_TextElementConfig config{.fontId = font_id, .fontSize = font_size};
	const Clay_Dimensions dims = measure_text(slice, &config);
	if (out_w)
		*out_w = static_cast<int>(dims.width);
	if (out_h)
		*out_h = static_cast<int>(dims.height);
}

void TextCache::draw_string(SDL_Renderer *r, StrView str, uint16_t font_id,
                            uint16_t font_size, float x, float y,
                            SDL_Color color) {
	float pen_x = SDL_roundf(x);
	const float line_y = SDL_roundf(y);
	const SDL_FColor fc{color.r / 255.0f, color.g / 255.0f, color.b / 255.0f,
	                    color.a / 255.0f};

	constexpr float PW = static_cast<float>(ATLAS_SIZE);
	const char *ptr = str.data;
	size_t remaining = str.size;

	SDL_Vertex verts[4 * 64];
	int inds[6 * 64];
	int nv = 0, ni = 0;

	while (remaining > 0) {
		const Uint32 cp = SDL_StepUTF8(&ptr, &remaining);
		if (!cp)
			break;
		const auto *g = get_glyph(cp, font_id, font_size);
		if (!g)
			continue;

		if (g->w > 0 && g->h > 0) {
			const float gx =
				  pen_x + (g->minx < 0 ? static_cast<float>(g->minx) : 0.0f);
			const float gy = line_y;
			const float gw = static_cast<float>(g->w);
			const float gh = static_cast<float>(g->h);

			const float u0 = static_cast<float>(g->rx) / PW;
			const float v0 = static_cast<float>(g->ry) / PW;
			const float u1 = static_cast<float>(g->rx + g->w) / PW;
			const float v1 = static_cast<float>(g->ry + g->h) / PW;

			const int b = nv;
			verts[nv++] = SDL_Vertex{{gx, gy}, fc, {u0, v0}};
			verts[nv++] = SDL_Vertex{{gx + gw, gy}, fc, {u1, v0}};
			verts[nv++] = SDL_Vertex{{gx + gw, gy + gh}, fc, {u1, v1}};
			verts[nv++] = SDL_Vertex{{gx, gy + gh}, fc, {u0, v1}};

			inds[ni++] = b;
			inds[ni++] = b + 1;
			inds[ni++] = b + 2;
			inds[ni++] = b;
			inds[ni++] = b + 2;
			inds[ni++] = b + 3;

			if (nv >= 4 * 60) {
				SDL_RenderGeometry(r, atlas.tex, verts, nv, inds, ni);
				nv = 0;
				ni = 0;
			}
		}
		pen_x += static_cast<float>(g->advance);
	}

	if (ni > 0) {
		SDL_RenderGeometry(r, atlas.tex, verts, nv, inds, ni);
	}
}

void TextCache::prewarm(DynArr<uint16_t> sizes) {
	for (const auto fid : {FontID::MAIN}) {
		for (const auto sz : sizes) {
			// Bake ASCII printable characters 32..126
			for (uint32_t c = 32; c <= 126; ++c) {
				get_glyph(c, fid, sz);
			}
			// Common German characters
			for (const uint32_t umlaut :
			     {0x00E4u, 0x00F6u, 0x00FCu, 0x00C4u, 0x00D6u, 0x00DCu,
			      0x00DFu}) { // ä, ö, ü, Ä, Ö, Ü, ß
				get_glyph(umlaut, fid, sz);
			}
		}
	}
}
