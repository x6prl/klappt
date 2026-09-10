#include "exercises.h"

#include "SDL3/SDL_log.h"
#include "app/app_context.h"
#include "base/arena.h"
#include "base/dyn_arr.h"
#include "base/measure.h"
#include "base/pair.h"
#include "base/profiler.h"
#include "base/shuffle.h"
#include "base/str_builder.h"
#include "base/str_view.h"
#include "engine.h"
#include "tokenizer.h"
#include "words.h"

#include <algorithm>
#include <cstdint>
#include <ctime>
#include <simdjson/simdjson.h>

namespace Engine {

namespace {

static uint64_t rng_state{50987654321}; // TODO: just use ctx->ticks

static Arr<StrView, 4> article_options_a = {"der"_v, "die"_v, "das"_v, "—"_v};
// constexpr Arr<StrView, 8> plurals = {
// 	  "-e"_v, "-en"_v, "-n"_v, "-er"_v, "-s"_v, "\"-e"_v, "\"-er"_v, "\"-"_v,
// };
constexpr Arr<StrView, 8> plurals = {
	  "-e"_v, "-en"_v, "-n"_v, "-er"_v, "-s"_v, "¨-e"_v, "¨-er"_v, "¨-"_v,
};

// strips explanatory parentheticals (), [] and extracts the top headwords
StrView sanitize_prompt_headword(Arena &scratch, Arena &a, StrView raw) {
	if (!raw) {
		return {};
	}

	char *buf = scratch.pushN<char>(raw.size);
	Size len = 0;
	int paren_depth = 0;
	int bracket_depth = 0;

	for (Size i = 0; i < raw.size; ++i) {
		char ch = raw[i];
		if (ch == '(') {
			paren_depth++;
		} else if (ch == ')') {
			if (paren_depth > 0) {
				paren_depth--;
			}
		} else if (ch == '[') {
			bracket_depth++;
		} else if (ch == ']') {
			if (bracket_depth > 0) {
				bracket_depth--;
			}
		} else if (paren_depth == 0 && bracket_depth == 0) {
			buf[len++] = ch;
		}
	}

	StrView cleaned = StrView{buf, len}.trim();
	if (!cleaned) {
		return {};
	}

	// extract the first clean headword term
	auto first_comma = cleaned.split_by(',').first.trim();
	auto first_semi = first_comma.split_by(';').first.trim();
	if (!first_semi) {
		return {};
	}

	// optionally include a short second term if space permits (<= 35 chars)
	auto after_comma = cleaned.split_by(',').second.trim();
	if (after_comma) {
		auto second_item = after_comma.split_by(',').first.trim();
		second_item = second_item.split_by(';').first.trim();
		if (second_item && first_semi.size + second_item.size + 2 <= 35) {
			return StrBuilder::concat(a, first_semi, ", "_v, second_item);
		}
	}

	return first_semi.copy(a);
}

// extracts primary prompt and sub-prompts
StrView extract_exercise_prompts(Arena &a, Arena &scratch, const Word &word,
                                 ExerciseState *exercise,
                                 simdjson::dom::parser &parser) {
	WordPayload payload{};
	bool has_payload =
		  word_json_parse(scratch, scratch, word.json_payload, payload, parser);

	StrView primary_raw{};
	DynArr<StrView> secondary_candidates{};
	StrView valency{};

	if (has_payload) {
		for (Size i = 0; i < payload.senses.size; ++i) {
			const auto &sense = payload.senses[i];
			for (Size j = 0; j < sense.translations.size; ++j) {
				if (sense.translations[j]) {
					if (!primary_raw) {
						primary_raw = sense.translations[j];
						if (sense.valency) {
							valency = sense.valency;
						}
					} else {
						secondary_candidates.push(scratch,
						                          sense.translations[j]);
					}
				}
			}
		}

		for (Size i = 0;
		     i < payload.synonyms.size && secondary_candidates.size < 4; ++i) {
			secondary_candidates.push(scratch, payload.synonyms[i]);
		}
	}

	if (!primary_raw) {
		auto [first, rest] = word.translations_raw.split_by(';');
		primary_raw = first ? first : word.translations_raw;
		if (rest) {
			secondary_candidates.push(scratch, rest);
		}
	}

	StrView primary = sanitize_prompt_headword(scratch, a, primary_raw);
	if (!primary) {
		primary = primary_raw.trim().copy(a);
	}

	// source_sub0: alternative translations / synonyms as a single line
	DynArr<StrView> clean_subs{};
	for (Size i = 0; i < secondary_candidates.size && clean_subs.size < 2;
	     ++i) {
		auto clean = sanitize_prompt_headword(scratch, scratch,
		                                      secondary_candidates[i]);
		if (clean && clean != primary && !clean_subs.is_contains(clean)) {
			clean_subs.push(scratch, clean);
		}
	}

	if (!clean_subs.is_empty()) {
		if (clean_subs.size == 1) {
			exercise->source_sub0 =
				  StrBuilder::concat(a, "also: "_v, clean_subs[0]);
		} else {
			exercise->source_sub0 = StrBuilder::concat(
				  a, "also: "_v, clean_subs[0], ", "_v, clean_subs[1]);
		}
	} else {
		exercise->source_sub0 = {};
	}

	// source_sub1: grammatical context / valency
	if (valency) {
		exercise->source_sub1 = valency.trim().copy(a);
	} else {
		exercise->source_sub1 = {};
	}

	return primary;
}

void append_article_stage(Arena &a, const Word &word, StrView source,
                          ExerciseState *exercise) {
	constexpr auto points_reward_for_correct_article = 2;
	auto gender = word.n.gender;
	auto correct_option_index = gender == Gender::m   ? 0
	                            : gender == Gender::f ? 1
	                            : gender == Gender::n ? 2
	                                                  : 3;
	ExerciseState::SubStage substage = {
		  .is_keypad = true,
		  .correct_option_index = correct_option_index,
		  .points_for_correct_answer = points_reward_for_correct_article,
		  .opts = DynArr<StrView>::from(article_options_a.data,
	                                    article_options_a.size())};
	exercise->stages.push(
		  a, {.source = source,
	          .form_type = FormType::Article,
	          .substages = DynArr<ExerciseState::SubStage>::with(a, substage)});
	exercise->points_max += points_reward_for_correct_article;
}

void append_common_stage_chunks(Arena &a, StrView str, Tokenizer::Kind kind,
                                FormType form_type, StrView source,
                                ExerciseState *exercise) {
	auto chunks = Tokenizer::to_chunks(a, str, kind);
	DynArr<ExerciseState::SubStage> substages{};
	for (auto &chunk : chunks) {
		auto opts = DynArr<StrView>::with<5>(a, chunk);
		auto distractors = Tokenizer::get_4_distractors_for_a_chunk(
			  chunk, static_cast<uint32_t>(rng_state));
		for (const auto &distractor : distractors) {
			opts.push(a, distractor.copy(a));
		}
		Size correct_option_index =
			  shuffle_str_views(opts.data, opts.size, &rng_state);
		substages.push(a, {.is_keypad = true,
		                   .correct_option_index = correct_option_index,
		                   .points_for_correct_answer = 1,
		                   .opts = opts});
	}
	exercise->stages.push(
		  a,
		  {.source = source, .form_type = form_type, .substages = substages});
	exercise->points_max += chunks.size;
}

void append_common_stage_compose(Arena &a, StrView str, FormType form_type,
                                 StrView source, ExerciseState *exercise) {
	auto letters = Tokenizer::to_letters(a, str);
	DynArr<ExerciseState::SubStage> substages{};
	// SDL_Log("Word: |" StrView_Fmt "|splitted to:", StrView_Arg(str));
	for (auto &letter : letters) {
		// SDL_Log("\tLetter: |" StrView_Fmt "|", StrView_Arg(letter));
		auto opts = DynArr<StrView>::with<5>(a, letter);
		auto distractors = Tokenizer::get_4_distractors_for_a_chunk(
			  letter, static_cast<uint32_t>(rng_state));
		for (const auto &distractor : distractors) {
			opts.push(a, distractor.copy(a));
		}
		Size correct_option_index =
			  shuffle_str_views(opts.data, opts.size, &rng_state);
		substages.push(a, {.is_keypad = true,
		                   .correct_option_index = correct_option_index,
		                   .points_for_correct_answer = 1,
		                   .opts = opts});
	}
	exercise->stages.push(
		  a,
		  {.source = source, .form_type = form_type, .substages = substages});
	exercise->points_max += letters.size;
}

/*
 * NOTE: str should not be empty
 */
void append_common_stage_gaps(Arena &a, StrView str, Tokenizer::Kind kind,
                              FormType form_type, StrView source,
                              ExerciseState *exercise) {
	auto chunks = Tokenizer::to_chunks(a, str, kind);

	Size rnd_idx{0};
	if (chunks.size > 1) {
		bool is_verb = kind == Tokenizer::Kind::Verb;
		if (is_verb) { // we do not want `en` to be missed — it is too light
			if (chunks.size != 2) {
				rnd_idx = random_num(1, chunks.size - 1, &rng_state);
			} else {
				rnd_idx = 0;
			}
		} else {
			rnd_idx = random_num(1, chunks.size, &rng_state);
		}
	}
	auto &chosen_chunk = chunks[rnd_idx];

	bool is_short_word = chunks.size < 5;
	// [from, to)
	Pair<Size, Size> left_part_idxs{};
	Pair<Size, Size> gap_idxs{};
	Pair<Size, Size> right_part_idxs{};
	// TODO: work with letters, not with bytes
	if (!is_short_word && 1 == chosen_chunk.size) {
		if (chunks[rnd_idx - 1].size == 1) {
			// merge to left
			left_part_idxs = {0, rnd_idx - 1};
			gap_idxs = {rnd_idx - 1, rnd_idx + 1};
			right_part_idxs = {rnd_idx + 1, chunks.size};
		} else if (chunks[rnd_idx + 1].size == 1) {
			// merge to right
			left_part_idxs = {0, rnd_idx};
			gap_idxs = {rnd_idx, rnd_idx + 1};
			right_part_idxs = {rnd_idx + 1, chunks.size};
		} else {
			left_part_idxs = {0, rnd_idx};
			gap_idxs = {rnd_idx, rnd_idx + 1};
			right_part_idxs = {rnd_idx + 1, chunks.size};
		}
	} else {
		left_part_idxs = {0, rnd_idx};
		gap_idxs = {rnd_idx, rnd_idx + 1};
		right_part_idxs = {rnd_idx + 1, chunks.size};
	}

	auto merge = [](Arena &a, DynArr<StrView> strs, auto indexes) -> StrView {
		auto [from, to] = indexes;
		StrBuilder arr{}; // TODO: do it straightforward
		for (auto i{from}; i < to; ++i) {
			arr.push(a, strs[i]);
		}
		return arr.join(a);
	};

	auto left_part = merge(a, chunks, left_part_idxs);
	auto gap = merge(a, chunks, gap_idxs);
	auto right_part = merge(a, chunks, right_part_idxs);
	const auto gap_len = gap.utf8_length();

	DynArr<ExerciseState::SubStage> substages{};

	{
		const auto points_for_correct_answer = gap_len;
		auto opts = DynArr<StrView>::with<5>(a, gap);
		auto distractors = Tokenizer::get_4_distractors_for_a_chunk(
			  gap, static_cast<uint32_t>(rng_state));
		for (const auto &distractor : distractors) {
			opts.push(a, distractor.copy(a));
		}
		Size correct_option_index =
			  shuffle_str_views(opts.data, opts.size, &rng_state);
		substages.push(a,
		               {.is_keypad = true,
		                .correct_option_index = correct_option_index,
		                .points_for_correct_answer = points_for_correct_answer,
		                .opts = opts});
	}

	StrBuilder answer_while_prompt_builder{};
	answer_while_prompt_builder.push(a, left_part);
	for (Size i{}; i < gap_len; ++i) {
		answer_while_prompt_builder.push(a, "_"_v);
	}
	answer_while_prompt_builder.push(a, right_part);

	exercise->stages.push(
		  a, {
				   .source = source,
				   .before_answer = answer_while_prompt_builder.join(a),
				   .form_type = form_type,
				   .substages = substages,
				   .gap = {.left_part = left_part, .right_part = right_part},
			 });
	exercise->points_max += gap_len;
}

void append_common_stage_entire(Arena &a, Arena &scratch,
                                const StrView correct_str_raw,
                                const DynArr<StrView> all_strs,
                                FormType form_type, StrView source,
                                ExerciseState *exercise) {
	constexpr Size OPTIONS_MAX = 5;
	const StrView correct_str = correct_str_raw.trim();
	auto points_reward_for_that_stage = correct_str.utf8_length();

	// 1. gather all candidates
	DynArr<StrView> candidates{};
	for (Size i = 0; i < all_strs.size; ++i) {
		auto candidate = all_strs[i].trim();
		if (candidate && candidate != correct_str &&
		    !candidates.is_contains(candidate)) {
			candidates.push(scratch, candidate);
		}
	}

	// 2. shuffle candidates in-place
	if (candidates.size > 1) {
		shuffle_str_views(candidates.data, candidates.size, &rng_state);
	}

	// 3. take distractors
	Size distractor_count = std::min<Size>(candidates.size, OPTIONS_MAX - 1);
	DynArr<StrView> opts = DynArr<StrView>::with<OPTIONS_MAX>(a, correct_str);
	for (Size i = 0; i < distractor_count; ++i) {
		opts.push(a, candidates[i]);
	}

	// 4. shuffle options
	auto correct_option_index =
		  shuffle_str_views(opts.data, opts.size, &rng_state);

	ExerciseState::SubStage substage = {
		  .is_keypad = false,
		  .correct_option_index = correct_option_index,
		  .points_for_correct_answer = points_reward_for_that_stage,
		  .opts = opts};
	exercise->stages.push(
		  a, {.source = source,
	          .form_type = form_type,
	          .substages = DynArr<ExerciseState::SubStage>::with(a, substage)});
	exercise->points_max += points_reward_for_that_stage;
}

void append_stages_aux_and_past_participle(
	  Arena &a, Arena &scratch, StrView aux_and_past_participle,
	  const DynArr<StrView> past_participle_list, StrView source_aux,
	  StrView source_pp, ExerciseState *exercise, Mode mode) {
	constexpr Size points_reward_for_aux_stage = 1;
	static Arr<StrView, 2> aux_list = {"hat"_v, "ist"_v};
	StrView aux{};
	StrView past_participle{};

	aux_and_past_participle = aux_and_past_participle.trim();
	if (aux_and_past_participle.is_contains(' ')) {
		auto split = aux_and_past_participle.split();
		aux = split.first.trim();
		past_participle = split.second.trim();
		if (!aux_list.is_contains(aux)) {
			SDL_LogError(
				  SDL_LOG_CATEGORY_ERROR,
				  "unknown verb aux: |" StrView_Fmt "| in |" StrView_Fmt "|",
				  StrView_Arg(aux), StrView_Arg(aux_and_past_participle));
			aux = aux_list[0];
		}
	} else {
		if (aux_list.is_contains(aux_and_past_participle)) {
			aux = aux_and_past_participle;
		} else {
			past_participle = aux_and_past_participle;
		}
	}
	if (aux) {
		Size correct_option_index = (aux == "ist"_v) ? 1 : 0;
		ExerciseState::SubStage substage = {
			  .is_keypad = false,
			  .correct_option_index = correct_option_index,
			  .points_for_correct_answer = points_reward_for_aux_stage,
			  .opts = {.data = aux_list.data,
		               .size = aux_list.size(),
		               .reserved = aux_list.size()}};
		exercise->stages.push(
			  a, {.source = source_aux,
		          .form_type = FormType::Auxiliary,
		          .substages =
		                DynArr<ExerciseState::SubStage>::with(a, substage)});
		exercise->points_max += points_reward_for_aux_stage;
	}
	if (past_participle) {
		switch (mode) {
		case Engine::Mode::Entire:
			append_common_stage_entire(
				  a, scratch, past_participle, past_participle_list,
				  FormType::Partizip2, source_pp, exercise);
			break;
		case Engine::Mode::Gaps:
			append_common_stage_gaps(a, past_participle, Tokenizer::Kind::Verb,
			                         FormType::Partizip2, source_pp, exercise);
			break;
		case Engine::Mode::Chunks:
			append_common_stage_chunks(
				  a, past_participle, Tokenizer::Kind::Verb,
				  FormType::Partizip2, source_pp, exercise);
			break;
		case Engine::Mode::Compose:
			append_common_stage_compose(a, past_participle, FormType::Partizip2,
			                            source_pp, exercise);
			break;
		default:
			break;
		}
	}
}

template <class OptionIndexFn>
StrView answered_response_from_exercise(Arena &tmpa, Arena &a,
                                        const ExerciseState &e,
                                        OptionIndexFn option_index_for) {
	StrBuilder parts{};
	bool has_any_answer = false;
	bool is_gaps_mode = e.mode == Mode::Gaps;
	Size last_stage_index_with_content = 0;
	for (Size i{0}; i < e.stages.size; ++i) {
		// SDL_Log("===stage %" PRSize "", i);
		auto &stage = e.stages[i];
		bool stage_started = false;
		// auto __debugj = 0;
		for (auto &substage : stage.substages) {
			// SDL_Log("substage %d", __debugj++);
			if (substage.selected_option_index < 0) {
				continue;
			}
			if (!stage_started) {
				if (has_any_answer) {
					parts.push(tmpa, " "_v);
				}
				stage_started = true;
				has_any_answer = true;
				last_stage_index_with_content = i;
			}
			auto str = substage.opts[option_index_for(substage)];
			// SDL_Log("Append from: " StrView_Fmt, StrView_Arg(str));
			if (!is_gaps_mode) {
				parts.push(tmpa, str);
			} else { // gaps mode
				parts.append(tmpa, stage.gap.get_answer_for_str(tmpa, str));
			}
		}
		if (!stage_started && i - 1 == last_stage_index_with_content) {
			// SDL_Log("i %" PRSize ", cur sta %" PRSize ", total sta %" PRSize
			        // " ",
			        // i, e.current_stage, e.stages.size);
			parts.push(tmpa, " "_v);
			parts.push(tmpa, e.stages[i].before_answer);
		}
	}
	// for (const auto &stage : e.stages) {
	// 	bool stage_started = false;
	// 	for (const auto &substage : stage.substages) {
	// 		if (substage.selected_option_index < 0) {
	// 			continue;
	// 		}
	// 		if (!stage_started) {
	// 			if (has_any_answer) {
	// 				parts.push(tmpa, " "_v);
	// 			}
	// 			stage_started = true;
	// 			has_any_answer = true;
	// 		}
	// 		parts.push(tmpa, substage.opts[option_index_for(substage)]);
	// 	}
	// }

	// auto r = parts.join(tmpa, '|');
	// SDL_Log("res: " StrView_Fmt, StrView_Arg(r));
	return parts.join(a);
}

void append_plural_stage(Arena &a, Arena &scratch, const Word &word,
                         StrView source, ExerciseState *exercise) {
	constexpr Size opts_count = 5;
	constexpr int points_for_correct_plural = 2;
	auto raw_plural = word.n.plural_suffix.trim();

	if (!raw_plural || raw_plural == "(Sg.)"_v || raw_plural == "(Pl.)"_v) {
		return;
	}

	// NOTE: normalize umlaut "-e -> ¨-e
	StrView correct_option = raw_plural;
	if (raw_plural == "\"-e"_v)
		correct_option = "¨-e"_v;
	else if (raw_plural == "\"-er"_v)
		correct_option = "¨-er"_v;
	else if (raw_plural == "\"-"_v)
		correct_option = "¨-"_v;

	DynArr<StrView> opts{};
	opts.push(a, correct_option);
	auto origin_perm = DynArr<Size>::with(scratch, 0, 1, 2, 3, 4);
	auto used =
		  DynArr<uint8_t>::filled_zero_or_default(scratch, plurals.size());
	for (; opts.size != opts_count;) {
		auto rndi = random_num(0, plurals.size(), &rng_state);
		if (plurals[rndi] != correct_option && !used[rndi]) {
			opts.push(a, plurals[rndi]);
			used[rndi] = 1;
		}
	}

	shuffle_str_views_and_indices(opts.data, origin_perm.data, opts.size,
	                              &rng_state);
	auto correct_option_it =
		  std::find(origin_perm.begin(), origin_perm.end(), 0);
	auto correct_option_index =
		  static_cast<Size>(correct_option_it - origin_perm.begin());

	ExerciseState::SubStage substage = {
		  .is_keypad = true,
		  .correct_option_index = correct_option_index,
		  .points_for_correct_answer = points_for_correct_plural,
		  .opts = opts};

	exercise->stages.push(
		  a, {.source = source,
	          .form_type = FormType::Plural,
	          .substages = DynArr<ExerciseState::SubStage>::with(a, substage)});
	exercise->points_max += points_for_correct_plural;
}

void submit_finished_exercise(AppContext *ctx, const ExerciseState &e) {
	KLAPPT_PROFILE_SCOPE_N("submit_finished_exercise");
	Measure m{__FUNCTION__};
	Review review{
		  e.points_earned,
		  e.points_max,
		  time(nullptr),
	};
	// TODO: cache state; in Words?
	State state{};
	{
		KLAPPT_PROFILE_SCOPE_N("estate: get");
		auto [success, found] = ctx->states.get(e.word_id, state);
		if (!success) {
			ctx->app_status.push_error("Cannot read word learning state"_v);
			return;
		}
		if (!found) {
			ctx->app_status.push_error("Word learning state is missing"_v);
			return;
		}
	}
	m.lap().printus("get");
	{
		KLAPPT_PROFILE_SCOPE_N("estate: update");
		if (!state.update(review)) {
			ctx->app_status.push_error("Cannot update word learning state"_v);
			return;
		}
	}
	m.lap().printus("update");
	{
		KLAPPT_PROFILE_SCOPE_N("estate: set");
		if (!ctx->states.set(e.word_id, state)) {
			ctx->app_status.push_error("Cannot save learning state"_v);
			return;
		}
	}
	m.lap().printus("set");
}

} // namespace

Size Exercises::generate_new_exercises(AppContext *ctx, Size n) {
	KLAPPT_PROFILE_SCOPE_N("Exercises::generate_new_exercises");
	Measure m{__FUNCTION__};
	reset();

	// TODO: investigate how big
	// NOTE: we may need a big arena
	auto &scratch = ctx->arena;
	auto g = scratch.guard();

	DynArr<StrView> noun_lemma_list;
	DynArr<StrView> verb_infinitive_list;
	DynArr<StrView> verb_third_person_list;
	DynArr<StrView> verb_praeteritum_list;
	DynArr<StrView> verb_past_participle_list;
	DynArr<StrView> adjective_lemma_list;
	DynArr<StrView> adjective_cmp_list;
	DynArr<StrView> adjective_sup_list;
	DynArr<StrView> phrase_words_list;

	auto now = time(nullptr);
	DynArr<WordId> due_id;
	DynArr<WordRef> due_ref;

	if (!ctx->states.collect_due(scratch, now, due_id)) {
		SDL_LogError(SDL_LOG_CATEGORY_ERROR, "LMDB error: cannot collect due");
		ctx->app_status.push_error("Cannot collect due words"_v);
		return 0;
	}
	if (due_id.size == 0) {
		return 0;
	}

	auto push_if_not_empty = [&a = scratch](DynArr<StrView> *list,
	                                        StrView str) {
		if (str) {
			list->push(a, str);
		}
	};

	auto &words = *ctx->words;
	// SDL_Log("collected %" PRSize "", due_id.size);
	// SDL_Log("words list %" PRSize "", words.size);
	// searching for them in learning list and setting due_ref
	// and lists of spare words
	// NOTE: big lists will contain due-words too
	// NOTE: the order of due_ref and due_id is not the same!!!
	// TODO: fix this?, cause it iterates over all the list
	for (auto i = words.begin(); i < words.end(); i.advance(&words)) {
		auto &word = words[i];
		if (due_id.is_contains(word.word_id)) {
			due_ref.push(scratch, i);
		}
		auto text = word.p.text; // hä...  TODO: be more elegant
		switch (word.type) {
		case WordType::Noun:
			// noun_word_ref_list.push(ctx->tmparena, i);
			push_if_not_empty(&noun_lemma_list, word.n.lemma);
			break;
		case WordType::Verb:
			// verb_word_ref_list.push(ctx->tmparena, i);
			push_if_not_empty(&verb_infinitive_list, word.v.infinitive);
			push_if_not_empty(&verb_third_person_list, word.v.third_person);
			push_if_not_empty(&verb_praeteritum_list, word.v.praeteritum);
			// we need to split aux and pII
			{
				auto pp = word.v.auxv_and_past_participle.split().second;
				push_if_not_empty(&verb_past_participle_list, pp);
			}
			break;
		case WordType::Adj:
			// adjective_word_ref_list.push(ctx->tmparena, i);
			push_if_not_empty(&adjective_lemma_list, word.a.lemma);
			push_if_not_empty(&adjective_cmp_list, word.a.comparative);
			push_if_not_empty(&adjective_sup_list, word.a.superlative);
			break;
		case WordType::Phrase:
			for (auto w = text.mut_split(); w; w = text.mut_split()) {
				auto trimmed_w = w.is_contains_punctuation_unicode()
				                       ? w.utf8_remove_punctuation(scratch)
				                       : w;
				push_if_not_empty(&phrase_words_list, trimmed_w);
			}
			break;
		default:
			break;
		}
	}

	auto phrase_words_list_and_nouns =
		  DynArr<StrView>::concat(scratch, phrase_words_list, noun_lemma_list);
	auto phrase_words_list_and_verbs = DynArr<StrView>::concat(
		  scratch, phrase_words_list, verb_infinitive_list);
	auto phrase_words_list_and_adjectives = DynArr<StrView>::concat(
		  scratch, phrase_words_list, adjective_lemma_list);

	// SDL_Log("due in words list %" PRSize "", due_ref.size);
	// TODO: check the sizes of the lists
	if (noun_lemma_list.size < 5) {
		// TODO: add more nouns from words store if too little of them present
	}

	const auto total_exercises = std::min(n, due_ref.size);
	// the main list to generate exercises
	auto exercise_words =
		  DynArr<WordRef>::filled_zero_or_default(scratch, total_exercises);

	// we fill it randomly
	for (exercise_words.size = 0; exercise_words.size < total_exercises;) {
		auto i = random_num(0, due_ref.size, &rng_state);
		if (!exercise_words.is_contains(due_ref[i])) {
			exercise_words.push(scratch, due_ref[i]);
		}
	}

	simdjson::dom::parser parser{};

	for (auto &word_ref : exercise_words) {
		auto &word = words[word_ref];
		State state{};
		auto [success, was_present] = ctx->states.get(word.word_id, state);
		if (!success) {
			SDL_LogError(SDL_LOG_CATEGORY_ERROR,
			             "LMDB error: failed to get state");
			ctx->app_status.push_error("failed to get state by ID"_v);
			return 0;
		}
		// state.mode = Engine::Mode::Compose; // NOTE: DEBUG

		ExerciseState exercise{.word_id = word.word_id,
		                       .word_ref = word_ref,
		                       .mode = state.mode,
		                       .word_type = word.type};

		auto append_common_stage = [&a = a, &scratch = scratch,
		                            mode = state.mode, e = &exercise](
										 StrView str, Tokenizer::Kind kind,
										 DynArr<StrView> spare_list,
										 StrView source,
										 FormType form_type = FormType::None) {
			switch (mode) {
			case Engine::Mode::Entire:
				append_common_stage_entire(a, scratch, str, spare_list,
				                           form_type, source, e);
				break;
			case Engine::Mode::Gaps:
				append_common_stage_gaps(a, str, kind, form_type, source, e);
				break;
			case Engine::Mode::Chunks:
				append_common_stage_chunks(a, str, kind, form_type, source, e);
				break;
			case Engine::Mode::Compose:
				append_common_stage_compose(a, str, form_type, source, e);
				break;
			default:
				break;
			}
		};

		if (word.type == WordType::Noun) {
			auto source =
				  extract_exercise_prompts(a, scratch, word, &exercise, parser);
			append_article_stage(a, word, source, &exercise);
			append_common_stage(word.n.lemma, Tokenizer::Kind::Noun,
			                    noun_lemma_list, source);
			append_plural_stage(a, scratch, word, source, &exercise);
		} else if (word.type == WordType::Verb) {
			auto source_clean =
				  extract_exercise_prompts(a, scratch, word, &exercise, parser);

			{
				append_common_stage(word.v.infinitive, Tokenizer::Kind::Verb,
				                    verb_infinitive_list, source_clean,
				                    FormType::Infinitive);
				if (word.v.third_person) {
					append_common_stage(word.v.third_person,
					                    Tokenizer::Kind::Verb,
					                    verb_third_person_list, source_clean,
					                    FormType::ThirdPerson);
				}
				if (word.v.praeteritum) {
					append_common_stage(word.v.praeteritum,
					                    Tokenizer::Kind::Verb,
					                    verb_praeteritum_list, source_clean,
					                    FormType::Praeteritum);
				}
				if (word.v.auxv_and_past_participle) {
					append_stages_aux_and_past_participle(
						  a, scratch, word.v.auxv_and_past_participle,
						  verb_past_participle_list, source_clean, source_clean,
						  &exercise, state.mode);
				}
			}
		} else if (word.type == WordType::Adj) {
			auto source_clean =
				  extract_exercise_prompts(a, scratch, word, &exercise, parser);

			{
				append_common_stage(word.a.lemma, Tokenizer::Kind::Adjective,
				                    adjective_lemma_list, source_clean);
				if (!word.a.is_indeclinable) {
					if (word.a.comparative) {
						append_common_stage(word.a.comparative,
						                    Tokenizer::Kind::Adjective,
						                    adjective_cmp_list, source_clean,
						                    FormType::Comparative);
					}
					if (word.a.superlative) {
						append_common_stage(word.a.superlative,
						                    Tokenizer::Kind::Adjective,
						                    adjective_sup_list, source_clean,
						                    FormType::Superlative);
					}
				}
			}
		} else if (word.type == WordType::Phrase) {
			StrView source = word.translations_raw
			                       ? word.translations_raw.trim().copy(a)
			                       : extract_exercise_prompts(
										   a, scratch, word, &exercise, parser);
			auto text = word.p.text;
			for (auto w = text.mut_split(); w; w = text.mut_split()) {
				auto trimmed_w = w.is_contains_punctuation_unicode()
				                       ? w.utf8_remove_punctuation(scratch)
				                       : w;
				if (!trimmed_w) {
					continue;
				}
				auto kind = Tokenizer::guess_kind(trimmed_w);
				DynArr<StrView> spare_words_list =
					  kind == Tokenizer::Kind::Noun
							? phrase_words_list_and_nouns
					  : kind == Tokenizer::Kind::Adjective
							? phrase_words_list_and_adjectives
							: phrase_words_list_and_verbs;
				append_common_stage(trimmed_w, kind, spare_words_list, source);
			}
		}
		// Size i{0};
		// for (auto &stage : exercise.stages) {
		// 	SDL_Log(StrView_Fmt, StrView_Arg(word.n.lemma));
		// 	SDL_Log("stage %" PRSize " of %" PRSize "", i++,
		// 	        exercise.stages.size);
		// 	Size j{0};
		// 	for (auto &substage : stage.substages) {
		// 		SDL_Log("substage %" PRSize " of %" PRSize
		// 		        ", total opts: %" PRSize "",
		// 		        j++, stage.substages.size, substage.opts.size);
		// 		for (auto &option : substage.opts) {
		// 			SDL_Log("opts " StrView_Fmt, StrView_Arg(option));
		// 		}
		// 	}
		// }
		exercises.push(a, exercise);
	}

	m.lap().printus();
	// SDL_Log("Exercise arena usage stats after generatig:");
	a.print_stats();
	return due_id.size;
}

StrView expected_response_from_exercise(Arena &tmpa, Arena &a,
                                        const ExerciseState &e) {
	auto res = answered_response_from_exercise(
		  tmpa, a, e, [](const ExerciseState::SubStage &substage) {
			  return substage.correct_option_index;
		  });
	// SDL_Log(StrView_Fmt, StrView_Arg(res));
	return res;
}

StrView actual_response_from_exercise(Arena &tmpa, Arena &a,
                                      const ExerciseState &e) {
	auto res = answered_response_from_exercise(
		  tmpa, a, e, [](const ExerciseState::SubStage &substage) {
			  return substage.selected_option_index;
		  });
	// SDL_Log(StrView_Fmt, StrView_Arg(res));
	return res;
}

bool Exercises::handler_back_pressed_ex(AppContext *ctx) {
	if (!is_initialized()) {
		return false;
	}

	auto &exercise = exercises[exercise_current_idx];
	if (!exercise.undo()) {
		return false;
	}

	exercise.response =
		  actual_response_from_exercise(ctx->arena_frame, a, exercise);
	if (exercise.response.size == 0) {
		exercise.response = ExerciseState::EMPTY_ANSWER;
	}
	pending_selection_index = -1;
	return true;
}

bool Exercises::handler_back_pressed_rv() {
	if (!is_initialized()) {
		return false;
	}
	// SDL_Log("trying from %" PRSize, exercise_current_idx);
	if (exercise_current_idx > 0) {
		exercise_current_idx--;
		return true;
	} else {
		return false;
	}
}

Triple<DynArr<StrView>, DynArr<StrView>, DynArr<int>>
find_diff(Arena &arena, const ExerciseState &e) {
	DynArr<StrView> expected_parts;
	DynArr<StrView> actual_parts;
	DynArr<int> is_right_actual_part;
	const bool is_gaps_mode = e.mode == Mode::Gaps;
	for (Size i{0}; i < e.stages.size; ++i) {
		if (i > 0) {
			expected_parts.push(arena, " "_v);
			actual_parts.push(arena, " "_v);
			is_right_actual_part.push(arena, 1);
		}

		const auto &stage = e.stages[i];
		for (auto &substage : stage.substages) {
			auto expected_part = substage.opts[substage.correct_option_index];
			auto actual_part = substage.opts[substage.selected_option_index];
			if (is_gaps_mode) {
				expected_parts.push(
					  arena, stage.gap.get_answer_for_str(arena, expected_part)
								   .join(arena));
				actual_parts.push(
					  arena, stage.gap.get_answer_for_str(arena, actual_part)
								   .join(arena));
			} else {
				expected_parts.push(arena, expected_part);
				actual_parts.push(arena, actual_part);
			}
			auto eq = substage.correct_option_index ==
			          substage.selected_option_index;
			is_right_actual_part.push(arena, eq ? 1 : 0);
		}
	}
	return {actual_parts, expected_parts, is_right_actual_part};
}

void Exercises::build_result_reviews(Arena &tmpa, bool is_only_failed) {
	results.reset_size_reserved();
	for (auto &exercise : exercises) {
		if (is_only_failed && exercise.points_earned == exercise.points_max) {
			continue;
		}
		auto expected = expected_response_from_exercise(tmpa, a, exercise);
		auto [actual_parts, expected_parts, is_right_actual_part] =
			  find_diff(a, exercise);
		results.push(a, {.expected = expected,
		                 .actual = exercise.response,
		                 .word_id = exercise.word_id,
		                 .word_ref = exercise.word_ref,
		                 .source = exercise.stages.first().source,
		                 .source_sub0 = exercise.source_sub0,
		                 .source_sub1 = exercise.source_sub1,
		                 .expected_parts = expected_parts,
		                 .actual_parts = actual_parts,
		                 .is_right_actual_part = is_right_actual_part});
	}
}

Exercises::CommitResult Exercises::commit(AppContext *ctx) {
	KLAPPT_PROFILE_SCOPE_N("Exercises::commit");
	if (!is_initialized())
		return CommitResult::None;
	if (pending_selection_index < 0) {
		return CommitResult::None;
	}
	if (ctx->screen() == Screen::ExerciseReview) {
		exercise_current_idx += 1;
		if (exercise_current_idx == results.size) {
			pending_selection_index = -1;
			return CommitResult::StartNextRound;
		}
		pending_selection_index = -1;
		return CommitResult::None;
	}

	// SDL_Log("%s", __PRETTY_FUNCTION__);
	// SDL_Log("pending selection %" PRSize " |" StrView_Fmt "|",
	//         pending_selection_index,
	//         StrView_Arg(substage().opts[pending_selection_index]));
	auto result = CommitResult::None;
	auto &exercise = exercises[exercise_current_idx];
	auto has_more_stages = exercise.submit(pending_selection_index);
	exercise.response =
		  actual_response_from_exercise(ctx->arena_frame, a, exercise);
	if (has_more_stages) {
		// there are more stages/substages to go
	} else {
		// this exercise is finished
		// here we should update learning state
		submit_finished_exercise(ctx, exercise);
		auto &word = (*ctx->words)[exercise.word_ref];
		// mark the word as one that has been submited at least once
		if (!word.was_learned) {
			ctx->word_store.set_was_learned(ctx->arena_frame, word);
		}

		// switching to the next exercise
		++exercise_current_idx;
		if (exercise_current_idx == exercise_total()) {
			// no more exercises
			SDL_Log("--- NO MORE EXERCISES ---");
			// reset();
			correct_exercise_count = 0;
			for (auto &ex : exercises) {
				if (ex.points_earned == ex.points_max) {
					++correct_exercise_count;
				}
			}
			exercise_current_idx = 0;
			result = CommitResult::ShowSummary;
		} else {
			// SDL_Log("--- Finished exercise %" PRSize " of %" PRSize " ---",
			//         exercise_current_idx, exercises.size);
			// we still have exercises
		}
	}
	// SDL_Log("--- resetting pending selection ---");
	pending_selection_index = -1;
	return result;
}

} // namespace Engine
