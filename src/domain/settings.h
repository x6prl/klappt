#pragma once

#include "platform/files.h"
#include "ui/themes.h"
#include "base/str_view.h"

#include <cstdint>
#include <cstring>

struct Settings {
	enum class TranslationLanguage : int32_t {
		Arabic,
		English,
		Russian,
		Turkish,
		COUNT
	};

	Theme::Type theme_type{};
	int32_t exercise_round_size{5};
	TranslationLanguage tr_language = TranslationLanguage::Russian;
	int32_t onboarding_stage{0};

	static void for_every_lang(auto f) {
		for (int32_t i{0};
		     i < static_cast<int>(Settings::TranslationLanguage::COUNT); ++i) {
			auto lang = static_cast<Settings::TranslationLanguage>(i);
			f(i, lang);
		}
	}

	static constexpr StrView
	translation_language_code(TranslationLanguage lang) {
		switch (lang) {
		case TranslationLanguage::Arabic:
			return "ar"_v;
		case TranslationLanguage::English:
			return "en"_v;
		case TranslationLanguage::Russian:
			return "ru"_v;
		case TranslationLanguage::Turkish:
			return "tr"_v;
		default:;
		}
		return "ru"_v;
	}

	static constexpr StrView translation_source_leaf(TranslationLanguage lang) {
		switch (lang) {
		case TranslationLanguage::Arabic:
			return "ar.txt"_v;
		case TranslationLanguage::English:
			return "en.txt"_v;
		case TranslationLanguage::Russian:
			return "ru.txt"_v;
		case TranslationLanguage::Turkish:
			return "tr.txt"_v;
		default:;
		}
		return "ru.txt"_v;
	}

	static constexpr StrView words_snapshot_leaf(TranslationLanguage lang) {
		switch (lang) {
		case TranslationLanguage::Arabic:
			return "words-ar.dat"_v;
		case TranslationLanguage::English:
			return "words-en.dat"_v;
		case TranslationLanguage::Russian:
			return "words-ru.dat"_v;
		case TranslationLanguage::Turkish:
			return "words-tr.dat"_v;
		default:;
		}
		return "words-ru.dat"_v;
	}

	static constexpr StrView word_store_leaf(TranslationLanguage lang) {
		switch (lang) {
		case TranslationLanguage::Arabic:
			return "words-ar.xapian"_v;
		case TranslationLanguage::English:
			return "words-en.xapian"_v;
		case TranslationLanguage::Russian:
			return "words-ru.xapian"_v;
		case TranslationLanguage::Turkish:
			return "words-tr.xapian"_v;
		default:;
		}
		return "words-ru.xapian"_v;
	}

	static constexpr StrView states_store_leaf(TranslationLanguage lang) {
		switch (lang) {
		case TranslationLanguage::Arabic:
			return "states-ar.lmdb"_v;
		case TranslationLanguage::English:
			return "states-en.lmdb"_v;
		case TranslationLanguage::Russian:
			return "states-ru.lmdb"_v;
		case TranslationLanguage::Turkish:
			return "states-tr.lmdb"_v;
		default:;
		}
		return "states-ru.lmdb"_v;
	}

	static StrView encode(Arena &a, const Settings &src) {
		constexpr auto size = sizeof(Settings);
		auto data = a.pushN<char>(size);
		memcpy(data, &src, size);
		return {data, size};
	}
	static void decode(void *src, Size size, Settings *dst) {
		if (sizeof(Settings) != size) {
			exit(72);
		}
		memcpy(dst, src, size);
	}
	void save(Arena &a) const {
		{
			// wha? TODO: rewrite
			auto settingsdat = Settings::encode(a, *this);
			file_save("settings.dat"_v, settingsdat.data, settingsdat.size);
		}
	}
};
