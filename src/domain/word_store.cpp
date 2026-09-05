#include "word_store.h"

#include <cstddef>
#include <filesystem>
#include <string>
#include <string_view>

#include <SDL3/SDL_log.h>
#include <xapian.h>

#include "base/arena.h"
#include "base/hash.h"
#include "base/profiler.h"
#include "base/shuffle.h"
#include "base/str_view.h"
#include "domain/grammar.h"
#include "domain/word.h"
#include "words_codec.h"

#ifdef __EMSCRIPTEN__
#include "platform/web_persist.h"
#endif

namespace {

constexpr char NEXT_LOCAL_WORD_ID_KEY[] = "next_local_word_id";

constexpr Xapian::valueno POPULARITY_VALUE_SLOT = 1;

// NOTE: [1 byte len] + [lowercase lemma]
constexpr Xapian::valueno LEMMA_KEY_VALUE_SLOT = 2;

constexpr char TYPE_PREFIX[] = "XY";
constexpr char LEMMA_PREFIX[] = "XL";
constexpr char FORM_PREFIX[] = "XF";
constexpr char TRANSLATION_PREFIX[] = "XT";

constexpr char DIRTY_TERM[] = "SDIRTY";
constexpr char NEW_TERM[] = "SNEW";

constexpr int WEIGHT_HIGHEST = 6;
constexpr int WEIGHT_HIGH = 5;
constexpr int WEIGHT_NORM = 1;
constexpr int WEIGHT_LOW = 1;

constexpr uint64_t LOCAL_WORD_ID_FLAG = 1ULL << 63;

inline bool is_local_temp_id(WordId id) {
	return (id.value & LOCAL_WORD_ID_FLAG) != 0;
}

// NOTE: yet unused
WordId generate_next_local_temp_id(Arena &scratch,
                                   Xapian::WritableDatabase &db) {
	auto g = scratch.guard();
	uint32_t counter = 1;
	const auto value = db.get_metadata(NEXT_LOCAL_WORD_ID_KEY);
	if (!value.empty()) {
		try {
			counter = static_cast<uint32_t>(std::stoul(value));
		} catch (...) {
			counter = 1;
		}
	}

	auto counter_str = StrView::from_number(scratch, counter + 1);
	db.set_metadata(NEXT_LOCAL_WORD_ID_KEY,
	                {counter_str.data, (size_t)counter_str.size});

	uint64_t id_val = LOCAL_WORD_ID_FLAG | counter;

	return WordId{id_val};
}

std::string_view word_id_term(Arena &a, WordId word_id) {
	auto str =
		  StrView::concat(a, "Q"_v, StrView::from_number_hex(a, word_id.value));
	return {str.data, static_cast<size_t>(str.size)};
}

std::string_view content_hash_term(Arena &a, uint64_t hash) {
	auto str = StrView::concat(a, "K"_v, StrView::from_number_hex(a, hash));
	return {str.data, static_cast<size_t>(str.size)};
}

// std::string_view encode_be_u64(Arena &a, uint64_t value) {
// 	auto data = a.pushN<char>(8);
// 	for (int i = 7; i >= 0; --i) {
// 		data[7 - i] = static_cast<char>((value >> (i * 8)) & 0xffu);
// 	}
// 	return {data, 8};
// }
//
// bool decode_be_u64(std::string_view data, uint64_t &value) {
// 	if (data.size() != 8) {
// 		return false;
// 	}
// 	value = 0;
// 	for (uint8_t ch : data) {
// 		value = (value << 8) | static_cast<uint64_t>(ch);
// 	}
// 	return true;
// }

std::string_view make_lemma_sort_key(Arena &scratch, StrView lemma) {
	if (!lemma) {
		char *buf = scratch.pushN<char>(1);
		buf[0] = 0;
		return {buf, 1};
	}

	// Lowercase via Arena
	StrView lower = lemma.utf8_to_lowercase_german(scratch);

	// Length byte: lemma.size < 256 ? lemma.size : 255
	uint8_t len_byte =
		  static_cast<uint8_t>(lower.size < 256 ? lower.size : 255);

	// Allocate 1 byte for length + payload size
	char *buf = scratch.pushN<char>(1 + lower.size);
	buf[0] = static_cast<char>(len_byte);
	std::memcpy(buf + 1, lower.data, static_cast<size_t>(lower.size));

	return {buf, static_cast<size_t>(1 + lower.size)};
}

bool is_list_contains_item(StrView list, StrView item, char delimiter) {
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
	bool is_changed = false;
	for (; extra;) {
		auto item = extra.mut_split_by(delimiter).trim();
		if (!item || is_list_contains_item(base, item, delimiter) ||
		    is_list_contains_item(merged, item, delimiter)) {
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
		is_changed = true;
	}
	if (is_changed && delimiter == ';' && merged.last() != ';') {
		merged = StrView::concat(a, merged, ";"_v);
	}
	return is_changed ? merged : base;
}

uint64_t word_hash(Arena &scratch, const Word &word) {
	auto guard = scratch.guard();
	auto field_size = [](StrView v) {
		return static_cast<Size>(sizeof(v.size) + (v ? v.size : 0));
	};

	Size size = 2; // type + alignment
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
		        field_size(word.v.auxv_and_past_participle) + 2;
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
		std::memcpy(cursor, &v.size, sizeof(v.size));
		cursor += sizeof(v.size);
		if (v) {
			std::memcpy(cursor, v.data, static_cast<size_t>(v.size));
			cursor += v.size;
		}
	};

	put_char(static_cast<char>(word.type));
	put_char('\0');

	// NOTE: we are not hashing json and plain translations
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
		put_char(static_cast<char>(word.v.is_separable_prefix));
		put_char('\0');
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

void index_field(Xapian::TermGenerator &generator, StrView text, int weight,
                 std::string_view prefix = {}) {
	if (!text)
		return;
	std::string_view sv{text.data, static_cast<size_t>(text.size)};
	generator.index_text_without_positions(sv, weight);
	if (!prefix.empty()) {
		generator.index_text_without_positions(sv, weight, prefix);
	}
}

void index_translation_fields(Arena &scratch, Xapian::TermGenerator &generator,
                              StrView translations_raw) {
	auto translations = word_translations_split(scratch, translations_raw);
	for (const auto &discrete_translation : translations) {
		index_field(generator, discrete_translation, WEIGHT_HIGH,
		            TRANSLATION_PREFIX);
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
		index_field(generator, word.n.lemma, WEIGHT_HIGHEST, LEMMA_PREFIX);
		// index plural if it differs
		{
			auto plural_form =
				  grammar::noun_plural_without_article(scratch, word.n);
			if (plural_form != word.n.lemma) {
				index_field(generator, plural_form, WEIGHT_NORM, FORM_PREFIX);
			}
		}
		// index singular with its article
		index_field(generator,
		            grammar::noun_singular_with_article(scratch, word.n),
		            WEIGHT_HIGH, FORM_PREFIX);
		break;
	case WordType::Verb:
		doc.add_boolean_term("XYverb");
		index_field(generator, word.v.infinitive, WEIGHT_HIGHEST, LEMMA_PREFIX);
		if (word.v.third_person) {
			auto third_p = grammar::verb_third_person_full(scratch, word.v);
			index_field(generator, third_p, WEIGHT_NORM, FORM_PREFIX);
		}
		index_field(generator, word.v.praeteritum, WEIGHT_NORM, FORM_PREFIX);
		if (word.v.auxv_and_past_participle) {
			index_field(generator,
			            grammar::verb_past_participle(scratch, word.v),
			            WEIGHT_NORM, FORM_PREFIX);
		}
		break;
	case WordType::Adj:
		doc.add_boolean_term("XYadj");
		index_field(generator, word.a.lemma, WEIGHT_HIGHEST, LEMMA_PREFIX);
		index_field(generator, word.a.comparative, WEIGHT_NORM, FORM_PREFIX);
		index_field(generator, word.a.superlative, WEIGHT_NORM, FORM_PREFIX);
		break;
	case WordType::Phrase:
		doc.add_boolean_term("XYphrase");
		index_field(generator, word.p.text, WEIGHT_LOW, LEMMA_PREFIX);
		break;
	}
	index_translation_fields(scratch, generator, word.translations_raw);
}

void configure_query_parser(Xapian::QueryParser &parser,
                            const Xapian::Database &db) {
	parser.set_database(db);
	parser.set_default_op(Xapian::Query::OP_AND);
	parser.add_prefix("lemma", LEMMA_PREFIX);
	parser.add_prefix("form", FORM_PREFIX);
	parser.add_prefix("tr", TRANSLATION_PREFIX);
	parser.add_boolean_prefix("type", TYPE_PREFIX);
}

bool build_document(Arena &scratch, const Word &word, Xapian::Document &doc,
                    bool is_mark_dirty_or_new = false) {
	KLAPPT_PROFILE_SCOPE_N("word_store.build_document");
	auto guard = scratch.guard();
	const auto payload = WordsCodec::word_encode(scratch, word);
	if (!payload) {
		return false;
	}

	doc = Xapian::Document{};
	doc.set_data(
		  std::string_view{payload.data, static_cast<size_t>(payload.size)});
	doc.add_boolean_term(word_id_term(scratch, word.word_id));
	doc.add_boolean_term(content_hash_term(scratch, word_hash(scratch, word)));

	char pop_byte = static_cast<char>(word.popularity);
	doc.add_value(POPULARITY_VALUE_SLOT, std::string_view{&pop_byte, 1});

	StrView lemma = word_primary_lemma(word);
	doc.add_value(LEMMA_KEY_VALUE_SLOT, make_lemma_sort_key(scratch, lemma));

	// NOTE: === popularity ===
	if (word.popularity >= 180) {
		doc.add_boolean_term("XP1"); // Top ~1000
		doc.add_boolean_term("XP2");
		doc.add_boolean_term("XP3");
	} else if (word.popularity >= 100) {
		doc.add_boolean_term("XP2"); // Top ~5000
		doc.add_boolean_term("XP3");
	} else if (word.popularity >= 40) {
		doc.add_boolean_term("XP3"); // Top ~20000
	}

	// setting sync flags
	if (is_mark_dirty_or_new) {
		if (is_local_temp_id(word.word_id)) {
			doc.add_boolean_term(NEW_TERM);
		} else {
			doc.add_boolean_term(DIRTY_TERM);
		}
	}

	index_word_fields(scratch, doc, word);
	return true;
}

bool find_existing_word(Arena &scratch, Xapian::WritableDatabase &db,
                        const Word &candidate, WordId &word_id) {
	KLAPPT_PROFILE_SCOPE_N("word_store.find_existing_word");
	const auto hash = word_hash(scratch, candidate);
	const auto term = content_hash_term(scratch, hash);

	for (auto it = db.postlist_begin(term); it != db.postlist_end(term); ++it) {
		const auto doc = db.get_document(*it);
		Word stored{};
		auto guard = scratch.guard();
		const auto data = doc.get_data();
		if (!WordsCodec::word_decode(scratch, data.data(),
		                             static_cast<Size>(data.size()), stored)) {
			continue;
		}
		if (!word_has_same_lexeme(stored, candidate)) {
			continue;
		}

		word_id = stored.word_id;

		auto merged = stored;
		bool is_changed = false;
		auto merged_translations =
			  merge_unique_items(scratch, stored.translations_raw,
		                         candidate.translations_raw, ';');
		if (merged_translations != stored.translations_raw) {
			merged.translations_raw = merged_translations;
			is_changed = true;
		}
		const auto merged_learning_list =
			  candidate.in_learning_list > stored.in_learning_list
					? candidate.in_learning_list
					: stored.in_learning_list;
		if (merged_learning_list != stored.in_learning_list) {
			merged.in_learning_list = merged_learning_list;
			is_changed = true;
		}
		if (is_changed) {
			merged.word_id = word_id;
			Xapian::Document merged_doc;
			if (build_document(scratch, merged, merged_doc)) {
				db.begin_transaction();
				db.replace_document(word_id_term(scratch, word_id), merged_doc);
				db.commit_transaction();
#ifdef __EMSCRIPTEN__
				web_persist_sync();
#endif
			}
		}
		return word_id.value != 0;
	}

	return false;
}

Xapian::Database *open_db_ro(StrView path) {
	if (!path) {
		return nullptr;
	}

	// std::string path_str{path.data, static_cast<size_t>(path.size)};
	// std::error_code ec;
	// std::filesystem::create_directories(
	// 	  std::filesystem::path(path_str).parent_path(), ec);
	// if (ec) {
	// 	SDL_LogError(SDL_LOG_CATEGORY_ERROR,
	// 	             "create_directories(%s) failed: %s", path_str.c_str(),
	// 	             ec.message().c_str());
	// 	return false;
	// }

	Xapian::Database *ret{nullptr};
	try {
		std::string_view pv{path.data, (size_t)path.size};
		ret = new Xapian::Database(pv, Xapian::DB_OPEN);
		SDL_Log("Successfully opened for reading " StrView_Fmt,
		        StrView_Arg(path));
		return ret;
	} catch (const Xapian::Error &e) {
		SDL_LogError(SDL_LOG_CATEGORY_ERROR,
		             "Opening Xapian DB " StrView_Fmt " for reading failed: %s",
		             StrView_Arg(path), e.get_description().c_str());
		delete ret;
		return nullptr;
	}
}

} // namespace

WordStore::~WordStore() { close(); }

bool WordStore::open(StrView requested_path) {
	KLAPPT_PROFILE_SCOPE_N("WordStore::open");
	close();

	if (!requested_path) {
		return false;
	}

	std::string path_str{requested_path.data,
	                     static_cast<size_t>(requested_path.size)};
	std::error_code ec;
	std::filesystem::create_directories(
		  std::filesystem::path(path_str).parent_path(), ec);
	if (ec) {
		SDL_LogError(SDL_LOG_CATEGORY_ERROR,
		             "create_directories(%s) failed: %s", path_str.c_str(),
		             ec.message().c_str());
		return false;
	}

	try {
		db = new Xapian::WritableDatabase(path_str, Xapian::DB_CREATE_OR_OPEN);
		SDL_Log("Successfully opened %s", path_str.c_str());
		return true;
	} catch (const Xapian::Error &e) {
		SDL_LogError(SDL_LOG_CATEGORY_ERROR, "Opening Xapian DB failed: %s",
		             e.get_description().c_str());
		delete db;
		db = nullptr;
		return false;
	}
}

bool WordStore::open_sub0(StrView path) {
	sub0 = open_db_ro(path);
	return !!sub0;
}

bool WordStore::open_sub1(StrView path) {
	sub1 = open_db_ro(path);
	return !!sub1;
}

void WordStore::close() {
	KLAPPT_PROFILE_SCOPE_N("WordStore::close");
	if (sub0) {
		delete sub0;
	}
	if (sub1) {
		delete sub1;
	}
	if (!db) {
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
	delete db;
	db = nullptr;
}

Size WordStore::word_count() const {
	if (!db)
		return 0;
	try {
		return static_cast<Size>(db->get_doccount());
	} catch (const Xapian::Error &e) {
		SDL_LogError(SDL_LOG_CATEGORY_ERROR, "WordStore::word_count failed: %s",
		             e.get_description().c_str());
		return 0;
	}
}

Size WordStore::matching_word_count(StrView query) const {
	KLAPPT_PROFILE_SCOPE_N("WordStore::matching_word_count");
	query.mut_trim();
	if (!query) {
		return word_count();
	}

	try {
		Xapian::MSet mset;
		if (!search_mset(query, 0, 0, mset)) {
			return 0;
		}
		return static_cast<Size>(mset.get_matches_estimated());
	} catch (const Xapian::Error &e) {
		SDL_LogError(SDL_LOG_CATEGORY_ERROR,
		             "Counting matching Xapian words failed: %s",
		             e.get_description().c_str());
		return 0;
	}
}

bool WordStore::search_mset(StrView query, Size start, Size count,
                            Xapian::MSet &mset) const {
	KLAPPT_PROFILE_SCOPE_N("WordStore::search_mset");
	query.mut_trim();
	if (!query || !db) {
		return false;
	}

	if (start < 0)
		start = 0;
	if (count < 0)
		count = 0;

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

		// sort by popularyty (descending = true)
		enquire.set_sort_by_value_then_relevance(POPULARITY_VALUE_SLOT, true);

		// sort shortest-to-longest (ascending = false)
		enquire.set_sort_by_value(LEMMA_KEY_VALUE_SLOT, false);

		constexpr auto CHECK_AT_LEAST = 30;
		mset = enquire.get_mset(static_cast<Xapian::doccount>(start),
		                        static_cast<Xapian::doccount>(count),
		                        CHECK_AT_LEAST);
		return true;
	} catch (const Xapian::Error &e) {
		SDL_LogError(SDL_LOG_CATEGORY_ERROR, "Xapian search failed: %s",
		             e.get_description().c_str());
		return false;
	}
}

// TODO: add another arena param
bool WordStore::find_and_fill_word_id(Arena &scratch, Word &word) {
	KLAPPT_PROFILE_SCOPE_N("WordStore::ensure_word");
	if (word.type == WordType::Nil || !db)
		return false;

	try {
		WordId existing_id;
		if (find_existing_word(scratch, *db, word, existing_id)) {
			word.word_id = existing_id;
			return true;
		}
	} catch (const Xapian::Error &e) {
		SDL_LogError(SDL_LOG_CATEGORY_ERROR, "Updating Xapian DB failed: %s",
		             e.get_description().c_str());
		try {
			db->cancel_transaction();
		} catch (...) {
		}
		return false;
	}
	return false;
}

bool WordStore::get_by_id(Arena &scratch, WordId word_id, Word &word) const {
	KLAPPT_PROFILE_SCOPE_N("WordStore::get_by_id");
	try {
		const auto term = word_id_term(scratch, word_id);
		auto it = db->postlist_begin(term);
		if (it == db->postlist_end(term)) {
			return false;
		}

		const auto doc = db->get_document(*it);
		const auto data = doc.get_data();

		return WordsCodec::word_decode(scratch, data.data(),
		                               static_cast<Size>(data.size()), word);

	} catch (const Xapian::Error &e) {
		SDL_LogError(SDL_LOG_CATEGORY_ERROR,
		             "Reading from Xapian DB failed: %s",
		             e.get_description().c_str());
		return false;
	}
}

bool WordStore::get_by_id_for_lang(Arena &scratch, WordId word_id, Word &word,
                                   int8_t lang_id, uint8_t app_lang_id) const {
	KLAPPT_PROFILE_SCOPE_N("WordStore::get_by_id_for_lang");

	const Xapian::Database *target_db = nullptr;
	if (lang_id == app_lang_id) {
		target_db = db;
	} else if (-1 == lang_id) {
		target_db = sub0;
	} else {
		target_db = sub1;
	}

	if (!target_db) {
		return false;
	}

	try {
		const auto term = word_id_term(scratch, word_id);
		auto it = target_db->postlist_begin(term);
		if (it == target_db->postlist_end(term)) {
			return false;
		}

		const auto doc = target_db->get_document(*it);
		const auto data = doc.get_data();

		return WordsCodec::word_decode(scratch, data.data(),
		                               static_cast<Size>(data.size()), word);
	} catch (const Xapian::Error &e) {
		SDL_LogError(SDL_LOG_CATEGORY_ERROR, "get_by_id_for_lang failed: %s",
		             e.get_description().c_str());
		return false;
	}
}

void WordStore::save_direct(Arena &scratch, const Word &word, bool mark_dirty) {
	if (word.type == WordType::Nil || !db)
		return;

	Xapian::Document doc;
	if (build_document(scratch, word, doc, mark_dirty)) {
		db->replace_document(word_id_term(scratch, word.word_id), doc);
	}
}

void WordStore::save(Arena &scratch, Word &word, bool mark_dirty) {
	if (word.type == WordType::Nil || !db)
		return;

	try {
		db->begin_transaction();
		save_direct(scratch, word, mark_dirty);
		db->commit_transaction();

#ifdef __EMSCRIPTEN__
		web_persist_sync();
#endif
	} catch (const Xapian::Error &e) {
		SDL_LogError(SDL_LOG_CATEGORY_ERROR, "Saving to Xapian DB failed: %s",
		             e.get_description().c_str());
		try {
			db->cancel_transaction();
		} catch (...) {
		}
	}
}

void WordStore::set_was_learned(Arena &scratch, Word &word) {
	word.was_learned = 1;
	save(scratch, word, false);
}

bool WordStore::remove_by_id(Arena &scratch, WordId word_id) {
	KLAPPT_PROFILE_SCOPE_N("WordStore::remove_by_id");
	if (!db)
		return false;

	try {
		auto term = word_id_term(scratch, word_id);
		if (!db->term_exists(term)) {
			return false;
		}

		db->begin_transaction();
		db->delete_document(term);
		db->commit_transaction();

#ifdef __EMSCRIPTEN__
		web_persist_sync();
#endif
		return true;
	} catch (const Xapian::Error &e) {
		SDL_LogError(SDL_LOG_CATEGORY_ERROR,
		             "Deleting from Xapian DB failed: %s",
		             e.get_description().c_str());
		try {
			db->cancel_transaction();
		} catch (...) {
		}
		return false;
	}
}

bool WordStore::mark_synced(Arena &scratch, WordId word_id) {
	KLAPPT_PROFILE_SCOPE_N("WordStore::mark_synced");
	Word word{};
	if (!get_by_id(scratch, word_id, word)) {
		return false;
	}
	save(scratch, word, false);
	return true;
}

bool WordStore::rekey_word(Arena &scratch, WordId old_id, WordId new_id) {
	KLAPPT_PROFILE_SCOPE_N("WordStore::rekey_word");
	if (!db || old_id == new_id)
		return false;

	try {
		Word word{};
		if (!get_by_id(scratch, old_id, word)) {
			return false;
		}

		word.word_id = new_id;
		Xapian::Document new_doc;
		if (!build_document(scratch, word, new_doc, false)) {
			return false;
		}

		db->begin_transaction();
		db->delete_document(word_id_term(scratch, old_id));
		db->replace_document(word_id_term(scratch, new_id), new_doc);
		db->commit_transaction();

#ifdef __EMSCRIPTEN__
		web_persist_sync();
#endif
		return true;
	} catch (const Xapian::Error &e) {
		SDL_LogError(SDL_LOG_CATEGORY_ERROR, "Rekeying Xapian word failed: %s",
		             e.get_description().c_str());
		try {
			db->cancel_transaction();
		} catch (...) {
		}
		return false;
	}
}

Size WordStore::get_random_unlearned_words(Arena &scratch, Arena &out_arena,
                                           Size count, DynArr<Word> &out,
                                           uint64_t *rng_state) const {
	KLAPPT_PROFILE_SCOPE_N("WordStore::get_random_unlearned_words");
	if (!db || count <= 0)
		return 0;

	const auto last_docid = db->get_lastdocid();
	if (last_docid == 0)
		return 0;

	Size picked = 0;
	const Size max_attempts = count * 20 + 50;
	Size attempts = 0;

	for (; picked < count && attempts < max_attempts;) {
		++attempts;
		auto guard = scratch.guard();

		// random docid [1, last_docid]
		const auto docid = static_cast<Xapian::docid>(
			  random_num(1, static_cast<int64_t>(last_docid) + 1, rng_state));

		try {
			const auto doc = db->get_document(docid);
			const auto data = doc.get_data();

			Word word{};
			if (!WordsCodec::word_decode(scratch, data.data(),
			                             static_cast<Size>(data.size()),
			                             word)) {
				continue;
			}

			// filter
			if (word.type == WordType::Phrase || word.type == WordType::Nil ||
			    word.in_learning_list != 0 || word.was_learned != 0) {
				continue;
			}

			bool is_duplicate = false;
			for (Size i = 0; i < out.size; ++i) {
				if (out[i].word_id == word.word_id) {
					is_duplicate = true;
					break;
				}
			}
			if (is_duplicate) {
				continue;
			}

			out.push(out_arena, word_clone(out_arena, word));
			++picked;
		} catch (const Xapian::DocNotFoundError &) {
			// doc was removed
			continue;
		} catch (const Xapian::Error &e) {
			SDL_LogError(SDL_LOG_CATEGORY_ERROR,
			             "Xapian error during random pick: %s",
			             e.get_description().c_str());
			break;
		}
	}

	return picked;
}

Size WordStore::get_smart_suggestions(Arena &scratch, Arena &out_arena,
                                      Size count, DynArr<Word> &out,
                                      uint64_t *rng_state) const {
	KLAPPT_PROFILE_SCOPE_N("WordStore::get_smart_suggestions");
	if (!db || count <= 0)
		return 0;

	auto try_pick_from_term = [&](const char *term, Size need) -> Size {
		if (!db->term_exists(term))
			return 0;
		const auto freq = db->get_termfreq(term);
		if (freq == 0)
			return 0;

		Size local_picked = 0;
		Size attempts = 0;
		const Size max_attempts = need * 5 + 10;

		for (; local_picked < need && attempts < max_attempts;) {
			++attempts;
			auto guard = scratch.guard();

			auto offset =
				  static_cast<Xapian::doccount>(random_num(0, freq, rng_state));
			auto it = db->postlist_begin(term);
			it.skip_to(offset);
			if (it == db->postlist_end(term))
				continue;

			try {
				const auto doc = db->get_document(*it);
				const auto data = doc.get_data();
				Word word{};
				if (!WordsCodec::word_decode(scratch, data.data(),
				                             static_cast<Size>(data.size()),
				                             word)) {
					continue;
				}

				if (word.type == WordType::Phrase ||
				    word.in_learning_list != 0 || word.was_learned != 0) {
					continue;
				}

				bool is_duplicate = false;
				for (Size i = 0; i < out.size; ++i) {
					if (out[i].word_id == word.word_id) {
						is_duplicate = true;
						break;
					}
				}
				if (is_duplicate)
					continue;

				out.push(out_arena, word_clone(out_arena, word));
				++local_picked;
			} catch (...) {
			}
		}
		return local_picked;
	};

	// 70% XP1
	Size need_xp1 = (count * 7) / 10;
	Size picked = try_pick_from_term("XP1", need_xp1);

	// 20% XP2
	Size need_xp2 = count - picked - (count / 10);
	if (need_xp2 > 0) {
		picked += try_pick_from_term("XP2", need_xp2);
	}

	// random
	if (out.size < count) {
		get_random_unlearned_words(scratch, out_arena, count - out.size, out,
		                           rng_state);
	}

	return out.size;
}
