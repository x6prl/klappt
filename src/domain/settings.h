#pragma once

#include <SDL3/SDL_log.h>

#include "app/assets_dl.h"
#include "app/sizes.h"
#include "app/themes.h"
#include "base/str_view.h"
#include "platform/files.h"
#include "ui/translations/langs.h"

struct Settings {
	Theme::Type theme_type{};
	Lang tr_language = lang_ru;

	/*
	 * FEATURES
	 */
	// subdicts
	bool is_subdict_de{false};
	bool is_subdict_en{false};
	// behaviour
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
	AssetsDL assets;

	static void for_every_lang(auto f) {
		for (int i{0}; i < (int)std::to_underlying(lang_COUNT); ++i) {
			auto lang = static_cast<Lang>(i);
			f(i, lang);
		}
	}

	AssetsDL::RemoteAsset &asset(AssetsDL::Type t) {
		return assets.get(t, tr_language);
	}

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
