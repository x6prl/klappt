#include "screen_helpers.h"

#include "app/app_context.h"
#include "app/words_init.h"
#include "app/worker.h"
#include "base/arr.h"
#include "base/str_view.h"
#include "domain/settings.h"
#include "platform/net_worker.h"

#include "ui/components/button.h"
#include "ui/components/net_download_row.h"
#include "ui/components/switch_button.h"

#include "ui/trs.h"

namespace {

static inline void onboarding_advance(AppContext *ctx, int delta = 1) {
	ctx->settings.onboarding_stage += delta;
	ctx->settings.save(ctx->arena_frame);
	ctx->push_one_frame();
}

static inline StrView get_language_display_name(Lang lang) {
	switch (lang) {
	case lang_en:
		return "English"_v;
	case lang_ru:
		return "Русский"_v;
	case lang_tr:
		return "Türkçe"_v;
	case lang_ar:
		return "العربية"_v;
	default:
		return lang_code(lang);
	}
}

static void draw_option_row(AppContext *ctx, Clay_ElementId id, StrView label,
                            StrView sub_text, bool is_turned_on,
                            auto on_switched) {
	const uint16_t label_size = sizes()->font.body_md;
	const uint16_t sub_size = sizes()->font.label_md;

	CLAY(id,
	     {
			   .layout =
					 {
						   .sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0)},
						   .padding = {sizes()->space.md, sizes()->space.md,
	                                   sizes()->space.xs, sizes()->space.xs},
						   .childGap = sizes()->space.md,
						   .childAlignment = {CLAY_ALIGN_X_CENTER,
	                                          CLAY_ALIGN_Y_CENTER},
						   .layoutDirection = CLAY_LEFT_TO_RIGHT,
					 },
			   .backgroundColor = theme()->surfaceContainer,
			   .cornerRadius = sizes()->radius.lg,
		 }) {
		CLAY(CLAY_IDI("LabelCol", id.id),
		     {
				   .layout =
						 {
							   .sizing = {CLAY_SIZING_GROW(0),
		                                  CLAY_SIZING_FIT(0)},
							   .childGap = sizes()->space.xs,
							   .layoutDirection = CLAY_TOP_TO_BOTTOM,
						 },
			 }) {
			draw_text(label, theme()->onSurface, label_size,
			          translation_font_id(ctx), CLAY_TEXT_WRAP_WORDS,
			          CLAY_TEXT_ALIGN_LEFT);

			auto sub_col = theme()->onSurface;
			sub_col.a = static_cast<uint8_t>(sub_col.a * 0.65f);
			draw_text(sub_text, sub_col, sub_size, translation_font_id(ctx),
			          CLAY_TEXT_WRAP_WORDS, CLAY_TEXT_ALIGN_LEFT);
		}

		if (switch_button(ctx, CLAY_IDI("Switch", id.id), is_turned_on)) {
			on_switched(!is_turned_on);
			ctx->push_one_frame();
		}
	}
}

static bool check_and_run_download_and_unpack(AppContext *ctx, StrView label,
                                              AssetsDL::Type t,
                                              auto should_be_downloaded_f) {
	auto &s = ctx->settings;
	auto &r = s.asset(t);
	if (should_be_downloaded_f(ctx) && !r.is_unpacked && !r.is_zip_removed) {
		auto pool_index = Worker::net_download_and_unpack_asset(ctx, t);
		if (pool_index >= 0) {
			download_track(ctx, pool_index, label);
		} else {
			if (pool_index == -2)
				return false;
		}
	}
	return true;
}

static bool run_download_and_unpack_tr_asset(AppContext *ctx) {
	return check_and_run_download_and_unpack(
		  ctx, tr()->screen_onboarding_asset_main_dict,
		  AssetsDL::Type::XAPIAN_TR, [](AppContext *ctx) {
			  (void)ctx;
			  return true;
		  });
}

#if NEURO
static bool run_download_and_unpack_optional_assets(AppContext *ctx) {
	auto ret = true;
	ret = ret && check_and_run_download_and_unpack(
					   ctx, tr()->screen_onboarding_asset_tts,
					   AssetsDL::Type::OPTIONAL_TTS, [](AppContext *ctx) {
						   return ctx->settings.is_module_tts;
					   });
	ret = ret && check_and_run_download_and_unpack(
					   ctx, tr()->screen_onboarding_asset_asr,
					   AssetsDL::Type::OPTIONAL_ASR, [](AppContext *ctx) {
						   return ctx->settings.is_module_asr;
					   });
	return ret;
}

