#include "screen_helpers.h"

#include <simdjson/simdjson.h>

#include "app/app_context.h"
#include "base/pair.h"
#include "base/profiler.h"
#include "base/str_builder.h"
#include "base/str_view.h"
#include "domain/grammar.h"
#include "domain/word.h"
#include "domain/word_payload.h"
#include "platform/neuro.h"

#include "ui/components/button.h"
#include "ui/components/lists.h"

#include "ui/trs.h"


namespace {

[[maybe_unused]]
static inline StrView word_to_str(Arena &scratch, Arena &a, const Word &w) {
	StrBuilder strs{};
	auto word_str = word_to_lexeme_str(scratch, a, w);
	strs.push(scratch, word_str);
	strs.push(scratch, w.translations_raw);

	SDL_Log("translations_raw: " StrView_Fmt, StrView_Arg(w.translations_raw));
	// SDL_Log("grammar: " StrView_Fmt, StrView_Arg(w.grammar));
	SDL_Log("in_learning_list: %d", static_cast<int>(w.in_learning_list));
	SDL_Log("was_learned: %d", static_cast<int>(w.was_learned));
	SDL_Log("ID: %llu\n______________________________",
	        static_cast<unsigned long long>(w.word_id.value));
	return strs.join(a, '\n');
}

static StrView noun_word_class_to_badge(StrView cls) {
	if (cls == "noun"_v) {
		return "Substantiv"_v;
	}
	if (cls == "name"_v || cls == "proper_noun"_v) {
		return "Eigenname"_v;
	}
	return cls;
}
static StrView verb_word_class_to_badge(StrView cls) {
	if (cls == "verb"_v) {
		return "Verb"_v;
	}
	return cls;
}
static StrView adj_word_class_to_badge(StrView cls) {
	if (cls == "adj"_v) {
		return "Adjektiv"_v;
	}
	if (cls == "adv"_v) {
		return "Adverb"_v;
	}
	if (cls == "adv,adj"_v || cls == "adj,adv"_v) {
		return "Adjektiv / Adverb"_v;
	}
	return cls;
}
static StrView phrase_word_class_to_badge(StrView cls) {
	if (cls == "pron"_v) {
		return "Pronomen"_v;
	}
	if (cls == "prep"_v) {
		return "Präposition"_v;
	}
	if (cls == "postp"_v) {
		return "Postposition"_v;
	}
	if (cls == "conj"_v) {
		return "Konjunktion"_v;
	}
	if (cls == "det"_v || cls == "art"_v) {
		return "Artikel"_v;
	}
	if (cls == "num"_v) {
		return "Numerale"_v; // or "Zahlwort"_v
	}
	if (cls == "particle"_v) {
		return "Partikel"_v;
	}
	if (cls == "intj"_v || cls == "interjection"_v) {
		return "Interjektion"_v;
	}
	if (cls == "phrase"_v) {
		return "Redewendung"_v; // or "Ausdruck"_v
	}
	if (cls == "id"_v || cls == "idiom"_v) {
		return "Redewendung"_v;
	}
	if (cls == "contraction"_v) {
		return "Kurzform"_v; // or "Kontraktion"_v (perfect for 's, is', etc.)
	}
	if (cls == "abbrev"_v || cls == "abbreviation"_v) {
		return "Abkürzung"_v;
	}
	if (cls == "name"_v) {
		return "Eigenname"_v;
	}
	if (cls == "affix"_v) {
		return "Affix"_v;
	}
	if (cls == "prefix"_v) {
		return "Präfix"_v;
	}
	if (cls == "suffix"_v) {
		return "Suffix"_v;
	}
	if (cls == "symbol"_v) {
		return "Symbol"_v;
	}
	if (cls == "punct"_v || cls == "punctuation"_v) {
		return "Satzzeichen"_v;
	}
	if (cls == "verb"_v) {
		return "Verb"_v;
	}
	if (cls == "adj"_v) {
		return "Adjektiv"_v;
	}
	if (cls == "adv"_v) {
		return "Adverb"_v;
	}
	return cls;
}

static StrView format_due_delta(Arena &a, Engine::Timestamp now,
                                Engine::Timestamp due) {
	const auto delta = static_cast<long long>(due - now);
	char *buf = a.pushN<char>(64);
	if (delta <= 0) {
		const auto overdue = -delta;
		const auto hours = overdue / (60 * 60);
		const auto mins = (overdue / 60) % 60;
		auto len = SDL_snprintf(buf, 64, StrView_Fmt " %lldh %lldm", StrView_Arg(tr()->screen_word_view_due_overdue), hours, mins);
		return {buf, std::min<Size>(len, 63)};
	}

	const auto days = delta / (60 * 60 * 24);
	const auto hours = (delta / (60 * 60)) % 24;
	const auto mins = (delta / 60) % 60;
	auto len =
		  SDL_snprintf(buf, 64, StrView_Fmt " %lldd %lldh %lldm", StrView_Arg(tr()->screen_word_view_due_in), days, hours, mins);
	return {buf, std::min<Size>(len, 63)};
}

static StrView mode_name(Engine::Mode mode) {
	switch (mode) {
	case Engine::Mode::Entire:
		return tr()->screen_word_view_mode_entire;
	case Engine::Mode::Gaps:
		return tr()->screen_word_view_mode_gaps;
	case Engine::Mode::Chunks:
		return tr()->screen_word_view_mode_chunks;
	case Engine::Mode::Compose:
		return tr()->screen_word_view_mode_compose;
	case Engine::Mode::Count:
		return "Count"_v;
	}
	return "Unknown"_v;
}

static StrView successful_reviews_to_next_mode(Arena &a,
                                               const Engine::State &state) {
	KLAPPT_PROFILE_SCOPE();
	if (state.mode >= Engine::Mode::Compose) {
		return tr()->screen_word_view_max_level;
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

static void draw_noun_title(AppContext *ctx, const Noun &n) {
	const uint16_t title_font_size = sizes()->font.title_lg;

	CLAY(CLAY_ID("NounTitleRow"),
	     {
			   .layout =
					 {
						   .sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0)},
						   .layoutDirection = CLAY_LEFT_TO_RIGHT,
					 },
		 }) {
		auto gap = [](int i) {
			CLAY(CLAY_IDI_LOCAL("Gap", i),
			     {
					   .layout =
							 {
								   .sizing =
										 {
											   CLAY_SIZING_FIXED(
													 static_cast<float>(
														   sizes()->space.sm)),
											   CLAY_SIZING_GROW(0),
										 },
							 },
				 }) {}
		};
		auto article = gender_to_article_nominative_strview(n.gender);
		if (article && article != " "_v && article != " — "_v) {
			draw_text(article, theme()->secondary, title_font_size);
		}
		gap(0);
		if (ctx->settings.is_show_noun_plural_as_suffix) {
			draw_text(n.lemma, theme()->onSurface, title_font_size);
			bool has_plural_suffix = n.plural_suffix;
			if (has_plural_suffix) {
				gap(1);
				// draw_text(StrView::concat(ctx->arena_frame, ", "_v,
				//                           n.plural_suffix),
				//           theme()->secondary, title_font_size);
				draw_text(n.plural_suffix, theme()->secondary, title_font_size);
			}
		} else {
			draw_text(n.lemma, theme()->onSurface, title_font_size);
		}
	}
}

static void draw_adj_title(AppContext *ctx, const Word &w) {
	draw_text(w.a.lemma, theme()->onSurface, sizes()->font.title_lg,
	          FontID::MAIN, CLAY_TEXT_WRAP_WORDS, CLAY_TEXT_ALIGN_LEFT);
}

static void draw_verb_title(AppContext *ctx, const Word &w) {
	draw_text(w.v.infinitive, theme()->onSurface, sizes()->font.title_lg,
	          FontID::MAIN, CLAY_TEXT_WRAP_WORDS, CLAY_TEXT_ALIGN_LEFT);
}

static void draw_phrase_title(AppContext *ctx, const Word &w) {
	const bool is_long_phrase = w.p.text.utf8_length() > 50;
	const Clay_TextAlignment text_align =
		  is_long_phrase ? CLAY_TEXT_ALIGN_LEFT : CLAY_TEXT_ALIGN_CENTER;

	draw_text(w.p.text, theme()->onSurface, sizes()->font.title_lg,
	          FontID::MAIN, CLAY_TEXT_WRAP_WORDS, text_align);
}

static void draw_word_card(AppContext *ctx, Clay_ElementId element_id,
                           const Word &w, const WordPayload &word_payload) {
	const float label_width = sizes()->dim.form_label_width;
	const uint16_t form_font_size = sizes()->font.body_md;
	const uint16_t translation_font_size = sizes()->font.body_sm;

	DynArr<Pair<StrView, StrView>> forms{};
	DynArr<StrView> badges{};
	StrView word_class_name{};

	switch (w.type) {
	case WordType::Noun: {
		word_class_name = noun_word_class_to_badge(word_payload.word_class);
		if (grammar::is_singular_only(w.n)) {
			badges.push(ctx->arena_frame, tr()->screen_word_view_singular_only);
		} else if (grammar::is_plural_only(w.n)) {
			badges.push(ctx->arena_frame, tr()->screen_word_view_plural_only);
		} else {
			if (!ctx->settings.is_show_noun_plural_as_suffix) {
				// NOTE: plural form is already shown as a suffix
				auto plural =
					  grammar::noun_plural_with_article(ctx->arena_frame, w.n);
				forms.push(ctx->arena_frame, {"Plural:"_v, plural});
			}
		}
	} break;
	case WordType::Verb: {
		word_class_name = verb_word_class_to_badge(word_payload.word_class);
		forms.push(ctx->arena_frame,
		           {"er/sie/es:"_v,
		            grammar::verb_third_person_full(ctx->arena_frame, w.v)});
		forms.push(ctx->arena_frame,
		           {"Präteritum:"_v,
		            grammar::verb_praeteritum_full(ctx->arena_frame, w.v)});
		forms.push(ctx->arena_frame,
		           {"Perfekt:"_v,
		            grammar::verb_perfect_full(ctx->arena_frame, w.v)});
	} break;
	case WordType::Adj: {
		word_class_name = adj_word_class_to_badge(word_payload.word_class);
		if (w.a.is_indeclinable) {
			badges.push(ctx->arena_frame, tr()->screen_word_view_indeclinable);
		} else if ((w.a.comparative || w.a.superlative)) {
			if (w.a.comparative) {
				forms.push(ctx->arena_frame, {"Kompar.:"_v, w.a.comparative});
			}
			if (w.a.superlative) {
				forms.push(ctx->arena_frame, {"Superl.:"_v, w.a.superlative});
			}
		}
	} break;
	case WordType::Phrase: {
		word_class_name = phrase_word_class_to_badge(word_payload.word_class);
	} break;
	default:
		break;
	}

	CLAY(element_id,
	     {
			   .layout =
					 {
						   .sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0)},
						   .padding = sizes()->pad.card,
						   .childGap = sizes()->space.md,
						   .childAlignment = {CLAY_ALIGN_X_LEFT,
	                                          CLAY_ALIGN_Y_TOP},
						   .layoutDirection = CLAY_TOP_TO_BOTTOM,
					 },
			   .backgroundColor = theme()->surfaceContainerLow,
			   .cornerRadius = sizes()->radius.lg,
			   // .border =
	           // {
	           //    .color = theme()->outline,
	           //    .width = {udpi(1.f), udpi(1.f), udpi(1.f),
	           //                             udpi(1.f)},
	           // },
		 }) {

		CLAY(CLAY_ID("WordHeader"),
		     {
				   .layout =
						 {
							   .sizing = {CLAY_SIZING_GROW(0),
		                                  CLAY_SIZING_FIT(0)},
							   .childGap = sizes()->space.xs,
							   .childAlignment = {CLAY_ALIGN_X_LEFT,
		                                          CLAY_ALIGN_Y_CENTER},
							   .layoutDirection = CLAY_LEFT_TO_RIGHT,
						 },
			 }) {
			Clay_ElementDeclaration badge_style_template = {
				  .layout = {.padding = sizes()->pad.badge},
				  .backgroundColor = theme()->secondary,
				  .cornerRadius = sizes()->radius.sm,
			};
			CLAY(CLAY_ID("WordClassBadge"), badge_style_template) {
				draw_text(word_class_name, theme()->onSecondary,
				          sizes()->font.label_sm);
			}

			badge_style_template.backgroundColor =
				  theme()->surfaceContainerHigh;
			for (Size i{0}; i < badges.size; ++i) {
				CLAY(CLAY_IDI("Badge", i), badge_style_template) {
					draw_text(badges[i], theme()->onSurfaceContainerHigh,
					          sizes()->font.label_sm);
				}
			}

			// stylistical tags (colloquial, vulgar, rare, formal, etc.)
			for (Size i{0}; i < word_payload.tags.size; ++i) {
				const auto &tag = word_payload.tags[i];
				bool is_warning = (tag == "vulgar"_v || tag == "obsolete"_v ||
				                   tag == "archaic"_v);
				Clay_Color bg = is_warning ? theme()->wrongContainer
				                           : theme()->surfaceContainerHigh;
				Clay_Color fg = is_warning ? theme()->onWrongContainer
				                           : theme()->onSurfaceContainerHigh;

				badge_style_template.backgroundColor = bg;
				CLAY(CLAY_IDI("StyleTagBadge", i), badge_style_template) {
					draw_text(tag, fg, sizes()->font.label_sm);
				}
			}

			badge_style_template.backgroundColor = theme()->surfaceContainer;
			if (w.in_learning_list) {
				CLAY(CLAY_ID("StatusBadge"), badge_style_template) {
					draw_text(tr()->screen_word_view_in_learning_list, theme()->onSurfaceContainer,
					          sizes()->font.label_sm);
				}
			}
		}

		CLAY(CLAY_ID("TitleBlock"),
		     {
				   .layout =
						 {
							   .sizing = {CLAY_SIZING_GROW(0),
		                                  CLAY_SIZING_FIT(0)},
							   .childGap = sizes()->space.xs,
							   .layoutDirection = CLAY_TOP_TO_BOTTOM,
						 },
			 }) {
			switch (w.type) {
			case WordType::Noun:
				draw_noun_title(ctx, w.n);
				break;
			case WordType::Verb:
				draw_verb_title(ctx, w);
				break;
			case WordType::Adj:
				draw_adj_title(ctx, w);
				break;
			case WordType::Phrase:
				draw_phrase_title(ctx, w);
				break;
			default:
				break;
			}

			if (!word_payload.ipa.is_empty() ||
			    !word_payload.audios.is_empty()) {
				CLAY(CLAY_ID("PronunciationRow"),
				     {
						   .layout =
								 {
									   .sizing = {CLAY_SIZING_GROW(0),
				                                  CLAY_SIZING_FIT(0)},
									   .childGap = sizes()->space.sm,
									   .childAlignment = {CLAY_ALIGN_X_LEFT,
				                                          CLAY_ALIGN_Y_CENTER},
									   .layoutDirection = CLAY_LEFT_TO_RIGHT,
								 },
					 }) {
					if (ctx->settings.is_show_ipa &&
					    !word_payload.ipa.is_empty()) {
						StrView ipa_joined =
							  StrBuilder(word_payload.ipa)
									.join(ctx->arena_frame, " · "_v);
						StrView ipa_display = StrBuilder::concat(
							  ctx->arena_frame, "["_v, ipa_joined, "]"_v);
						draw_text(ipa_display, theme()->onSurfaceContainer,
						          sizes()->font.label_md, FontID::MAIN);
					}

					// TODO: add
					// if (!word_payload.audios.is_empty()) {
					// 	CLAY(CLAY_ID("AudioPlayButton"),
					// 	     {
					// 			   .layout =
					// 					 {
					// 						   .padding = {udpi(6.f),
					// udpi(8.f), udpi(2.f), udpi(2.f)}, .childAlignment =
					// {CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER},
					// 					 },
					// 			   .backgroundColor =
					// theme()->surfaceContainerHigh, .cornerRadius =
					// CLAY_CORNER_RADIUS(dpi(12.f)),
					// 		 }) {
					// 		// "🔊" ?
					// 		draw_text(Icons::PLAY, theme()->primary,
					// 		          static_cast<uint16_t>(udpi(11.f)));
					// 	}
					// }
				}
			}
		}

		if (!forms.is_empty()) {
			CLAY(CLAY_ID("WordFormsBlock"),
			     {
					   .layout =
							 {
								   .sizing = {CLAY_SIZING_GROW(0),
			                                  CLAY_SIZING_FIT(0)},
								   .padding = sizes()->pad.card_compact,
								   .childGap = sizes()->space.xs,
								   .childAlignment = {CLAY_ALIGN_X_LEFT,
			                                          CLAY_ALIGN_Y_TOP},
								   .layoutDirection = CLAY_TOP_TO_BOTTOM,
							 },
					   .backgroundColor = theme()->surfaceContainer,
					   .cornerRadius = sizes()->radius.md,
				 }) {
				int counter = 0;
				for (auto [label, value] : forms) {
					CLAY(CLAY_IDI("FormRow", counter++),
					     {
							   .layout =
									 {
										   .sizing = {CLAY_SIZING_GROW(0),
					                                  CLAY_SIZING_FIT(0)},
										   .childGap = sizes()->space.sm,
										   .layoutDirection =
												 CLAY_LEFT_TO_RIGHT,
									 },
						 }) {
						CLAY(CLAY_IDI("FormLabelCol", counter),
						     {
								   .layout =
										 {.sizing = {value ? CLAY_SIZING_FIXED(
																   label_width)
						                                   : CLAY_SIZING_FIT(0),
						                             CLAY_SIZING_FIT(0)}},
							 }) {
							draw_text(label, theme()->onSurfaceContainer,
							          form_font_size);
						}
						if (value) {
							draw_text(value, theme()->onSurface,
							          form_font_size);
						}
					}
				}
			}
		}

		CLAY(CLAY_ID("WordCardDivider"),
		     {
				   .layout =
						 {
							   .sizing =
									 {CLAY_SIZING_GROW(0),
		                              CLAY_SIZING_FIXED(
											1.f)}, // TODO: think about scale
						 },
				   .backgroundColor = theme()->outline,
			 }) {}

		if (!word_payload.senses.is_empty()) {
			CLAY(CLAY_ID("WordSensesList"),
			     {
					   .layout =
							 {
								   .sizing = {CLAY_SIZING_GROW(0),
			                                  CLAY_SIZING_FIT(0)},
								   .childGap = sizes()->space.md,
								   .layoutDirection = CLAY_TOP_TO_BOTTOM,
							 },
				 }) {
				for (Size sense_index{0};
				     sense_index < word_payload.senses.size; ++sense_index) {
					auto &sense = word_payload.senses[sense_index];

					CLAY(CLAY_IDI("SenseBlock", sense_index),
					     {
							   .layout =
									 {
										   .sizing = {CLAY_SIZING_GROW(0),
					                                  CLAY_SIZING_FIT(0)},
										   .childGap = sizes()->space.sm,
										   .childAlignment = {CLAY_ALIGN_X_LEFT,
					                                          CLAY_ALIGN_Y_TOP},
										   .layoutDirection =
												 CLAY_LEFT_TO_RIGHT,
									 },
						 }) {
						const float badge_dim = sizes()->dim.icon_sm;
						if (word_payload.senses.size > 1) {
							CLAY(CLAY_IDI("SenseNumberBadge", sense_index),
							     {
									   .layout =
											 {
												   .sizing = {CLAY_SIZING_FIXED(
																	badge_dim),
							                                  CLAY_SIZING_FIXED(
																	badge_dim)},
												   .childAlignment =
														 {CLAY_ALIGN_X_CENTER,
							                              CLAY_ALIGN_Y_CENTER},
											 },
									   .backgroundColor =
											 theme()->surfaceContainerHigh,
									   .cornerRadius = sizes()->radius.full,
								 }) {
								draw_text(StrView::from_number(ctx->arena_frame,
								                               sense_index + 1),
								          theme()->onSurface,
								          sizes()->font.label_sm);
							}
						}

						CLAY(CLAY_IDI("SenseContent", sense_index),
						     {
								   .layout =
										 {
											   .sizing = {CLAY_SIZING_GROW(0),
						                                  CLAY_SIZING_FIT(0)},
											   .childGap = sizes()->space.xs,
											   .layoutDirection =
													 CLAY_TOP_TO_BOTTOM,
										 },
							 }) {

							if (sense.valency) {
								CLAY(CLAY_IDI("ValencyBadge", sense_index),
								     {
										   .layout =
												 {.sizing = {CLAY_SIZING_FIT(0),
								                             CLAY_SIZING_FIT(
																   0)},
								                  .padding =
								                        sizes()
								                              ->pad
								                              .badge_compact},
										   .backgroundColor =
												 theme()->surfaceContainerHigh,
										   .cornerRadius = sizes()->radius.xs,
									 }) {
									draw_text(sense.valency,
									          theme()->onSurfaceContainerHigh,
									          sizes()->font.label_sm);
								}
							}

							for (Size translation_index{0};
							     translation_index < sense.translations.size;
							     ++translation_index) {
								if (!sense.translations[translation_index]) {
									continue;
								}

								CLAY(CLAY_IDI("TransItem",
								              (sense_index << 8) |
								                    translation_index),
								     {
										   .layout =
												 {
													   .sizing =
															 {CLAY_SIZING_GROW(
																	0),
								                              CLAY_SIZING_FIT(
																	0)},
													   .childGap =
															 sizes()->space.xs,
													   .childAlignment =
															 {CLAY_ALIGN_X_LEFT,
								                              CLAY_ALIGN_Y_TOP},
													   .layoutDirection =
															 CLAY_LEFT_TO_RIGHT,
												 },
									 }) {
									draw_text("•"_v, theme()->outline,
									          translation_font_size);
									draw_text(
										  sense.translations[translation_index],
										  theme()->onSurface,
										  translation_font_size,
										  translation_font_id(ctx),
										  CLAY_TEXT_WRAP_WORDS,
										  CLAY_TEXT_ALIGN_LEFT);
								}
							}
						}
					}
				}
			}
		} else if (!word_payload.de_glosses.is_empty()) {
			// NOTE: de only
			CLAY(CLAY_ID("DeGlossesList"),
			     {
					   .layout =
							 {
								   .sizing = {CLAY_SIZING_GROW(0),
			                                  CLAY_SIZING_FIT(0)},
								   .childGap = sizes()->space.xs,
								   .layoutDirection = CLAY_TOP_TO_BOTTOM,
							 },
				 }) {
				for (Size i{0}; i < word_payload.de_glosses.size; ++i) {
					CLAY(CLAY_IDI("DeGlossItem", i),
					     {
							   .layout =
									 {
										   .sizing = {CLAY_SIZING_GROW(0),
					                                  CLAY_SIZING_FIT(0)},
										   .childGap = sizes()->space.xs,
										   .layoutDirection =
												 CLAY_LEFT_TO_RIGHT,
									 },
						 }) {
						draw_text("•"_v, theme()->outline,
						          translation_font_size);
						draw_text(word_payload.de_glosses[i],
						          theme()->onSurface, translation_font_size,
						          FontID::MAIN, CLAY_TEXT_WRAP_WORDS,
						          CLAY_TEXT_ALIGN_LEFT);
					}
				}
			}
		} else if (w.translations_raw) {
			draw_text(w.translations_raw, theme()->onSurface,
			          translation_font_size, translation_font_id(ctx),
			          CLAY_TEXT_WRAP_WORDS, CLAY_TEXT_ALIGN_LEFT);
		}

		// NOTE: phrase only
		if (w.type == WordType::Phrase && !word_payload.words.is_empty()) {
			CLAY(CLAY_ID("PhraseKeywordsRow"),
			     {
					   .layout =
							 {
								   .sizing = {CLAY_SIZING_GROW(0),
			                                  CLAY_SIZING_FIT(0)},
								   .childGap = sizes()->space.xs,
								   .childAlignment = {CLAY_ALIGN_X_LEFT,
			                                          CLAY_ALIGN_Y_CENTER},
								   .layoutDirection = CLAY_LEFT_TO_RIGHT,
							 },
				 }) {
				draw_text(tr()->screen_word_view_phrase_words, theme()->outline, sizes()->font.label_md);
				for (Size k{0}; k < word_payload.words.size; ++k) {
					CLAY(CLAY_IDI("KeywordPill", k),
					     {
							   .layout = {.padding =
					                            sizes()->pad.badge_compact},
							   .backgroundColor = theme()->surfaceContainer,
							   .cornerRadius = sizes()->radius.xs,
						 }) {
						draw_text(word_payload.words[k], theme()->primary,
						          sizes()->font.label_sm);
					}
				}
			}
		}

		if (!word_payload.examples.is_empty()) {
			CLAY(CLAY_ID("ExamplesDivider"),
			     {
					   .layout =
							 {
								   .sizing = {CLAY_SIZING_GROW(0),
			                                  CLAY_SIZING_FIXED(
													1.f)}, // TODO: think about
			                                               // scale
							 },
					   .backgroundColor = theme()->outline,
				 }) {}

			draw_text(tr()->screen_word_view_examples, theme()->onSurfaceContainer,
			          sizes()->font.title_md);

			CLAY(CLAY_ID("ExamplesList"),
			     {
					   .layout =
							 {
								   .sizing = {CLAY_SIZING_GROW(0),
			                                  CLAY_SIZING_FIT(0)},
								   .childGap = sizes()->space.sm,
								   .layoutDirection = CLAY_TOP_TO_BOTTOM,
							 },
				 }) {
				for (Size example_index{0};
				     example_index < word_payload.examples.size;
				     ++example_index) {
					const auto &ex = word_payload.examples[example_index];

					CLAY(CLAY_IDI("ExampleCard", example_index),
					     {
							   .layout =
									 {
										   .sizing = {CLAY_SIZING_GROW(0),
					                                  CLAY_SIZING_FIT(0)},
										   .padding =
												 sizes()->pad.example_quote,
										   .childGap = sizes()->space.xs,
										   .layoutDirection =
												 CLAY_TOP_TO_BOTTOM,
									 },
							   .backgroundColor = theme()->surfaceContainer,
							   // .cornerRadius =
					           // {
					           //    0,
					           //    dpi(8.f),
					           //    0,
					           //    dpi(8.f),
					           // }, // CLAY_CORNER_RADIUS(dpi(8.f)),
							   .border =
									 {.color = theme()->secondary,
					                  .width =
					                        {.left = static_cast<uint16_t>(
												   sizes()
														 ->dim
														 .accent_border_width)}},
						 }) {
						draw_text(ex.text, theme()->onSurface,
						          sizes()->font.body_md, FontID::MAIN,
						          CLAY_TEXT_WRAP_WORDS, CLAY_TEXT_ALIGN_LEFT);

						if (ex.translation) {
							draw_text(
								  ex.translation, theme()->onSurfaceContainer,
								  sizes()->font.body_sm,
								  translation_font_id(ctx),
								  CLAY_TEXT_WRAP_WORDS, CLAY_TEXT_ALIGN_LEFT);
						}
					}
				}
			}
		}

		if (!word_payload.synonyms.is_empty() ||
		    !word_payload.antonyms.is_empty() ||
		    !word_payload.hypernyms.is_empty()) {
			const float syn_ant_label_width =
				  static_cast<float>(sizes()->space.xxl + sizes()->space.xs);

			CLAY(CLAY_ID("SynAntContainer"),
			     {
					   .layout =
							 {
								   .sizing = {CLAY_SIZING_GROW(0),
			                                  CLAY_SIZING_FIT(0)},
								   .padding = {.top = sizes()->space.xs},
								   .childGap = sizes()->space.xs,
								   .layoutDirection = CLAY_TOP_TO_BOTTOM,
							 },
				 }) {
				if (!word_payload.synonyms.is_empty()) {
					CLAY(CLAY_ID("SynRow"),
					     {
							   .layout =
									 {
										   .sizing = {CLAY_SIZING_GROW(0),
					                                  CLAY_SIZING_FIT(0)},
										   .childGap = sizes()->space.xs,
										   .childAlignment = {CLAY_ALIGN_X_LEFT,
					                                          CLAY_ALIGN_Y_TOP},
										   .layoutDirection =
												 CLAY_LEFT_TO_RIGHT,
									 },
						 }) {
						CLAY(CLAY_ID("SynLabelCol"),
						     {
								   .layout =
										 {
											   .sizing =
													 {CLAY_SIZING_FIXED(
															syn_ant_label_width),
						                              CLAY_SIZING_FIT(0)},
										 },
							 }) {
							draw_text(tr()->screen_word_view_syn, theme()->outline,
							          sizes()->font.label_md);
						}
						CLAY(CLAY_ID("SynTextCol"),
						     {
								   .layout = {.sizing = {CLAY_SIZING_GROW(0),
						                                 CLAY_SIZING_FIT(0)}},
							 }) {
							auto syn_text =
								  StrBuilder(word_payload.synonyms)
										.join(ctx->arena_frame, ", "_v);
							draw_text(syn_text, theme()->onSurfaceContainer,
							          sizes()->font.label_md, FontID::MAIN,
							          CLAY_TEXT_WRAP_WORDS,
							          CLAY_TEXT_ALIGN_LEFT);
						}
					}
				}

				if (!word_payload.antonyms.is_empty()) {
					CLAY(CLAY_ID("AntRow"),
					     {
							   .layout =
									 {
										   .sizing = {CLAY_SIZING_GROW(0),
					                                  CLAY_SIZING_FIT(0)},
										   .childGap = sizes()->space.xs,
										   .childAlignment = {CLAY_ALIGN_X_LEFT,
					                                          CLAY_ALIGN_Y_TOP},
										   .layoutDirection =
												 CLAY_LEFT_TO_RIGHT,
									 },
						 }) {
						CLAY(CLAY_ID("AntLabelCol"),
						     {
								   .layout =
										 {
											   .sizing =
													 {CLAY_SIZING_FIXED(
															syn_ant_label_width),
						                              CLAY_SIZING_FIT(0)},
										 },
							 }) {
							draw_text(tr()->screen_word_view_ant, theme()->outline,
							          sizes()->font.label_md);
						}
						CLAY(CLAY_ID("AntTextCol"),
						     {
								   .layout = {.sizing = {CLAY_SIZING_GROW(0),
						                                 CLAY_SIZING_FIT(0)}},
							 }) {
							auto ant_text =
								  StrBuilder(word_payload.antonyms)
										.join(ctx->arena_frame, ", "_v);
							draw_text(ant_text, theme()->onSurfaceContainer,
							          sizes()->font.label_md, FontID::MAIN,
							          CLAY_TEXT_WRAP_WORDS,
							          CLAY_TEXT_ALIGN_LEFT);
						}
					}
				}

				if (!word_payload.hypernyms.is_empty()) {
					CLAY(CLAY_ID("HyperRow"),
					     {
							   .layout =
									 {
										   .sizing = {CLAY_SIZING_GROW(0),
					                                  CLAY_SIZING_FIT(0)},
										   .childGap = sizes()->space.xs,
										   .childAlignment = {CLAY_ALIGN_X_LEFT,
					                                          CLAY_ALIGN_Y_TOP},
										   .layoutDirection =
												 CLAY_LEFT_TO_RIGHT,
									 },
						 }) {
						CLAY(CLAY_ID("HyperLabelCol"),
						     {
								   .layout =
										 {
											   .sizing =
													 {CLAY_SIZING_FIXED(
															syn_ant_label_width),
						                              CLAY_SIZING_FIT(0)},
										 },
							 }) {
							draw_text(tr()->screen_word_view_hyp, theme()->outline,
							          sizes()->font.label_md);
						}
						CLAY(CLAY_ID("HyperTextCol"),
						     {
								   .layout = {.sizing = {CLAY_SIZING_GROW(0),
						                                 CLAY_SIZING_FIT(0)}},
							 }) {
							auto hyp_text =
								  StrBuilder(word_payload.hypernyms)
										.join(ctx->arena_frame, ", "_v);
							draw_text(hyp_text, theme()->onSurfaceContainer,
							          sizes()->font.label_md, FontID::MAIN,
							          CLAY_TEXT_WRAP_WORDS,
							          CLAY_TEXT_ALIGN_LEFT);
						}
					}
				}
			}
		}

		if (ctx->settings.is_show_origin && word_payload.etymology) {
			CLAY(CLAY_ID("EtymologyCard"),
			     {
					   .layout =
							 {
								   .sizing = {CLAY_SIZING_GROW(0),
			                                  CLAY_SIZING_FIT(0)},
								   .padding = sizes()->pad.card_compact,
								   .childGap = sizes()->space.xs,
								   .layoutDirection = CLAY_TOP_TO_BOTTOM,
							 },
					   .backgroundColor = theme()->surfaceContainer,
					   .cornerRadius = sizes()->radius.sm,
				 }) {
				draw_text(tr()->screen_word_view_origin, theme()->secondary,
				          sizes()->font.label_sm);

				draw_text(word_payload.etymology, theme()->onSurfaceContainer,
				          sizes()->font.body_sm, FontID::MAIN,
				          CLAY_TEXT_WRAP_WORDS, CLAY_TEXT_ALIGN_LEFT);
			}
		}
	}
}

