#pragma once

#include "SDL3/SDL_log.h"

#include <filesystem>

#include "app/app_context.h"
#include "base/measure.h"
#include "domain/word_store.h"
#include "domain/words_codec.h"
#include "domain/wparser.h"
#include "platform/files.h"

inline StrView str_view(const std::string &value) {
	return {value.data(), static_cast<Size>(value.size())};
}

inline void save_words_dat(Arena &tmp, const Settings &settings,
                           const Words &words) {
	auto guard = tmp.guard();
	auto encoded = WordsCodec::encode(tmp, words);
	if (!encoded) {
		SDL_LogError(SDL_LOG_CATEGORY_ERROR, "WordsCodec::encode failed");
		return;
	}
	const auto leaf = Settings::words_snapshot_leaf(settings.tr_language);
	if (!file_save(leaf, encoded.data, encoded.size)) {
		SDL_LogError(SDL_LOG_CATEGORY_ERROR, "Saving " StrView_Fmt " failed",
		             StrView_Arg(leaf));
	}
}

inline bool sync_learning_words_to_store(Arena &scratch, WordStore &store,
                                         Words &words, bool &changed) {
	for (Size ref = 1; ref < Words::MAX_WORDS; ++ref) {
		if (!words.is_used({ref})) {
			continue;
		}
		auto &word = words[{ref}];
		auto previous_id = word.word_id;
		if (!store.ensure_word(scratch, word)) {
			return false;
		}
		changed = changed || word.word_id != previous_id;
	}
	return true;
}

// inline bool import_new_parsed_words(Arena &scratch, WordStore &store,
//                                     const Words &parsed, Size &added_count) {
// 	for (Size ref = 1; ref < Words::MAX_WORDS; ++ref) {
// 		if (!parsed.is_used({ref})) {
// 			continue;
// 		}
// 		auto imported = parsed[ref];
// 		bool was_new = false;
// 		if (!store.ensure_word(scratch, imported, &was_new)) {
// 			return false;
// 		}
// 		if (!was_new) {
// 			continue;
// 		}
// 		++added_count;
// 	}
// 	return true;
// }

inline bool import_new_parsed_words(Arena &scratch, WordStore &store,
                                    const DynArr<Word> &parsed,
                                    Size &added_count) {
	for (Size i = 0; i < parsed.size; ++i) {
		const auto &word = parsed[i];
		auto imported = word;
		// SDL_Log("import parsed word #%d type=%d lemma=" StrView_Fmt, i,
		//         static_cast<int>(word.type),
		//         StrView_Arg(most_meaningfull_lemma(word)));
		bool was_new = false;
		if (!store.ensure_word(scratch, imported, &was_new)) {
			SDL_LogError(SDL_LOG_CATEGORY_ERROR,
			             "Importing parsed word #%d failed (" StrView_Fmt ")",
			             i, StrView_Arg(most_meaningfull_lemma(word)));
			return false;
		}
		// SDL_Log("import parsed word #%d done new=%d id=%llu", i,
		//         was_new ? 1 : 0,
		//         static_cast<unsigned long long>(imported.word_id.value));
		if (!was_new) {
			continue;
		}
		++added_count;
	}
	return true;
}

struct LearningListSeedSpec {
	WordType type;
	StrView key;
};

inline bool learning_list_seed_matches(const Word &word,
                                       const LearningListSeedSpec &spec) {
	if (word.type != spec.type) {
		return false;
	}
	switch (word.type) {
	case WordType::Verb:
		return word.v.infinitive == spec.key;
	case WordType::Adj:
		return word.a.lemma == spec.key;
	case WordType::Noun:
		return word.n.lemma == spec.key;
	case WordType::Phrase:
		return word.p.text == spec.key;
	case WordType::Nil:
		return false;
	}
	return false;
}

inline const Word *
find_learning_list_seed_word(const DynArr<Word> &parsed,
                             const LearningListSeedSpec &spec) {
	for (Size i = 0; i < parsed.size; ++i) {
		if (learning_list_seed_matches(parsed[i], spec)) {
			return &parsed[i];
		}
	}
	return nullptr;
}