static bool are_all_selected_assets_fully_unpacked(AppContext *ctx) {
	using AType = AssetsDL::Type;
	auto &s = ctx->settings;

	if (!s.asset(AType::XAPIAN_TR).is_unpacked) {
		return false;
	}
	if (s.is_module_tts && !s.asset(AType::OPTIONAL_TTS).is_unpacked) {
		return false;
	}
	if (s.is_module_asr && !s.asset(AType::OPTIONAL_ASR).is_unpacked) {
		return false;
	}
	return true;
}
#else
static bool are_all_selected_assets_fully_unpacked(AppContext *ctx) {
	return ctx->settings.asset(AssetsDL::Type::XAPIAN_TR).is_unpacked;
}
#endif

static void draw_segmented_progress_bar(AppContext *ctx, int current_step,
                                        int total_steps) {
	(void)ctx;
	CLAY(CLAY_ID("OnboardingProgressBar"),
	     {
			   .layout =
					 {
						   .sizing = {CLAY_SIZING_GROW(0),
	                                  CLAY_SIZING_FIXED(static_cast<float>(
											sizes()->space.xs))},
						   .childGap = sizes()->space.xs,
						   .childAlignment = {CLAY_ALIGN_X_CENTER,
	                                          CLAY_ALIGN_Y_CENTER},
						   .layoutDirection = CLAY_LEFT_TO_RIGHT,
					 },
		 }) {
		for (int i = 0; i < total_steps; ++i) {
			const bool is_filled = (i <= current_step);
			CLAY(CLAY_IDI("ProgressSeg", i),
			     {
					   .layout =
							 {
								   .sizing = {CLAY_SIZING_GROW(0),
			                                  CLAY_SIZING_GROW(0)},
							 },
					   .backgroundColor = is_filled
			                                    ? theme()->primary
			                                    : theme()->surfaceContainerHigh,
					   .cornerRadius = sizes()->radius.xs,
				 }) {}
		}
	}
}

// ====================
// onboarding screens
// ====================

static void step_draw_language(AppContext *ctx) {
	const uint16_t title_size = sizes()->font.title_lg;

	CLAY(CLAY_ID("StepCardLang"),
	     {
			   .layout =
					 {
						   .sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0)},
						   .padding = sizes()->pad.card,
						   .childGap = sizes()->space.lg,
						   .childAlignment = {CLAY_ALIGN_X_CENTER,
	                                          CLAY_ALIGN_Y_TOP},
						   .layoutDirection = CLAY_TOP_TO_BOTTOM,
					 },
			   .backgroundColor = theme()->surfaceContainerLow,
			   .cornerRadius = sizes()->radius.lg,
		 }) {

		draw_text("Choose your language\nВыберите язык"_v, theme()->onSurface,
		          title_size, FontID::MAIN, CLAY_TEXT_WRAP_WORDS,
		          CLAY_TEXT_ALIGN_CENTER);

		CLAY(CLAY_ID("LangOptionsGroup"),
		     {
				   .layout =
						 {
							   .sizing = {CLAY_SIZING_GROW(0),
		                                  CLAY_SIZING_FIT(0)},
							   .childGap = sizes()->space.sm,
							   .childAlignment = {CLAY_ALIGN_X_CENTER,
		                                          CLAY_ALIGN_Y_CENTER},
							   .layoutDirection = CLAY_TOP_TO_BOTTOM,
						 },
			 }) {

			Settings::for_every_lang([&](int i, Lang lang) {
				if (lang == lang_ar || lang == lang_tr) {
					return;
				}
				auto btn_style = mobile_button_style_surface_container_high();
				btn_style.font_size = sizes()->font.body_md;
				btn_style.padding_y = sizes()->space.sm;
				btn_style.font_id =
					  (lang == lang_ar) ? FontID::ARABIC_MAIN : FontID::MAIN;

				auto btn =
					  mobile_button(ctx, CLAY_IDI("LangSelectBtn", i),
				                    get_language_display_name(lang), btn_style);
				if (btn.activated()) {
					set_language(lang);
					ctx->settings.tr_language = lang;
					{ // NOTE: english users need this button, but russians —
						// mostly not
						bool is_eng = lang == lang_en;
						ctx->settings
							  .is_show_dictionary_search_only_translations_button =
							  is_eng;
					}
					ctx->settings.save(ctx->arena_frame);
					onboarding_advance(ctx, 1);
				}
			});
		}

#ifdef __EMSCRIPTEN__
		draw_text(tr()->screen_onboarding_web_version_note, theme()->onSurface,
		          title_size, FontID::MAIN, CLAY_TEXT_WRAP_WORDS,
		          CLAY_TEXT_ALIGN_CENTER);
#endif
	}
}

