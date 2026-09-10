#include "textcache.h"

#include <algorithm>
#include <iterator>

#include <SDL3/SDL_log.h>
#include <SDL3_ttf/SDL_ttf.h>

#include "base/profiler.h"

namespace {

// constexpr uint32_t rgba_u32(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
// 	return (uint32_t(r) << 24) | (uint32_t(g) << 16) | (uint32_t(b) << 8) |
// 	       uint32_t(a);
// }
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

bool is_arabic_font(uint16_t font_id) { return font_id == FontID::ARABIC_MAIN; }

bool is_arabic_codepoint(uint32_t cp) {
	return (cp >= 0x0600u && cp <= 0x06FFu) ||
	       (cp >= 0x0750u && cp <= 0x077Fu) ||
	       (cp >= 0x0870u && cp <= 0x089Fu) ||
	       (cp >= 0x08A0u && cp <= 0x08FFu) ||
	       (cp >= 0xFB50u && cp <= 0xFDFFu) ||
	       (cp >= 0xFE70u && cp <= 0xFEFFu) ||
	       (cp >= 0x10E60u && cp <= 0x10E7Fu) ||
	       (cp >= 0x1EE00u && cp <= 0x1EEFFu);
}

bool str_contains_arabic(StrView str) {
	KLAPPT_PROFILE_SCOPE_N("TextCache::str_contains_arabic");
	const char *ptr = str.data;
	size_t remaining = static_cast<size_t>(str.size);
	while (remaining > 0) {
		const auto cp = SDL_StepUTF8(&ptr, &remaining);
		if (!cp) {
			break;
		}
		if (is_arabic_codepoint(cp)) {
			return true;
		}
	}
	return false;
}

uint16_t normalize_font_size(uint16_t font_size) {
	if (font_size < 24) {
		return font_size;
	}
	// round large fonts around 2px
	return static_cast<uint16_t>(((font_size + 1u) / 2u) * 2u);
}

void configure_text_for_layout(TTF_Text *text, bool use_arabic_layout) {
	KLAPPT_PROFILE_SCOPE_N("TextCache::configure_text_for_layout");
	if (!text) {
		return;
	}
	if (use_arabic_layout) {
		TTF_SetTextDirection(text, TTF_DIRECTION_RTL);
		TTF_SetTextScript(text, TTF_StringToTag("Arab"));
	} else {
		TTF_SetTextDirection(text, TTF_DIRECTION_LTR);
		TTF_SetTextScript(text, TTF_StringToTag("Latn"));
	}
}

TextCache::Idx lp_home_index(Hash h, TextCache::Idx table_size) {
	return h & (table_size - 1);
}
bool key_is_null(TextCache::TextEntry k) { return k.text == nullptr; }
void key_set_null(TextCache::TextEntry *k) {
	if (k->text) {
		TTF_DestroyText(k->text);
		k->text = nullptr;
	}
}
Hash htable_hash(const TextCache::TextEntry &d) { return d.hash; }
bool key_cmp(TextCache::TextEntry &d, Hash hash, StrView str, uint16_t font_id,
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
	TextEntry tmp = text_cache_data[a];
	text_cache_data[a] = text_cache_data[b];
	text_cache_data[b] = tmp;
}

Pair<TextCache::Idx, Hash> TextCache::htable_lookup(StrView str,
                                                    uint16_t font_id,
                                                    uint16_t font_size,
                                                    uint32_t color) {
	KLAPPT_PROFILE_SCOPE_N("TextCache::htable_lookup");
	const auto h = hash_text(str, font_id, font_size, color);
	const auto home = lp_home_index(h, TEXT_CACHE_HASHMAP_SIZE);
	const TextCache::Idx mask = TEXT_CACHE_HASHMAP_SIZE - 1;

	for (TextCache::Idx step = 0; step < TEXT_CACHE_HASHMAP_SIZE; ++step) {
		TextCache::Idx idx = (home + step) & mask;
		if (key_is_null(text_cache_data[idx])) {
			return {TEXT_CACHE_HASHMAP_SIZE, h};
		}
		if (key_cmp(text_cache_data[idx], h, str, font_id, font_size, color)) {
			return {idx, h};
		}
	}
	return {TEXT_CACHE_HASHMAP_SIZE, h};
}

TextCache::Idx TextCache::htable_erase(TextCache::Idx start_idx) {
	KLAPPT_PROFILE_SCOPE_N("TextCache::htable_erase");

	const TextCache::Idx mask = TEXT_CACHE_HASHMAP_SIZE - 1;
	auto hole = start_idx;
	auto next = hole;

	while (true) {
		next = (next + 1) & mask;

		if (key_is_null(text_cache_data[next]) || next == start_idx) {
			break;
		}

		const TextCache::Idx home = lp_home_index(
			  htable_hash(text_cache_data[next]), TEXT_CACHE_HASHMAP_SIZE);

		if (((hole - home) & mask) < ((next - home) & mask)) {
			htable_swap(hole, next);
			hole = next;
		}
	}

	key_set_null(&text_cache_data[hole]);
	if (active_count > 0) {
		--active_count;
	}
	return hole;
}

TextCache::Idx TextCache::lp_find_free_slot(Hash h, TextCache::TimestampSec t) {
	KLAPPT_PROFILE_SCOPE_N("TextCache::lp_find_free_slot");
	TextCache::Idx home = lp_home_index(h, TEXT_CACHE_HASHMAP_SIZE);

	// evict oldest entry if threshold exceeded
	// TODO: gather stats............
	if (active_count >= MAX_OCCUPIED) {
		KLAPPT_PROFILE_SCOPE_N("TextCache::evict_oldest_entries");
		TextCache::Idx oldest = home;
		for (TextCache::Idx i = 0; i < TEXT_CACHE_HASHMAP_SIZE; ++i) {
			if (!key_is_null(text_cache_data[i])) {
				if (key_is_null(text_cache_data[oldest]) ||
				    text_cache_data[i].timestamp.tss <
				          text_cache_data[oldest].timestamp.tss) {
					oldest = i;
				}
			}
		}
		if (!key_is_null(text_cache_data[oldest])) {
			htable_erase(oldest);
		}
	}

	TextCache::Idx oldest = home;

	for (TextCache::Idx step = 0; step < TEXT_CACHE_HASHMAP_SIZE; ++step) {
		const TextCache::Idx i = (home + step) & (TEXT_CACHE_HASHMAP_SIZE - 1);
		if (key_is_null(text_cache_data[i])) {
			return i;
		}
		if (text_cache_data[i].is_obsolete(t)) {
			return htable_erase(i);
		}
		if (text_cache_data[i].timestamp.tss <
		    text_cache_data[oldest].timestamp.tss) {
			oldest = i;
		}
	}

	// if no obsolete entry was found on the probe chain
	SDL_LogError(SDL_LOG_CATEGORY_ERROR,
	             "TextCache full; evicting oldest entry");
	// TODO: gather stats
	htable_erase(oldest);

	// but probe from home
	if (key_is_null(text_cache_data[home])) {
		return home;
	} else {
		++home;
		// lp_statistics.hash_collision_count++;
	repeat:
		for (; home < TEXT_CACHE_HASHMAP_SIZE; ++home) {
			// lp_statistics.total_went_due_to_collisions++;
			if (key_is_null(text_cache_data[home])) {
				return home;
			}
		}
		home = 0;
		goto repeat;
	}
	std::unreachable();
	// unreachable since we removed one
}

TTF_Font *TextCache::get_font(uint16_t font_id, uint16_t font_size) {
	KLAPPT_PROFILE_SCOPE_N("TextCache::get_font");
	font_size = normalize_font_size(font_size);

	// TODO: gather misses
	if (font_id < FontID::COUNT && font_size < FAST_FONT_MAX) {
		TTF_Font *cached = fast_fonts[font_id][font_size];
		if (cached) {
			return cached;
		}
	}

	auto it = fonts.begin();
	for (; it != fonts.end() && it->second; ++it) {
		if (it->first.font_size == font_size && it->first.font_id == font_id) {
			if (font_id < FontID::COUNT && font_size < FAST_FONT_MAX) {
				fast_fonts[font_id][font_size] = it->second;
			}
			return it->second;
		}
	}

	// NOTE: not found
	// TODO: gather stats
	if (it != fonts.end()) {
		KLAPPT_PROFILE_SCOPE_N("Create font");
		KLAPPT_PROFILE_NAME_F("TextCache::get_font id=%u size=%u", font_id,
		                      font_size);

		TTF_Font *font = TTF_CopyFont(base_fonts[font_id]);
		if (!font) {
			SDL_LogError(SDL_LOG_CATEGORY_ERROR,
			             "cannot create font size %u id %u: %s", font_size,
			             font_id, SDL_GetError());
			exit(-6);
		}
		TTF_SetFontSize(font, font_size);

		if (is_arabic_font(font_id)) {
			TTF_SetFontDirection(font, TTF_DIRECTION_RTL);
			TTF_SetFontScript(font, TTF_StringToTag("Arab"));
		} else {
			TTF_SetFontDirection(font, TTF_DIRECTION_LTR);
			TTF_SetFontScript(font, TTF_StringToTag("Latn"));
		}

		it->first = {font_id, font_size};
		it->second = font;

		if (font_id < FontID::COUNT && font_size < FAST_FONT_MAX) {
			fast_fonts[font_id][font_size] = font;
		}
		return font;
	} else {
		SDL_LogError(SDL_LOG_CATEGORY_ERROR,
		             "cannot create font size %u id %u: no space", font_size,
		             font_id);
		exit(-6);
	}
}

TTF_Text *TextCache::get(StrView str, uint16_t font_id, uint16_t font_size,
                         Clay_Color clay_color) {
	KLAPPT_PROFILE_SCOPE_N("TextCache::get");
	font_size = normalize_font_size(font_size);
	auto color = clay_color_to_u32(clay_color);
	auto [idx, hash] = htable_lookup(str, font_id, font_size, color);
	if (idx == TEXT_CACHE_HASHMAP_SIZE) {
		auto font = get_font(font_id, font_size);
		const bool use_arabic_layout =
			  is_arabic_font(font_id) && str_contains_arabic(str);
		auto *text = TTF_CreateText(engine, font, str.data, str.size);
		configure_text_for_layout(text, use_arabic_layout);
		auto ncolor = clay_color_normalize(clay_color);
		TTF_SetTextColorFloat(text, ncolor.r, ncolor.g, ncolor.b, ncolor.a);
		auto free_idx = lp_find_free_slot(hash, current_time);
		if (free_idx != TEXT_CACHE_HASHMAP_SIZE) {
			text_cache_data[free_idx] = {
				  hash,    static_cast<uint32_t>(str.size),
				  font_id, font_size,
				  color,   current_time,
				  text};
			++active_count;
		} else {
			SDL_LogError(SDL_LOG_CATEGORY_ERROR,
			             "No space for the text cache!");
			// TODO: evict some...
			exit(-7);
		}
		return text;
	} else {
		text_cache_data[idx].timestamp = current_time;
		return text_cache_data[idx].text;
	}
}

Clay_Dimensions TextCache::measure_text(Clay_StringSlice slice,
                                        Clay_TextElementConfig *config) {
	if (slice.length <= 0) {
		return {0.0f, static_cast<float>(config->fontSize)};
	}

	KLAPPT_PROFILE_SCOPE_N("TextCache::measure_text");
	const auto font_size = normalize_font_size(config->fontSize);
	const StrView text{slice.chars, static_cast<Size>(slice.length)};
	const Hash h = hash_text(text, config->fontId, font_size, 0);

	// NOTE: cache probe up to MEASURE_CACHE_FAST_PROBE times
	const Idx home = lp_home_index(h, MEASURE_CACHE_SIZE);
	const size_t check_size =
		  std::min((size_t)slice.length, std::size(MeasureEntry{}.text));

	for (Idx step = 0; step < MEASURE_CACHE_FAST_PROBE; ++step) {
		const Idx idx = (home + step) & (MEASURE_CACHE_SIZE - 1);
		const auto &entry = measure_cache_data[idx];

		if (entry.hash == h && entry.font_id == config->fontId &&
		    entry.font_size == font_size && entry.text_size == slice.length) {
			if (std::memcmp(entry.text, slice.chars, check_size) == 0) {
				return entry.dims;
			}
		}
		if (entry.text_size == 0) {
			break;
		}
	}

	// NOTE: cache miss
	// TODO: gather stats and maybe move out .text from MeasureEntry?
	auto font = get_font(config->fontId, font_size);
	int width = 0;
	int height = 0;
	{
		KLAPPT_PROFILE_SCOPE_N("TTF_GetStringSize");
		if (!TTF_GetStringSize(font, slice.chars, slice.length, &width,
		                       &height)) {
			return {(float)slice.length * ((float)font_size / 2.f),
			        static_cast<float>(font_size)};
		}
	}

	const Clay_Dimensions dims{static_cast<float>(width),
	                           static_cast<float>(height)};

	// NOTE: store calculated
	Idx store_idx = home;
	for (Idx step = 0; step < MEASURE_CACHE_FAST_PROBE; ++step) {
		const Idx idx = (home + step) & (MEASURE_CACHE_SIZE - 1);
		if (measure_cache_data[idx].text_size == 0) {
			store_idx = idx;
			break;
		}
	}
	// if there is no empty slot, we put it right to the home position
	// TODO: play with MEASURE_CACHE_FAST_PROBE

	auto &dest = measure_cache_data[store_idx];
	dest.hash = h;
	dest.font_id = config->fontId;
	dest.font_size = font_size;
	dest.text_size = static_cast<uint16_t>(slice.length);
	dest.dims = dims;
	std::memcpy(dest.text, slice.chars, check_size);

	return dims;
}

void TextCache::prewarm(float scale, SDL_Renderer *renderer) {
	// TODO: think about it.............

	// KLAPPT_PROFILE_SCOPE_N("TextCache::prewarm");
	//
	// static constexpr auto COMMON_GLYPHS = "abcdefghijklmnopqrstuvwxyz"
	// 									  "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
	// 									  "0123456789"
	// 									  " .,:;!?'\"-()[]/"_v;
	//
	// // TODO: gather stats((
	// also NOTE: that we round big fonts
	//
	// uint16_t word_card_text_size = (static_cast<uint16_t>(scale * 13.f));
	// uint16_t CARD_SIZES[] = {word_card_text_size}; //, udpi(20)};
	// uint16_t FONTS[] = {
	// 	  FontID::MONOSPACE_REGULAR, // word cart geman font
	// 	  FontID::MAIN,              // word cart translation non-arabic
	// };
	//
	// static Size i = 0;
	// static Size j = 0;
	// static Size offset = 0;
	// for (; i < std::size(CARD_SIZES) ; ++i) {
	// 	KLAPPT_PROFILE_SCOPE_N("TextCache::CARD_LOOP");
	// 	for (j = 0; j < std::size(FONTS) ;) {
	// 		KLAPPT_PROFILE_SCOPE_N("TextCache::CARD_FONT_LOOP");
	// 		auto size = CARD_SIZES[i];
	// 		auto font_id = FONTS[j];
	// 		TTF_Font *font = get_font(font_id, size);
	// 		if (!font)
	// 			continue;
	//
	// 		auto shift = COMMON_GLYPHS.size / 8;
	// 		TTF_Text *text =
	// 			  TTF_CreateText(engine, font, COMMON_GLYPHS.data + offset,
	// 		                     shift);
	// 		if (text) {
	// 			TTF_DrawRendererText(text, 0.0f, 0.0f);
	// 			TTF_DestroyText(text);
	// 		}
	// 		if (offset + shift >= COMMON_GLYPHS.size) {
	// 			offset = 0;
	// 			++j;
	// 			return;
	// 		} else {
	// 			offset += shift;
	// 			return;
	// 		}
	// 	}
	// }
}
