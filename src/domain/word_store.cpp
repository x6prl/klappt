#include "word_store.h"

#include "SDL3/SDL_log.h"
#include "xapian.h"

#include <filesystem>
#include <string>

#include "base/profiler.h"
#include "platform/files.h"
#ifdef __EMSCRIPTEN__
#include "platform/web_persist.h"
#endif
#include "base/arena.h"
#include "base/hash.h"
#include "words_codec.h"

namespace {

constexpr char NEXT_WORD_ID_KEY[] = "next_word_id";
constexpr Xapian::valueno WORD_ID_VALUE_SLOT = 0;
constexpr char TYPE_PREFIX[] = "XY";
constexpr char LEMMA_PREFIX[] = "XL";
constexpr char FORM_PREFIX[] = "XF";
constexpr char TRANSLATION_PREFIX[] = "XT";
constexpr char GRAMMAR_PREFIX[] = "XG";

std::string default_word_store_path(StrView leaf = "words.xapian"_v) {
	return FileLoader::path_for(leaf);
}

std::string hex_u64(uint64_t value) {
	static constexpr char HEX[] = "0123456789abcdef";
	std::string out(16, '0');
	for (int i = 15; i >= 0; --i) {
		out[static_cast<size_t>(i)] = HEX[value & 0x0f];
		value >>= 4;
	}
	return out;
}

std::string word_id_term(WordId word_id) {
	return "Q" + hex_u64(word_id.value);
}

std::string content_hash_term(uint64_t hash) { return "K" + hex_u64(hash); }

std::string encode_be_u64(uint64_t value) {
	std::string out(8, '\0');
	for (int i = 7; i >= 0; --i) {
		out[static_cast<size_t>(7 - i)] =
			  static_cast<char>((value >> (i * 8)) & 0xffu);
	}
	return out;
}

bool decode_be_u64(const std::string &data, uint64_t &value) {
	if (data.size() != 8) {
		return false;
	}
	value = 0;
	for (unsigned char ch : data) {
		value = (value << 8) | static_cast<uint64_t>(ch);
	}
	return true;
}

bool list_contains_item(StrView list, StrView item, char delimiter) {
	item.mut_trim();
	if (!item) {
		return true;
	}
	for (; list;) {
		auto part = list.mut_split_by(delimiter).trim();
		if (part == item) {
			return true;
		}
	}
	return false;
}

StrView merge_unique_items(Arena &a, StrView base, StrView extra,
                           char delimiter) {
	StrView merged = base;
	bool changed = false;
	for (; extra;) {
		auto item = extra.mut_split_by(delimiter).trim();
		if (!item || list_contains_item(base, item, delimiter) ||
		    list_contains_item(merged, item, delimiter)) {
			continue;
		}
		if (merged) {
			if (delimiter == ';') {
				if (merged.last() != ';') {
					merged = StrView::concat(a, merged, ";"_v);
				}
				auto spaced = StrView::concat(a, merged, " "_v);
				merged = StrView::concat(a, spaced, item);
			} else {
				merged = StrView::concat_with(a, merged, item, delimiter);
			}
		} else {
			merged = item.copy(a);
		}
		changed = true;
	}
	if (changed && delimiter == ';' && merged.last() != ';') {
		merged = StrView::concat(a, merged, ";"_v);
	}
	return changed ? merged : base;
}

uint64_t content_hash(Arena &scratch, const Word &word) {
	auto guard = scratch.guard();
	auto field_size = [](StrView v) {
		return static_cast<Size>(sizeof(v.size) + (v ? v.size : 0));
	};
	Size size = 2 + field_size(word.grammar);
	switch (word.type) {
	case WordType::Nil:
		break;
	case WordType::Noun:
		size += 2 + field_size(word.n.lemma) + field_size(word.n.plural_suffix);
		break;
	case WordType::Verb:
		size += field_size(word.v.infinitive) +
		        field_size(word.v.third_person) +
		        field_size(word.v.praeteritum) +
		        field_size(word.v.auxv_and_past_participle);
		break;
	case WordType::Adj:
		size += field_size(word.a.lemma) + field_size(word.a.comparative) +
		        field_size(word.a.superlative) + 2;
		break;
	case WordType::Phrase:
		size += field_size(word.p.text);
		break;
	}
	char *data = scratch.pushN<char>(size);
	char *cursor = data;
	auto put_char = [&](char ch) { *cursor++ = ch; };
	auto feed = [&](StrView v) {
		memcpy(cursor, &v.size, sizeof(v.size));
		cursor += sizeof(v.size);
		if (v) {
			memcpy(cursor, v.data, static_cast<size_t>(v.size));
			cursor += v.size;
		}
	};
	put_char(static_cast<char>(word.type));
	put_char('\0');
	feed(word.grammar);
	switch (word.type) {
	case WordType::Nil:
		break;
	case WordType::Noun:
		put_char(static_cast<char>(word.n.gender));
		put_char('\0');
		feed(word.n.lemma);
		feed(word.n.plural_suffix);
		break;
	case WordType::Verb:
		feed(word.v.infinitive);
		feed(word.v.third_person);
		feed(word.v.praeteritum);
		feed(word.v.auxv_and_past_participle);
		break;
	case WordType::Adj:
		feed(word.a.lemma);
		feed(word.a.comparative);
		feed(word.a.superlative);
		put_char(static_cast<char>(word.a.is_indeclinable));
		put_char('\0');
		break;
	case WordType::Phrase:
		feed(word.p.text);
		break;
	}
	return hash_str_view({data, size});
}

void index_field(Xapian::TermGenerator &generator, StrView text,
                 std::string_view prefix = {}) {
	if (!text) {
		return;
	}
	generator.index_text_without_positions(
		  std::string_view{text.data, static_cast<size_t>(text.size)});
	if (!prefix.empty()) {
		generator.index_text_without_positions(
			  std::string_view{text.data, static_cast<size_t>(text.size)}, 1,
			  prefix);
	}
}

void index_translation_fields(Arena &scratch, Xapian::TermGenerator &generator,
                              StrView translations_raw) {
	auto translations = translations_from_raw(scratch, translations_raw);
	for (const auto &translation : translations) {
		index_field(generator, translation.base, TRANSLATION_PREFIX);
		for (Size i = 0; i < translation.cue_count; ++i) {
			index_field(generator, translation.cues[i].second,
			            TRANSLATION_PREFIX);
		}
	}
}

void index_word_fields(Arena &scratch, Xapian::Document &doc,
                       const Word &word) {
	Xapian::TermGenerator generator;
	generator.set_document(doc);

	switch (word.type) {
	case WordType::Nil:
		break;
	case WordType::Noun:
		doc.add_boolean_term("XYnoun");
		index_field(generator, word.n.lemma, LEMMA_PREFIX);
		index_field(generator, word.n.plural_suffix, FORM_PREFIX);
		break;
	case WordType::Verb:
		doc.add_boolean_term("XYverb");
		index_field(generator, word.v.infinitive, LEMMA_PREFIX);
		index_field(generator, word.v.third_person, FORM_PREFIX);
		index_field(generator, word.v.praeteritum, FORM_PREFIX);
		index_field(generator, word.v.auxv_and_past_participle, FORM_PREFIX);
		break;
	case WordType::Adj:
		doc.add_boolean_term("XYadj");
		index_field(generator, word.a.lemma, LEMMA_PREFIX);
		index_field(generator, word.a.comparative, FORM_PREFIX);
		index_field(generator, word.a.superlative, FORM_PREFIX);
		break;
	case WordType::Phrase:
		doc.add_boolean_term("XYphrase");
		index_field(generator, word.p.text, LEMMA_PREFIX);
		break;
	}

	index_translation_fields(scratch, generator, word.translations_raw);
	index_field(generator, word.grammar, GRAMMAR_PREFIX);
}

void configure_query_parser(Xapian::QueryParser &parser,
                            const Xapian::Database &db) {
	parser.set_database(db);
	parser.set_default_op(Xapian::Query::OP_AND);
	parser.add_prefix("lemma", LEMMA_PREFIX);
	parser.add_prefix("form", FORM_PREFIX);
	parser.add_prefix("tr", TRANSLATION_PREFIX);
	parser.add_prefix("grammar", GRAMMAR_PREFIX);
	parser.add_boolean_prefix("type", TYPE_PREFIX);
}

bool build_document(Arena &scratch, const Word &word, Xapian::Document &doc) {
	KLAPPT_PROFILE_SCOPE_N("word_store.build_document");
	auto guard = scratch.guard();
	const auto payload = WordsCodec::encode_word(scratch, word);
	if (!payload) {
		return false;
	}

	doc = Xapian::Document{};
	doc.set_data(std::string(payload.data, static_cast<size_t>(payload.size)));
	doc.add_boolean_term(word_id_term(word.word_id));
	doc.add_boolean_term(content_hash_term(content_hash(scratch, word)));
	doc.add_value(WORD_ID_VALUE_SLOT, encode_be_u64(word.word_id.value));
	index_word_fields(scratch, doc, word);
	return true;
}

bool get_next_word_id(const Xapian::WritableDatabase &db, WordId &word_id) {
	const auto value = db.get_metadata(NEXT_WORD_ID_KEY);
	if (value.empty()) {
		word_id.value = 1;
		return true;
	}

	try {
		word_id = WordId{std::stoull(value)};
		return word_id.value != 0;
	} catch (...) {
		return false;
	}
}

bool find_existing_word(Arena &scratch, Xapian::WritableDatabase &db,
                        const Word &candidate, WordId &word_id) {
	KLAPPT_PROFILE_SCOPE_N("word_store.find_existing_word");
	const auto hash = content_hash(scratch, candidate);
	const auto term = content_hash_term(hash);

	for (auto it = db.postlist_begin(term); it != db.postlist_end(term); ++it) {
		const auto doc = db.get_document(*it);
		Word stored{};
		auto guard = scratch.guard();
		const auto data = doc.get_data();
		if (!WordsCodec::decode_word(scratch, data.data(),
		                             static_cast<Size>(data.size()), stored)) {
			continue;
		}
		if (!same_lexeme(stored, candidate)) {
			continue;
		}

		uint64_t decoded_word_id{};
		if (decode_be_u64(doc.get_value(WORD_ID_VALUE_SLOT), decoded_word_id)) {
			word_id.value = decoded_word_id;
		} else {
			word_id = stored.word_id;
		}
		auto merged = stored;
		bool changed = false;
		auto merged_translations =
			  merge_unique_items(scratch, stored.translations_raw,
		                         candidate.translations_raw, ';');
		if (merged_translations != stored.translations_raw) {
			SDL_Log(
				  "word_store: merging translations for existing word_id=%llu",
				  static_cast<unsigned long long>(word_id.value));
			merged.translations_raw = merged_translations;
			changed = true;
		}
		const auto merged_learning_list =
			  candidate.in_learning_list > stored.in_learning_list
					? candidate.in_learning_list
					: stored.in_learning_list;
		if (merged_learning_list != stored.in_learning_list) {
			merged.in_learning_list = merged_learning_list;
			changed = true;
		}
		if (changed) {
			merged.word_id = word_id;
			Xapian::Document merged_doc;
			if (build_document(scratch, merged, merged_doc)) {
				db.replace_document(word_id_term(word_id), merged_doc);
			}
		}
		return word_id.value != 0;
	}

	return false;
}

} // namespace