static void step_draw_main_asset_download(AppContext *ctx) {
	const uint16_t title_size = sizes()->font.title_lg;

	CLAY(CLAY_ID("StepCardMainDownload"),
	     {
			   .layout =
					 {
						   .sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0)},
						   .padding = sizes()->pad.card,
						   .childGap = sizes()->space.md,
						   .childAlignment = {CLAY_ALIGN_X_CENTER,
	                                          CLAY_ALIGN_Y_TOP},
						   .layoutDirection = CLAY_TOP_TO_BOTTOM,
					 },
			   .backgroundColor = theme()->surfaceContainerLow,
			   .cornerRadius = sizes()->radius.lg,
		 }) {

		draw_text(tr()->screen_onboarding_download_dictionary,
		          theme()->onSurface, title_size, FontID::MAIN,
		          CLAY_TEXT_WRAP_WORDS, CLAY_TEXT_ALIGN_CENTER);

		auto sub_col = theme()->onSurface;
		sub_col.a = static_cast<uint8_t>(sub_col.a * 0.65f);
		draw_text(tr()->screen_onboarding_download_dictionary_desc, sub_col,
		          sizes()->font.body_sm, FontID::MAIN, CLAY_TEXT_WRAP_WORDS,
		          CLAY_TEXT_ALIGN_CENTER);

		auto discl_col = theme()->onSurface;
		discl_col.a = static_cast<uint8_t>(discl_col.a * 0.45f);
		draw_text(tr()->screen_settings_about_wiktionary, discl_col,
		          sizes()->font.label_sm, FontID::MAIN, CLAY_TEXT_WRAP_WORDS,
		          CLAY_TEXT_ALIGN_CENTER);

		auto dlbtn = mobile_button(ctx, CLAY_ID("DLStartBtn"),
		                           tr()->screen_onboarding_action_download,
		                           mobile_button_style_primary());
		if (dlbtn.activated()) {
			(void)run_download_and_unpack_tr_asset(ctx);
			onboarding_advance(ctx, 1);
		}
	}
}

#if NEURO
static void step_draw_optional_assets(AppContext *ctx) {
	const uint16_t title_size = sizes()->font.title_lg;

	CLAY(CLAY_ID("StepCardOptionalAssets"),
	     {
			   .layout =
					 {
						   .sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0)},
						   .padding = sizes()->pad.card,
						   .childGap = sizes()->space.md,
						   .childAlignment = {CLAY_ALIGN_X_CENTER,
	                                          CLAY_ALIGN_Y_TOP},
						   .layoutDirection = CLAY_TOP_TO_BOTTOM,
					 },
			   .backgroundColor = theme()->surfaceContainerLow,
			   .cornerRadius = sizes()->radius.lg,
		 }) {

		draw_text(tr()->screen_onboarding_optional_resources,
		          theme()->onSurface, title_size, FontID::MAIN,
		          CLAY_TEXT_WRAP_WORDS, CLAY_TEXT_ALIGN_CENTER);

		draw_option_row(ctx, CLAY_ID("TTSOpt"),
		                tr()->screen_onboarding_tts_label,
		                tr()->screen_onboarding_tts_desc,
		                ctx->settings.is_module_tts, [ctx](bool val) {
							ctx->settings.is_module_tts = val;
							ctx->settings.save(ctx->arena_frame);
						});

		draw_option_row(ctx, CLAY_ID("ASROpt"),
		                tr()->screen_onboarding_asr_label,
		                tr()->screen_onboarding_asr_desc,
		                ctx->settings.is_module_asr, [ctx](bool val) {
							ctx->settings.is_module_asr = val;
							ctx->settings.save(ctx->arena_frame);
						});

		auto next_btn = mobile_button(ctx, CLAY_ID("AssetsNextBtn"),
		                              tr()->screen_onboarding_action_continue,
		                              mobile_button_style_primary());
		if (next_btn.activated()) {
			run_download_and_unpack_optional_assets(ctx);
			onboarding_advance(ctx, 1);
		}
	}
}
#endif

