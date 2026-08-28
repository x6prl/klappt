#include <SDL3/SDL_log.h>
#include <SDL3/SDL_stdinc.h>

#include <simdjson/simdjson.h>

#include "app/app_context.h"
#include "base/dyn_arr.h"
#include "base/pair.h"
#include "base/profiler.h"
#include "base/str_builder.h"
#include "base/str_view.h"
#include "domain/word.h"
#include "domain/word_payload.h"
#include "platform/neuro.h"
#include "ui/components/button.h"
#include "ui/components/lists.h"
#include "ui/dpi.h"
#include "ui/textcache.h"
#include "ui/themes.h"

#include "screen_helpers.h"

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
	KLAPPT_PROFILE_SCOPE();
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

static void draw_noun_title(AppContext *ctx, const Noun &n) {
	const uint16_t title_font_size = static_cast<uint16_t>(udpi(26.f));
	const uint16_t form_font_size = static_cast<uint16_t>(udpi(20.f));
	const float label_width = udpi(60.f);

	CLAY(CLAY_ID("NounTitleRow"),
	     {
			   .layout =
					 {
						   .sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0)},
						   .childGap = udpi(8.f),
						   .layoutDirection = CLAY_LEFT_TO_RIGHT,
					 },
		 }) {
		auto article = gender_to_article_nominative_strview(n.gender);
		if (article && article != " "_v && article != " — "_v) {
			draw_text(article, theme()->secondary, title_font_size);
		}
		draw_text(n.lemma, theme()->onSurface, title_font_size);
		// TODO: do we want it?..
		// draw_text(StrView::concat(ctx->arena_frame, " "_v, n.lemma),
		//           theme()->onSurface, title_font_size);
		// bool has_plural_suffix = n.plural_suffix;
		// if (has_plural_suffix) {
		// 	draw_text(
		// 		  StrView::concat(ctx->arena_frame, ", "_v, n.plural_suffix),
		// 		  theme()->secondary, title_font_size);
		// }
	}
}

static void draw_adj_title(AppContext *ctx, const Word &w) {
	const uint16_t title_font_size = static_cast<uint16_t>(udpi(26.f));

	draw_text(w.a.lemma, theme()->onSurface, title_font_size, FontID::MAIN,
	          CLAY_TEXT_WRAP_WORDS, CLAY_TEXT_ALIGN_LEFT);
}

static void draw_verb_title(AppContext *ctx, const Word &w) {
	const uint16_t title_font_size = static_cast<uint16_t>(udpi(26.f));

	draw_text(w.v.infinitive, theme()->onSurface, title_font_size, FontID::MAIN,
	          CLAY_TEXT_WRAP_WORDS, CLAY_TEXT_ALIGN_LEFT);
}

static void draw_phrase_title(AppContext *ctx, const Word &w) {
	const bool is_long_phrase = w.p.text.utf8_length() > 50;
	const Clay_TextAlignment text_align =
		  is_long_phrase ? CLAY_TEXT_ALIGN_LEFT : CLAY_TEXT_ALIGN_CENTER;

	const uint16_t title_font_size = static_cast<uint16_t>(udpi(26.f));

	draw_text(w.p.text, theme()->onSurface, title_font_size, FontID::MAIN,
	          CLAY_TEXT_WRAP_WORDS, text_align);
}

