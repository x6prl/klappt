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
#include "ui/dpi.h"
#include "ui/themes.h"

#include "screen_helpers.h"
#include "ui/translations/langs.h"

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
	const auto label_size = static_cast<uint16_t>(udpi(15.5f));
	const auto sub_size = static_cast<uint16_t>(udpi(12.f));

	CLAY(id,
	     {
			   .layout =
					 {
						   .sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0)},
						   .padding = CLAY_PADDING_ALL(udpi(14.0f)),
						   .childGap = udpi(12.0f),
						   .childAlignment = {CLAY_ALIGN_X_CENTER,
	                                          CLAY_ALIGN_Y_CENTER},
						   .layoutDirection = CLAY_LEFT_TO_RIGHT,
					 },
			   .backgroundColor = theme()->surfaceContainer,
			   .cornerRadius = CLAY_CORNER_RADIUS(dpi(16.f)),
		 }) {
		CLAY(CLAY_IDI("LabelCol", id.id),
		     {
				   .layout =
						 {
							   .sizing = {CLAY_SIZING_GROW(0),
		                                  CLAY_SIZING_FIT(0)},
							   .childGap = udpi(3.0f),
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

		if (switch_button(ctx, CLAY_IDI("Switch", id.id), is_turned_on, 30.f)) {
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
		  ctx, "Main dictionary"_v, AssetsDL::Type::XAPIAN_TR,
		  [](AppContext *_) { return true; });
}

static bool run_download_and_unpack_optional_assets(AppContext *ctx) {
	auto ret = true;
	ret = ret &&
	      check_and_run_download_and_unpack(
				ctx, "German Wiktionary"_v, AssetsDL::Type::OPTIONAL_XAPIAN_DE,
				[](AppContext *ctx) { return ctx->settings.is_using_also_de; });
	// ret = ret && check_and_run_download_and_unpack(
	// 				   ctx, "English Wiktionary"_v,
	// 				   AssetsDL::Type::OPTIONAL_XAPIAN_EN, [](AppContext *ctx) {
	// 					   return ctx->settings.is_using_also_subdict_en;
	// 				   });
#if NEURO
	ret = ret &&
	      check_and_run_download_and_unpack(
				ctx, "Text-to-speech"_v, AssetsDL::Type::OPTIONAL_TTS,
				[](AppContext *ctx) { return ctx->settings.is_using_tts; });
	ret = ret &&
	      check_and_run_download_and_unpack(
				ctx, "Voice recognition"_v, AssetsDL::Type::OPTIONAL_ASR,
				[](AppContext *ctx) { return ctx->settings.is_using_asr; });
#endif
	return ret;
}

static bool are_all_selected_assets_fully_unpacked(AppContext *ctx) {
	using AType = AssetsDL::Type;
	auto &s = ctx->settings;

	if (!s.asset(AType::XAPIAN_TR).is_unpacked) {
		return false;
	}

	// Опциональные ассеты
	if (s.is_using_also_de && !s.asset(AType::OPTIONAL_XAPIAN_DE).is_unpacked) {
		return false;
	}
#if NEURO
	if (s.is_using_tts && !s.asset(AType::OPTIONAL_TTS).is_unpacked) {
		return false;
	}
	if (s.is_using_asr && !s.asset(AType::OPTIONAL_ASR).is_unpacked) {
		return false;
	}
#endif

	return true;
}

static void draw_segmented_progress_bar(AppContext *ctx, int current_step,
                                        int total_steps) {
	CLAY(CLAY_ID("OnboardingProgressBar"),
	     {
			   .layout =
					 {
						   .sizing = {CLAY_SIZING_GROW(0),
	                                  CLAY_SIZING_FIXED(dpi(4.0f))},
						   .childGap = udpi(6.0f),
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
					   .cornerRadius = CLAY_CORNER_RADIUS(dpi(2.0f)),
				 }) {}
		}
	}
}

// ====================
// onboarding screens
// ====================

static void step_draw_language(AppContext *ctx) {
	const uint16_t title_size = static_cast<uint16_t>(udpi(22.f));

	CLAY(CLAY_ID("StepCardLang"),
	     {
			   .layout =
					 {
						   .sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0)},
						   .padding = CLAY_PADDING_ALL(udpi(24.0f)),
						   .childGap = udpi(20.0f),
						   .childAlignment = {CLAY_ALIGN_X_CENTER,
	                                          CLAY_ALIGN_Y_CENTER},
						   .layoutDirection = CLAY_TOP_TO_BOTTOM,
					 },
			   .backgroundColor = theme()->surfaceContainerLow,
			   .cornerRadius = CLAY_CORNER_RADIUS(dpi(24.f)),
		 }) {

		draw_text("Choose your language"_v, theme()->onSurface, title_size,
		          FontID::MAIN, CLAY_TEXT_WRAP_WORDS, CLAY_TEXT_ALIGN_CENTER);

		CLAY(CLAY_ID("LangOptionsGroup"),
		     {
				   .layout =
						 {
							   .sizing = {CLAY_SIZING_GROW(0),
		                                  CLAY_SIZING_FIT(0)},
							   .childGap = udpi(10.0f),
							   .childAlignment = {CLAY_ALIGN_X_CENTER,
		                                          CLAY_ALIGN_Y_CENTER},
							   .layoutDirection = CLAY_TOP_TO_BOTTOM,
						 },
			 }) {

			Settings::for_every_lang([&](int i, Lang lang) {
				if (lang == lang_ar || lang == lang_tr) {
					// NOTE: only ru and en now...
					return;
				}
				auto btn_style = mobile_button_style_surface_container_high();
				btn_style.font_size = 17.f;
				btn_style.padding_y = dpi(10.f);
				btn_style.font_id =
					  (lang == lang_ar) ? FontID::ARABIC_MAIN : FontID::MAIN;

				auto btn =
					  mobile_button(ctx, CLAY_IDI("LangSelectBtn", i),
				                    get_language_display_name(lang), btn_style);
				if (btn.activated()) {
					ctx->settings.tr_language = lang;
					ctx->settings.save(ctx->arena_frame);
					onboarding_advance(ctx, 1);
				}
			});
		}
	}
}

