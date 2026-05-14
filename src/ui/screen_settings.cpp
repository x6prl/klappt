#include "screen_helpers.h"
#include "ui/components/switch_button.h"
#include "ui/dpi.h"

void screen_settings_push(AppContext *ctx) { ctx->push(Screen::Settings); }

void screen_settings_draw(AppContext *ctx) {
	CLAY(CLAY_ID("SettingsScreen"),
	     {.layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)},
	                 .padding = CLAY_PADDING_ALL(udpi(16.0f)),
	                 .childGap = udpi(14.0f),
	                 .layoutDirection = CLAY_TOP_TO_BOTTOM}}) {
		auto text_size = udpi(24.f);
		CLAY(CLAY_ID("ThemeSwitchRow"),
		     {.layout =
		            {
						  .sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0)},
						  .padding = CLAY_PADDING_ALL(udpi(4.0f)),
						  .childGap = udpi(14.0f),
						  .childAlignment = {CLAY_ALIGN_X_CENTER,
		                                     CLAY_ALIGN_Y_CENTER},
					},
		      .border = {
					.color = theme()->outline,
					.width = {.bottom = udpi(1.f)},
			  }}) {
			const bool is_dark = theme()->theme == Theme::Dark;
			CLAY(CLAY_ID("LabelTheme"),
			     {
					   .layout =
							 {
								   .sizing = {CLAY_SIZING_GROW(0),
			                                  CLAY_SIZING_FIT(0)},
							 },
				 })
			draw_text("Dark theme"_v, theme()->onSurface, text_size);
			if (switch_button(ctx, CLAY_ID("Switch"), is_dark, 30.f)) {
				auto new_theme = !is_dark ? Theme::Dark : Theme::Light;
				theme_set(new_theme);
				ctx->settings.theme_type = new_theme;
				ctx->settings.save(ctx->tmparena);
				ctx->anim();
			}
		}
		CLAY(CLAY_ID("ExercisesPerRoundRow"),
		     {.layout =
		            {
						  .sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0)},
						  .padding = CLAY_PADDING_ALL(udpi(4.0f)),
						  .childAlignment = {CLAY_ALIGN_X_CENTER,
		                                     CLAY_ALIGN_Y_CENTER},
					},
		      .border = {
					.color = theme()->outline,
					.width = {.bottom = udpi(1.f)},
			  }}) {
			CLAY(CLAY_ID("LabelRoundSize"),
			     {
					   .layout =
							 {
								   .sizing = {CLAY_SIZING_GROW(0),
			                                  CLAY_SIZING_FIT(0)},
							 },
				 }) {
				draw_text("Exercises per round"_v, theme()->onSurface,
				          text_size);
			}

			auto button_style = mobile_button_style_surface_container_high();
			button_style.font_id = FontID::ICONS;
			button_style.background = {};
			button_style.border_width = 0.f;
			auto PLUS = "+"_v;
			auto MINUS = ""_v;
			auto current_value = ctx->settings.exercise_round_size;
			auto current_value_str =
				  StrView::from_number(ctx->tmparena, current_value);
			auto button_minus = mobile_button(ctx, CLAY_ID("MinusButton"),
			                                  MINUS, button_style);
			draw_text(current_value_str, theme()->onSurface, text_size);
			auto button_plus =
				  mobile_button(ctx, CLAY_ID("PlusButton"), PLUS, button_style);

			constexpr auto ROUND_SIZE_MAX = 20;
			if (button_minus.activated() && (current_value > 1)) {
				ctx->settings.exercise_round_size -= 1;
				ctx->settings.save(ctx->tmparena);
			} else if (button_plus.activated() &&
			           (current_value < ROUND_SIZE_MAX)) {
				ctx->settings.exercise_round_size += 1;
				ctx->settings.save(ctx->tmparena);
			}
		}
		CLAY(CLAY_ID("LanguageRow"),
		     {.layout =
		            {
						  .sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0)},
						  .padding = CLAY_PADDING_ALL(udpi(4.0f)),
						  .childAlignment = {CLAY_ALIGN_X_CENTER,
		                                     CLAY_ALIGN_Y_CENTER},
					},
		      .border = {
					.color = theme()->outline,
					.width = {.bottom = udpi(1.f)},
			  }}) {
			CLAY(CLAY_ID("LabelLanguage"),
			     {
					   .layout =
							 {
								   .sizing = {CLAY_SIZING_GROW(0),
			                                  CLAY_SIZING_FIT(0)},
							 },
				 }) {
				draw_text("Language"_v, theme()->onSurface, text_size);
			}
			auto button_unchosen_style =
				  mobile_button_style_surface_container_high();
			button_unchosen_style.padding_x = button_unchosen_style.padding_y =
				  dpi(4.f);
			button_unchosen_style.font_id = FontID::ICONS;
			button_unchosen_style.background = {};
			button_unchosen_style.border_width = 0.f;
			auto button_chosen_style = button_unchosen_style;
			button_chosen_style.border_width = dpi(1.f);
			button_chosen_style.border = theme()->outline;
			auto current_lang = ctx->settings.tr_language;
			Settings::for_every_lang(
				  [&](int i, Settings::TranslationLanguage lang) {
					  auto style = lang == current_lang ? button_chosen_style
				                                        : button_unchosen_style;
					  auto btn = mobile_button(
							ctx, CLAY_IDI("LangButton", i),
							Settings::translation_language_code(lang), style);
					  if (btn.activated()) {
						  ctx->settings.tr_language = lang;
						  ctx->settings.save(ctx->tmparena);
						  ctx->app_status.push_error("language changed. TODO: notify user about exit"_v);
						  ctx->app_status.set_exit_normal();
					  }
				  });
		}
	}
}
