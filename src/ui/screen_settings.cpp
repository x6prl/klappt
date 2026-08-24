#include "app/app_context.h"
#include "base/arr.h"
#include "base/str_view.h"
#include "screen_helpers.h"
#include "ui/components/lists.h"
#include "ui/components/switch_button.h"
#include "ui/dpi.h"
#include "ui/textcache.h"
#include "ui/themes.h"
#include "ui/trs.h"
#include <utility>

void screen_settings_push(AppContext *ctx) { ctx->push(Screen::Settings); }

namespace {

// void draw_spacer
} // namespace

void screen_settings_draw(AppContext *ctx) {
	CLAY(CLAY_ID("SettingsScreen"),
	     {.layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)},
	                 .padding = CLAY_PADDING_ALL(udpi(16.0f)),
	                 // .childGap = udpi(14.0f),
	                 .layoutDirection = CLAY_TOP_TO_BOTTOM}}) {
		auto text_size = udpi(24.f);
		Clay_Padding padding = {udpi(4.0f), udpi(4.0f), udpi(12.0f),
		                        udpi(12.0f)};
		auto settings_item_count = 4;
		list::vertical_dynamic(
			  ctx, CLAY_ID("SettingsScreen"), settings_item_count,
			  [text_size, padding](AppContext *ctx, Size i,
		                           Clay_ElementId item_clay_id) {
				  switch (i) {
				  case 0: // ThemeSwitchRow
					  CLAY(item_clay_id,
				           {.layout =
				                  {
										.sizing = {CLAY_SIZING_GROW(0),
				                                   CLAY_SIZING_FIT(0)},
										.padding = padding,
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
							   }) {
							  draw_text(tr()->screen_settings_dark_theme,
						                theme()->onSurface, text_size,
						                translation_font_id(ctx));
						  }
						  if (switch_button(ctx, CLAY_ID("Switch"), is_dark,
					                        30.f)) {
							  auto new_theme =
									!is_dark ? Theme::Dark : Theme::Light;
							  theme_set(new_theme);
							  ctx->settings.theme_type = new_theme;
							  ctx->settings.save(ctx->arena_frame);
							  ctx->push_one_frame();
						  }
					  }
					  break;
				  case 1: // ExercisesPerRoundRow
					  CLAY(item_clay_id,
				           {.layout =
				                  {
										.sizing = {CLAY_SIZING_GROW(0),
				                                   CLAY_SIZING_FIT(0)},
										.padding = padding,
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
							  draw_text(
									tr()->screen_settings_exercises_per_round,
									theme()->onSurface, text_size,
									translation_font_id(ctx),
									CLAY_TEXT_WRAP_WORDS, CLAY_TEXT_ALIGN_LEFT);
						  }

						  auto button_style =
								mobile_button_style_surface_container_high();
						  button_style.font_id = FontID::ICONS;
						  button_style.background = {};
						  button_style.border_width = 0.f;
						  auto PLUS = "+"_v;
						  auto MINUS = ""_v;
						  auto current_value =
								ctx->settings.exercise_round_size;
						  auto current_value_str = StrView::from_number(
								ctx->arena_frame, current_value);
						  auto button_minus =
								mobile_button(ctx, CLAY_ID("MinusButton"),
					                          MINUS, button_style);
						  draw_text(current_value_str, theme()->onSurface,
					                text_size);
						  auto button_plus = mobile_button(
								ctx, CLAY_ID("PlusButton"), PLUS, button_style);

						  constexpr auto ROUND_SIZE_MAX = 20;
						  if (button_minus.activated() && (current_value > 1)) {
							  ctx->settings.exercise_round_size -= 1;
							  ctx->settings.save(ctx->arena_frame);
						  } else if (button_plus.activated() &&
					                 (current_value < ROUND_SIZE_MAX)) {
							  ctx->settings.exercise_round_size += 1;
							  ctx->settings.save(ctx->arena_frame);
						  }
					  }
					  break;
				  case 2: // DefaultScreenRow
					  // CLAY(CLAY_ID("LanguageRow"),
				      //      {.layout =
				      //             {
				      // 				  .sizing = {CLAY_SIZING_GROW(0),
				      // CLAY_SIZING_FIT(0)}, 				  .padding =
				      // CLAY_PADDING_ALL(udpi(4.0f)), .childAlignment =
				      // {CLAY_ALIGN_X_CENTER,
				      //                                      CLAY_ALIGN_Y_CENTER},
				      // 			},
				      //       .border = {
				      // 			.color = theme()->outline,
				      // 			.width = {.bottom = udpi(1.f)},
				      // 	  }}) {
				      // 	CLAY(CLAY_ID("LabelLanguage"),
				      // 	     {
				      // 			   .layout =
				      // 					 {
				      // 						   .sizing =
				      // {CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0)},
				      // 					 },
				      // 		 }) {
				      // 		draw_text(tr()->screen_settings_language,
				      // theme()->onSurface, 		          text_size,
				      // translation_font_id(ctx));
				      // 	}
				      // 	auto button_unchosen_style =
				      // 		  mobile_button_style_surface_container_high();
				      // 	button_unchosen_style.padding_x =
				      // button_unchosen_style.padding_y = 		  dpi(4.f);
				      // 	button_unchosen_style.font_id = FontID::ICONS;
				      // 	button_unchosen_style.background = {};
				      // 	button_unchosen_style.border_width = 0.f;
				      // 	auto button_chosen_style = button_unchosen_style;
				      // 	button_chosen_style.border_width = dpi(1.f);
				      // 	button_chosen_style.border = theme()->outline;
				      // 	auto current_lang = ctx->settings.tr_language;
				      // 	Settings::for_every_lang(
				      // 		  [&](int i, Lang lang) {
				      // 			  auto style = lang == current_lang ?
				      // button_chosen_style : button_unchosen_style;
				      // auto btn = mobile_button( 					ctx,
				      // CLAY_IDI("LangButton", i), lang_code(lang), style);
				      // if (btn.activated()) {
				      // ctx->settings.tr_language = lang;
				      // 				  ctx->settings.save(ctx->arena_frame);
				      // 				  ctx->app_status.push_error(
				      // 						tr()->screen_settings_language_changed_exit);
				      // 				  ctx->app_status.set_exit_normal();
				      // 			  }
				      // 		  });
				      // }

					  CLAY(item_clay_id,
				           {.layout =
				                  {
										.sizing = {CLAY_SIZING_GROW(0),
				                                   CLAY_SIZING_FIT(0)},
										.padding = padding,
										.childAlignment = {CLAY_ALIGN_X_CENTER,
				                                           CLAY_ALIGN_Y_CENTER},
								  },
				            .border = {
								  .color = theme()->outline,
								  .width = {.bottom = udpi(1.f)},
							}}) {
						  CLAY(CLAY_ID("LabelDefaultScreen"),
					           {
									 .layout =
										   {
												 .sizing = {CLAY_SIZING_GROW(0),
					                                        CLAY_SIZING_FIT(0)},
										   },
							   }) {
							  draw_text("Default screen"_v, theme()->onSurface,
						                text_size, translation_font_id(ctx));
						  }
						  auto button_unchosen_style =
								mobile_button_style_surface_container_high();
						  button_unchosen_style.font_size = udpi(18);
						  button_unchosen_style.padding_x =
								button_unchosen_style.padding_y = dpi(4.f);
						  button_unchosen_style.font_id = FontID::MAIN;
						  button_unchosen_style.background = {};
						  button_unchosen_style.border_width = 0.f;
						  auto button_chosen_style = button_unchosen_style;
						  button_chosen_style.border_width = dpi(1.f);
						  button_chosen_style.border = theme()->outline;
						  auto current_default_screen = static_cast<Screen>(
								ctx->settings.default_screen);
						  Arr<Pair<Screen, StrView>, 2> options{
								{{Screen::WordsList, "Wortschatz"_v},
					             {Screen::Start, "Trainer"_v}}};
						  int option_counter = 0;
						  for (auto &[screen, label] : options) {
							  auto style = screen == current_default_screen
						                         ? button_chosen_style
						                         : button_unchosen_style;
							  auto btn = mobile_button(
									ctx,
									CLAY_IDI_LOCAL("OptionButton",
						                           option_counter++),
									label, style);
							  if (btn.activated()) {
								  ctx->settings.default_screen =
										std::to_underlying(screen);
								  ctx->settings.save(ctx->arena_frame);
							  }
						  }
					  }
					  break;
				  case 3: // About
					  CLAY(item_clay_id,
				           {
								 .layout =
									   {
											 .sizing = {CLAY_SIZING_GROW(0),
				                                        CLAY_SIZING_GROW(0)},
											 .padding =
												   padding,
											 .childGap = udpi(14.0f),
											 .childAlignment =
												   {CLAY_ALIGN_X_CENTER,
				                                    CLAY_ALIGN_Y_BOTTOM},
											 .layoutDirection =
												   CLAY_TOP_TO_BOTTOM,
									   },
								 //    .border = {
				                 // .color = theme()->outline,
				                 // .width = {.bottom = udpi(1.f)},
				                 // }
						   }) {
						  auto notes_text_size = udpi(12);
						  draw_text(
								"This product includes data from Wiktionary (https://www.wiktionary.org/)"_v,
								theme()->onSurface, notes_text_size,
								translation_font_id(ctx));
						  draw_text(
								"Wiktionary content is licensed under the Creative Commons Attribution-ShareAlike 4.0 International (CC BY-SA 4.0)."_v,
								theme()->onSurface, notes_text_size,
								translation_font_id(ctx));
						  draw_text(
								"https://creativecommons.org/licenses/by-sa/4.0/"_v,
								theme()->onSurface, notes_text_size,
								translation_font_id(ctx));
						  draw_text(
								"The data has been extracted and transformed using Wiktextract and then using selfmade scripts."_v,
								theme()->onSurface, notes_text_size,
								translation_font_id(ctx));
					  }
					  break;
				  }
			  });
	}
}