static void step_draw_assets(AppContext *ctx) {
	const uint16_t title_size = static_cast<uint16_t>(udpi(22.f));

	CLAY(CLAY_ID("StepCardAssets"),
	     {
			   .layout =
					 {
						   .sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0)},
						   .padding = CLAY_PADDING_ALL(udpi(20.0f)),
						   .childGap = udpi(16.0f),
						   .childAlignment = {CLAY_ALIGN_X_CENTER,
	                                          CLAY_ALIGN_Y_CENTER},
						   .layoutDirection = CLAY_TOP_TO_BOTTOM,
					 },
			   .backgroundColor = theme()->surfaceContainerLow,
			   .cornerRadius = CLAY_CORNER_RADIUS(dpi(24.f)),
		 }) {

		if (ctx->downloads.is_empty()) {
			draw_text("Ready to download dictionary?"_v, theme()->onSurface,
			          title_size, FontID::MAIN, CLAY_TEXT_WRAP_WORDS,
			          CLAY_TEXT_ALIGN_CENTER);

			auto sub_col = theme()->onSurface;
			sub_col.a = static_cast<uint8_t>(sub_col.a * 0.65f);
			draw_text(
				  "The offline dictionary will be installed on your device for fast lookup without internet."_v,
				  sub_col, static_cast<uint16_t>(udpi(13.f)), FontID::MAIN,
				  CLAY_TEXT_WRAP_WORDS, CLAY_TEXT_ALIGN_CENTER);

			auto dlbtn = mobile_button(ctx, CLAY_ID("DLStartBtn"),
			                           "Download & Continue"_v,
			                           mobile_button_style_primary());
			if (dlbtn.activated()) {
				(void)run_download_and_unpack_tr_asset(ctx);
			}
		} else {
			// ctx->settings.is_using_also_de = false;
			draw_option_row(
				  ctx, CLAY_ID("DEDictOpt"), "German glossary"_v,
				  "Can be useful, if you can understand a little bit of German. ~110MB"_v,
				  ctx->settings.is_using_also_de, [ctx](bool val) {
					  ctx->settings.is_using_also_de = val;
					  ctx->settings.save(ctx->arena_frame);
				  });
			if (ctx->settings.tr_language != lang_en) {
				draw_option_row(
					  ctx, CLAY_ID("ENDictOpt"), "English dictionary"_v,
					  "English dictionary contains more words. It can be usefull, if you can read English. ~70MB"_v,
					  ctx->settings.is_using_also_subdict_en, [ctx](bool val) {
						  ctx->settings.is_using_also_subdict_en = val;
						  ctx->settings.save(ctx->arena_frame);
					  });
			} else {
				// ctx->settings.is_using_also_subdict_en = false;
			}

#if NEURO
			draw_option_row(
				  ctx, CLAY_ID("TTSOpt"), "Text-to-speech"_v,
				  "Offline pronunciation generation for words and phrases. ~80MB"_v,
				  ctx->settings.is_using_tts, [ctx](bool val) {
					  ctx->settings.is_using_tts = val;
					  ctx->settings.save(ctx->arena_frame);
				  });

			draw_option_row(ctx, CLAY_ID("ASROpt"), "Speech recognition"_v,
			                "Voice input training model. ~160MB"_v,
			                ctx->settings.is_using_asr, [ctx](bool val) {
								ctx->settings.is_using_asr = val;
								ctx->settings.save(ctx->arena_frame);
							});
#endif

			auto next_btn =
				  mobile_button(ctx, CLAY_ID("AssetsNextBtn"), "Next"_v,
			                    mobile_button_style_primary());
			if (next_btn.activated()) {
				run_download_and_unpack_optional_assets(ctx);
				onboarding_advance(ctx, 1);
			}
		}
	}
}

