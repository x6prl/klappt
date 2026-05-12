#include "textcache.h"

#include <cstdint>
#include <cstdlib>
#include <cstring>

// using htable_index_t = Hash;
// using htable_key_t = FontCache::Data;

#include "SDL3/SDL_error.h"
#include "SDL3/SDL_log.h"
#include "SDL3_ttf/SDL_ttf.h"
#include "base/profiler.h"
#include "base/str_view.h"
// #include "linear_probing.h"

namespace {
bool is_arabic_font(uint16_t font_id) { return font_id == FontID::ARABIC_MAIN; }

void configure_arabic_font(TTF_Font *font) {
	if (!font) {
		return;
	}
	TTF_SetFontDirection(font, TTF_DIRECTION_RTL);
	TTF_SetFontScript(font, TTF_StringToTag("Arab"));
}

void configure_arabic_text(TTF_Text *text) {
	if (!text) {
		return;
	}
	TTF_SetTextDirection(text, TTF_DIRECTION_RTL);
	TTF_SetTextScript(text, TTF_StringToTag("Arab"));
}

TextCache::Idx lp_home_index(Hash h, TextCache::Idx table_size) {
	return h & (table_size - 1);
}
bool key_is_null(TextCache::Data k) { return k.text == nullptr; }
void key_set_null(TextCache::Data *k) {
	if (k->text) {
		TTF_DestroyText(k->text);
		k->text = nullptr;
	}
}
Hash htable_hash(const TextCache::Data &d) { return d.hash; }
bool key_cmp(TextCache::Data &d, Hash hash, StrView str, uint16_t font_id,
             uint16_t font_size, uint32_t color) {
	if (!d.text) {
		return false;
	}
	return d.hash == hash && str.size == d.text_size && font_id == d.font_id &&
	       font_size == d.font_size && color == d.color &&
	       (0 == std::memcmp(d.text->text, str.data, str.size));
}
} // namespace

void TextCache::htable_swap(Idx a, Idx b) {
	Data tmp = data[a];
	data[a] = data[b];
	data[b] = tmp;
}

Pair<TextCache::Idx, Hash> TextCache::htable_lookup(StrView str,
                                                    uint16_t font_id,
                                                    uint16_t font_size,
                                                    uint32_t color) {
	KLAPPT_PROFILE_SCOPE_N("TextCache::htable_lookup");
	const auto h = hash_text(str, font_id, font_size, color);
	const auto i = lp_home_index(h, MAP_SIZE);
	if (key_cmp(data[i], h, str, font_id, font_size, color)) {
		return {i, h};
	}
	auto j = i + 1;
	for (; j < MAP_SIZE; ++j) {
		if (key_is_null(data[j])) {
			return {MAP_SIZE, h};
		}
		if (key_cmp(data[j], h, str, font_id, font_size, color)) {
			return {j, h};
		}
	}
	j = 0;
	for (; j < i; ++j) {
		if (key_is_null(data[j])) {
			return {MAP_SIZE, h};
		}
		if (key_cmp(data[j], h, str, font_id, font_size, color)) {
			return {j, h};
		}
	}
	return {MAP_SIZE, h};
}

TextCache::Idx TextCache::htable_erase(TextCache::Idx start_idx) {
	KLAPPT_PROFILE_SCOPE_N("TextCache::htable_erase");
	auto hole = start_idx;
	auto next = hole;

	while (true) {
		next = next + 1;
		if (next == MAP_SIZE) {
			next = 0;
		}

		if (key_is_null(data[next]) || next == start_idx) {
			break;
		}

		auto home = lp_home_index(htable_hash(data[next]), MAP_SIZE);

		if (((next > hole) && ((home <= hole) || (home > next))) ||
		    ((next < hole) && ((home <= hole) && (home > next)))) {
			htable_swap(hole, next);

			hole = next;
		}
	}

	key_set_null(&data[hole]);
	return hole;
}