static void draw_learning_state(AppContext *ctx, const Engine::State &s) {
	const auto now = std::time(nullptr);
	const bool is_new = (s.total_reviews == 0);
	const bool is_overdue = (s.due <= now);
	const int current_step = static_cast<int>(s.mode);

	CLAY(CLAY_ID("LearningStateCard"),
	     {
			   .layout =
					 {
						   .sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0)},
						   .padding = sizes()->pad.card,
						   .childGap = sizes()->space.sm,
						   .childAlignment = {CLAY_ALIGN_X_LEFT,
	                                          CLAY_ALIGN_Y_TOP},
						   .layoutDirection = CLAY_TOP_TO_BOTTOM,
					 },
			   .backgroundColor = theme()->surfaceContainerLow,
			   .cornerRadius = sizes()->radius.lg,
			   .border =
					 {
						   .color = theme()->outline,
						   .width = {(1), (1), (1),
	                                 (1)}, // TODO: think about scale
					 },
		 }) {

		CLAY(CLAY_ID("LearningHeaderRow"),
		     {
				   .layout =
						 {
							   .sizing = {CLAY_SIZING_GROW(0),
		                                  CLAY_SIZING_FIT(0)},
							   .childGap = sizes()->space.sm,
							   .childAlignment = {CLAY_ALIGN_X_LEFT,
		                                          CLAY_ALIGN_Y_CENTER},
							   .layoutDirection = CLAY_LEFT_TO_RIGHT,
						 },
			 }) {
			draw_text(mode_name(s.mode), theme()->onSurface,
			          sizes()->font.body_md);

			draw_text(StrBuilder::concat(ctx->arena_frame, "("_v,
			                             StrView::from_number(ctx->arena_frame,
			                                                  current_step + 1),
			                             "/4)"_v),
			          theme()->onSurfaceContainer, sizes()->font.body_sm);

			StrView due_str{};
			Clay_Color due_bg{};
			Clay_Color due_fg{};

			if (is_new) {
				due_str = tr()->screen_word_view_due_new;
				due_bg = theme()->surfaceContainer;
				due_fg = theme()->onSurfaceContainer;
			} else if (is_overdue) {
				due_str = tr()->screen_word_view_due_ready;
				due_bg = theme()->wrongContainer;
				due_fg = theme()->onWrongContainer;
			} else {
				due_str = format_due_delta(ctx->arena_frame, now, s.due);
				due_bg = theme()->surfaceContainerHigh;
				due_fg = theme()->onSurfaceContainerHigh;
			}

			CLAY(CLAY_ID("DueBadge"),
			     {
					   .layout =
							 {
								   .padding = sizes()->pad.badge_compact,
							 },
					   .backgroundColor = due_bg,
					   .cornerRadius = sizes()->radius.sm,
				 }) {
				draw_text(due_str, due_fg, sizes()->font.label_sm);
			}
		}

		CLAY(CLAY_ID("ProgressBar"),
		     {
				   .layout =
						 {
							   .sizing = {CLAY_SIZING_GROW(0),
		                                  CLAY_SIZING_FIT(0)},
							   .childGap = sizes()->space.xs,
							   .layoutDirection = CLAY_LEFT_TO_RIGHT,
						 },
			 }) {
			for (int i = 0; i < 4; ++i) {
				const bool is_filled = (i <= current_step);
				CLAY(CLAY_IDI("ProgressStep", i),
				     {
						   .layout =
								 {
									   .sizing =
											 {CLAY_SIZING_GROW(0),
				                              CLAY_SIZING_FIXED(
													(float)sizes()->space.xs)},
								 },
						   .backgroundColor =
								 is_filled ? theme()->secondary
										   : theme()->surfaceContainerHigh,
						   .cornerRadius = sizes()->radius.xs,
					 }) {}
			}
		}

		if (s.mode < Engine::Mode::Compose) {
			const auto left =
				  successful_reviews_to_next_mode(ctx->arena_frame, s);
			draw_text(StrBuilder::concat(ctx->arena_frame, tr()->screen_word_view_next_level_in,
			                             left, tr()->screen_word_view_reviews_count),
			          theme()->onSurfaceContainer, sizes()->font.label_md);
		} else {
			draw_text(tr()->screen_word_view_mastered_max_level, theme()->onSurfaceContainer,
			          sizes()->font.label_md);
		}
	}
}

} // namespace