static void step_draw_card_display_settings(AppContext *ctx) {
	const uint16_t title_size = sizes()->font.title_lg;

	CLAY(CLAY_ID("StepCardWordCardStyle"),
	     {
			   .layout =
					 {
						   .sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0)},
						   .padding = sizes()->pad.card,
						   .childGap = sizes()->space.sm,
						   .childAlignment = {CLAY_ALIGN_X_CENTER,
	                                          CLAY_ALIGN_Y_TOP},
						   .layoutDirection = CLAY_TOP_TO_BOTTOM,
					 },
			   .backgroundColor = theme()->surfaceContainerLow,
			   .cornerRadius = sizes()->radius.lg,
		 }) {

		draw_text(tr()->screen_onboarding_card_style_title, theme()->onSurface,
		          title_size, FontID::MAIN, CLAY_TEXT_WRAP_WORDS,
		          CLAY_TEXT_ALIGN_CENTER);

		CLAY(CLAY_ID("ListPreviewBox"),
		     {
				   .layout =
						 {
							   .sizing = {CLAY_SIZING_GROW(0),
		                                  CLAY_SIZING_FIT(0)},
							   .padding = CLAY_PADDING_ALL(sizes()->space.xs),
							   .childGap = sizes()->space.xs,
							   .layoutDirection = CLAY_TOP_TO_BOTTOM,
						 },
				   .backgroundColor = theme()->surfaceContainerHigh,
				   .cornerRadius = sizes()->radius.md,
			 }) {

			auto draw_preview_card = [&](Clay_ElementId row_id, StrView pre,
			                             StrView main, StrView post) {
				CLAY(row_id,
				     {
						   .layout =
								 {
									   .sizing = {CLAY_SIZING_GROW(0),
				                                  CLAY_SIZING_FIT(0)},
									   .padding = {sizes()->space.md,
				                                   sizes()->space.md,
				                                   sizes()->space.xs,
				                                   sizes()->space.xs},
									   .childGap = sizes()->space.xs,
									   .childAlignment = {CLAY_ALIGN_X_LEFT,
				                                          CLAY_ALIGN_Y_CENTER},
									   .layoutDirection = CLAY_LEFT_TO_RIGHT,
								 },
						   .backgroundColor = theme()->surfaceContainer,
						   .cornerRadius = sizes()->radius.sm,
					 }) {
					auto pcolor = theme()->onSurfaceContainer;
					pcolor.a = static_cast<uint8_t>(pcolor.a * 0.5f);

					if (pre) {
						draw_text(pre, pcolor, sizes()->font.body_md,
						          FontID::MAIN);
					}
					draw_text(main, theme()->onSurfaceContainer,
					          sizes()->font.body_md, FontID::MAIN,
					          CLAY_TEXT_WRAP_WORDS, CLAY_TEXT_ALIGN_LEFT);
					if (post) {
						draw_text(post, pcolor, sizes()->font.body_md,
						          FontID::MAIN);
					}
				}
			};

			draw_preview_card(CLAY_ID("PrevBuch"), "das "_v, "Buch"_v,
			                  " \"-er"_v);

			StrView um_fahren{};
			if (ctx->settings.is_mark_verb_separable_prefix) {
				um_fahren = "um|fahren"_v;
			} else {
				um_fahren = "umfahren"_v;
			}
			StrView um_fahren_post{};
			if (ctx->settings.is_mark_verb_irregular &&
			    ctx->settings.is_mark_verb_aux_sein) {
				um_fahren_post = "!*"_v;
			} else if (ctx->settings.is_mark_verb_irregular) {
				um_fahren_post = "!"_v;
			} else if (ctx->settings.is_mark_verb_aux_sein) {
				um_fahren_post = "*"_v;
			} else if (ctx->settings.is_mark_verb_type) {
				um_fahren_post = "ᵛ"_v;
			}
			draw_preview_card(CLAY_ID("PrevUmFahren"), {}, um_fahren,
			                  um_fahren_post);

			StrView gross_post =
				  ctx->settings.is_mark_adj_type ? "ᵃ"_v : StrView{};
			draw_preview_card(CLAY_ID("PrevGross"), {}, "groß"_v, gross_post);

			StrView machen_post =
				  ctx->settings.is_mark_verb_type ? "ᵛ"_v : StrView{};
			draw_preview_card(CLAY_ID("PrevMachen"), {}, "machen"_v,
			                  machen_post);

			draw_preview_card(CLAY_ID("PrevPhrase"), {},
			                  "Sein oder Nichtsein, das ist hier die Frage"_v,
			                  {});
		}

		draw_option_row(ctx, CLAY_ID("OptMarkIrr"),
		                tr()->screen_onboarding_opt_mark_irr,
		                tr()->screen_onboarding_opt_mark_irr_desc,
		                ctx->settings.is_mark_verb_irregular, [ctx](bool val) {
							ctx->settings.is_mark_verb_irregular = val;
							ctx->settings.save(ctx->arena_frame);
						});

		draw_option_row(ctx, CLAY_ID("OptMarkSepPref"),
		                tr()->screen_onboarding_opt_mark_sep_pref,
		                tr()->screen_onboarding_opt_mark_sep_pref_desc,
		                ctx->settings.is_mark_verb_separable_prefix,
		                [ctx](bool val) {
							ctx->settings.is_mark_verb_separable_prefix = val;
							ctx->settings.save(ctx->arena_frame);
						});

		draw_option_row(ctx, CLAY_ID("OptMarkSein"), "Mark 'sein' verbs"_v,
		                "Shows * for verbs using 'sein' as auxiliary"_v,
		                ctx->settings.is_mark_verb_aux_sein, [ctx](bool val) {
							ctx->settings.is_mark_verb_aux_sein = val;
							ctx->settings.save(ctx->arena_frame);
						});

		draw_option_row(ctx, CLAY_ID("OptMarkVerbTag"),
		                tr()->screen_onboarding_opt_mark_verb_tag,
		                tr()->screen_onboarding_opt_mark_verb_tag_desc,
		                ctx->settings.is_mark_verb_type, [ctx](bool val) {
							ctx->settings.is_mark_verb_type = val;
							ctx->settings.save(ctx->arena_frame);
						});

		draw_option_row(ctx, CLAY_ID("OptMarkAdjTag"),
		                tr()->screen_onboarding_opt_mark_adj_tag,
		                tr()->screen_onboarding_opt_mark_adj_tag_desc,
		                ctx->settings.is_mark_adj_type, [ctx](bool val) {
							ctx->settings.is_mark_adj_type = val;
							ctx->settings.save(ctx->arena_frame);
						});

		auto hint_col = theme()->onSurface;
		hint_col.a = static_cast<uint8_t>(hint_col.a * 0.45f);
		draw_text(tr()->screen_onboarding_settings_customize_hint, hint_col,
		          sizes()->font.label_sm, FontID::MAIN, CLAY_TEXT_WRAP_WORDS,
		          CLAY_TEXT_ALIGN_CENTER);

		auto next_btn = mobile_button(ctx, CLAY_ID("CardStyleNextBtn"),
		                              tr()->screen_onboarding_action_continue,
		                              mobile_button_style_primary());
		if (next_btn.activated()) {
			onboarding_advance(ctx, 1);
		}
	}
}

