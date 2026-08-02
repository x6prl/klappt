#include "base/str_builder.h"
#include "base/str_view.h"
#include "domain/word.h"
#include "platform/neuro.h"
#include "screen_helpers.h"
#include "ui/components/button.h"
#include "ui/dpi.h"
#include "ui/textcache.h"
#include <SDL3/SDL_log.h>
#include <SDL3/SDL_stdinc.h>
#include <algorithm>

// TODO: rewrite

namespace {
static StrView format_due_delta(Arena &a, Engine::Timestamp now,
                                Engine::Timestamp due) {
	const auto delta = static_cast<long long>(due - now);
	char *buf = a.pushN<char>(64);
	if (delta <= 0) {
		const auto overdue = -delta;
		const auto hours = overdue / (60 * 60);
		const auto mins = (overdue / 60) % 60;
		auto len = SDL_snprintf(buf, 64, "Overdue %lldh %lldm", hours, mins);
		return {buf, std::min<Size>(len, 63)};
	}

	const auto days = delta / (60 * 60 * 24);
	const auto hours = (delta / (60 * 60)) % 24;
	const auto mins = (delta / 60) % 60;
	auto len =
		  SDL_snprintf(buf, 64, "Due in %lldd %lldh %lldm", days, hours, mins);
	return {buf, std::min<Size>(len, 63)};
}

static StrView mode_name(Engine::Mode mode) {
	switch (mode) {
	case Engine::Mode::Entire:
		return "Entire"_v;
	case Engine::Mode::Gaps:
		return "Gaps"_v;
	case Engine::Mode::Chunks:
		return "Chunks"_v;
	case Engine::Mode::Compose:
		return "Compose"_v;
	case Engine::Mode::Count:
		return "Count"_v;
	}
	return "Unknown"_v;
}

static StrView successful_reviews_to_next_mode(Arena &a,
                                               const Engine::State &state) {
	if (state.mode >= Engine::Mode::Compose) {
		return "Max level"_v;
	}

	Engine::State probe = state;
	const auto start_mode = probe.mode;
	Engine::Timestamp at = std::time(nullptr);
	constexpr int max_reviews = 1000;

	for (int reviews = 1; reviews <= max_reviews; ++reviews) {
		if (probe.due > at) {
			at = probe.due;
		}
		if (probe.last_review >= at) {
			at = probe.last_review + 1;
		}

		Engine::Review review{
			  .right = 1,
			  .size = 1,
			  .at = at,
		};
		if (!probe.update(review)) {
			return "Unknown"_v;
		}
		if (probe.mode != start_mode) {
			return StrView::from_number(a, reviews);
		}
	}

	return ">1000"_v;
}

static inline StrView word_to_lexemme_str(Arena &scratch, Arena &a,
                                          const Word &w) {
	StrBuilder strs{};
	switch (w.type) {
	case WordType::Nil:
		return "<empty word>"_v;
		break;
	case WordType::Noun:
		SDL_Log("noun %d " StrView_Fmt " " StrView_Fmt, (int)w.n.gender,
		        StrView_Arg(w.n.lemma), StrView_Arg(w.n.plural_suffix));
		strs.push(scratch, gender_to_article_nominative_strview(w.n.gender));
		strs.push(scratch, w.n.lemma);
		strs.push(scratch, w.n.plural_suffix);
		break;
	case WordType::Verb:
		strs.push(scratch, w.v.infinitive);
		if (w.v.third_person) {
			strs.push(scratch, w.v.third_person);
		}
		if (w.v.praeteritum) {
			strs.push(scratch, w.v.praeteritum);
		}
		if (w.v.auxv_and_past_participle) {
			strs.push(scratch, w.v.auxv_and_past_participle);
		}
		if (w.v.third_person) {
			SDL_Log("verb " StrView_Fmt " / " StrView_Fmt " / " StrView_Fmt
			        " / " StrView_Fmt,
			        StrView_Arg(w.v.infinitive), StrView_Arg(w.v.third_person),
			        StrView_Arg(w.v.praeteritum),
			        StrView_Arg(w.v.auxv_and_past_participle));
		} else if (w.v.praeteritum || w.v.auxv_and_past_participle) {
			SDL_Log("verb " StrView_Fmt " / " StrView_Fmt " / " StrView_Fmt,
			        StrView_Arg(w.v.infinitive), StrView_Arg(w.v.praeteritum),
			        StrView_Arg(w.v.auxv_and_past_participle));
		} else {
			SDL_Log("verb " StrView_Fmt, StrView_Arg(w.v.infinitive));
		}
		break;
	case WordType::Adj:
		strs.push(scratch, w.a.lemma);
		if (w.a.is_indeclinable) {
			SDL_Log("adj " StrView_Fmt " (indecl.)", StrView_Arg(w.a.lemma));
			strs.push(scratch, "(indecl.)"_v);
		} else if (w.a.comparative || w.a.superlative) {
			SDL_Log("adj " StrView_Fmt " / " StrView_Fmt " / " StrView_Fmt,
			        StrView_Arg(w.a.lemma), StrView_Arg(w.a.comparative),
			        StrView_Arg(w.a.superlative));
			if (w.a.comparative) {
				strs.push(scratch, w.a.comparative);
			}
			if (w.a.superlative) {
				strs.push(scratch, w.a.superlative);
			}
		} else {
			SDL_Log("adj " StrView_Fmt, StrView_Arg(w.a.lemma));
		}
		break;
	case WordType::Phrase:
		strs.push(scratch, w.a.superlative);
		SDL_Log("phrase " StrView_Fmt, StrView_Arg(w.p.text));
		break;
	}

	return strs.join(a, ' ');
}

static void draw_word_container(AppContext *ctx, const Word &w) {
	const auto row_gap = udpi(4.f);
	CLAY(CLAY_ID("WordContainer"),
	     {.layout = {
				.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0)},
				.padding = CLAY_PADDING_ALL(udpi(10.f)),
				.childGap = udpi(8.f),
				.childAlignment = {CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER},
				.layoutDirection = CLAY_TOP_TO_BOTTOM,
		  }}) {
		CLAY(CLAY_ID("WordRows"),
		     {.layout = {
					.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0)},
					.childGap = row_gap,
					.childAlignment = {CLAY_ALIGN_X_CENTER,
		                               CLAY_ALIGN_Y_CENTER},
					.layoutDirection = CLAY_TOP_TO_BOTTOM,
			  }}) {
			draw_text(
				  word_to_lexemme_str(ctx->arena_frame, ctx->arena_frame, w),
				  theme()->onSurface, udpi(16), FontID::MONOSPACE_REGULAR);
			// draw_text(w.grammar, theme()->onSurface, udpi(16),
			//           FontID::MONOSPACE_REGULAR);
			// if (w.was_learned) {
			// 	draw_text("wl"_v, theme()->onSurface, udpi(16),
			// 	          FontID::MONOSPACE_REGULAR);
			// }
			// if (w.in_learning_list) {
			// 	draw_text("ill"_v, theme()->onSurface, udpi(16),
			// 	          FontID::MONOSPACE_REGULAR);
			// }
		}

		CLAY(CLAY_ID("WordTranslations"),
		     {.layout = {
					.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0)},
					.childGap = row_gap,
					.childAlignment = {CLAY_ALIGN_X_CENTER,
		                               CLAY_ALIGN_Y_CENTER},
					.layoutDirection = CLAY_TOP_TO_BOTTOM,
			  }}) {
			auto trs =
				  translations_from_raw(ctx->arena_frame, w.translations_raw);
			// draw_text(w.translations_raw, theme()->onSurface, udpi(16),
			// FontID::MONOSPACE_REGULAR);
			// SDL_Log("%d", trs.size);
			StrBuilder strs{};
			for (auto &tr : trs) {
				StrBuilder v{};
				v.push(ctx->arena_frame, tr.grammar);
				v.push(ctx->arena_frame, tr.text);
				strs.push(ctx->arena_frame, v.join(ctx->arena_frame, ' '));
			}
			draw_text(strs.join(ctx->arena_frame, "\n"_v), theme()->onSurface,
			          udpi(16), FontID::MONOSPACE_REGULAR, CLAY_TEXT_WRAP_WORDS,
			          CLAY_TEXT_ALIGN_LEFT);
		}
	}
}