inline bool add_word_to_learning_list_seeded(Arena &tmparena, Word &word,
                                             Words &words,
                                             WordStore &word_store,
                                             Engine::States &states) {
	constexpr int8_t LEARNING_LIST_ID = 1;
	word.in_learning_list = LEARNING_LIST_ID;
	auto word_ref = words.add();
	if (word_ref != Words::null_index()) {
		words[word_ref] = word;
		word_store.save(tmparena, word);
		Engine::State state{};
		auto [success, was_found] = states.get(word.word_id, state);
		(void)success;
		if (!was_found) {
			states.set(word.word_id, state);
		}
		return true;
	} else {
		return false;
	}
}

inline bool seed_default_learning_list(const DynArr<Word> &parsed,
                                       AppContext &state) {
	static constexpr LearningListSeedSpec DEFAULT_SEEDS[] = {
		  {WordType::Verb, "sein"_v},
		  {WordType::Verb, "kommen"_v},
		  {WordType::Verb, "sehen"_v},
		  {WordType::Verb, "geben"_v},
		  {WordType::Verb, "nehmen"_v},
		  {WordType::Adj, "alt"_v},
		  {WordType::Adj, "neu"_v},
		  {WordType::Adj, "groß"_v},
		  {WordType::Adj, "klein"_v},
		  {WordType::Adj, "wichtig"_v},
		  {WordType::Noun, "Auto"_v},
		  {WordType::Noun, "Buch"_v},
		  {WordType::Noun, "Computer"_v},
		  {WordType::Noun, "Apfel"_v},
		  {WordType::Noun, "Wohnung"_v},
		  {WordType::Phrase, "Los geht's!"_v},
		  {WordType::Phrase, "Ich bin dafür!"_v},
	};

	bool added_any = false;
	for (const auto &spec : DEFAULT_SEEDS) {
		const auto *parsed_word = find_learning_list_seed_word(parsed, spec);
		if (!parsed_word) {
			SDL_LogError(SDL_LOG_CATEGORY_ERROR,
			             "Default learning-list seed not found: " StrView_Fmt,
			             StrView_Arg(spec.key));
			continue;
		}

		auto word = clone_word(state.arena, *parsed_word);
		if (!state.word_store.ensure_word(state.tmparena, word)) {
			SDL_LogError(
				  SDL_LOG_CATEGORY_ERROR,
				  "Ensuring default learning-list seed failed: " StrView_Fmt,
				  StrView_Arg(spec.key));
			return false;
		}
		if (!add_word_to_learning_list_seeded(state.tmparena, word,
		                                      *state.words, state.word_store,
		                                      state.states)) {
			state.app_status.push_error("Failed to seed learning list"_v);
		}
		added_any = true;
	}

	if (added_any) {
		save_words_dat(state.tmparena, state.settings, *state.words);
	}
	return true;
}