void screen_word_view_push(AppContext *ctx, WordId word_id) {
	KLAPPT_PROFILE_SCOPE_N("screen_word_view_push");

	SDL_Log(__PRETTY_FUNCTION__);
	ctx->push(Screen::WordView);

	auto &state = *ctx->word_view_state;
	state.word_id = word_id;
	state.has_learning_state = false;
	bool is_word_copied = false;

	{
		KLAPPT_PROFILE_SCOPE_N("copy Word words");
		// NOTE: try to get it from the learning list
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
	// NOTE: if the word not in the learning list -> get it from the store
	if (!is_word_copied) {
		KLAPPT_PROFILE_SCOPE_N("copy Word store");
		// get it from xapian
		ctx->word_store.get_by_id(ctx->arena_screen(), word_id,
		                          state.word_copy);
		is_word_copied = true;
	}
	{
		KLAPPT_PROFILE_SCOPE_N("copy State");
		// get it from lmdb
		auto [is_success, was_found] =
			  ctx->states.get(word_id, state.learning_state_copy);
		if (is_success) {
			state.has_learning_state = was_found;
		} else {
			ctx->app_status.push_error("lmdb get() error"_v);
		}
	}
	{
		KLAPPT_PROFILE_SCOPE_N("parse JSON");
		simdjson::dom::parser parser{};

		if (!word_json_parse(ctx->arena_frame, ctx->arena_screen(),
		                     state.word_copy.json_payload, state.word_payload,
		                     parser)) {
			ctx->app_status.push_error("word_json_parse() error"_v);
		} else {
			// log_word_payload(state.word_payload);
		}
	}
}

void screen_word_view_draw(AppContext *ctx) {
	KLAPPT_PROFILE_SCOPE_N("screen_word_view_draw");
	auto &state = *ctx->word_view_state;

	CLAY(CLAY_ID("WordViewScreen"),
	     {
			   .layout =
					 {
						   .sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)},
						   .childAlignment = {CLAY_ALIGN_X_CENTER,
	                                          CLAY_ALIGN_Y_TOP},
						   .layoutDirection = CLAY_TOP_TO_BOTTOM,
					 },
			   .backgroundColor = theme()->surface,
		 }) {

		CLAY(CLAY_ID("WordViewScrollArea"),
		     {
				   .layout =
						 {
							   .sizing = {CLAY_SIZING_GROW(0),
		                                  CLAY_SIZING_GROW(0)},
						 },
			 }) {
			const Size count = state.has_learning_state ? 2 : 1;

			auto draw_cards = [&state](AppContext *ctx, Size i,
			                           Clay_ElementId item_clay_id) {
				if (i == 0) {
					draw_word_card(ctx, item_clay_id, state.word_copy,
					               state.word_payload);
				} else if (i == 1 && state.has_learning_state) {
					CLAY(item_clay_id,
					     {
							   .layout =
									 {
										   .sizing = {CLAY_SIZING_GROW(0),
					                                  CLAY_SIZING_FIT(0)},
									 },
						 }) {
						draw_learning_state(ctx, state.learning_state_copy);
					}
				}
			};

			list::vertical_dynamic_rich(ctx, CLAY_ID("WordCardsList"),
			                            sizes()->space.md, sizes()->pad.screen,
			                            count, draw_cards);
		}

		CLAY(CLAY_ID("WordViewBottomBar"),
		     {
				   .layout =
						 {
							   .sizing = {CLAY_SIZING_GROW(0),
		                                  CLAY_SIZING_FIT(0)},
							   .padding =
									 {
										   .left = sizes()->space.lg,
										   .right = sizes()->space.lg,
										   .top = sizes()->space.sm,
										   .bottom = sizes()->space.lg,
									 },
							   .childGap = sizes()->space.md,
							   .childAlignment = {CLAY_ALIGN_X_CENTER,
		                                          CLAY_ALIGN_Y_CENTER},
							   .layoutDirection = CLAY_LEFT_TO_RIGHT,
						 },
				   .backgroundColor = theme()->surface,
				   // .border =
		           // {
		           //    .color = theme()->outline,
		           //    .width = {0, 0, udpi(1.f), 0},
		           // },
			 }) {
			auto word_ref = state.word_ref;
			auto add_or_remove_btn = mobile_icon_button<false>(
				  ctx, CLAY_ID("RemoveButton"),
				  state.word_copy.in_learning_list ? Icons::REMOVE
												   : Icons::SAVE);
			if (add_or_remove_btn.activated()) {
				toggle_word_from_learning_list_and_save_words_dat(
					  ctx, &state.word_copy);
			}
			auto back_button = mobile_icon_button<false>(
				  ctx, CLAY_ID_LOCAL("BackButton"), Icons::BACK);
			if (back_button.activated()) {
				ctx->pop();
			}

#if NEURO
			if (ctx->settings.is_module_tts) {
				auto play = mobile_icon_button<true>(ctx, CLAY_ID("PlayButton"),
				                                     Icons::PLAY);
				if (play.activated()) {
					auto tts_string =
						  word_tts_full(ctx->arena_screen(), state.word_copy);
					run_tts(ctx, tts_string);
				}
			}
#endif // NEURO

			if (ctx->screen_prev() == Screen::Dictionary) {
				auto back_and_clear_and_focus = mobile_icon_button<false>(
					  ctx, CLAY_ID_LOCAL("BackClearFocusButton"),
					  Icons::ROTATE);
				if (back_and_clear_and_focus.activated()) {
					ctx->dictionary_search.clear();
					ctx->mobile_text_input.activate_text_input = true;
					ctx->pop();
				}
			}
		}
	}
	// log_word_payload(state.word_payload);
	// SDL_Log("RAW\n" StrView_Fmt "\n",
	//         StrView_Arg(state.word_copy.json_payload));
}
