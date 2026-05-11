#include "app/words_init.h"
#include "screen_helpers.h"
#include "ui/components/button.h"
#include "ui/dpi.h"

#include <filesystem>

void screen_onboarding_go(AppContext *ctx) { ctx->go(Screen::Onboarding); }

void screen_onboarding_draw(AppContext *ctx) {
	static int load_language_data = 0;
	auto text_size = dpi(24.f);
	auto &settings = ctx->settings;
	CLAY(CLAY_ID("OnboardingContainer"),
	     {
			   .layout =
					 {
						   .sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)},
						   .padding = CLAY_PADDING_ALL(udpi(4.0f)),
						   .childGap = udpi(160.f),
						   .childAlignment = {CLAY_ALIGN_X_CENTER,
	                                          CLAY_ALIGN_Y_CENTER},
						   .layoutDirection = CLAY_TOP_TO_BOTTOM,
					 },
		 }) {
		if (load_language_data) {
			if (load_language_data > 1) {

#if __ANDROID__
				std::filesystem::path basePath = "";
#else
				auto basePathPtr = SDL_GetBasePath();
				if (not basePathPtr) {
					exit(-34);
				}
				const std::filesystem::path basePath = basePathPtr;
#endif
				if (!init_words(*ctx, basePath)) {
					exit(-35);
				}
				load_language_data = 0;
				settings.onboarding_stage += 1;
			} else {
				CLAY(CLAY_ID("LanguageLabel"),
				     {
						   .layout =
								 {
									   .sizing = {CLAY_SIZING_GROW(0),
				                                  CLAY_SIZING_FIT(0)},
									   .padding = CLAY_PADDING_ALL(udpi(4.0f)),
									   .childAlignment = {CLAY_ALIGN_X_CENTER,
				                                          CLAY_ALIGN_Y_CENTER},
									   .layoutDirection = CLAY_TOP_TO_BOTTOM,
								 },
					 }) {
					draw_text("Loading words"_v, theme()->onSurface, text_size);
					draw_text("Please wait"_v, theme()->onSurface, text_size);
					load_language_data++;
				}
			}
		} else {
			switch (settings.onboarding_stage) {
			case 0:
				CLAY(CLAY_ID("LanguageLabel"),
				     {
						   .layout =
								 {
									   .sizing = {CLAY_SIZING_GROW(0),
				                                  CLAY_SIZING_FIT(0)},
									   .padding = CLAY_PADDING_ALL(udpi(4.0f)),
									   .childAlignment = {CLAY_ALIGN_X_CENTER,
				                                          CLAY_ALIGN_Y_CENTER},
								 },
					 }) {
					draw_text("Chose language"_v, theme()->onSurface,
					          text_size);
				}

				CLAY(CLAY_ID("LanguageRow"),
				     {
						   .layout =
								 {
									   .sizing = {CLAY_SIZING_GROW(0),
				                                  CLAY_SIZING_FIT(0)},
									   .padding = CLAY_PADDING_ALL(udpi(4.0f)),
									   .childGap = udpi(14.0f),
									   .childAlignment = {CLAY_ALIGN_X_CENTER,
				                                          CLAY_ALIGN_Y_CENTER},
								 },
					 }) {
					auto button_style =
						  mobile_button_style_surface_container_high();
					Settings::for_every_lang(
						  [&](int i, Settings::TranslationLanguage lang) {
							  auto btn = mobile_button(
									ctx, CLAY_IDI("LangButton", i),
									Settings::translation_language_code(lang),
									button_style);
							  if (btn.activated()) {
								  settings.tr_language = lang;
								  settings.save(ctx->tmparena);
								  load_language_data = 1;
							  }
						  });
				}
				break;
			default:
				settings.onboarding_stage = -1;
				settings.save(ctx->tmparena);
				screen_start_go(ctx);
			}
		}
	}
}
