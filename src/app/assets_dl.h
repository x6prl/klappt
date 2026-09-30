#pragma once

#include "base/arr.h"
#include "base/str_builder.h"
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

static constexpr auto TTS_ESPEAKNG_DATA_AND_PIPER_ZIP =
	  "espeak-ng-data-and-piper.zip"_v;
static constexpr auto ASR_WHISPER_BASE_ZIP = "sherpa-onnx-whisper-base.zip"_v;

// NOTE: making changes, check everywhere used the order and the indices
enum class Type : int32_t {
	XAPIAN_USER_LANG = 0,
	OPTIONAL_XAPIAN_DE = 1,
	OPTIONAL_XAPIAN_EN,
	OPTIONAL_TTS,
	OPTIONAL_ASR,
	_COUNT
};

struct RemoteAssetStatus {
	bool is_zip_ready_to_unpack{false};
	bool is_unpacked{false};
	bool is_zip_removed{false};

	Size expected_size{-1};
};

inline constexpr StrView word_store_leaf_de = "words-de.xapian"_v;

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

inline constexpr StrView word_store_leaf[lang_COUNT] = {
	  [lang_en] = "words-en.xapian"_v,
	  [lang_ru] = "words-ru.xapian"_v,
	  [lang_tr] = "words-tr.xapian"_v,
	  [lang_ar] = "words-ar.xapian"_v,
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

inline Arr<RemoteAssetStatus, (Size)lang_COUNT> xapian_trs = {};
inline Arr<RemoteAssetStatus, (Size)Type::_COUNT - (Size)Type::OPTIONAL_XAPIAN_DE>
	  optional_assets{};

inline RemoteAssetStatus &get(AssetsDL::Type type, Lang tr_language) {
	switch (type) {
	case Type::XAPIAN_USER_LANG:
		return xapian_trs[tr_language];
	case Type::OPTIONAL_XAPIAN_EN:
		return xapian_trs[(Size)(lang_en)];
	case Type::OPTIONAL_XAPIAN_DE:
	case Type::OPTIONAL_TTS:
	case Type::OPTIONAL_ASR:
		return optional_assets[(int)(type) - (int)(Type::OPTIONAL_XAPIAN_DE)];
	case Type::_COUNT:
	default:
		break;
	}
	return xapian_trs[tr_language];
}

inline auto for_each_optional(auto f) {
	for (auto i{(Size)(Type::OPTIONAL_XAPIAN_DE)}; i < (Size)(Type::_COUNT);
	     ++i) {
		const auto type = static_cast<Type>(i);
		f(get(type, lang_en), type);
	}
}

struct AssetFileTarget {
	StrView name{};
	StrView suffix{};
};

inline AssetFileTarget asset_file_target(AssetsDL::Type type,
                                         Lang tr_language) {
	switch (type) {
	case Type::XAPIAN_USER_LANG:
		return {word_store_leaf[tr_language], ".zip"_v};
	case Type::OPTIONAL_XAPIAN_DE:
		return {word_store_leaf_de, ".zip"_v};
	case Type::OPTIONAL_XAPIAN_EN:
		return {word_store_leaf[lang_en], ".zip"_v};
	case Type::OPTIONAL_TTS:
		return {TTS_ESPEAKNG_DATA_AND_PIPER_ZIP, {}};
	case Type::OPTIONAL_ASR:
		return {ASR_WHISPER_BASE_ZIP, {}};
	case Type::_COUNT:
		break;
	default:
		break;
	}
	return {};
}

inline StrView zip_path(Arena &a, AssetsDL::Type type, Lang tr_language) {
	const auto [name, suffix] = asset_file_target(type, tr_language);
	return suffix ? get_writable_file_path_for(a, name, suffix)
	              : get_writable_file_path_for(a, name);
}

inline StrView get_url_for(Arena &a, StrView name, StrView suffix) {
	if (suffix) {
		StrBuilder strs{};
		strs.push(a, HOST);
		strs.push(a, name);
		strs.push(a, suffix);
		return strs.join(a);
	} else {
		return StrView::concat(a, HOST, name);
	}
}

inline StrView zip_url(Arena &a, AssetsDL::Type type, Lang tr_language) {
	const auto [name, suffix] = asset_file_target(type, tr_language);
	return get_url_for(a, name, suffix);
}
} // namespace AssetsDL
