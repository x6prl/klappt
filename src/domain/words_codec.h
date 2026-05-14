#pragma once

#include "base/arena.h"
#include "words.h"
#include <cstdint>
#include <cstring>
#include <memory>

// TODO: rewrite
namespace WordsCodec {

constexpr uint32_t MAGIC = 0x57434432; // "WCD2"
constexpr uint32_t VERSION = 1;
constexpr Size MAX_ENCODED_STRING_SIZE =
	  1 << 20; // 1 MiB per field is already excessive here.
constexpr Size MAX_ENCODED_BLOB_SIZE =
	  8 << 20; // Keep serialization well below the web temp arena size.
constexpr uint32_t SINGLE_WORD_MAGIC = 0x574F5244; // "WORD"
constexpr uint32_t SINGLE_WORD_VERSION = 1;

struct Entry {
	uint64_t word_id{};
	uint16_t ref{};
	uint8_t type{};
	int8_t aux{};
	int8_t in_learning_list{};
	int8_t was_learned{};
};

struct Header {
	uint32_t magic{MAGIC};
	uint32_t version{VERSION};
	uint32_t entry_count{};
	int32_t next_free{1};
};

struct SingleWordHeader {
	uint32_t magic{SINGLE_WORD_MAGIC};
	uint32_t version{SINGLE_WORD_VERSION};
	uint64_t word_id{};
	uint8_t type{};
	int8_t aux{};
	int8_t in_learning_list{};
	int8_t was_learned{};
	uint8_t reserved{};
};

inline bool is_valid_encoded_str_view(StrView view) {
	return view.size >= 0 && view.size <= MAX_ENCODED_STRING_SIZE;
}

inline bool write_str_view(uint8_t *&cursor, const uint8_t *end, StrView src) {
	if (!is_valid_encoded_str_view(src)) {
		return false;
	}

	const auto src_size = static_cast<size_t>(src.size);
	if (static_cast<size_t>(end - cursor) < sizeof(Size) + src_size) {
		return false;
	}

	std::memcpy(cursor, &src.size, sizeof(Size));
	cursor += sizeof(Size);
	if (src_size) {
		std::memcpy(cursor, src.data, src_size);
		cursor += src_size;
	}
	return true;
}

inline bool read_str_view(const uint8_t *&cursor, const uint8_t *end,
                          StrView &dst) {
	dst = {};

	if (static_cast<size_t>(end - cursor) < sizeof(Size)) {
		return false;
	}

	Size size{};
	std::memcpy(&size, cursor, sizeof(Size));
	cursor += sizeof(Size);
	if (size < 0 ||
	    static_cast<size_t>(end - cursor) < static_cast<size_t>(size)) {
		return false;
	}

	if (size) {
		dst = {reinterpret_cast<const char *>(cursor), size};
		cursor += size;
	}
	return true;
}

template <class F> inline bool for_each_str_view(const Word &word, F &&f) {
	if (!f(word.translations_raw) || !f(word.grammar)) {
		return false;
	}
	switch (word.type) {
	case WordType::Nil:
		return false;
	case WordType::Noun:
		return f(word.n.lemma) && f(word.n.plural_suffix);
	case WordType::Verb:
		return f(word.v.infinitive) && f(word.v.third_person) &&
		       f(word.v.praeteritum) && f(word.v.auxv_and_past_participle);
	case WordType::Adj:
		return f(word.a.lemma) && f(word.a.comparative) &&
		       f(word.a.superlative);
	case WordType::Phrase:
		return f(word.p.text);
	}

	return false;
}

template <class F> inline bool for_each_str_view(Word &word, F &&f) {
	if (!f(word.translations_raw) || !f(word.grammar)) {
		return false;
	}
	switch (word.type) {
	case WordType::Nil:
		return false;
	case WordType::Noun:
		return f(word.n.lemma) && f(word.n.plural_suffix);
	case WordType::Verb:
		return f(word.v.infinitive) && f(word.v.third_person) &&
		       f(word.v.praeteritum) && f(word.v.auxv_and_past_participle);
	case WordType::Adj:
		return f(word.a.lemma) && f(word.a.comparative) &&
		       f(word.a.superlative);
	case WordType::Phrase:
		return f(word.p.text);
	}

	return false;
}

inline Size entry_size(const Word &word) {
	uint64_t total = sizeof(Entry);
	const auto ok = for_each_str_view(word, [&](StrView view) {
		if (!is_valid_encoded_str_view(view)) {
			return false;
		}
		total += sizeof(Size) + static_cast<uint64_t>(view.size);
		return total <= static_cast<uint64_t>(MAX_ENCODED_BLOB_SIZE);
	});
	return ok ? static_cast<Size>(total) : 0;
}

inline uint32_t used_count(const Words &words) {
	uint32_t count = 0;
	for (auto ref = words.begin(); ref < words.end(); ref.advance(&words)) {
		++count;
	}
	return count;
}

inline Size encoded_size(const Words &words) {
	uint64_t total = sizeof(Header);
	for (auto ref = words.begin(); ref < words.end(); ref.advance(&words)) {
		const auto size = entry_size(words.words[ref.value]);
		if (size <= 0) {
			return 0;
		}
		total += static_cast<uint64_t>(size);
		if (total > static_cast<uint64_t>(MAX_ENCODED_BLOB_SIZE)) {
			return 0;
		}
	}
	return static_cast<Size>(total);
}

inline void log_invalid_word_for_encode(uint16_t ref, const Word &word) {
	auto log_field = [&](const char *name, StrView view) {
		if (!is_valid_encoded_str_view(view)) {
			SDL_LogError(
				  SDL_LOG_CATEGORY_ERROR,
				  "WordsCodec::encode invalid field ref=%u field=%s size=%d",
				  ref, name, view.size);
		}
	};

	log_field("translations_raw", word.translations_raw);
	log_field("grammar", word.grammar);
	switch (word.type) {
	case WordType::Nil:
		break;
	case WordType::Noun:
		log_field("noun.lemma", word.n.lemma);
		log_field("noun.plural_suffix", word.n.plural_suffix);
		break;
	case WordType::Verb:
		log_field("verb.infinitive", word.v.infinitive);
		log_field("verb.third_person", word.v.third_person);
		log_field("verb.praeteritum", word.v.praeteritum);
		log_field("verb.auxv_and_past_participle",
		          word.v.auxv_and_past_participle);
		break;
	case WordType::Adj:
		log_field("adj.lemma", word.a.lemma);
		log_field("adj.comparative", word.a.comparative);
		log_field("adj.superlative", word.a.superlative);
		break;
	case WordType::Phrase:
		log_field("phrase.text", word.p.text);
		break;
	}
}

inline bool encode_entry(uint8_t *&cursor, const uint8_t *end, uint16_t ref,
                         const Word &word) {
	if (static_cast<size_t>(end - cursor) < sizeof(Entry)) {
		return false;
	}

	Entry entry{};
	entry.ref = ref;
	entry.type = static_cast<uint8_t>(word.type);
	entry.word_id = word.word_id.value;
	entry.in_learning_list = word.in_learning_list;
	entry.was_learned = word.was_learned;

	switch (word.type) {
	case WordType::Nil:
		return false;
	case WordType::Noun:
		entry.aux = static_cast<int8_t>(word.n.gender);
		break;
	case WordType::Verb:
		break;
	case WordType::Adj:
		entry.aux = word.a.is_indeclinable ? 1 : 0;
		break;
	case WordType::Phrase:
		break;
	}

	std::memcpy(cursor, &entry, sizeof(entry));
	cursor += sizeof(Entry);

	return for_each_str_view(word, [&](StrView view) {
		return write_str_view(cursor, end, view);
	});
}

inline Size encoded_word_size(const Word &word) {
	uint64_t total = sizeof(SingleWordHeader);
	const auto ok = for_each_str_view(word, [&](StrView view) {
		if (!is_valid_encoded_str_view(view)) {
			return false;
		}
		total += sizeof(Size) + static_cast<uint64_t>(view.size);
		return total <= static_cast<uint64_t>(MAX_ENCODED_BLOB_SIZE);
	});
	return ok ? static_cast<Size>(total) : 0;
}

inline StrView encode(Arena &a, const Words &words) {
	const auto count = used_count(words);
	const auto total_size = encoded_size(words);
	if (total_size <= 0 || total_size > MAX_ENCODED_BLOB_SIZE) {
		for (auto ref = words.begin(); ref < words.end(); ref.advance(&words)) {
			if (entry_size(words.words[ref.value]) <= 0) {
				log_invalid_word_for_encode(static_cast<uint16_t>(ref.value),
				                            words.words[ref.value]);
			}
		}
		SDL_LogError(SDL_LOG_CATEGORY_ERROR,
		             "WordsCodec::encode rejected blob size %d", total_size);
		return {};
	}
	auto *data = static_cast<uint8_t *>(a.push(total_size));
	std::memset(data, 0, static_cast<size_t>(total_size));

	auto *header = reinterpret_cast<Header *>(data);
	*header = Header{};
	header->entry_count = count;
	header->next_free = words.next_free;

	auto *cursor = reinterpret_cast<uint8_t *>(header + 1);
	auto *end = data + total_size;
	for (auto ref = words.begin(); ref < words.end(); ref.advance(&words)) {
		if (!encode_entry(cursor, end, static_cast<uint16_t>(ref.value),
		                  words.words[ref.value])) {
			return {};
		}
	}

	if (cursor != end) {
		return {};
	}
	return {reinterpret_cast<const char *>(data), total_size};
}

inline bool decode_entry(uint8_t type, int8_t aux, uint64_t word_id,
                         int8_t in_learning_list, int8_t was_learned,
                         const uint8_t *&cursor, const uint8_t *end,
                         Word &word) {
	word = {};
	word.word_id = WordId{word_id};
	word.type = static_cast<WordType>(type);
	word.in_learning_list = in_learning_list;
	word.was_learned = was_learned;

	switch (word.type) {
	case WordType::Nil:
		return false;
	case WordType::Noun:
		word.n.gender = static_cast<Gender>(aux);
		break;
	case WordType::Verb:
		break;
	case WordType::Adj:
		word.a.is_indeclinable = aux != 0;
		break;
	case WordType::Phrase:
		break;
	}

	return for_each_str_view(word, [&](StrView &view) {
		return read_str_view(cursor, end, view);
	});
}

inline StrView encode_word(Arena &a, const Word &word) {
	const auto total_size = encoded_word_size(word);
	if (total_size <= 0 || total_size > MAX_ENCODED_BLOB_SIZE) {
		return {};
	}

	auto *data = static_cast<uint8_t *>(a.push(total_size));
	std::memset(data, 0, static_cast<size_t>(total_size));

	auto *header = reinterpret_cast<SingleWordHeader *>(data);
	*header = SingleWordHeader{};
	header->word_id = word.word_id.value;
	header->type = static_cast<uint8_t>(word.type);
	header->in_learning_list = word.in_learning_list;
	header->was_learned = word.was_learned;
	switch (word.type) {
	case WordType::Nil:
		return {};
	case WordType::Noun:
		header->aux = static_cast<int8_t>(word.n.gender);
		break;
	case WordType::Verb:
		break;
	case WordType::Adj:
		header->aux = word.a.is_indeclinable ? 1 : 0;
		break;
	case WordType::Phrase:
		break;
	}

	auto *cursor = reinterpret_cast<uint8_t *>(header + 1);
	auto *end = data + total_size;
	if (!for_each_str_view(word, [&](StrView view) {
			return write_str_view(cursor, end, view);
		})) {
		return {};
	}
	if (cursor != end) {
		return {};
	}
	return {reinterpret_cast<const char *>(data), total_size};
}

inline bool decode_word(Arena &a, const void *data, Size size, Word &word) {
	if (!data || size < static_cast<Size>(sizeof(SingleWordHeader)) ||
	    size > MAX_ENCODED_BLOB_SIZE) {
		return false;
	}

	const auto offset_before = a.offset;
	auto *blob = static_cast<uint8_t *>(a.push(size, 1));
	std::memcpy(blob, data, static_cast<size_t>(size));

	SingleWordHeader header{};
	std::memcpy(&header, blob, sizeof(header));
	if (header.magic != SINGLE_WORD_MAGIC ||
	    header.version != SINGLE_WORD_VERSION) {
		a.offset = offset_before;
		return false;
	}

	const uint8_t *cursor = blob + sizeof(SingleWordHeader);
	const uint8_t *end = blob + size;
	if (!decode_entry(header.type, header.aux, header.word_id,
	                  header.in_learning_list, header.was_learned, cursor, end,
	                  word) ||
	    cursor != end) {
		a.offset = offset_before;
		return false;
	}

	return true;
}

inline bool decode(Arena &a, const void *data, Size size, Words &words) {
	if (!data || size < static_cast<Size>(sizeof(Header)) ||
	    size > MAX_ENCODED_BLOB_SIZE) {
		return false;
	}

	Header header{};
	std::memcpy(&header, data, sizeof(header));
	if (header.magic != MAGIC || header.version != VERSION ||
	    header.entry_count >= Words::MAX_WORDS) {
		return false;
	}

	auto offset_before = a.offset;
	auto *blob = static_cast<uint8_t *>(a.push(size, 1));
	std::memcpy(blob, data, static_cast<size_t>(size));

	auto *cursor = reinterpret_cast<const uint8_t *>(blob + sizeof(Header));
	auto *end = blob + size;

	auto decoded_ptr = std::make_unique<Words>();
	auto &decoded = *decoded_ptr;
	for (uint32_t i = 0; i < header.entry_count; ++i) {
		if (static_cast<size_t>(end - cursor) < sizeof(Entry)) {
			a.offset = offset_before;
			return false;
		}

		Entry entry{};
		std::memcpy(&entry, cursor, sizeof(entry));
		cursor += sizeof(Entry);

		const auto ref = static_cast<Size>(entry.ref);
		const auto type = entry.type;
		const auto aux = entry.aux;
		const auto word_id = entry.word_id;

		if (ref <= 0 || ref >= Words::MAX_WORDS || decoded.used[ref] ||
		    !decode_entry(type, aux, word_id, entry.in_learning_list,
		                  entry.was_learned, cursor, end, decoded.words[ref])) {
			a.offset = offset_before;
			return false;
		}
		decoded.used[ref] = true;
		if (ref >= decoded.next_free) {
			decoded.next_free = ref + 1;
		}
	}

	if (cursor != end) {
		a.offset = offset_before;
		return false;
	}

	decoded.size = static_cast<Size>(header.entry_count);
	if (decoded.next_free < 1 || decoded.next_free > Words::MAX_WORDS) {
		decoded.next_free = 1;
	}
	for (; decoded.next_free > 1 && !decoded.used[decoded.next_free - 1];
	     --decoded.next_free) {
	}

	words = decoded;
	return true;
}

} // namespace WordsCodec
