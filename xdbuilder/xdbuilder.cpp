#include <filesystem>
#include <string>
#include <vector>

#include <SDL3/SDL_log.h>
#include <sqlite3.h>
#include <xapian.h>

#include "base/arena.h"
#include "base/measure.h"
#include "base/str_view.h"
#include "domain/word.h"
#include "domain/word_store.h"

namespace {

int8_t lang_code_to_id(const std::string &lang) {
	if (lang == "de") {
		return -1;
	}
	if (lang == "en") {
		return static_cast<int8_t>(lang_en); // 0
	}
	if (lang == "ru") {
		return static_cast<int8_t>(lang_ru); // 1
	}
	if (lang == "tr") {
		return static_cast<int8_t>(lang_tr); // 2
	}
	if (lang == "ar") {
		return static_cast<int8_t>(lang_ar); // 3
	}
	return -2; // unknown
}

bool parse_line(Arena &a, StrView line, Word &word) {
	line.mut_trim();
	if (!line || line.size < 2) {
		return false;
	}

	auto tail = line.slice(line.size - 5);
	switch (line.first()) {
	case 'd':
	case '(':
		if (line.size > 5 && ' ' == line[3] &&
		    str_to_gender(line.data) != Gender::unknown &&
		    line.slice(5).is_contains('-')) {
			const char *dash = line.slice(5).find('-');
			char prev = dash[-1];
			if (' ' == prev || '"' == prev) {
				word.type = WordType::Noun;
			} else {
				word.type = WordType::Phrase;
			}
			break;
		}
		if (tail == "(sg.)" || tail == "(pl.)") {
			word.type = WordType::Noun;
			break;
		}
		word.type = WordType::Phrase;
		break;
	case 'v':
		if (line.size >= 2 && line[1] == ' ') {
			word.type = WordType::Verb;
		} else {
			word.type = WordType::Phrase;
		}
		break;
	case 'a':
		if (line.size >= 2 && line[1] == ' ') {
			word.type = WordType::Adj;
		} else {
			word.type = WordType::Phrase;
		}
		break;
	default:
		word.type = WordType::Phrase;
	}

	switch (word.type) {
	case WordType::Noun: {
		word.n.gender = str_to_gender(line.mut_split().data);
		if (Gender::unknown == word.n.gender) {
			return false;
		}
		if (line) {
			word.n.lemma = line.mut_split().copy(a);
		} else {
			return false;
		}
		if (line) {
			word.n.plural_suffix = line.copy(a);
		} else {
			return false;
		}
	} break;

	case WordType::Verb: {
		line = line.slice(2);
		auto present_tense = line.mut_split_by('/').trimr();
		auto [inf, exception] = present_tense.split_by('-');

		constexpr char SEPARABLE_PREFIX_SEPARATOR = '|';
		if (inf.is_contains(SEPARABLE_PREFIX_SEPARATOR)) {
			auto [h, t] = inf.split_by(SEPARABLE_PREFIX_SEPARATOR);
			word.v.infinitive = StrView::concat(a, h, t);
			word.v.separable_prefix_size = h.size;
		} else {
			word.v.infinitive = inf.copy(a);
		}

		word.v.third_person = exception.triml().copy(a);

		if (line.mut_trim()) {
			auto pt = line.mut_split_by('/').trim();
			if (pt && '-' != pt.first()) {
				word.v.praeteritum = pt.copy(a);
			}
			word.v.auxv_and_past_participle = line.mut_trim().copy(a);
		}
	} break;

	case WordType::Adj: {
		line = line.slice(2);
		auto [lemma, tail_adj] = line.split();

		bool is_valid_adj =
			  (tail_adj && lemma && lemma.first() == tail_adj.first()) ||
			  tail_adj.is_contains_substr("am "_v) || tail_adj == "(indecl.)"_v;

		if (!tail_adj || is_valid_adj) {
			word.a.lemma = lemma.copy(a);
			if (!tail_adj)
				break;

			word.a.is_indeclinable = (tail_adj == "(indecl.)");
			if (word.a.is_indeclinable)
				break;

			word.a.comparative = tail_adj.mut_split().copy(a);
			word.a.superlative = tail_adj.copy(a);
		} else {
			word.type = WordType::Phrase;
			word.p.text = line.copy(a);
		}
	} break;

	case WordType::Phrase:
		word.type = WordType::Phrase;
		word.p.text = line.copy(a);
		break;

	default:
		break;
	}

	return true;
}

bool build_language_database(sqlite3 *db, const std::string &lang,
                             const std::string &output_dir) {
	Measure m{lang.c_str()};
	SDL_Log("========================================");
	SDL_Log("[BUILD] Starting Xapian DB for lang: %s", lang.c_str());

	const std::string out_path = output_dir + "words-" + lang + ".xapian";
	std::filesystem::remove_all(out_path);

	WordStore ws;
	if (!ws.open(
			  StrView{out_path.data(), static_cast<Size>(out_path.size())})) {
		SDL_LogError(SDL_LOG_CATEGORY_ERROR, "Failed to create Xapian DB: %s",
		             out_path.c_str());
		return false;
	}

	const char *query_sql = R"(
		SELECT 
			l.id, 
			l.signature, 
			t.translation_plain, 
			t.payload, 
			COALESCE(s.popularity, 0)
		FROM lexemes l
		JOIN translations t ON l.id = t.lexeme_id
		LEFT JOIN stats s ON l.id = s.lexeme_id
		WHERE t.lang = ?
		ORDER BY l.id ASC;
	)";

