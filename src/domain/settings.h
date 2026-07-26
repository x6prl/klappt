#pragma once

#include "app/assets_dl.h"
#include "base/str_view.h"
#include "platform/files.h"
#include "ui/themes.h"
#include "ui/translations/langs.h"

#include <SDL3/SDL_log.h>
#include <cstdint>
#include <cstring>
#include <utility>

struct Settings {
	Theme::Type theme_type{};
	int32_t exercise_round_size{5};
	Lang tr_language = lang_ru;
	int32_t onboarding_stage{0};
	AssetsDL assets;
	bool is_using_also_de{true};
	bool is_using_tts{true};
	bool is_using_asr{false};

	static void for_every_lang(auto f) {
		for (int32_t i{0};
		     i < std::to_underlying(lang_COUNT);
		     ++i) {
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
			return false;
		}
		memcpy(dst, src, size);
		return true;
	}
	void save(Arena &scratch) const {
		auto settingsdat = Settings::encode(scratch, *this);
		file_save(scratch, "settings.dat"_v, settingsdat.data,
		settingsdat.size);
		SDL_Log("Settings saved!");
	}
};
