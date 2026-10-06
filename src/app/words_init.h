#pragma once

#include "SDL3/SDL_log.h"

#include "app/app_context.h"
#include "app/assets_dl.h"
#include "base/arena.h"
#include "base/measure.h"
#include "base/str_view.h"
#include "domain/word.h"
#include "domain/word_store.h"
#include "domain/words_codec.h"
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
	auto encoded = WordsCodec::words_encode(scratch, words);
	if (!encoded) {
		SDL_LogError(SDL_LOG_CATEGORY_ERROR, "WordsCodec::encode failed");
		return;
	}
	const auto leaf = words_snapshot_leaf[settings.tr_language];
	if (!file_save_relative(scratch, leaf, encoded.data, encoded.size)) {
		SDL_LogError(SDL_LOG_CATEGORY_ERROR, "Saving " StrView_Fmt " failed",
		             StrView_Arg(leaf));
	}
}

inline void sync_learning_words_to_store(Arena &scratch, const WordStore &store,
                                         Words &words, bool &changed) {
	for (auto ref = words.begin(); ref < words.end(); ref.advance(&words)) {
		auto &word = words[ref];

		if (word.word_id.value == 0 || word.type == WordType::Nil) {
			words.remove_by_ref(ref);
			changed = true;
			continue;
		}

		if (!store.ensure_word(scratch, word)) {
			SDL_Log("Word ID %" PRSize
			        " not found in store, removing from list",
			        word.word_id.value);
			words.remove_by_ref(ref);
			changed = true;
			continue;
		}
	}
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

inline bool add_word_to_learning_list_seeded(Arena &tmparena, Word &word,
                                             Words &words,
                                             WordStore &word_store,
                                             Engine::States &states) {
	word.in_learning_list = true;
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
	}
	return false;
}

inline bool seed_default_learning_list(AppContext &ctx) {
	(void)ctx;
	return true; // TODO: rethink
}

inline bool init_runtime_data(AppContext &ctx) {
	KLAPPT_PROFILE_SCOPE_N("init_runtime_data");

#if NEURO
	if (ctx.settings.is_module_tts) {
		init_tts(&ctx);
	}
	if (ctx.settings.is_module_asr) {
		init_asr(&ctx);
	}
#endif

	Measure m{__FUNCTION__};

	WebPersistBatch persist_batch;

	const auto lang = ctx.settings.tr_language;
	const auto asset_id = AssetsDL::dict_asset_id_for_lang(lang);

	auto &scratch = ctx.arena_frame;
	auto g = scratch.guard();

	if (!AssetsDL::is_installed(scratch, asset_id)) {
		auto code = lang_code(lang);
		SDL_LogError(SDL_LOG_CATEGORY_ERROR,
		             "Cannot init runtime data: dictionary for " StrView_Fmt
		             " is not installed",
		             StrView_Arg(code));
		return false;
	}

	const auto words_leaf = words_snapshot_leaf[lang];
	const auto states_leaf = states_store_leaf[lang];
	const auto word_store_path = AssetsDL::target_path(scratch, asset_id);

	const auto lcode = lang_code(lang);
	SDL_Log("Active translation language: " StrView_Fmt, StrView_Arg(lcode));

	{ 
		ctx.word_store.close();
		ctx.states.close();

		auto states_path = get_writable_file_path_for(scratch, states_leaf);
		if (!ctx.states.open(states_path)) {
			SDL_LogError(SDL_LOG_CATEGORY_ERROR,
			             "Opening states storage failed");
			return false;
		}
		m.lap().printus("learning states opened");

		if (!ctx.word_store.open(word_store_path)) {
			SDL_LogError(SDL_LOG_CATEGORY_ERROR, "Opening Xapian words failed");
			return false;
		}
		m.lap().printus("xapian opened");

		if (ctx.words) {
			delete ctx.words;
			ctx.words = nullptr;
		}
		ctx.words = new Words;

		FileLoader fl{};
		if (fl.load_from_writable(scratch, words_leaf)) {
			SDL_Log(StrView_Fmt " loaded: %" PRSize " bytes",
			        StrView_Arg(words_leaf), fl.size);
			if (!WordsCodec::words_decode(ctx.arena, fl.data, fl.size,
			                              *ctx.words)) {
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
				SDL_Log("main arena usage after decode: %" PRSize " / %" PRSize
				        " bytes",
				        ctx.arena.offset, ctx.arena.capacity);
			}
		} else {
			SDL_Log(StrView_Fmt
			        " not found; starting with an empty learning list",
			        StrView_Arg(words_leaf));
		}
		SDL_Log(StrView_Fmt " size: %" PRSize " words", StrView_Arg(words_leaf),
		        ctx.words->size);
		m.lap().printus("learning list opened");
	}

	bool words_list_changed = false;
	sync_learning_words_to_store(ctx.arena_frame, ctx.word_store, *ctx.words,
	                             words_list_changed);
	m.lap().printus("sync words snapshot");

	Size added_states = 0;
	if (!sync_learning_words_to_states(ctx.states, *ctx.words, added_states)) {
		SDL_LogError(SDL_LOG_CATEGORY_ERROR,
		             "Syncing learning list to states failed");
		return false;
	}
	if (added_states > 0) {
		SDL_Log("Created %" PRSize " missing learning states", added_states);
	}
	m.lap().printus("sync states snapshot");

	if (words_list_changed) {
		save_words_dat(ctx.arena_frame, ctx.settings, *ctx.words);
		m.lap().printus("save remapped snapshot");
	}

	SDL_Log("main arena usage after startup load: %" PRSize " / %" PRSize
	        " bytes",
	        ctx.arena.offset, ctx.arena.capacity);
	SDL_Log("==================");
	return true;
}