static void step_draw_word_view_settings(AppContext *ctx) {
	const uint16_t title_size = sizes()->font.title_lg;

	CLAY(CLAY_ID("StepCardWordViewStyle"),
	     {
			   .layout =
					 {
						   .sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0)},
						   .padding = sizes()->pad.card,
						   .childGap = sizes()->space.sm,
						   .childAlignment = {CLAY_ALIGN_X_CENTER,
	                                          CLAY_ALIGN_Y_TOP},
						   .layoutDirection = CLAY_TOP_TO_BOTTOM,
					 },
			   .backgroundColor = theme()->surfaceContainerLow,
			   .cornerRadius = sizes()->radius.lg,
		 }) {

		draw_text(tr()->screen_onboarding_word_view_title, theme()->onSurface,
		          title_size, FontID::MAIN, CLAY_TEXT_WRAP_WORDS,
		          CLAY_TEXT_ALIGN_CENTER);

		CLAY(CLAY_ID("ViewPreviewCard"),
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

			CLAY(CLAY_ID("PrevNounTitleRow"),
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
				draw_text("das"_v, theme()->secondary, sizes()->font.title_lg);
				draw_text("Buch"_v, theme()->onSurface, sizes()->font.title_lg);

				if (ctx->settings.is_show_noun_plural_as_suffix) {
					draw_text("\"-er"_v, theme()->secondary,
					          sizes()->font.title_lg);
				}
			}

			if (ctx->settings.is_show_ipa) {
				CLAY(CLAY_ID("PrevIPARow"),
				     {
						   .layout = {.sizing = {CLAY_SIZING_GROW(0),
				                                 CLAY_SIZING_FIT(0)}},
					 }) {
					draw_text("[buːx]"_v, theme()->onSurfaceContainer,
					          sizes()->font.label_md, FontID::MAIN);
				}
			}

			if (!ctx->settings.is_show_noun_plural_as_suffix) {
				CLAY(CLAY_ID("PrevFormsBlock"),
				     {
						   .layout =
								 {
									   .sizing = {CLAY_SIZING_GROW(0),
				                                  CLAY_SIZING_FIT(0)},
									   .padding = {sizes()->space.sm,
				                                   sizes()->space.sm,
				                                   sizes()->space.xs,
				                                   sizes()->space.xs},
									   .childGap = sizes()->space.sm,
									   .childAlignment = {CLAY_ALIGN_X_LEFT,
				                                          CLAY_ALIGN_Y_CENTER},
									   .layoutDirection = CLAY_LEFT_TO_RIGHT,
								 },
						   .backgroundColor = theme()->surfaceContainerHigh,
						   .cornerRadius = sizes()->radius.sm,
					 }) {
					draw_text("Plural:"_v, theme()->onSurfaceContainer,
					          sizes()->font.body_sm);
					draw_text("die Bücher"_v, theme()->onSurface,
					          sizes()->font.body_sm);
				}
			}

			if (ctx->settings.is_show_origin) {
				CLAY(CLAY_ID("PrevOriginBlock"),
				     {
						   .layout =
								 {
									   .sizing = {CLAY_SIZING_GROW(0),
				                                  CLAY_SIZING_FIT(0)},
									   .padding = {sizes()->space.sm,
				                                   sizes()->space.sm,
				                                   sizes()->space.xs,
				                                   sizes()->space.xs},
									   .childGap = sizes()->space.xs,
									   .childAlignment = {CLAY_ALIGN_X_LEFT,
				                                          CLAY_ALIGN_Y_TOP},
									   .layoutDirection = CLAY_TOP_TO_BOTTOM,
								 },
						   .backgroundColor = theme()->surfaceContainerHigh,
						   .cornerRadius = sizes()->radius.sm,
					 }) {
					draw_text("Origin"_v, theme()->secondary,
					          sizes()->font.label_sm);
					draw_text(
						  "From Middle High German būch, from Old High German būh."_v,
						  theme()->onSurfaceContainer, sizes()->font.label_md,
						  FontID::MAIN, CLAY_TEXT_WRAP_WORDS,
						  CLAY_TEXT_ALIGN_LEFT);
				}
			}
		}

		draw_option_row(ctx, CLAY_ID("OptShowIPA"),
		                tr()->screen_onboarding_opt_show_ipa,
		                tr()->screen_onboarding_opt_show_ipa_desc,
		                ctx->settings.is_show_ipa, [ctx](bool val) {
							ctx->settings.is_show_ipa = val;
							ctx->settings.save(ctx->arena_frame);
						});

		draw_option_row(ctx, CLAY_ID("OptPluralSuffix"),
		                tr()->screen_onboarding_opt_plural_header,
		                tr()->screen_onboarding_opt_plural_header_desc,
		                ctx->settings.is_show_noun_plural_as_suffix,
		                [ctx](bool val) {
							ctx->settings.is_show_noun_plural_as_suffix = val;
							ctx->settings.save(ctx->arena_frame);
						});

		draw_option_row(ctx, CLAY_ID("OptShowOrigin"),
		                tr()->screen_onboarding_opt_show_origin,
		                tr()->screen_onboarding_opt_show_origin_desc,
		                ctx->settings.is_show_origin, [ctx](bool val) {
							ctx->settings.is_show_origin = val;
							ctx->settings.save(ctx->arena_frame);
						});

		auto hint_col = theme()->onSurface;
		hint_col.a = static_cast<uint8_t>(hint_col.a * 0.45f);
		draw_text(tr()->screen_onboarding_settings_customize_hint, hint_col,
		          sizes()->font.label_sm, FontID::MAIN, CLAY_TEXT_WRAP_WORDS,
		          CLAY_TEXT_ALIGN_CENTER);

		auto next_btn = mobile_button(ctx, CLAY_ID("ViewStyleNextBtn"),
		                              tr()->screen_onboarding_action_continue,
		                              mobile_button_style_primary());
		if (next_btn.activated()) {
			onboarding_advance(ctx, 1);
		}
	}
}