static void step_draw_default_screen(AppContext *ctx) {
	const uint16_t title_size = static_cast<uint16_t>(udpi(22.f));

	CLAY(CLAY_ID("StepCardDefaultScreen"),
	     {
			   .layout =
					 {
						   .sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0)},
						   .padding = CLAY_PADDING_ALL(udpi(24.0f)),
						   .childGap = udpi(20.0f),
						   .childAlignment = {CLAY_ALIGN_X_CENTER,
	                                          CLAY_ALIGN_Y_CENTER},
						   .layoutDirection = CLAY_TOP_TO_BOTTOM,
					 },
			   .backgroundColor = theme()->surfaceContainerLow,
			   .cornerRadius = CLAY_CORNER_RADIUS(dpi(24.f)),
		 }) {

		draw_text("What would you like to open on launch?"_v,
		          theme()->onSurface, title_size, FontID::MAIN,
		          CLAY_TEXT_WRAP_WORDS, CLAY_TEXT_ALIGN_CENTER);

		Arr<Pair<Screen, StrView>, 2> options{{
			  {Screen::Trainer, "Word Trainer"_v},
			  {Screen::Dictionary, "Dictionary & Search"_v},
		}};

		int counter = 0;
		for (auto &[screen, label] : options) {
			auto b_style = mobile_button_style_surface_container_high();
			b_style.font_size = 16.f;
			b_style.padding_y = dpi(10.f);

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
			run_download_and_unpack_optional_assets(ctx);
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
						   .padding = CLAY_PADDING_ALL(udpi(20.0f)),
						   .childGap = udpi(14.0f),
						   .childAlignment = {CLAY_ALIGN_X_CENTER,
	                                          CLAY_ALIGN_Y_CENTER},
						   .layoutDirection = CLAY_TOP_TO_BOTTOM,
					 },
			   .backgroundColor = theme()->surfaceContainerLow,
			   .cornerRadius = CLAY_CORNER_RADIUS(dpi(24.f)),
		 }) {

		draw_text("Preparing resources…"_v, theme()->onSurface,
		          static_cast<uint16_t>(udpi(20.f)), FontID::MAIN,
		          CLAY_TEXT_WRAP_WORDS, CLAY_TEXT_ALIGN_CENTER);

		auto sub_col = theme()->onSurface;
		sub_col.a = static_cast<uint8_t>(sub_col.a * 0.6f);
		draw_text("Downloading and setting up offline dictionary"_v, sub_col,
		          static_cast<uint16_t>(udpi(13.f)), FontID::MAIN,
		          CLAY_TEXT_WRAP_WORDS, CLAY_TEXT_ALIGN_CENTER);

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
		ctx->app_status.push_error("Failed to init runtime data"_v);
	}
	if (ctx->words && ctx->words->size == 0 &&
	    !seed_default_learning_list(*ctx)) {
		ctx->app_status.push_error("Seeding default learning list failed"_v);
	}

	screen_trainer_go(ctx);
}