WordStore::~WordStore() = default;

bool WordStore::open(StrView requested_path) {
	KLAPPT_PROFILE_SCOPE_N("WordStore::open");
	close();
	has_cached_word_count = false;
	cached_word_count = 0;

	path = requested_path
	             ? std::string(requested_path.data,
	                           static_cast<size_t>(requested_path.size))
	             : default_word_store_path();
	if (path.empty()) {
		return false;
	}

	std::error_code ec;
	std::filesystem::create_directories(
		  std::filesystem::path(path).parent_path(), ec);
	if (ec) {
		SDL_LogError(SDL_LOG_CATEGORY_ERROR,
		             "create_directories(%s) failed: %s", path.c_str(),
		             ec.message().c_str());
		return false;
	}

	try {
		db = std::make_unique<Xapian::WritableDatabase>(
			  path, Xapian::DB_CREATE_OR_OPEN);
		return true;
	} catch (const Xapian::Error &e) {
		SDL_LogError(SDL_LOG_CATEGORY_ERROR, "Opening Xapian DB failed: %s",
		             e.get_description().c_str());
		db.reset();
		return false;
	}
}

void WordStore::close() {
	KLAPPT_PROFILE_SCOPE_N("WordStore::close");
	if (!db) {
		has_cached_word_count = false;
		cached_word_count = 0;
		return;
	}
	try {
		db->commit();
#ifdef __EMSCRIPTEN__
		web_persist_sync();
#endif
	} catch (const Xapian::Error &e) {
		SDL_LogError(SDL_LOG_CATEGORY_ERROR, "Closing Xapian DB failed: %s",
		             e.get_description().c_str());
	}
	db.reset();
	has_cached_word_count = false;
	cached_word_count = 0;
}