	sqlite3_stmt *stmt = nullptr;
	if (sqlite3_prepare_v2(db, query_sql, -1, &stmt, nullptr) != SQLITE_OK) {
		SDL_LogError(SDL_LOG_CATEGORY_ERROR, "SQLite prepare failed: %s",
		             sqlite3_errmsg(db));
		return false;
	}

	sqlite3_bind_text(stmt, 1, lang.c_str(), -1, SQLITE_STATIC);

	ws.db->begin_transaction();

	Arena scratch(8 * 1024 * 1024); // 8 MB
	Size count = 0;
	const int8_t current_lang_id = lang_code_to_id(lang);

	while (sqlite3_step(stmt) == SQLITE_ROW) {
		auto guard = scratch.guard();

		const uint64_t word_id =
			  static_cast<uint64_t>(sqlite3_column_int64(stmt, 0));

		const auto *sig_raw =
			  reinterpret_cast<const char *>(sqlite3_column_text(stmt, 1));
		const auto sig_len = static_cast<Size>(sqlite3_column_bytes(stmt, 1));
		const StrView sig =
			  (sig_raw && sig_len > 0) ? StrView{sig_raw, sig_len} : StrView{};

		const auto *plain_raw =
			  reinterpret_cast<const char *>(sqlite3_column_text(stmt, 2));
		const auto plain_len = static_cast<Size>(sqlite3_column_bytes(stmt, 2));
		const StrView plain_tr = (plain_raw && plain_len > 0)
		                               ? StrView{plain_raw, plain_len}
		                               : StrView{};

		const auto *payload_raw =
			  reinterpret_cast<const char *>(sqlite3_column_text(stmt, 3));
		const auto payload_len =
			  static_cast<Size>(sqlite3_column_bytes(stmt, 3));
		const StrView payload = (payload_raw && payload_len > 0)
		                              ? StrView{payload_raw, payload_len}
		                              : StrView{};

		const uint8_t pop = static_cast<uint8_t>(sqlite3_column_int(stmt, 4));

		Word word{};
		word.word_id = WordId{word_id};
		word.popularity = pop;
		word.lang_id = current_lang_id;

		if (!parse_line(scratch, sig, word)) {
			continue;
		}

		if (plain_tr) {
			word.translations_raw = plain_tr.copy(scratch);
		}
		if (payload) {
			word.json_payload = payload.copy(scratch);
		}

		ws.save_direct(scratch, word, /*mark_dirty=*/false);
		++count;

		if (count % 25000 == 0) {
			SDL_Log("[%s] Processed %" PRSize " words...", lang.c_str(), count);
		}
	}

	sqlite3_finalize(stmt);

	ws.db->commit_transaction();
	ws.close();

	m.lap().printus();
	SDL_Log("[%s] Finished! Total words indexed: %" PRSize, lang.c_str(),
	        count);
	return true;
}

} // namespace

int main(int argc, char **argv) {
	std::string sqlite_path = "dictionary_master.db";
	std::string output_dir = "../klappt-resources";

	if (argc > 1)
		sqlite_path = argv[1];
	if (argc > 2)
		output_dir = argv[2];

	SDL_Log("Opening SQLite Master DB: %s", sqlite_path.c_str());
	SDL_Log("Output resources directory: %s", output_dir.c_str());

	std::filesystem::create_directories(output_dir);

	sqlite3 *db = nullptr;
	if (sqlite3_open_v2(sqlite_path.c_str(), &db, SQLITE_OPEN_READONLY,
	                    nullptr) != SQLITE_OK) {
		SDL_LogError(SDL_LOG_CATEGORY_ERROR, "Cannot open SQLite DB: %s",
		             sqlite3_errmsg(db));
		return 1;
	}

	const std::vector<std::string> languages = {"de", "ru", "en", "tr"};

	bool all_ok = true;
	for (const auto &lang : languages) {
		if (!build_language_database(db, lang, output_dir)) {
			all_ok = false;
			break;
		}
	}

	sqlite3_close(db);

	if (all_ok) {
		SDL_Log("ALL XAPIAN DATABASES SUCCESSFULLY BUILT!");
		return 0;
	}
	return 1;
}