static void draw_word_card(AppContext *ctx, Clay_ElementId element_id,
                           const Word &w, const WordPayload &word_payload) {
	const float label_width = udpi(100.f);
	const uint16_t form_font_size = static_cast<uint16_t>(udpi(15.f));
	const uint16_t translation_font_size = static_cast<uint16_t>(udpi(15.f));

	DynArr<Pair<StrView, StrView>> forms{};
	DynArr<StrView> badges{};
	StrView type{};

	switch (w.type) {
	case WordType::Noun: {
		type = "Noun"_v;
		if (is_singular_only(w.n)) {
			badges.push(ctx->arena_frame, "Singular only"_v);
		} else if (is_plural_only(w.n)) {
			badges.push(ctx->arena_frame, "Plural only"_v);
		} else {
			auto plural =
				  word_noun_get_plural_with_artikel(ctx->arena_frame, w.n);
			forms.push(ctx->arena_frame, {"Plural:"_v, plural});
		}
	} break;
	case WordType::Verb: {
		type = "Verb"_v;
		forms.push(ctx->arena_frame,
		           {"er/sie/es:"_v,
		            word_verb_get_third_person_full(ctx->arena_frame, w.v)});
		forms.push(ctx->arena_frame,
		           {"Präteritum:"_v,
		            word_verb_get_praeteritum_full(ctx->arena_frame, w.v)});
		forms.push(ctx->arena_frame,
		           {"Perfekt:"_v,
		            word_verb_get_perfect_full(ctx->arena_frame, w.v)});
	} break;
	case WordType::Adj: {
		type = "Adjective"_v;
		if (w.a.is_indeclinable) {
			badges.push(ctx->arena_frame, "Indeclinable"_v);
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
		type = "Phrase"_v;
	} break;
	default:
		break;
	}

	CLAY(element_id,
	     {
			   .layout =
					 {
						   .sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0)},
						   .padding =
								 {
									   .left = udpi(20.f),
									   .right = udpi(20.f),
									   .top = udpi(16.f),
									   .bottom = udpi(20.f),
								 },
						   .childGap = udpi(12.f),
						   .childAlignment = {CLAY_ALIGN_X_LEFT,
	                                          CLAY_ALIGN_Y_TOP},
						   .layoutDirection = CLAY_TOP_TO_BOTTOM,
					 },
			   .backgroundColor = theme()->surfaceContainerLow,
			   .cornerRadius = CLAY_CORNER_RADIUS(dpi(16.f)),
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
							   .childGap = udpi(8.f),
							   .childAlignment = {CLAY_ALIGN_X_LEFT,
		                                          CLAY_ALIGN_Y_CENTER},
							   .layoutDirection = CLAY_LEFT_TO_RIGHT,
						 },
			 }) {
			CLAY(CLAY_ID("WordTypeBadge"),
			     {
					   .layout =
							 {
								   .padding = {udpi(8.f), udpi(8.f), udpi(4.f),
			                                   udpi(4.f)},
							 },
					   .backgroundColor = theme()->secondary,
					   .cornerRadius = CLAY_CORNER_RADIUS(dpi(6.f)),
				 }) {
				draw_text(type, theme()->onSecondary,
				          static_cast<uint16_t>(udpi(12.f)));
			}

			for (Size i{0}; i < badges.size; ++i) {
				CLAY(CLAY_IDI("Badge", i),
				     {
						   .layout = {.padding = {udpi(8.f), udpi(8.f),
				                                  udpi(4.f), udpi(4.f)}},
						   .backgroundColor = theme()->surfaceContainerHigh,
						   .cornerRadius = CLAY_CORNER_RADIUS(dpi(6.f)),
					 }) {
					draw_text(badges[i], theme()->onSurfaceContainerHigh,
					          static_cast<uint16_t>(udpi(12.f)));
				}
			}

			if (w.in_learning_list > 0) {
				CLAY(CLAY_ID("StatusBadge"),
				     {
						   .layout =
								 {
									   .padding = {udpi(8.f), udpi(8.f),
				                                   udpi(4.f), udpi(4.f)},
									   .childGap = udpi(8.f),
									   .childAlignment = {CLAY_ALIGN_X_LEFT,
				                                          CLAY_ALIGN_Y_CENTER},
								 },
						   .backgroundColor = theme()->surfaceContainer,
						   .cornerRadius = CLAY_CORNER_RADIUS(dpi(6.f)),
					 }) {
					draw_text("In learning list"_v, theme()->onSurfaceContainer,
					          static_cast<uint16_t>(udpi(12.f)));
				}
			}
		}

		CLAY(CLAY_ID("TitleBlock"),
		     {
				   .layout =
						 {
							   .sizing = {CLAY_SIZING_GROW(0),
		                                  CLAY_SIZING_FIT(0)},
							   .childGap = udpi(4.f),
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

			// TODO: last — the german?
			if (!word_payload.ipa.is_empty() && word_payload.ipa.last()) {
				draw_text(StrBuilder::concat(ctx->arena_frame, "/"_v,
				                             word_payload.ipa.last(), "/"_v),
				          theme()->onSurfaceContainer,
				          static_cast<uint16_t>(udpi(14.f)), FontID::MAIN,
				          CLAY_TEXT_WRAP_WORDS, CLAY_TEXT_ALIGN_LEFT);
			}
		}

		if (!forms.is_empty()) {
			CLAY(CLAY_ID("WordFormsBlock"),
			     {
					   .layout =
							 {
								   .sizing = {CLAY_SIZING_GROW(0),
			                                  CLAY_SIZING_FIT(0)},
								   .padding =
										 {
											   .left = udpi(12.f),
											   .right = udpi(12.f),
											   .top = udpi(10.f),
											   .bottom = udpi(10.f),
										 },
								   .childGap = udpi(6.f),
								   .childAlignment = {CLAY_ALIGN_X_LEFT,
			                                          CLAY_ALIGN_Y_TOP},
								   .layoutDirection = CLAY_TOP_TO_BOTTOM,
							 },
					   .backgroundColor = theme()->surfaceContainer,
					   .cornerRadius = CLAY_CORNER_RADIUS(dpi(10.f)),
				 }) {
				int counter = 0;
				for (auto [label, value] : forms) {
					CLAY(CLAY_IDI("FormRow", counter++),
					     {
							   .layout =
									 {
										   .sizing = {CLAY_SIZING_GROW(0),
					                                  CLAY_SIZING_FIT(0)},
										   .childGap = udpi(6.f),
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
							   .sizing = {CLAY_SIZING_GROW(0),
		                                  CLAY_SIZING_FIXED(dpi(1.f))},
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
								   .childGap = udpi(12.f),
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
										   .childGap = udpi(8.f),
										   .childAlignment = {CLAY_ALIGN_X_LEFT,
					                                          CLAY_ALIGN_Y_TOP},
										   .layoutDirection =
												 CLAY_LEFT_TO_RIGHT,
									 },
						 }) {

						if (word_payload.senses.size > 1) {
							CLAY(CLAY_IDI("SenseNumberBadge", sense_index),
							     {
									   .layout =
											 {
												   .sizing = {CLAY_SIZING_FIXED(
																	dpi(18.f)),
							                                  CLAY_SIZING_FIXED(
																	dpi(18.f))},
												   .childAlignment =
														 {CLAY_ALIGN_X_CENTER,
							                              CLAY_ALIGN_Y_CENTER},
											 },
									   .backgroundColor =
											 theme()->surfaceContainerHigh,
									   .cornerRadius =
											 CLAY_CORNER_RADIUS(dpi(999.f)),
								 }) {
								draw_text(StrView::from_number(ctx->arena_frame,
								                               sense_index + 1),
								          theme()->onSurface,
								          static_cast<uint16_t>(udpi(11.f)));
							}
						}
						// if (word_payload.senses.size > 1) {
						// 	CLAY(CLAY_IDI("SenseNumberBadge", sense_index),
						// 	     {
						// 			   .layout =
						// 					 {
						// 						   .sizing =
						// 								 {CLAY_SIZING_FIT(0),
						// 	                              CLAY_SIZING_FIT(0)},
						// 						   .padding = {udpi(5.f),
						// 	                                   udpi(5.f),
						// 	                                   udpi(2.f),
						// 	                                   udpi(2.f)},
						// 						   .childAlignment =
						// 								 {CLAY_ALIGN_X_CENTER,
						// 	                              CLAY_ALIGN_Y_CENTER},
						// 					 },
						// 			   .backgroundColor =
						// 					 theme()->surfaceContainerHigh,
						// 			   .cornerRadius =
						// 					 CLAY_CORNER_RADIUS(dpi(4.f)),
						// 		 }) {
						// 		draw_text(StrView::from_number(ctx->arena_frame,
						// 		                               sense_index + 1),
						// 		          theme()->onSurfaceContainerHigh,
						// 		          static_cast<uint16_t>(udpi(11.f)));
						// 	}
						// }

						CLAY(CLAY_IDI("SenseContent", sense_index),
						     {
								   .layout =
										 {
											   .sizing = {CLAY_SIZING_GROW(0),
						                                  CLAY_SIZING_FIT(0)},
											   .childGap = udpi(4.f),
											   .layoutDirection =
													 CLAY_TOP_TO_BOTTOM,
										 },
							 }) {

							if (sense.valency) {
								CLAY(CLAY_IDI("ValencyBadge", sense_index),
								     {
										   .layout =
												 {
													   .sizing =
															 {CLAY_SIZING_FIT(
																	0),
								                              CLAY_SIZING_FIXED(
																	dpi(18.f))},
													   .padding = {udpi(6.f),
								                                   udpi(6.f),
								                                   udpi(2.f),
								                                   udpi(2.f)},
												 },
										   .backgroundColor =
												 theme()->surfaceContainerHigh,
										   .cornerRadius =
												 CLAY_CORNER_RADIUS(dpi(4.f)),
									 }) {
									draw_text(
										  sense.valency,
										  theme()->onSurfaceContainerHigh,
										  static_cast<uint16_t>(udpi(11.f)));
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
													   .childGap = udpi(6.f),
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
										  theme()->onSurfaceContainer,
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
		} else if (w.translations_raw) {
			// NOTE: fallback
			draw_text(w.translations_raw, theme()->onSurfaceContainer,
			          translation_font_size, translation_font_id(ctx),
			          CLAY_TEXT_WRAP_WORDS, CLAY_TEXT_ALIGN_LEFT);
		}

		if (!word_payload.examples.is_empty()) {
			CLAY(CLAY_ID("ExamplesDivider"),
			     {
					   .layout =
							 {
								   .sizing = {CLAY_SIZING_GROW(0),
			                                  CLAY_SIZING_FIXED(dpi(1.f))},
							 },
					   .backgroundColor = theme()->outline,
				 }) {}

			draw_text("Examples"_v, theme()->onSurface,
			          static_cast<uint16_t>(udpi(13.f)));

			CLAY(CLAY_ID("ExamplesList"),
			     {
					   .layout =
							 {
								   .sizing = {CLAY_SIZING_GROW(0),
			                                  CLAY_SIZING_FIT(0)},
								   .childGap = udpi(6.f),
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
												 {
													   .left = udpi(10.f),
													   .right = udpi(10.f),
													   .top = udpi(8.f),
													   .bottom = udpi(8.f),
												 },
										   .childGap = udpi(3.f),
										   .layoutDirection =
												 CLAY_TOP_TO_BOTTOM,
									 },
							   .backgroundColor = theme()->surfaceContainer,
							   .cornerRadius = CLAY_CORNER_RADIUS(dpi(8.f)),
						 }) {
						draw_text(ex.text, theme()->onSurface,
						          static_cast<uint16_t>(udpi(14.f)),
						          FontID::MAIN, CLAY_TEXT_WRAP_WORDS,
						          CLAY_TEXT_ALIGN_LEFT);

						if (ex.translation) {
							draw_text(
								  ex.translation, theme()->onSurfaceContainer,
								  static_cast<uint16_t>(udpi(13.f)),
								  translation_font_id(ctx),
								  CLAY_TEXT_WRAP_WORDS, CLAY_TEXT_ALIGN_LEFT);
						}
					}
				}
			}
		}
		if (!word_payload.synonyms.is_empty() ||
		    !word_payload.antonyms.is_empty()) {
			const float syn_ant_label_width = udpi(32.f);

			CLAY(CLAY_ID("SynAntContainer"),
			     {
					   .layout =
							 {
								   .sizing = {CLAY_SIZING_GROW(0),
			                                  CLAY_SIZING_FIT(0)},
								   .padding = {.top = udpi(4.f)},
								   .childGap = udpi(6.f),
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
										   .childGap = udpi(4.f),
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
							draw_text("Syn:"_v, theme()->onSurface,
							          static_cast<uint16_t>(udpi(12.f)));
						}

						CLAY(CLAY_ID("SynTextCol"),
						     {
								   .layout =
										 {
											   .sizing = {CLAY_SIZING_GROW(0),
						                                  CLAY_SIZING_FIT(0)},
										 },
							 }) {
							auto syn_text =
								  StrBuilder(word_payload.synonyms)
										.join(ctx->arena_frame, ", "_v);
							draw_text(syn_text, theme()->onSurfaceContainer,
							          static_cast<uint16_t>(udpi(12.f)),
							          FontID::MAIN, CLAY_TEXT_WRAP_WORDS,
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
										   .childGap = udpi(4.f),
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
							draw_text("Ant:"_v, theme()->onSurface,
							          static_cast<uint16_t>(udpi(12.f)));
						}

						CLAY(CLAY_ID("AntTextCol"),
						     {
								   .layout =
										 {
											   .sizing = {CLAY_SIZING_GROW(0),
						                                  CLAY_SIZING_FIT(0)},
										 },
							 }) {
							auto ant_text =
								  StrBuilder(word_payload.antonyms)
										.join(ctx->arena_frame, ", "_v);
							draw_text(ant_text, theme()->onSurfaceContainer,
							          static_cast<uint16_t>(udpi(12.f)),
							          FontID::MAIN, CLAY_TEXT_WRAP_WORDS,
							          CLAY_TEXT_ALIGN_LEFT);
						}
					}
				}
			}
		}
		if (word_payload.etymology) {
			CLAY(CLAY_ID("EtymologyCard"),
			     {
					   .layout =
							 {
								   .sizing = {CLAY_SIZING_GROW(0),
			                                  CLAY_SIZING_FIT(0)},
								   .padding =
										 {
											   .left = udpi(10.f),
											   .right = udpi(10.f),
											   .top = udpi(8.f),
											   .bottom = udpi(8.f),
										 },
								   .childGap = udpi(3.f),
								   .layoutDirection = CLAY_TOP_TO_BOTTOM,
							 },
					   .backgroundColor = theme()->surfaceContainer,
					   .cornerRadius = CLAY_CORNER_RADIUS(dpi(8.f)),
				 }) {
				draw_text("Origin"_v, theme()->secondary,
				          static_cast<uint16_t>(udpi(11.f)));

				draw_text(word_payload.etymology, theme()->onSurfaceContainer,
				          static_cast<uint16_t>(udpi(13.f)), FontID::MAIN,
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
						   .padding =
								 {
									   .left = udpi(16.f),
									   .right = udpi(16.f),
									   .top = udpi(14.f),
									   .bottom = udpi(14.f),
								 },
						   .childGap = udpi(10.f),
						   .childAlignment = {CLAY_ALIGN_X_LEFT,
	                                          CLAY_ALIGN_Y_TOP},
						   .layoutDirection = CLAY_TOP_TO_BOTTOM,
					 },
			   .backgroundColor = theme()->surfaceContainerLow,
			   .cornerRadius = CLAY_CORNER_RADIUS(dpi(16.f)),
			   .border =
					 {
						   .color = theme()->outline,
						   .width = {udpi(1.f), udpi(1.f), udpi(1.f),
	                                 udpi(1.f)},
					 },
		 }) {

		CLAY(CLAY_ID("LearningHeaderRow"),
		     {
				   .layout =
						 {
							   .sizing = {CLAY_SIZING_GROW(0),
		                                  CLAY_SIZING_FIT(0)},
							   .childGap = udpi(8.f),
							   .childAlignment = {CLAY_ALIGN_X_LEFT,
		                                          CLAY_ALIGN_Y_CENTER},
							   .layoutDirection = CLAY_LEFT_TO_RIGHT,
						 },
			 }) {
			draw_text(mode_name(s.mode), theme()->onSurface,
			          static_cast<uint16_t>(udpi(15.f)));

			draw_text(StrBuilder::concat(ctx->arena_frame, "("_v,
			                             StrView::from_number(ctx->arena_frame,
			                                                  current_step + 1),
			                             "/4)"_v),
			          theme()->onSurfaceContainer,
			          static_cast<uint16_t>(udpi(13.f)));

			StrView due_str{};
			Clay_Color due_bg{};
			Clay_Color due_fg{};

			if (is_new) {
				due_str = "New"_v;
				due_bg = theme()->surfaceContainer;
				due_fg = theme()->onSurfaceContainer;
			} else if (is_overdue) {
				due_str = "Ready to review"_v;
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
								   .padding = {udpi(8.f), udpi(8.f), udpi(3.f),
			                                   udpi(3.f)},
							 },
					   .backgroundColor = due_bg,
					   .cornerRadius = CLAY_CORNER_RADIUS(dpi(6.f)),
				 }) {
				draw_text(due_str, due_fg, static_cast<uint16_t>(udpi(11.f)));
			}
		}

		CLAY(CLAY_ID("ProgressBar"),
		     {
				   .layout =
						 {
							   .sizing = {CLAY_SIZING_GROW(0),
		                                  CLAY_SIZING_FIT(0)},
							   .childGap = udpi(4.f),
							   .layoutDirection = CLAY_LEFT_TO_RIGHT,
						 },
			 }) {
			for (int i = 0; i < 4; ++i) {
				const bool is_filled = (i <= current_step);
				CLAY(CLAY_IDI("ProgressStep", i),
				     {
						   .layout =
								 {
									   .sizing = {CLAY_SIZING_GROW(0),
				                                  CLAY_SIZING_FIXED(dpi(5.f))},
								 },
						   .backgroundColor =
								 is_filled ? theme()->secondary
										   : theme()->surfaceContainerHigh,
						   .cornerRadius = CLAY_CORNER_RADIUS(dpi(3.f)),
					 }) {}
			}
		}

		if (s.mode < Engine::Mode::Compose) {
			const auto left =
				  successful_reviews_to_next_mode(ctx->arena_frame, s);
			draw_text(StrBuilder::concat(ctx->arena_frame, "Next level in "_v,
			                             left, " review(s)"_v),
			          theme()->onSurfaceContainer,
			          static_cast<uint16_t>(udpi(12.f)));
		} else {
			draw_text("Mastered (Max level)"_v, theme()->onSurfaceContainer,
			          static_cast<uint16_t>(udpi(12.f)));
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

		if (parse_word_json(ctx->arena_frame, ctx->arena_screen(),
		                    state.word_copy.json_payload, state.word_payload,
		                    parser)) {
			log_word_payload(state.word_payload);
		}
	}
}

void screen_word_view_draw(AppContext *ctx) {
	KLAPPT_PROFILE_SCOPE_N("screen_word_view_draw");
	auto &state = *ctx->word_view_state;

	// Корневой контейнер экрана: вертикальная колонка во весь экран
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

		// =====================================================================
		// 1. Скроллируемая область карточек (занимает всё пространство сверху)
		// =====================================================================
		CLAY(CLAY_ID("WordViewScrollArea"),
		     {
				   .layout =
						 {
							   .sizing = {CLAY_SIZING_GROW(0),
		                                  CLAY_SIZING_GROW(0)},
						 },
			 }) {
			const auto padding = CLAY_PADDING_ALL(udpi(14.f));
			const uint16_t gap = udpi(12.f);

			// Только реальные карточки (1 или 2, без фиктивных Dummy-элементов)
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

			list::vertical_dynamic_rich(ctx, CLAY_ID("WordCardsList"), gap,
			                            padding, count, draw_cards);
		}

		// =====================================================================
		// 2. Фиксированная нижняя панель (Bottom Action Bar)
		// =====================================================================
		CLAY(CLAY_ID("WordViewBottomBar"),
		     {
				   .layout =
						 {
							   .sizing = {CLAY_SIZING_GROW(0),
		                                  CLAY_SIZING_FIT(0)},
							   .padding =
									 {
										   .left = udpi(16.f),
										   .right = udpi(16.f),
										   .top = udpi(10.f),
										   .bottom = udpi(16.f),
									 },
							   .childGap = udpi(16.f),
							   .childAlignment = {CLAY_ALIGN_X_CENTER,
		                                          CLAY_ALIGN_Y_CENTER},
							   .layoutDirection = CLAY_LEFT_TO_RIGHT,
						 },
				   .backgroundColor = theme()->surface,
				   // Тонкая разделительная черта сверху панели
		           // .border =
		           // {
		           //    .color = theme()->outline,
		           //    .width = {0, 0, udpi(1.f), 0},
		           // },
			 }) {

			// Кнопка "Назад"
			auto back_button = mobile_icon_button<false>(
				  ctx, CLAY_ID_LOCAL("BackButton"), Icons::BACK);
			if (back_button.activated()) {
				ctx->pop();
			}

			// Кнопка "Озвучить (TTS)"
#if NEURO
			if (ctx->settings.is_using_tts) {
				auto play = mobile_icon_button<true>(ctx, CLAY_ID("PlayButton"),
				                                     Icons::PLAY);
				if (play.activated()) {
					auto tts_string =
						  word_tts_full(ctx->arena_screen(), state.word_copy);
					run_tts(ctx, tts_string);
				}
			}
#endif // NEURO

			// Кнопка "Сброс поиска и переход к поисковой строке"
			auto back_and_clear_and_focus = mobile_icon_button<false>(
				  ctx, CLAY_ID_LOCAL("BackClearFocusButton"), Icons::ROTATE);
			if (back_and_clear_and_focus.activated()) {
				ctx->words_search.clear();
				ctx->mobile_text_input.activate_text_input = true;
				ctx->pop();
			}
		}
	}
	log_word_payload(state.word_payload);
	SDL_Log("RAW\n" StrView_Fmt "\n",
	        StrView_Arg(state.word_copy.json_payload));
}