Size WordStore::word_count() const {
	KLAPPT_PROFILE_SCOPE_N("WordStore::word_count");
	if (has_cached_word_count) {
		return cached_word_count;
	}

	Size count = 0;
	try {
		for (auto it = db->allterms_begin("Q"); it != db->allterms_end("Q");
		     ++it) {
			++count;
		}
	} catch (const Xapian::Error &e) {
		SDL_LogError(SDL_LOG_CATEGORY_ERROR, "Counting Xapian words failed: %s",
		             e.get_description().c_str());
		return 0;
	}

	cached_word_count = count;
	has_cached_word_count = true;
	return cached_word_count;
}

Size WordStore::matching_word_count(StrView query) const {
	KLAPPT_PROFILE_SCOPE_N("WordStore::matching_word_count");
	query.mut_trim();
	if (!query) {
		return word_count();
	}

	Size count = 0;
	try {
		Xapian::MSet mset;
		if (!search_mset(query, 0, 0, mset)) {
			return 0;
		}
		count = static_cast<Size>(mset.get_matches_estimated());
	} catch (const Xapian::Error &e) {
		SDL_LogError(SDL_LOG_CATEGORY_ERROR,
		             "Counting matching Xapian words failed: %s",
		             e.get_description().c_str());
		return 0;
	}
	return count;
}