static void draw_learning_state(AppContext *ctx, const Engine::State &s) {
	const auto row_gap = udpi(4.f);
	CLAY(CLAY_ID("LearningState"),
	     {.layout = {
				.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0)},
				.padding = CLAY_PADDING_ALL(udpi(10.f)),
				.childGap = udpi(8.f),
				.layoutDirection = CLAY_TOP_TO_BOTTOM,
		  }}) {
		draw_text("Learning state"_v, theme()->onSurface, udpi(18));

		CLAY(CLAY_ID("LearningStateRows"),
		     {.layout = {
					.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0)},
					.childGap = row_gap,
					.layoutDirection = CLAY_TOP_TO_BOTTOM,
			  }}) {
			draw_text(StrView::concat_with(ctx->arena_frame, "Mode"_v,
			                               mode_name(s.mode), ':'),
			          theme()->onSurface, udpi(16), FontID::MONOSPACE_REGULAR);
			draw_text(
				  StrView::concat_with(
						ctx->arena_frame, "Successes to next mode"_v,
						successful_reviews_to_next_mode(ctx->arena_frame, s),
						':'),
				  theme()->onSurface, udpi(16), FontID::MONOSPACE_REGULAR);
			draw_text(StrView::concat_with(ctx->arena_frame, "Due"_v,
			                               format_due_delta(ctx->arena_frame,
			                                                std::time(nullptr),
			                                                s.due),
			                               ':'),
			          theme()->onSurface, udpi(16), FontID::MONOSPACE_REGULAR);
			draw_text(
				  StrView::concat_with(
						ctx->arena_frame, "Difficulty"_v,
						StrView::from_number(ctx->arena_frame, s.difficulty),
						':'),
				  theme()->onSurface, udpi(16), FontID::MONOSPACE_REGULAR);
			draw_text(
				  StrView::concat_with(
						ctx->arena_frame, "Reviews"_v,
						StrView::from_number(ctx->arena_frame, s.total_reviews),
						':'),
				  theme()->onSurface, udpi(16), FontID::MONOSPACE_REGULAR);
			draw_text(StrView::concat_with(
							ctx->arena_frame, "Lapses"_v,
							StrView::from_number(ctx->arena_frame, s.lapses),
							':'),
			          theme()->onSurface, udpi(16), FontID::MONOSPACE_REGULAR);
			draw_text(
				  StrView::concat_with(ctx->arena_frame, "Recent failures"_v,
			                           StrView::from_number(ctx->arena_frame,
			                                                s.recent_failures),
			                           ':'),
				  theme()->onSurface, udpi(16), FontID::MONOSPACE_REGULAR);
		}

		CLAY(CLAY_ID("LearningStateModes"),
		     {.layout = {
					.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0)},
					.childGap = row_gap,
					.layoutDirection = CLAY_TOP_TO_BOTTOM,
			  }}) {
			for (int i = 0; i < Engine::MODE_COUNT; ++i) {
				const auto mode = Engine::imode(i);
				const auto &m = s.memory[i];
				if (m.reviews <= 0) {
					continue;
				}

				StrBuilder row{};
				row.push(ctx->arena_frame, mode_name(mode));
				{
					StrBuilder reviews{};
					reviews.push(
						  ctx->arena_frame,
						  StrView::from_number(ctx->arena_frame, m.reviews));
					reviews.push(ctx->arena_frame, "r"_v);
					row.push(ctx->arena_frame, reviews.join(ctx->arena_frame));
				}
				{
					StrBuilder quality{};
					quality.push(ctx->arena_frame,
					             StrView::from_number(ctx->arena_frame,
					                                  m.quality_ewma));
					quality.push(ctx->arena_frame, "q"_v);
					row.push(ctx->arena_frame, quality.join(ctx->arena_frame));
				}
				{
					StrBuilder stability{};
					stability.push(ctx->arena_frame,
					               StrView::from_number(ctx->arena_frame,
					                                    m.stability_days));
					stability.push(ctx->arena_frame, "d"_v);
					row.push(ctx->arena_frame,
					         stability.join(ctx->arena_frame));
				}
				draw_text(row.join(ctx->arena_frame, ' '), theme()->onSurface,
				          udpi(15), FontID::MONOSPACE_REGULAR);
			}
		}
	}
}
} // namespace