static void step_draw_default_screen(AppContext *ctx) {
	const uint16_t title_size = sizes()->font.title_lg;

	CLAY(CLAY_ID("StepCardDefaultScreen"),
	     {
			   .layout =
					 {
						   .sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0)},
						   .padding = sizes()->pad.card,
						   .childGap = sizes()->space.lg,
						   .childAlignment = {CLAY_ALIGN_X_CENTER,
	                                          CLAY_ALIGN_Y_TOP},
						   .layoutDirection = CLAY_TOP_TO_BOTTOM,
					 },
			   .backgroundColor = theme()->surfaceContainerLow,
			   .cornerRadius = sizes()->radius.lg,
		 }) {

		draw_text(tr()->screen_onboarding_default_screen_prompt,
		          theme()->onSurface, title_size, FontID::MAIN,
		          CLAY_TEXT_WRAP_WORDS, CLAY_TEXT_ALIGN_CENTER);

		Arr<Pair<Screen, StrView>, 2> options{{
			  {Screen::Trainer, tr()->screen_onboarding_def_screen_trainer},
			  {Screen::Dictionary,
		       tr()->screen_onboarding_def_screen_dictionary},
		}};

		int counter = 0;
		for (auto &[screen, label] : options) {
			auto b_style = mobile_button_style_surface_container_high();
			b_style.font_size = sizes()->font.body_md;
			b_style.padding_y = sizes()->space.sm;

			auto btn =
				  mobile_button(ctx, CLAY_IDI_LOCAL("DefScreenOpt", counter++),
			                    label, b_style);
			if (btn.activated()) {
				ctx->settings.default_screen = std::to_underlying(screen);
				ctx->settings.save(ctx->arena_frame);
				onboarding_advance(ctx, 1);
			}
		}
	}
}

