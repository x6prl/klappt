#pragma once

#include <utility>

#include "base/arr.h"
#include "base/str_builder.h"
#include "base/str_view.h"
#include "platform/files.h"
#include "ui/translations/langs.h"

struct AssetsDL {
#ifdef __EMSCRIPTEN__
    static constexpr auto HOST = "./"_v;
	// static constexpr auto HOST =
	// 	  "https://github.com/x6prl/klappt-resources/releases/latest/download/"_v;
	// static constexpr auto HOST = "https://0.0.0.0:1234/"_v;
#elif __ANDROID__
#ifndef TRACY_ENABLE
	// static constexpr auto HOST = "http://172.25.16.46:8000/"_v;
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
	static constexpr auto ASR_WHISPER_BASE_ZIP =
		  "sherpa-onnx-whisper-base.zip"_v;

	// NOTE: making changes, check everywhere used the order and the indices
	enum class Type : int32_t {
		XAPIAN_TR = 0,
		OPTIONAL_XAPIAN_DE = 1,
		OPTIONAL_XAPIAN_EN,
		OPTIONAL_TTS,
		OPTIONAL_ASR,
		_COUNT
	};
	struct RemoteAsset {
		bool is_zip_ready_to_unpack{false};
		bool is_unpacked{false};
		bool is_zip_removed{false};

		Size expected_size{-1};
	};
	Arr<RemoteAsset, std::to_underlying(lang_COUNT)> xapian_trs = {};
	Arr<RemoteAsset, std::to_underlying(Type::_COUNT) -
	                       std::to_underlying(Type::OPTIONAL_XAPIAN_DE)>
		  optional_assets{};

	auto for_each_optional(auto f) {
		for (auto i{std::to_underlying(Type::OPTIONAL_XAPIAN_DE)};
		     i < std::to_underlying(Type::_COUNT); ++i) {
			f(optional_assets[i], static_cast<Type>(i));
		}
	}

	StrView zip_path(Arena &a, AssetsDL::Type type, Lang tr_language) {
		switch (type) {
		case Type::XAPIAN_TR:
			return get_writable_file_path_for(a, word_store_leaf(tr_language),
			                                  ".zip"_v);
			break;
		case Type::OPTIONAL_XAPIAN_DE:
			return get_writable_file_path_for(a, word_store_leaf_de(),
			                                  ".zip"_v);
		case Type::OPTIONAL_XAPIAN_EN:
			return get_writable_file_path_for(a, word_store_leaf(lang_en),
			                                  ".zip"_v);
			break;
		case Type::OPTIONAL_ASR:
			return get_writable_file_path_for(a, ASR_WHISPER_BASE_ZIP);
		case Type::OPTIONAL_TTS:
			return get_writable_file_path_for(a,
			                                  TTS_ESPEAKNG_DATA_AND_PIPER_ZIP);
		case Type::_COUNT:
			std::unreachable();
		}
		std::unreachable();
	}

	StrView zip_url(Arena &a, AssetsDL::Type type, Lang tr_language) {
		switch (type) {
		case Type::XAPIAN_TR:
			return get_url_for(a, word_store_leaf(tr_language), ".zip"_v);
			break;
		case Type::OPTIONAL_XAPIAN_DE:
			return get_url_for(a, word_store_leaf_de(), ".zip"_v);
			break;
		case Type::OPTIONAL_XAPIAN_EN:
			return get_url_for(a, word_store_leaf(lang_en), ".zip"_v);
			break;
		case Type::OPTIONAL_TTS:
			return get_url_for(a, TTS_ESPEAKNG_DATA_AND_PIPER_ZIP, {});
			break;
		case Type::OPTIONAL_ASR:
			return get_url_for(a, ASR_WHISPER_BASE_ZIP, {});
			break;
		case Type::_COUNT:
			std::unreachable();
		}
		std::unreachable();
	}

	RemoteAsset &get(AssetsDL::Type type, Lang tr_language) {
		switch (type) {
		case Type::XAPIAN_TR:
			return xapian_trs[std::to_underlying(tr_language)];
		case Type::OPTIONAL_XAPIAN_EN:
			return xapian_trs[std::to_underlying(lang_en)];
		case Type::OPTIONAL_XAPIAN_DE:
		case Type::OPTIONAL_TTS:
		case Type::OPTIONAL_ASR:
			return optional_assets[std::to_underlying(type) -
			                       std::to_underlying(
										 Type::OPTIONAL_XAPIAN_DE)];
		case Type::_COUNT:
			std::unreachable();
		}
		std::unreachable();
	}

	static constexpr StrView words_snapshot_leaf(Lang lang) {
		switch (lang) {
		case lang_ar:
			return "words-ar.dat"_v;
		case lang_en:
			return "words-en.dat"_v;
		case lang_ru:
			return "words-ru.dat"_v;
		case lang_tr:
			return "words-tr.dat"_v;
		default:
			std::unreachable();
		}
		std::unreachable();
	}

	static constexpr StrView word_store_leaf_de() {
		return "words-de.xapian"_v;
	}

	static constexpr StrView word_store_leaf(Lang lang) {
		switch (lang) {
		case lang_ar:
			return "words-ar.xapian"_v;
		case lang_en:
			return "words-en.xapian"_v;
		case lang_ru:
			return "words-ru.xapian"_v;
		case lang_tr:
			return "words-tr.xapian"_v;
		default:
			std::unreachable();
		}
		std::unreachable();
	}

	static constexpr StrView states_store_leaf(Lang lang) {
		switch (lang) {
		case lang_ar:
			return "states-ar.lmdb"_v;
		case lang_en:
			return "states-en.lmdb"_v;
		case lang_ru:
			return "states-ru.lmdb"_v;
		case lang_tr:
			return "states-tr.lmdb"_v;
		default:
			std::unreachable();
		}
		std::unreachable();
	}

	static StrView get_url_for(Arena &a, StrView name, StrView suffix) {
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
};