struct OnboardingStepDescriptor {
	void (*draw)(AppContext *ctx);
	bool is_interactive_step{true};
};

static constexpr Arr<OnboardingStepDescriptor, 4> ONBOARDING_STEPS = {{
	  {step_draw_language, true},
	  {step_draw_assets, true},
	  {step_draw_default_screen, true},
	  {step_draw_downloading_and_setup, false},
}};

static constexpr int TOTAL_INTERACTIVE_STEPS = 3;

} // namespace

void screen_onboarding_go(AppContext *ctx) { ctx->go(Screen::Onboarding); }

void screen_onboarding_draw(AppContext *const ctx) {
	const int stage = ctx->settings.onboarding_stage;
	const auto padding = udpi(20.0f);

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
						   .padding = CLAY_PADDING_ALL(padding),
						   .childGap = udpi(16.0f),
						   .childAlignment = {CLAY_ALIGN_X_CENTER,
	                                          CLAY_ALIGN_Y_TOP},
						   .layoutDirection = CLAY_TOP_TO_BOTTOM,
					 },
			   .backgroundColor = theme()->surface,
		 }) {

		if (current_step.is_interactive_step) {
			CLAY(CLAY_ID("OnboardingTopBar"),
			     {
					   .layout =
							 {
								   .sizing = {CLAY_SIZING_GROW(0),
			                                  CLAY_SIZING_FIXED(dpi(40.0f))},
								   .childGap = udpi(12.0f),
								   .childAlignment = {CLAY_ALIGN_X_CENTER,
			                                          CLAY_ALIGN_Y_CENTER},
								   .layoutDirection = CLAY_LEFT_TO_RIGHT,
							 },
				 }) {

				if (stage > 0) {
					auto back_btn = mobile_icon_button<false>(
						  ctx, CLAY_ID("OnboardingBackBtn"), Icons::BACK);
					if (back_btn.activated()) {
						onboarding_advance(ctx, -1);
					}
				} else {
					CLAY(CLAY_ID("BackSpacer"),
					     {
							   .layout =
									 {
										   .sizing = {CLAY_SIZING_FIXED(
															dpi(40.0f)),
					                                  CLAY_SIZING_FIXED(
															dpi(40.0f))},
									 },
						 }) {}
				}

				draw_segmented_progress_bar(ctx, stage,
				                            TOTAL_INTERACTIVE_STEPS);

				CLAY(CLAY_ID("RightSpacer"),
				     {
						   .layout =
								 {
									   .sizing = {CLAY_SIZING_FIXED(dpi(40.0f)),
				                                  CLAY_SIZING_FIXED(
														dpi(40.0f))},
								 },
					 }) {}
			}
		}

		CLAY(CLAY_ID("OnboardingStepCenterWrapper"),
		     {
				   .layout =
						 {
							   .sizing = {CLAY_SIZING_GROW(0),
		                                  CLAY_SIZING_GROW(0)},
							   .childAlignment = {CLAY_ALIGN_X_CENTER,
		                                          CLAY_ALIGN_Y_CENTER},
							   .layoutDirection = CLAY_TOP_TO_BOTTOM,
						 },
			 }) {

			current_step.draw(ctx);
		}
	}
}