static void step_draw_downloading_and_setup(AppContext *ctx) {
	ctx->anim();

	if (ctx->downloads.is_empty()) {
		if (ctx->net) {
			run_download_and_unpack_tr_asset(ctx);
#if NEURO
			run_download_and_unpack_optional_assets(ctx);
#endif
		}
	} else {
		if (ctx->net) {
			SDL_SignalCondition(ctx->net_worker_job_queue.cond);
		}
	}

	CLAY(CLAY_ID("StepCardDownloading"),
	     {
			   .layout =
					 {
						   .sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0)},
						   .padding = sizes()->pad.card,
						   .childGap = sizes()->space.md,
						   .childAlignment = {CLAY_ALIGN_X_CENTER,
	                                          CLAY_ALIGN_Y_CENTER},
						   .layoutDirection = CLAY_TOP_TO_BOTTOM,
					 },
			   .backgroundColor = theme()->surfaceContainerLow,
			   .cornerRadius = sizes()->radius.lg,
		 }) {

		draw_text(tr()->screen_onboarding_preparing_resources,
		          theme()->onSurface, sizes()->font.title_md, FontID::MAIN,
		          CLAY_TEXT_WRAP_WORDS, CLAY_TEXT_ALIGN_CENTER);

		auto sub_col = theme()->onSurface;
		sub_col.a = static_cast<uint8_t>(sub_col.a * 0.6f);
		draw_text(tr()->screen_onboarding_downloading_setup_desc, sub_col,
		          sizes()->font.body_sm, FontID::MAIN, CLAY_TEXT_WRAP_WORDS,
		          CLAY_TEXT_ALIGN_CENTER);

		for (auto &dl : ctx->downloads) {
			download_row(ctx, dl);
		}

		if (are_all_selected_assets_fully_unpacked(ctx)) {
			onboarding_advance(ctx, 1);
		}
	}
}

static void finish_onboarding_and_start(AppContext *ctx) {
	ctx->settings.onboarding_stage = -1;
	ctx->settings.save(ctx->arena_frame);

	if (!init_runtime_data(*ctx)) {
		SDL_LogError(SDL_LOG_CATEGORY_ERROR, "failed to init runtime data");
		ctx->app_status.push_error(tr()->screen_onboarding_err_init_runtime);
	}
	if (ctx->words && ctx->words->size == 0 &&
	    !seed_default_learning_list(*ctx)) {
		ctx->app_status.push_error(tr()->screen_onboarding_err_seed_words);
	}

	screen_trainer_go(ctx);
}

struct OnboardingStepDescriptor {
	void (*draw)(AppContext *ctx);
	bool is_interactive_step{true};
	int prev_stage{-1};
};

