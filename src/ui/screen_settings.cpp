#include "app/app_context.h"
#include "base/arr.h"
#include "base/str_view.h"
#include "ui/components/button.h"
#include "ui/components/switch_button.h"
#include "ui/dpi.h"
#include "ui/textcache.h"
#include "ui/themes.h"
#include "ui/trs.h"

#include "screen_helpers.h"

void screen_settings_push(AppContext *ctx) { ctx->push(Screen::Settings); }

namespace {

static inline void draw_settings_divider(AppContext *ctx, uint32_t index) {
	CLAY(CLAY_IDI("SettingsDivider", index),
	     {
			   .layout =
					 {
						   .sizing = {CLAY_SIZING_GROW(0),
	                                  CLAY_SIZING_FIXED(dpi(1.0f))},
					 },
			   .backgroundColor = theme()->outline,
		 }) {}
}

} // namespace

void screen_settings_draw(AppContext *ctx) {
	const auto padding = udpi(16.0f);
	const auto row_padding =
		  Clay_Padding{padding, padding, udpi(16.0f), udpi(16.0f)};
	const uint16_t title_font_size = static_cast<uint16_t>(udpi(17.0f));
	const auto trans_font = translation_font_id(ctx);

	CLAY(CLAY_ID("SettingsScreenRoot"),
	     {
			   .layout =
					 {
						   .sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)},
						   .padding = CLAY_PADDING_ALL(0),
						   .childAlignment = {CLAY_ALIGN_X_CENTER,
	                                          CLAY_ALIGN_Y_TOP},
						   .layoutDirection = CLAY_TOP_TO_BOTTOM,
					 },
			   .backgroundColor = theme()->surface,
		 }) {

		CLAY(CLAY_ID("SettingsRowsGroup"),
		     {
				   .layout =
						 {
							   .sizing = {CLAY_SIZING_GROW(0),
		                                  CLAY_SIZING_GROW(0)},
							   .childAlignment = {CLAY_ALIGN_X_CENTER,
		                                          CLAY_ALIGN_Y_TOP},
							   .layoutDirection = CLAY_TOP_TO_BOTTOM,
						 },
			 }) {

			CLAY(CLAY_ID("ThemeRow"),
			     {
					   .layout =
							 {
								   .sizing = {CLAY_SIZING_GROW(0),
			                                  CLAY_SIZING_FIT(0)},
								   .padding = row_padding,
								   .childAlignment = {CLAY_ALIGN_X_CENTER,
			                                          CLAY_ALIGN_Y_CENTER},
								   .layoutDirection = CLAY_LEFT_TO_RIGHT,
							 },
				 }) {
				const bool is_dark = (theme()->theme == Theme::Dark);

				CLAY(CLAY_ID("LabelThemeCol"),
				     {
						   .layout =
								 {
									   .sizing = {CLAY_SIZING_GROW(0),
				                                  CLAY_SIZING_FIT(0)},
								 },
					 }) {
					draw_text(tr()->screen_settings_dark_theme,
					          theme()->onSurface, title_font_size, trans_font);
				}

				if (switch_button(ctx, CLAY_ID("ThemeSwitch"), is_dark, 28.f)) {
					auto new_theme = !is_dark ? Theme::Dark : Theme::Light;
					theme_set(new_theme);
					ctx->settings.theme_type = new_theme;
					ctx->settings.save(ctx->arena_frame);
					ctx->push_one_frame();
				}
			}

			draw_settings_divider(ctx, 0);

			CLAY(CLAY_ID("RoundSizeRow"),
			     {
					   .layout =
							 {
								   .sizing = {CLAY_SIZING_GROW(0),
			                                  CLAY_SIZING_FIT(0)},
								   .padding = row_padding,
								   .childAlignment = {CLAY_ALIGN_X_CENTER,
			                                          CLAY_ALIGN_Y_CENTER},
								   .layoutDirection = CLAY_LEFT_TO_RIGHT,
							 },
				 }) {
				CLAY(CLAY_ID("LabelRoundCol"),
				     {
						   .layout =
								 {
									   .sizing = {CLAY_SIZING_GROW(0),
				                                  CLAY_SIZING_FIT(0)},
								 },
					 }) {
					draw_text(tr()->screen_settings_exercises_per_round,
					          theme()->onSurface, title_font_size, trans_font,
					          CLAY_TEXT_WRAP_WORDS, CLAY_TEXT_ALIGN_LEFT);
				}

				CLAY(CLAY_ID("StepperGroup"),
				     {
						   .layout =
								 {
									   .childGap = udpi(14.f),
									   .childAlignment = {CLAY_ALIGN_X_CENTER,
				                                          CLAY_ALIGN_Y_CENTER},
									   .layoutDirection = CLAY_LEFT_TO_RIGHT,
								 },
					 }) {

					auto btn_style =
						  mobile_button_style_surface_container_high();
					btn_style.background.a = 0.f;
					btn_style.background_pressed.a = 0.f;
					btn_style.font_size = 20.f;
					btn_style.font_id = FontID::MAIN;

					auto current_value = ctx->settings.exercise_round_size;

					auto btn_minus = mobile_button(ctx, CLAY_ID("MinusBtn"),
					                               "−"_v, btn_style);

					draw_text(
						  StrView::from_number(ctx->arena_frame, current_value),
						  theme()->primary, static_cast<uint16_t>(udpi(19.f)),
						  FontID::MAIN);

					auto btn_plus = mobile_button(ctx, CLAY_ID("PlusBtn"),
					                              "+"_v, btn_style);

					constexpr auto ROUND_SIZE_MAX = 50;
					if (btn_minus.activated() && (current_value > 1)) {
						ctx->settings.exercise_round_size -= 1;
						ctx->settings.save(ctx->arena_frame);
					} else if (btn_plus.activated() &&
					           (current_value < ROUND_SIZE_MAX)) {
						ctx->settings.exercise_round_size += 1;
						ctx->settings.save(ctx->arena_frame);
					}
				}
			}

			draw_settings_divider(ctx, 1);

			CLAY(CLAY_ID("DefaultScreenRow"),
			     {
					   .layout =
							 {
								   .sizing = {CLAY_SIZING_GROW(0),
			                                  CLAY_SIZING_FIT(0)},
								   .padding = row_padding,
								   .childAlignment = {CLAY_ALIGN_X_CENTER,
			                                          CLAY_ALIGN_Y_CENTER},
								   .layoutDirection = CLAY_LEFT_TO_RIGHT,
							 },
				 }) {
				CLAY(CLAY_ID("LabelScreenCol"),
				     {
						   .layout =
								 {
									   .sizing = {CLAY_SIZING_GROW(0),
				                                  CLAY_SIZING_FIT(0)},
								 },
					 }) {
					draw_text("Default screen"_v, theme()->onSurface,
					          title_font_size, trans_font);
				}

				CLAY(CLAY_ID("ScreenOptions"),
				     {
						   .layout =
								 {
									   .childGap = udpi(8.f),
									   .childAlignment = {CLAY_ALIGN_X_CENTER,
				                                          CLAY_ALIGN_Y_CENTER},
									   .layoutDirection = CLAY_LEFT_TO_RIGHT,
								 },
					 }) {

					auto current_default =
						  static_cast<Screen>(ctx->settings.default_screen);
					Arr<Pair<Screen, StrView>, 2> options{
						  {{Screen::Dictionary, "Wortschatz"_v},
					       {Screen::Trainer, "Trainer"_v}}};

					int counter = 0;
					for (auto &[screen, label] : options) {
						const bool is_selected = (screen == current_default);

						auto b_style =
							  mobile_button_style_surface_container_high();
						b_style.font_size = 15.f;
						b_style.padding_x = dpi(10.f);
						b_style.padding_y = dpi(6.f);

						if (is_selected) {
							b_style.background = theme()->surfaceContainerHigh;
							b_style.text = theme()->primary;
						} else {
							b_style.background.a = 0.f;
							b_style.text = theme()->onSurfaceContainer;
						}

						auto btn = mobile_button(
							  ctx, CLAY_IDI("ScreenOpt", counter++), label,
							  b_style);
						if (btn.activated()) {
							ctx->settings.default_screen =
								  std::to_underlying(screen);
							ctx->settings.save(ctx->arena_frame);
						}
					}
				}
			}

			draw_settings_divider(ctx, 2);

			CLAY(CLAY_ID("SuggestionsRow"),
			     {
					   .layout =
							 {
								   .sizing = {CLAY_SIZING_GROW(0),
			                                  CLAY_SIZING_FIT(0)},
								   .padding = row_padding,
								   .childAlignment = {CLAY_ALIGN_X_CENTER,
			                                          CLAY_ALIGN_Y_CENTER},
								   .layoutDirection = CLAY_LEFT_TO_RIGHT,
							 },
				 }) {
				const bool is_using_suggestions =
					  (ctx->settings.is_using_suggestions);

				CLAY(CLAY_ID("LabelSuggestionsCol"),
				     {
						   .layout =
								 {
									   .sizing = {CLAY_SIZING_GROW(0),
				                                  CLAY_SIZING_FIT(0)},
								 },
					 }) {
					draw_text("Suggestions"_v,
					          theme()->onSurface, title_font_size, trans_font);
				}

				if (switch_button(ctx, CLAY_ID("SuggestionsSwitch"),
				                  is_using_suggestions, 28.f)) {
					auto new_val = !is_using_suggestions;
					ctx->settings.is_using_suggestions = new_val;
					ctx->settings.save(ctx->arena_frame);
					ctx->push_one_frame();
				}
			}
		}

		CLAY(CLAY_ID("AboutFooter"),
		     {
				   .layout =
						 {
							   .sizing = {CLAY_SIZING_GROW(0),
		                                  CLAY_SIZING_FIT(0)},
							   .padding =
									 {
										   .left = padding,
										   .right = padding,
										   .top = udpi(16.f),
										   .bottom = udpi(20.f),
									 },
							   .childGap = udpi(6.0f),
							   .childAlignment = {CLAY_ALIGN_X_CENTER,
		                                          CLAY_ALIGN_Y_BOTTOM},
							   .layoutDirection = CLAY_TOP_TO_BOTTOM,
						 },
			 }) {

			auto sub_color = theme()->onSurface;
			sub_color.a = static_cast<uint8_t>(sub_color.a * 0.5f);
			const uint16_t note_font_size = static_cast<uint16_t>(udpi(11.0f));

			draw_text(
				  "This product includes data from Wiktionary (https://www.wiktionary.org/) licensed under CC BY-SA 4.0."_v,
				  sub_color, note_font_size, FontID::MAIN, CLAY_TEXT_WRAP_WORDS,
				  CLAY_TEXT_ALIGN_CENTER);

			draw_text(
				  "Extracted and transformed with Wiktextract and custom scripts."_v,
				  sub_color, note_font_size, FontID::MAIN, CLAY_TEXT_WRAP_WORDS,
				  CLAY_TEXT_ALIGN_CENTER);
		}
	}
}
