#pragma once

#include "SDL3/SDL_log.h"

#include <filesystem>

#include "app/app_context.h"
#include "app/assets_dl.h"
#include "base/arena.h"
#include "base/measure.h"
#include "base/str_view.h"
#include "domain/word.h"
#include "domain/word_store.h"
#include "domain/words_codec.h"
#include "domain/wparser.h"
#include "platform/files.h"
#include "platform/neuro.h"
#include "platform/web_persist.h"
#include "ui/translations/langs.h"

inline StrView str_view(const std::string &value) {
	return {value.data(), static_cast<Size>(value.size())};
}

inline void save_words_dat(Arena &scratch, const Settings &settings,
                           const Words &words) {
	auto guard = scratch.guard();
	auto encoded = WordsCodec::encode(scratch, words);
	if (!encoded) {
		SDL_LogError(SDL_LOG_CATEGORY_ERROR, "WordsCodec::encode failed");
		return;
	}
	const auto leaf = AssetsDL::words_snapshot_leaf(settings.tr_language);
	if (!file_save(scratch, leaf, encoded.data, encoded.size)) {
		SDL_LogError(SDL_LOG_CATEGORY_ERROR, "Saving " StrView_Fmt " failed",
		             StrView_Arg(leaf));
	}
}

inline bool sync_learning_words_to_store(Arena &scratch, WordStore &store,
                                         Words &words, bool &changed) {
	for (auto ref = words.begin(); ref < words.end(); ref.advance(&words)) {
		auto &word = words[ref];
		auto previous_id = word.word_id;
		if (!store.ensure_word(scratch, word)) {
			return false;
		}
		changed = changed || word.word_id != previous_id;
	}
	return true;
}

inline bool sync_learning_words_to_states(Engine::States &states,
                                          const Words &words,
                                          Size &added_count) {
	for (auto ref = words.begin(); ref < words.end(); ref.advance(&words)) {
		const auto &word = words[ref];
		Engine::State state{};
		auto [success, was_found] = states.get(word.word_id, state);
		if (!success) {
			SDL_LogError(SDL_LOG_CATEGORY_ERROR,
			             "Syncing learning state failed for word_id=%llu",
			             static_cast<unsigned long long>(word.word_id.value));
			return false;
		}
		if (was_found) {
			continue;
		}
		SDL_Log("Creating learning state for word_id=%llu",
		        static_cast<unsigned long long>(word.word_id.value));
		if (!states.set(word.word_id, state)) {
			SDL_LogError(SDL_LOG_CATEGORY_ERROR,
			             "Creating learning state failed for word_id=%llu",
			             static_cast<unsigned long long>(word.word_id.value));
			return false;
		}
		++added_count;
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
			             i, StrView_Arg(word_most_meaningfull_lemma(word)));
			return false;
		}
		// SDL_Log("import parsed word #%d done new=%d id=%llu", i,
		//         was_new ? 1 : 0,
		//         static_cast<unsigned long long>(imported.word_id.value));
		if (!was_new) {
			continue;
		}
		++added_count;
		if (added_count % 1024 == 0) {
			SDL_Log("added %d of %d", added_count, parsed.size);
		}
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
		return word.n.lemma == spec.key && word.n.gender != Gender::none;
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
		if (!success) {
			return false;
		}
		if (!was_found) {
			if (!states.set(word.word_id, state)) {
				return false;
			}
		}
		return true;
	} else {
		return false;
	}
}

inline bool seed_default_learning_list(const DynArr<Word> &parsed,
                                       AppContext &ctx) {
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

		auto word = word_clone(ctx.arena, *parsed_word);
		if (!ctx.word_store.ensure_word(ctx.arena_frame, word)) {
			SDL_LogError(
				  SDL_LOG_CATEGORY_ERROR,
				  "Ensuring default learning-list seed failed: " StrView_Fmt,
				  StrView_Arg(spec.key));
			return false;
		}
		if (!add_word_to_learning_list_seeded(ctx.arena_frame, word, *ctx.words,
		                                      ctx.word_store, ctx.states)) {
			ctx.app_status.push_error("Failed to seed learning list"_v);
		}
		added_any = true;
	}

	if (added_any) {
		save_words_dat(ctx.arena_frame, ctx.settings, *ctx.words);
	}
	return true;
}

