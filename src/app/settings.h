#pragma once

#include <SDL3/SDL_log.h>

#include "app/sizes.h"
#include "app/themes.h"
#include "base/str_view.h"
#include "platform/files.h"

#include "ui/translations/langs.h"

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wc99-designator"
#elif defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#endif
inline constexpr StrView states_store_leaf[lang_COUNT] = {
	  [lang_en] = "states-en.lmdb"_v,
	  [lang_ru] = "states-ru.lmdb"_v,
	  [lang_tr] = "states-tr.lmdb"_v,
	  [lang_ar] = "states-ar.lmdb"_v,
};

inline constexpr StrView words_snapshot_leaf[lang_COUNT] = {
	  [lang_en] = "words-en.dat"_v,
	  [lang_ru] = "words-ru.dat"_v,
	  [lang_tr] = "words-tr.dat"_v,
	  [lang_ar] = "words-ar.dat"_v,
};
#if defined(__clang__)
#pragma clang diagnostic pop
#elif defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

struct Settings {
	uint8_t version{0};
	uint8_t _reserved[3];
	Theme::Type theme_type{};
	Lang tr_language = lang_ru;

	/*
	 * FEATURES
	 */
	// subdicts
	bool is_subdict_de{false};
	bool is_subdict_en{false};
	// global features
	bool is_using_suggestions{false};
	// displaying
	bool is_mark_verb_aux_sein{true};
	bool is_mark_verb_irregular{true};
	bool is_mark_verb_separable_prefix{true};
	bool is_mark_verb_type{false};
	bool is_mark_adj_type{false};
	bool is_show_ipa{false};
	bool is_show_origin{false};
	bool is_show_noun_plural_as_suffix{false};
	bool is_show_dictionary_search_only_translations_button{false};
	// modules
	bool is_module_tts{true};
	bool is_module_asr{false};

	uint8_t exercise_round_size{5};

	uint8_t default_screen{0};

	DensityMode density{DensityMode::Normal};
	FontScaleLevel font_scale{FontScaleLevel::Normal};

	int32_t onboarding_stage{0};

	static StrView encode(Arena &a, const Settings &src) {
		// TODO: :O :O :O fixme
		constexpr auto size = sizeof(Settings);
		auto data = a.pushN<char>(size);
		memcpy(data, &src, size);
		return {data, size};
	}
	static bool decode(void *src, Size size, Settings *dst) {
		// TODO: :O :O :O fixme
		if (sizeof(Settings) != size) {
			*dst = Settings{};
			return true;
			return false;
		}
		memcpy(dst, src, size);
		return true;
	}
	void save(Arena &scratch) const {
		auto settingsdat = Settings::encode(scratch, *this);
		file_save_relative(scratch, "settings.dat"_v, settingsdat.data,
		                   settingsdat.size);
		SDL_Log("Settings saved!");
	}
};