// // NOTE: unused
// static inline StrView word_to_str(Arena &scratch, Arena &a, const Word &w) {
// 	StrBuilder strs{};
// 	auto word_str = word_to_lexemme_str(scratch, a, w);
// 	strs.push(scratch, word_str);
// 	strs.push(scratch, w.translations_raw);
// 	strs.push(scratch, w.grammar);
//
// 	SDL_Log("translations_raw: " StrView_Fmt, StrView_Arg(w.translations_raw));
// 	SDL_Log("grammar: " StrView_Fmt, StrView_Arg(w.grammar));
// 	SDL_Log("in_learning_list: %d", static_cast<int>(w.in_learning_list));
// 	SDL_Log("was_learned: %d", static_cast<int>(w.was_learned));
// 	SDL_Log("ID: %llu\n______________________________",
// 	        static_cast<unsigned long long>(w.word_id.value));
// 	return strs.join(a, '\n');
// }

void screen_word_view_push(AppContext *ctx, WordId word_id) {
	KLAPPT_PROFILE_SCOPE_N("screen_word_view_push");
	auto &state = *ctx->word_view_state;
	state.word_id = word_id;
	state.has_state = false;
	bool is_word_copied = false;

	{
		KLAPPT_PROFILE_SCOPE_N("get Word words");
		// try to get it from the learning list
		for (auto word_ref = ctx->words->begin(); word_ref < ctx->words->end();
		     word_ref.advance(ctx->words)) {
			// NOTE: we are not copying strings
			auto &word = (*ctx->words)[word_ref];
			if (word.word_id == word_id) {
				state.word_copy = word;
				is_word_copied = true;
				break;
			}
		}
	}
	if (!is_word_copied) {
		KLAPPT_PROFILE_SCOPE_N("copy Word store");
		// get it from xapian
		// TODO: use a separate arena
		ctx->word_store.get_by_id(ctx->arena, word_id, state.word_copy);
		is_word_copied = true;
	}
	{
		KLAPPT_PROFILE_SCOPE_N("copy State");
		// get it from lmdb
		auto [is_success, was_found] =
			  ctx->states.get(word_id, state.state_copy);
		if (is_success) {
			state.has_state = was_found;
		} else {
			ctx->app_status.push_error("lmdb get() error"_v);
		}
	}
	state.title = word_most_meaningfull_lemma(state.word_copy);

	ctx->push(Screen::WordView);
}