inline bool txt_to_xapian(AppContext &ctx,
                          const std::filesystem::path &basePath) {
	const auto source_leaf = "en.txt"_v; // NOTE: _____!!!!!!!!! HERE
	Measure m{__FUNCTION__};
	const auto lang = ctx.settings.tr_language;
	// const auto words_leaf = Settings::words_snapshot_leaf(lang);
	// const auto word_store_leaf = Settings::word_store_leaf(lang);
	// const auto states_leaf = Settings::states_store_leaf(lang);

	Arena a(1 << 30); // TODO: think harder

	// ***************************************************
	// .txt file parsing and xapian db update
	DynArr<Word> parsed_words;

	const auto source_path =
		  basePath / "word_data" /
		  std::string(source_leaf.data, static_cast<size_t>(source_leaf.size));
	if (!wparse_file(a, source_path.c_str(), parsed_words, &ctx.app_status)) {
		SDL_LogError(SDL_LOG_CATEGORY_ERROR,
		             "Loading source words from " StrView_Fmt " failed",
		             StrView_Arg(source_leaf));
		return false;
	}
	m.point().printus("txt file parsed");
	if (parsed_words.size > ctx.word_store.word_count()) {
		// TODO: update logic
		SDL_Log("parsed %d, but we have %d. Should import the words",
		        parsed_words.size, ctx.word_store.word_count());

		Size added_count = 0;
		if (!import_new_parsed_words(a, ctx.word_store, parsed_words,
		                             added_count)) {
			SDL_LogError(SDL_LOG_CATEGORY_ERROR,
			             "Importing parsed words from " StrView_Fmt " failed",
			             StrView_Arg(source_leaf));
			return false;
		}
		m.point().printus("new parsed words imported");
		if (added_count > 0) {
			SDL_Log("Imported %d new words from " StrView_Fmt,
			        static_cast<int>(added_count), StrView_Arg(source_leaf));
		}

		if (ctx.words->size == 0 &&
		    !seed_default_learning_list(parsed_words, ctx)) {
			SDL_LogError(SDL_LOG_CATEGORY_ERROR,
			             "Seeding default learning list failed");
			return false;
		}
		m.point().printus("seeded if had been needed");
	} else {
		SDL_Log("parsed %d and we have %d", parsed_words.size,
		        ctx.word_store.word_count());
	}
	m.lap().printus("import active dictionary");
	// ***************************************************

	return true;
}

inline bool init_runtime_data(AppContext &ctx) {
	KLAPPT_PROFILE_SCOPE_N("init_runtime_data");

	if (ctx.settings.is_using_tts) {
		init_tts(&ctx);
	}
	if (ctx.settings.is_using_asr) {
		init_asr(&ctx);
	}

	Measure m{__FUNCTION__};

	WebPersistBatch persist_batch;

	const auto lang = ctx.settings.tr_language;
	const auto words_leaf = AssetsDL::words_snapshot_leaf(lang);
	const auto word_store_leaf = AssetsDL::word_store_leaf(lang);
	const auto states_leaf = AssetsDL::states_store_leaf(lang);

	const auto lcode = lang_code(lang);
	SDL_Log("Active translation language: " StrView_Fmt, StrView_Arg(lcode));

	auto &scratch = ctx.arena_frame;
	auto g = scratch.guard();
	{ // opening
		auto states_path = get_writable_file_path_for(scratch, states_leaf);
		if (!ctx.states.open(states_path)) {
			SDL_LogError(SDL_LOG_CATEGORY_ERROR,
			             "Opening states storage failed");
			return false;
		}
		m.lap().printus("learning states opened");

		auto word_store_path =
			  get_writable_file_path_for(scratch, word_store_leaf);
		if (!ctx.word_store.open(word_store_path)) {
			SDL_LogError(SDL_LOG_CATEGORY_ERROR, "Opening Xapian words failed");
			return false;
		}
		m.lap().printus("xapian opened");

		ctx.words = new Words;
		FileLoader fl{};
		if (fl.load(scratch, words_leaf)) {
			SDL_Log(StrView_Fmt " loaded: %d bytes", StrView_Arg(words_leaf),
			        fl.size);
			if (!WordsCodec::decode(ctx.arena, fl.data, fl.size, *ctx.words)) {
				SDL_Log(StrView_Fmt
				        " exists but is not a valid WordsCodec blob; "
				        "resetting learning list snapshot",
				        StrView_Arg(words_leaf));
				auto fpath = get_writable_file_path_for(scratch, words_leaf);
				if (SDL_RemovePath(fpath.to_cstr(scratch))) {
					SDL_Log(StrView_Fmt ": removed", StrView_Arg(words_leaf));
				} else {
					SDL_Log(StrView_Fmt ": failed to remove",
					        StrView_Arg(words_leaf));
				}
				*ctx.words = {};
			} else {
				SDL_Log("main arena usage after decode: %td / %d bytes",
				        ctx.arena.offset, ctx.arena.allocated_size);
			}
		} else {
			SDL_Log(StrView_Fmt
			        " not found; starting with an empty learning list",
			        StrView_Arg(words_leaf));
		}
		SDL_Log(StrView_Fmt " size: %d words", StrView_Arg(words_leaf),
		        ctx.words->size);
		m.lap().printus("learning list opened");
	}

	bool words_list_changed = false;
	if (!sync_learning_words_to_store(ctx.arena_frame, ctx.word_store,
	                                  *ctx.words, words_list_changed)) {
		SDL_LogError(SDL_LOG_CATEGORY_ERROR,
		             "Syncing learning list to Xapian failed");
		return false;
	}
	m.lap().printus("sync words snapshot");

	Size added_states = 0;
	if (!sync_learning_words_to_states(ctx.states, *ctx.words, added_states)) {
		SDL_LogError(SDL_LOG_CATEGORY_ERROR,
		             "Syncing learning list to states failed");
		return false;
	}
	if (added_states > 0) {
		SDL_Log("Created %d missing learning states",
		        static_cast<int>(added_states));
	}
	m.lap().printus("sync states snapshot");

	if (words_list_changed) {
		save_words_dat(ctx.arena_frame, ctx.settings, *ctx.words);
		m.lap().printus("save remapped snapshot");
	}

	SDL_Log("main arena usage after startup load: %td / %d bytes",
	        ctx.arena.offset, ctx.arena.allocated_size);
	SDL_Log("==================");
	return true;
}
