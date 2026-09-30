#pragma once

#include "base/arr.h"
#include "base/str_view.h"

#include "platform/files.h"

#include "ui/translations/langs.h"

namespace AssetsDL {
#ifdef __EMSCRIPTEN__
static constexpr auto HOST = "./"_v;
// static constexpr auto HOST =
// 	  "https://github.com/x6prl/klappt-resources/releases/latest/download/"_v;
// static constexpr auto HOST = "https://0.0.0.0:1234/"_v;
#elif __ANDROID__
#ifndef TRACY_ENABLE
// static constexpr auto HOST = "http://172.25.16.46:8000/"_v;
// static constexpr auto HOST = "http://10.42.0.1:8000/"_v;
static constexpr auto HOST =
	  "https://github.com/x6prl/klappt-resources/releases/latest/download/"_v;
#else
static constexpr auto HOST = "http://10.42.0.1:8000/"_v;
#endif
#else
static constexpr auto HOST = "http://0.0.0.0:8000/"_v;
// static constexpr auto HOST = "http://10.224.66.46:8000/"_v;
#endif

enum AssetId : uint8_t {
	DICT_EN = 0,
	DICT_RU,
	DICT_TR,
	DICT_AR,
	DICT_DE,
	OPT_TTS,
	OPT_ASR,
	_COUNT
};

inline constexpr AssetId dict_asset_id_for_lang(Lang lang) noexcept {
	switch (lang) {
	case lang_en: return AssetId::DICT_EN;
	case lang_ru: return AssetId::DICT_RU;
	case lang_tr: return AssetId::DICT_TR;
	case lang_ar: return AssetId::DICT_AR;
	default:      return AssetId::DICT_EN;
	}
}

struct AssetDesc {
	StrView id_name;       // e.g. "words-ru"
	StrView target_file;   // e.g. "words-ru.xapian"
	StrView zip_file;      // e.g. "words-ru.xapian.zip"
};

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wc99-designator"
#elif defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#endif
inline constexpr Arr<AssetDesc, (Size)AssetId::_COUNT> ASSET_REGISTRY = {{
	[AssetId::DICT_EN] = {"words-en"_v, "words-en.xapian"_v, "words-en.xapian.zip"_v},
	[AssetId::DICT_RU] = {"words-ru"_v, "words-ru.xapian"_v, "words-ru.xapian.zip"_v},
	[AssetId::DICT_TR] = {"words-tr"_v, "words-tr.xapian"_v, "words-tr.xapian.zip"_v},
	[AssetId::DICT_AR] = {"words-ar"_v, "words-ar.xapian"_v, "words-ar.xapian.zip"_v},
	[AssetId::DICT_DE] = {"words-de"_v, "words-de.xapian"_v, "words-de.xapian.zip"_v},
	[AssetId::OPT_TTS] = {"tts"_v,      "espeak-ng-data-and-piper"_v, "espeak-ng-data-and-piper.zip"_v},
	[AssetId::OPT_ASR] = {"asr"_v,      "sherpa-onnx-whisper-base"_v, "sherpa-onnx-whisper-base.zip"_v},
}};

#if defined(__clang__)
#pragma clang diagnostic pop
#elif defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

inline AssetId find_asset_by_path(StrView file_path) {
	for (Size i = 0; i < (Size)AssetId::_COUNT; ++i) {
		const auto &desc = ASSET_REGISTRY[i];
		if (file_path.is_contains_substr(desc.zip_file)) {
			return static_cast<AssetId>(i);
		}
	}
	return AssetId::_COUNT;
}

inline StrView zip_path(Arena &a, AssetId id) {
	return get_writable_file_path_for(a, ASSET_REGISTRY[(Size)id].zip_file);
}

inline StrView zip_tmp_path(Arena &a, AssetId id) {
	return get_writable_file_path_for(a, ASSET_REGISTRY[(Size)id].zip_file, ".tmp"_v);
}

inline StrView marker_installed_path(Arena &a, AssetId id) {
	return get_writable_file_path_for(a, ASSET_REGISTRY[(Size)id].target_file, ".installed"_v);
}

inline StrView target_path(Arena &a, AssetId id) {
	return get_writable_file_path_for(a, ASSET_REGISTRY[(Size)id].target_file);
}

inline StrView zip_url(Arena &a, AssetId id) {
	return StrView::concat(a, HOST, ASSET_REGISTRY[(Size)id].zip_file);
}

inline bool is_installed(Arena &a, AssetId id) {
	return fs_exists(marker_installed_path(a, id));
}

inline void mark_installed(Arena &a, AssetId id) {
	fs_touch(marker_installed_path(a, id));
}

inline void clean_asset(Arena &a, AssetId id) {
	fs_remove(zip_tmp_path(a, id));
	fs_remove(zip_path(a, id));
	fs_remove(marker_installed_path(a, id));
	fs_remove_all(target_path(a, id));
}
} // namespace AssetsDL