bool WordStore::search_mset(StrView query, Size start, Size count,
                            Xapian::MSet &mset) const {
	KLAPPT_PROFILE_SCOPE_N("WordStore::search_mset");
	query.mut_trim();
	if (!query) {
		return false;
	}

	if (start < 0) {
		start = 0;
	}
	if (count < 0) {
		count = 0;
	}

	try {
		Xapian::QueryParser parser;
		configure_query_parser(parser, *db);
		const auto parsed = parser.parse_query(
			  std::string_view{query.data, static_cast<size_t>(query.size)},
			  Xapian::QueryParser::FLAG_BOOLEAN |
					Xapian::QueryParser::FLAG_LOVEHATE |
					Xapian::QueryParser::FLAG_PARTIAL);
		Xapian::Enquire enquire(*db);
		enquire.set_query(parsed);
		mset = enquire.get_mset(static_cast<Xapian::doccount>(start),
		                        static_cast<Xapian::doccount>(count));
		return true;
	} catch (const Xapian::Error &e) {
		SDL_LogError(SDL_LOG_CATEGORY_ERROR, "Xapian search failed: %s",
		             e.get_description().c_str());
		return false;
	}
}

bool WordStore::ensure_word(Arena &scratch, Word &word, bool *was_new) {
	KLAPPT_PROFILE_SCOPE_N("WordStore::ensure_word");
	if (was_new) {
		*was_new = false;
	}
	if (word.type == WordType::Nil) {
		return false;
	}
	// SDL_Log("word_store.ensure_word begin type=%d id=%llu lemma="
	// StrView_Fmt,
	//         static_cast<int>(word.type),
	//         static_cast<unsigned long long>(word.word_id.value),
	//         StrView_Arg(most_meaningfull_lemma(word)));

	try {
		WordId existing_id;
		if (find_existing_word(scratch, *db, word, existing_id)) {
			word.word_id = existing_id;
			// SDL_Log("word_store.ensure_word existing id=%llu",
			//         static_cast<unsigned long long>(word.word_id.value));
			return true;
		}

		WordId next_word_id;
		if (!get_next_word_id(*db, next_word_id)) {
			SDL_LogError(SDL_LOG_CATEGORY_ERROR,
			             "Xapian next_word_id metadata is invalid");
			return false;
		}

		// let us hope for the better... btw, dead index is not a big problem
		word.word_id = next_word_id;
		Xapian::Document doc;
		if (!build_document(scratch, word, doc)) {
			SDL_LogError(SDL_LOG_CATEGORY_ERROR,
			             "Serializing word for Xapian failed");
			return false;
		}

		db->begin_transaction();
		db->replace_document(word_id_term(word.word_id), doc);
		db->set_metadata(NEXT_WORD_ID_KEY,
		                 std::to_string(word.word_id.value + 1));
		db->commit_transaction();
#ifdef __EMSCRIPTEN__
		web_persist_sync();
#endif
		if (was_new) {
			*was_new = true;
		}
		if (has_cached_word_count) {
			++cached_word_count;
		}
		// SDL_Log("word_store.ensure_word inserted id=%llu",
		//         static_cast<unsigned long long>(word.word_id.value));
		return true;
	} catch (const Xapian::Error &e) {
		SDL_LogError(SDL_LOG_CATEGORY_ERROR, "Updating Xapian DB failed: %s",
		             e.get_description().c_str());
		try {
			db->cancel_transaction();
		} catch (...) {
		}
		return false;
	}
}

