#pragma once

#include "base/arena.h"
#include "base/str_view.h"
#include "domain/word.h"
#include "domain/words.h"

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace WordsCodec {

constexpr uint8_t CODEC_VERSION = 1;
constexpr Size MAX_ENCODED_STRING_SIZE = 1 << 20; // 1 MiB
constexpr Size MAX_ENCODED_BLOB_SIZE = 8 << 20;   // 8 MiB

namespace codec_detail {

inline bool write_varint(unsigned char *&cursor, const unsigned char *end,
                         uint64_t val) {
	for (; val >= 0x80;) {
		if (cursor >= end)
			return false;
		*cursor++ = static_cast<unsigned char>((val & 0x7F) | 0x80);
		val >>= 7;
	}
	if (cursor >= end)
		return false;
	*cursor++ = static_cast<unsigned char>(val & 0x7F);
	return true;
}

inline bool read_varint(const unsigned char *&cursor, const unsigned char *end,
                        uint64_t &val) {
	val = 0;
	uint32_t shift = 0;
	for (; cursor < end;) {
		const unsigned char byte = *cursor++;
		val |= static_cast<uint64_t>(byte & 0x7F) << shift;
		if ((byte & 0x80) == 0)
			return true;
		shift += 7;
		if (shift >= 64)
			return false;
	}
	return false;
}

inline bool write_str(unsigned char *&cursor, const unsigned char *end,
                      StrView str) {
	if (str.size < 0 || str.size > MAX_ENCODED_STRING_SIZE)
		return false;
	if (!write_varint(cursor, end, static_cast<uint64_t>(str.size)))
		return false;
	if (str.size > 0) {
		if (static_cast<size_t>(end - cursor) < static_cast<size_t>(str.size))
			return false;
		std::memcpy(cursor, str.data, static_cast<size_t>(str.size));
		cursor += str.size;
	}
	return true;
}

inline bool read_str(const unsigned char *&cursor, const unsigned char *end,
                     StrView &dst) {
	dst = {};
	uint64_t size = 0;
	if (!read_varint(cursor, end, size))
		return false;
	if (size > static_cast<uint64_t>(MAX_ENCODED_STRING_SIZE) ||
	    static_cast<size_t>(end - cursor) < size) {
		return false;
	}
	if (size > 0) {
		dst = {reinterpret_cast<const char *>(cursor), static_cast<Size>(size)};
		cursor += size;
	}
	return true;
}

template <typename F> inline bool for_each_str(auto &word, F &&f) {
	switch (word.type) {
	case WordType::Nil:
		return false;
	case WordType::Noun:
		if (!f(word.n.lemma) || !f(word.n.plural_suffix))
			return false;
		break;
	case WordType::Verb:
		if (!f(word.v.infinitive) || !f(word.v.third_person) ||
		    !f(word.v.praeteritum) || !f(word.v.auxv_and_past_participle))
			return false;
		break;
	case WordType::Adj:
		if (!f(word.a.lemma) || !f(word.a.comparative) ||
		    !f(word.a.superlative))
			return false;
		break;
	case WordType::Phrase:
		if (!f(word.p.text))
			return false;
		break;
	}

	if (!f(word.translations_raw)) {
		return false;
	}

	return f(word.json_payload);
}

inline bool encode_entry_payload(unsigned char *&cursor,
                                 const unsigned char *end, const Word &word) {
	if (word.type == WordType::Nil || cursor + 2 > end) {
		return false;
	}

	// byte 0: ver | WordType
	*cursor++ = (CODEC_VERSION & 0x0F) | (static_cast<uint8_t>(word.type) << 4);

	// byte 1: bitflags
	uint8_t flags = 0;
	switch (word.type) {
	case WordType::Noun:
		// Gender: unknown(-1)->0, none(0)->1, m(1)->2, f(2)->3, n(3)->4
		flags |= static_cast<uint8_t>(static_cast<int32_t>(word.n.gender) + 1) &
		         0x07;
		break;
	case WordType::Verb:
		if (word.v.is_separable_prefix)
			flags |= (1 << 3);
		break;
	case WordType::Adj:
		if (word.a.is_indeclinable)
			flags |= (1 << 0);
		break;
	default:
		break;
	}

	if (word.was_learned)
		flags |= (1 << 4);
	if (word.in_learning_list)
		flags |= (1 << 5);
	if (word.timestamp != 0)
		flags |= (1 << 6); // has timestamp
	if (word.popularity != 0)
		flags |= (1 << 7); // has popularity
	*cursor++ = flags;

	// WordID
	if (!write_varint(cursor, end, word.word_id.value)) {
		return false;
	}

	// lang_id
	if (cursor >= end)
		return false;
	*cursor++ = word.lang_id;

	// timestamp if != 0
	if (word.timestamp != 0) {
		if (!write_varint(cursor, end, word.timestamp))
			return false;
	}

	// popularity if != 0
	if (word.popularity != 0) {
		if (cursor >= end)
			return false;
		*cursor++ = word.popularity;
	}

	// then strings
	return for_each_str(word,
	                    [&](StrView v) { return write_str(cursor, end, v); });
}

inline bool decode_entry_payload(const unsigned char *&cursor,
                                 const unsigned char *end, Word &word) {
	if (cursor + 2 > end)
		return false;

	const uint8_t byte0 = *cursor++;
	const uint8_t version = byte0 & 0x0F;
	const auto type = static_cast<WordType>(byte0 >> 4);

	if (version != CODEC_VERSION || type == WordType::Nil) {
		return false;
	}

	const uint8_t flags = *cursor++;

	uint64_t raw_word_id = 0;
	if (!read_varint(cursor, end, raw_word_id))
		return false;

	uint8_t lang_id = 0;
	if (cursor >= end)
		return false;
	lang_id = *cursor++;

	uint64_t timestamp = 0;
	if (flags & (1 << 6)) {
		if (!read_varint(cursor, end, timestamp))
			return false;
	}

	uint8_t popularity = 0;
	if (flags & (1 << 7)) {
		if (cursor >= end)
			return false;
		popularity = *cursor++;
	}

	word = {};
	word.word_id = WordId{raw_word_id};
	word.type = type;
	word.was_learned = (flags & (1 << 4)) ? 1 : 0;
	word.in_learning_list = (flags & (1 << 5)) ? 1 : 0;
	word.timestamp = timestamp;
	word.popularity = popularity;
	word.lang_id = lang_id;

	switch (word.type) {
	case WordType::Noun:
		word.n.gender =
			  static_cast<Gender>(static_cast<int32_t>(flags & 0x07) - 1);
		break;
	case WordType::Verb:
		word.v.is_separable_prefix = (flags & (1 << 3)) != 0;
		break;
	case WordType::Adj:
		word.a.is_indeclinable = (flags & (1 << 0)) != 0;
		break;
	default:
		break;
	}

	return for_each_str(word,
	                    [&](StrView &v) { return read_str(cursor, end, v); });
}

} // namespace codec_detail

inline StrView word_encode(Arena &a, const Word &word) {
	if (word.type == WordType::Nil)
		return {};

	uint64_t max_size = 2 + 10 + 10; // header + id + timestamp
	const bool valid = codec_detail::for_each_str(word, [&](StrView v) {
		if (v.size < 0 || v.size > MAX_ENCODED_STRING_SIZE)
			return false;
		max_size += 5 + static_cast<uint64_t>(v.size);
		return max_size <= static_cast<uint64_t>(MAX_ENCODED_BLOB_SIZE);
	});

	if (!valid || max_size > static_cast<uint64_t>(MAX_ENCODED_BLOB_SIZE)) {
		return {};
	}

	auto *data =
		  static_cast<unsigned char *>(a.push(static_cast<Size>(max_size)));
	auto *cursor = data;
	auto *end = data + max_size;

	if (!codec_detail::encode_entry_payload(cursor, end, word)) {
		return {};
	}

	return {reinterpret_cast<const char *>(data),
	        static_cast<Size>(cursor - data)};
}

inline bool word_decode(Arena &a, const void *data, Size size, Word &word) {
	if (!data || size < 3 || size > MAX_ENCODED_BLOB_SIZE)
		return false;

	const auto offset_before = a.offset;
	auto *blob = static_cast<unsigned char *>(a.push(size, 1));
	std::memcpy(blob, data, static_cast<size_t>(size));

	const unsigned char *cursor = blob;
	const unsigned char *end = blob + size;

	if (!codec_detail::decode_entry_payload(cursor, end, word) ||
	    cursor != end) {
		a.offset = offset_before;
		return false;
	}
	return true;
}

inline StrView words_encode(Arena &a, const Words &words) {
	uint64_t max_size = 1 + 5 + 5; // version + entry_count + next_free
	Size count = 0;

	for (auto ref = words.begin(); ref < words.end(); ref.advance(&words)) {
		const auto &w = words[ref];
		if (w.type == WordType::Nil)
			continue;
		++count;
		max_size += 5 + 2 + 10 + 10; // ref + header + id + timestamp
		const bool valid = codec_detail::for_each_str(w, [&](StrView v) {
			if (v.size < 0 || v.size > MAX_ENCODED_STRING_SIZE)
				return false;
			max_size += 5 + static_cast<uint64_t>(v.size);
			return max_size <= static_cast<uint64_t>(MAX_ENCODED_BLOB_SIZE);
		});
		if (!valid || max_size > static_cast<uint64_t>(MAX_ENCODED_BLOB_SIZE)) {
			return {};
		}
	}

	auto *data =
		  static_cast<unsigned char *>(a.push(static_cast<Size>(max_size)));
	auto *cursor = data;
	auto *end = data + max_size;

	*cursor++ = CODEC_VERSION;
	if (!codec_detail::write_varint(cursor, end, static_cast<uint64_t>(count)))
		return {};
	if (!codec_detail::write_varint(cursor, end,
	                                static_cast<uint64_t>(words.next_free)))
		return {};

	for (auto ref = words.begin(); ref < words.end(); ref.advance(&words)) {
		const auto &w = words[ref];
		if (w.type == WordType::Nil)
			continue;

		if (!codec_detail::write_varint(cursor, end,
		                                static_cast<uint64_t>(ref.value)))
			return {};
		if (!codec_detail::encode_entry_payload(cursor, end, w))
			return {};
	}

	return {reinterpret_cast<const char *>(data),
	        static_cast<Size>(cursor - data)};
}

inline bool words_decode(Arena &a, const void *data, Size size, Words &words) {
	if (!data || size < 3 || size > MAX_ENCODED_BLOB_SIZE)
		return false;

	const auto offset_before = a.offset;
	auto *blob = static_cast<unsigned char *>(a.push(size, 1));
	std::memcpy(blob, data, static_cast<size_t>(size));

	const unsigned char *cursor = blob;
	const unsigned char *end = blob + size;

	const uint8_t version = *cursor++;
	if (version != CODEC_VERSION) {
		a.offset = offset_before;
		return false;
	}

	uint64_t entry_count = 0;
	uint64_t next_free = 0;
	if (!codec_detail::read_varint(cursor, end, entry_count) ||
	    !codec_detail::read_varint(cursor, end, next_free)) {
		a.offset = offset_before;
		return false;
	}

	if (entry_count > Words::MAX_WORDS || next_free > Words::MAX_WORDS) {
		a.offset = offset_before;
		return false;
	}

	words = {};
	words.next_free = static_cast<Size>(next_free);

	for (uint64_t i = 0; i < entry_count; ++i) {
		uint64_t ref_val = 0;
		if (!codec_detail::read_varint(cursor, end, ref_val)) {
			a.offset = offset_before;
			return false;
		}

		if (ref_val == 0 || ref_val >= Words::MAX_WORDS ||
		    words.used[ref_val]) {
			a.offset = offset_before;
			return false;
		}

		auto &word = words.words[ref_val];
		if (!codec_detail::decode_entry_payload(cursor, end, word)) {
			a.offset = offset_before;
			return false;
		}

		words.used[ref_val] = true;
	}

	if (cursor != end) {
		a.offset = offset_before;
		return false;
	}

	words.size = static_cast<Size>(entry_count);
	return true;
}

} // namespace WordsCodec