inline bool init_words(AppContext &state,
                       const std::filesystem::path &basePath) {
	Measure m{__PRETTY_FUNCTION__};
	WebPersistBatch persist_batch;
	const auto lang = state.settings.tr_language;
	const auto lang_code = Settings::translation_language_code(lang);
	const auto source_leaf = Settings::translation_source_leaf(lang);
	const auto words_leaf = Settings::words_snapshot_leaf(lang);
	const auto word_store_leaf = Settings::word_store_leaf(lang);
	const auto states_leaf = Settings::states_store_leaf(lang);

	SDL_Log("Active translation language: " StrView_Fmt,
	        StrView_Arg(lang_code));

	state.words = new Words;
	const auto states_path = FileLoader::path_for(states_leaf);
	if (states_path.empty() || !state.states.open(str_view(states_path))) {
		SDL_LogError(SDL_LOG_CATEGORY_ERROR, "Opening states storage failed");
		return false;
	}
	m.lap().printus("states");
	const auto word_store_path = FileLoader::path_for(word_store_leaf);
	if (word_store_path.empty() ||
	    !state.word_store.open(str_view(word_store_path))) {
		SDL_LogError(SDL_LOG_CATEGORY_ERROR, "Opening Xapian words failed");
		return false;
	}
	m.lap().printus("xapian");

	FileLoader fl{};
	if (fl.load(words_leaf)) {
		SDL_Log(StrView_Fmt " loaded: %d bytes", StrView_Arg(words_leaf),
		        fl.size);
		if (!WordsCodec::decode(state.arena, fl.data, fl.size, *state.words)) {
			SDL_Log(StrView_Fmt " exists but is not a valid WordsCodec blob; "
			                    "resetting learning list snapshot",
			        StrView_Arg(words_leaf));
			auto fpath = FileLoader::path_for(words_leaf);
			if (SDL_RemovePath(fpath.c_str())) {
				SDL_Log(StrView_Fmt ": removed", StrView_Arg(words_leaf));
			} else {
				SDL_Log(StrView_Fmt ": failed to remove",
				        StrView_Arg(words_leaf));
			}
			*state.words = {};
		} else {
			SDL_Log("main arena usage after decode: %td / %d bytes",
			        state.arena.offset, state.arena.allocated_size);
		}
	} else {
		SDL_Log(StrView_Fmt " not found; starting with an empty learning list",
		        StrView_Arg(words_leaf));
	}
	SDL_Log(StrView_Fmt " size: %d words", StrView_Arg(words_leaf),
	        state.words->size);
	m.lap().printus("load words snapshot");

	bool changed = false;
	if (!sync_learning_words_to_store(state.tmparena, state.word_store,
	                                  *state.words, changed)) {
		SDL_LogError(SDL_LOG_CATEGORY_ERROR,
		             "Syncing learning list to Xapian failed");
		return false;
	}
	m.lap().printus("sync words snapshot");
	if (changed) {
		save_words_dat(state.tmparena, state.settings, *state.words);
		m.lap().printus("save remapped snapshot");
	}

	{
		auto guard = state.tmparena.guard();
		DynArr<Word> parsed_words;
		const auto source_path =
			  basePath / "word_data" /
			  std::string(source_leaf.data,
		                  static_cast<size_t>(source_leaf.size));
		if (!wparse_file(state.tmparena, source_path.c_str(), parsed_words,
		                 &state.app_status)) {
			SDL_LogError(SDL_LOG_CATEGORY_ERROR,
			             "Loading source words from " StrView_Fmt " failed",
			             StrView_Arg(source_leaf));
			return false;
		}
		m.point().printus("file parsed");
		if (parsed_words.size != state.word_store.word_count()) {
			SDL_Log("parsed %d, but we have %d. Should import the words",
			        parsed_words.size, state.word_store.word_count());

			Size added_count = 0;
			if (!import_new_parsed_words(state.tmparena, state.word_store,
			                             parsed_words, added_count)) {
				SDL_LogError(SDL_LOG_CATEGORY_ERROR,
				             "Importing parsed words from " StrView_Fmt
				             " failed",
				             StrView_Arg(source_leaf));
				return false;
			}
			m.point().printus("new parsed words imported");
			if (added_count > 0) {
				SDL_Log("Imported %d new words from " StrView_Fmt,
				        static_cast<int>(added_count),
				        StrView_Arg(source_leaf));
			}

			if (state.words->size == 0 &&
			    !seed_default_learning_list(parsed_words, state)) {
				SDL_LogError(SDL_LOG_CATEGORY_ERROR,
				             "Seeding default learning list failed");
				return false;
			}
			m.point().printus("seeded if was needed");
		} else {
			SDL_Log("parsed %d and we have %d", parsed_words.size,
			        state.word_store.word_count());
		}
	}
	m.lap().printus("import active dictionary");

	SDL_Log("main arena usage after startup load: %td / %d bytes",
	        state.arena.offset, state.arena.allocated_size);
	SDL_Log("==================");
	return true;
}