bool WordStore::get_by_id(Arena &scratch, WordId word_id, Word &word) {
	KLAPPT_PROFILE_SCOPE_N("WordStore::get_by_id");
	try {
		auto term = word_id_term(word_id);

		if (!db->term_exists(term)) {
			return false;
		}

		auto it = db->postlist_begin(term);
		if (it != db->postlist_end(term)) {
			auto doc = db->get_document(*it);
			const auto data = doc.get_data();
			if (!WordsCodec::decode_word(scratch, data.data(),
			                             static_cast<Size>(data.size()),
			                             word)) {
				return false;
			}
		}
		return true;
	} catch (const Xapian::Error &e) {
		SDL_LogError(SDL_LOG_CATEGORY_ERROR, "Updating Xapian DB failed: %s",
		             e.get_description().c_str());
		return false;
	}
}

void WordStore::save(Arena &scratch, Word &word) {
	KLAPPT_PROFILE_SCOPE_N("WordStore::save");
	if (word.type == WordType::Nil) {
		return;
	}

	try {
		Xapian::Document doc;
		if (!build_document(scratch, word, doc)) {
			SDL_LogError(SDL_LOG_CATEGORY_ERROR,
			             "Serializing word for Xapian failed");
			return;
		}

		{
			KLAPPT_PROFILE_SCOPE_N("begin_transaction");
			db->begin_transaction();
		}
		{
			KLAPPT_PROFILE_SCOPE_N("replace_document");
			db->replace_document(word_id_term(word.word_id), doc);
		}
		{
			KLAPPT_PROFILE_SCOPE_N("commit_transaction");
			db->commit_transaction();
		}
#ifdef __EMSCRIPTEN__
		web_persist_sync();
#endif
	} catch (const Xapian::Error &e) {
		SDL_LogError(SDL_LOG_CATEGORY_ERROR, "Updating Xapian DB failed: %s",
		             e.get_description().c_str());
		try {
			db->cancel_transaction();
		} catch (...) {
		}
	}
}

void WordStore::set_was_learned(Arena &scratch, Word &word) {
	KLAPPT_PROFILE_SCOPE_N("WordStore::set_was_learned");
	try {
		Xapian::Document doc;
		word.was_learned = 1;
		if (!build_document(scratch, word, doc)) {
			SDL_LogError(SDL_LOG_CATEGORY_ERROR,
			             "Serializing word for Xapian failed");
			return;
		}
		{
			KLAPPT_PROFILE_SCOPE_N("begin_transaction");
			db->begin_transaction();
		}
		{
			KLAPPT_PROFILE_SCOPE_N("replace_document");
			db->replace_document(word_id_term(word.word_id), doc);
		}
		{
			KLAPPT_PROFILE_SCOPE_N("commit_transaction");
			db->commit_transaction();
		}
#ifdef __EMSCRIPTEN__
		web_persist_sync();
#endif
	} catch (const Xapian::Error &e) {
		SDL_LogError(SDL_LOG_CATEGORY_ERROR, "Updating Xapian DB failed: %s",
		             e.get_description().c_str());
		try {
			db->cancel_transaction();
		} catch (...) {
		}
	}
}