void screen_word_view_draw(AppContext *ctx) {
	KLAPPT_PROFILE_SCOPE_N("screen_word_view_draw");
	const auto padding = udpi(6.f);
	CLAY(CLAY_ID("WordViewScreenShell"),
	     {.layout = {
				.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)},
				.padding = CLAY_PADDING_ALL(padding),
				.childGap = udpi(6.f),
				.layoutDirection = CLAY_TOP_TO_BOTTOM,
		  }}) {
		auto &state = *ctx->word_view_state;
		draw_word_container(ctx, state.word_copy);
		// draw_text(word_to_lexemme_str(ctx->tmparena, ctx->tmparena,
		//                               state.word_copy),
		//           theme()->onSurface, udpi(20), FontID::MONOSPACE_REGULAR);
		// draw_text(state.word_copy.translations_raw, theme()->onSurface,
		//           udpi(20), FontID::MONOSPACE_REGULAR);
		// if (state.word_copy.grammar) {
		// 	draw_text(state.word_copy.grammar, theme()->onSurface, udpi(20),
		// 	          FontID::MONOSPACE_REGULAR);
		// }
		if (state.has_state) {
			draw_learning_state(ctx, state.state_copy);
		} else {
			draw_text("Learning state unavailable"_v, theme()->onSurface,
			          udpi(18));
		}

		CLAY(CLAY_ID("Buttons"),
		     {
				   .layout =
						 {

							   .sizing = {CLAY_SIZING_GROW(0),
		                                  CLAY_SIZING_GROW(0)},
							   .padding = CLAY_PADDING_ALL(udpi(16.0f)),
							   .childGap = udpi(14.0f),
							   .childAlignment = {CLAY_ALIGN_X_CENTER,
		                                          CLAY_ALIGN_Y_CENTER},
						 },
			 }) {
			// auto remove_word = mobile_icon_button<false>(
			// 	  ctx, CLAY_ID("RemoveButton"), Icons::REMOVE);
			// if (remove_word.activated()) {
			// 	// TODO: prompt user
			// 	// screen_word_edit_push(ctx, state.word_id);
			// }
			auto edit = mobile_icon_button<false>(ctx, CLAY_ID("EditButton"),
			                                      Icons::EDIT);
			if (edit.activated()) {
				screen_word_edit_push(ctx);
			}
#if NEURO
			if (ctx->settings.is_using_tts) {
				auto play = mobile_icon_button<true>(ctx, CLAY_ID("PlayButton"),
				                                     Icons::PLAY);
				// play on pressed
				if (play.activated()) {
					auto tts_string =
						  word_tts_full(ctx->arena_screen(), ctx->arena_frame,
					                    state.word_copy);
					// worker_job_push(ctx, {.type = Job::Type::TTS, .tts_text =
					// tts_string});
					run_tts(ctx, tts_string);
				}
			}
#endif // NEURO
		}
	}
}