TextCache::Idx TextCache::lp_find_free_slot(Hash h, TextCache::TimestampSec t) {
	KLAPPT_PROFILE_SCOPE_N("TextCache::lp_find_free_slot");
	const TextCache::Idx home = lp_home_index(h, MAP_SIZE);
	TextCache::Idx oldest = home;

	for (TextCache::Idx step = 0; step < MAP_SIZE; ++step) {
		const TextCache::Idx i = (home + step) & (MAP_SIZE - 1);
		if (key_is_null(data[i])) {
			return i;
		}
		if (data[i].is_obsolete(t)) {
			return htable_erase(i);
		}
		if (data[i].timestamp.tss < data[oldest].timestamp.tss) {
			oldest = i;
		}
	}

	SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
	            "TextCache full; evicting oldest entry");
	return htable_erase(oldest);
}

TTF_Font *TextCache::get_font(uint16_t font_id, uint16_t font_size) {
	KLAPPT_PROFILE_SCOPE_N("TextCache::get_font");
	KLAPPT_PROFILE_NAME_F("TextCache::get_font id=%u size=%u", font_id,
	                      font_size);
	auto it = fonts.begin();
	for (; it != fonts.end() && it->second; ++it) {
		if (it->first.font_size == font_size && it->first.font_id == font_id) {
			return it->second;
		}
	}
	if (it != fonts.end()) {
		// not found, create font
		KLAPPT_PROFILE_SCOPE_N("Create font");
		TTF_Font *font = TTF_CopyFont(base_fonts[font_id]);
		if (!font) {
			SDL_LogError(SDL_LOG_CATEGORY_ERROR,
			             "cannot create font size %u id %u: %s", font_size,
			             font_id, SDL_GetError());
		}
		TTF_SetFontSize(font, font_size);
		if (is_arabic_font(font_id)) {
			configure_arabic_font(font);
		}
		it->first = {font_id, font_size};
		it->second = font;
		return font;
	} else {
		// no space
		SDL_LogError(SDL_LOG_CATEGORY_ERROR,
		             "cannot create font size %u id %u: no space", font_size,
		             font_id);
		exit(-6);
	}
}
TTF_Text *TextCache::get(StrView str, uint16_t font_id, uint16_t font_size,
                         Clay_Color clay_color, uint64_t ticks) {
	KLAPPT_PROFILE_SCOPE_N("TextCache::get");
	KLAPPT_PROFILE_NAME_F("TextCache::get id=%u size=%u len=%d", font_id,
	                      font_size, str.size);
	auto t = TextCache::tss_from_ticks(ticks);
	auto color = clay_color_to_u32(clay_color);
	auto [idx, hash] = htable_lookup(str, font_id, font_size, color);
	if (idx == MAP_SIZE) {
		// text is not found, create
		auto font = get_font(font_id, font_size);
		auto *text = TTF_CreateText(engine, font, str.data, str.size);
		if (is_arabic_font(font_id)) {
			configure_arabic_text(text);
		}
		auto ncolor = clay_color_normalize(clay_color);
		TTF_SetTextColorFloat(text, ncolor.r, ncolor.g, ncolor.b, ncolor.a);
		auto idx = lp_find_free_slot(hash, t);
		if (idx != MAP_SIZE) {
			data[idx] = {hash, str.size, font_id, font_size, color, t, text};
		} else {
			SDL_LogError(SDL_LOG_CATEGORY_ERROR,
			             "No space for the text cache!");
			exit(-7);
		}
		return text;
	} else {
		data[idx].timestamp = t;
		return data[idx].text;
	}
}

Clay_Dimensions TextCache::measure_text(Clay_StringSlice slice,
                                        Clay_TextElementConfig *config) {
	KLAPPT_PROFILE_SCOPE_N("TextCache::measure_text");
	auto font = get_font(config->fontId, config->fontSize);
	int width = 0;
	int height = 0;
	if (!TTF_GetStringSize(font, slice.chars, slice.length, &width, &height)) {
		return {static_cast<float>((float)slice.length *
		                           ((float)config->fontSize / 2.f)),
		        static_cast<float>(config->fontSize)};
	}

	return {static_cast<float>(width), static_cast<float>(height)};
}
// inline void htable_swap(htable_index_t a, htable_index_t b);
// inline uint64_t htable_hash(htable_key_t data) {
//                return hash_text(data.text, data.font_id, data.font_size,
//                data.color);
// }
//
// // implemented in storage.c
// inline bool key_cmp(htable_key_t a, htable_key_t b);
// inline bool key_is_null(htable_key_t a);
// inline void key_set_null(htable_key_t *a);