#if NEURO
static constexpr Arr<OnboardingStepDescriptor, 7> ONBOARDING_STEPS = {{
	  {step_draw_language, true, -1},
	  {step_draw_main_asset_download, true, 0},
	  {step_draw_optional_assets, true, -1},
	  {step_draw_card_display_settings, true, -1},
	  {step_draw_word_view_settings, true, 3},
	  {step_draw_default_screen, true, 4},
	  {step_draw_downloading_and_setup, false, -1},
}};
static constexpr int TOTAL_INTERACTIVE_STEPS = 6;
#else
static constexpr Arr<OnboardingStepDescriptor, 6> ONBOARDING_STEPS = {{
	  {step_draw_language, true, -1},
	  {step_draw_main_asset_download, true, 0},
	  {step_draw_card_display_settings, true, -1},
	  {step_draw_word_view_settings, true, 2},
	  {step_draw_default_screen, true, 3},
	  {step_draw_downloading_and_setup, false, -1},
}};
static constexpr int TOTAL_INTERACTIVE_STEPS = 5;
#endif

} // namespace

void screen_onboarding_go(AppContext *ctx) { ctx->go(Screen::Onboarding); }

void screen_onboarding_draw(AppContext *const ctx) {
	const int stage = ctx->settings.onboarding_stage;

	if (stage < 0 || stage >= static_cast<int>(ONBOARDING_STEPS.size())) {
		finish_onboarding_and_start(ctx);
		return;
	}

	const auto &current_step = ONBOARDING_STEPS[stage];

	CLAY(CLAY_ID("OnboardingRoot"),
	     {
			   .layout =
					 {
						   .sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)},
						   .padding = sizes()->pad.screen,
						   .childGap = sizes()->space.xs,
						   .childAlignment = {CLAY_ALIGN_X_CENTER,
	                                          CLAY_ALIGN_Y_TOP},
						   .layoutDirection = CLAY_TOP_TO_BOTTOM,
					 },
			   .backgroundColor = theme()->surface,
		 }) {

		if (current_step.is_interactive_step) {
			const float top_bar_h = sizes()->dim.action_btn_size;

			CLAY(CLAY_ID("OnboardingTopBar"),
			     {
					   .layout =
							 {
								   .sizing = {CLAY_SIZING_GROW(0),
			                                  CLAY_SIZING_FIXED(top_bar_h)},
								   .childGap = sizes()->space.md,
								   .childAlignment = {CLAY_ALIGN_X_CENTER,
			                                          CLAY_ALIGN_Y_CENTER},
								   .layoutDirection = CLAY_LEFT_TO_RIGHT,
							 },
				 }) {

				if (current_step.prev_stage >= 0) {
					auto back_btn = mobile_icon_button<false>(
						  ctx, CLAY_ID("OnboardingBackBtn"), Icons::BACK);
					if (back_btn.activated()) {
						ctx->settings.onboarding_stage =
							  current_step.prev_stage;
						ctx->settings.save(ctx->arena_frame);
						ctx->push_one_frame();
					}
				} else {
					CLAY(CLAY_ID("BackSpacer"),
					     {
							   .layout =
									 {
										   .sizing =
												 {CLAY_SIZING_FIXED(top_bar_h),
					                              CLAY_SIZING_FIXED(top_bar_h)},
									 },
						 }) {}
				}

				draw_segmented_progress_bar(ctx, stage,
				                            TOTAL_INTERACTIVE_STEPS);

				CLAY(CLAY_ID("RightSpacer"),
				     {
						   .layout =
								 {
									   .sizing = {CLAY_SIZING_FIXED(top_bar_h),
				                                  CLAY_SIZING_FIXED(top_bar_h)},
								 },
					 }) {}
			}
		}

		CLAY(CLAY_ID("OnboardingScrollContainer"),
		     {
				   .layout =
						 {
							   .sizing = {CLAY_SIZING_GROW(0),
		                                  CLAY_SIZING_GROW(0)},
							   .padding = {.top = sizes()->space.xs,
		                                   .bottom = sizes()->space.md},
							   .childAlignment = {CLAY_ALIGN_X_CENTER,
		                                          CLAY_ALIGN_Y_CENTER},
							   .layoutDirection = CLAY_TOP_TO_BOTTOM,
						 },
				   .clip =
						 {
							   .vertical = true,
							   .childOffset = Clay_GetScrollOffset(),
						 },
			 }) {

			current_step.draw(ctx);
		}
	}
}
