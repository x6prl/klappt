#include "screen_helpers.h"

#include "app/app_context.h"
#include "base/arr.h"
#include "base/str_view.h"

#include "ui/components/button.h"
#include "ui/components/switch_button.h"

#include "ui/trs.h"

void screen_settings_push(AppContext *ctx) { ctx->push(Screen::Settings); }

void screen_settings_draw(AppContext *ctx) {
	const auto row_padding = Clay_Padding{sizes()->space.lg, sizes()->space.lg,
	                                      sizes()->space.lg, sizes()->space.lg};
	const uint16_t title_font_size = sizes()->font.body_md;
	const auto trans_font = translation_font_id(ctx);

	int settings_divider_counter{};
	auto draw_setting_divider = [&settings_divider_counter]() {
		const float divider_h =
			  std::max(1.0f, roundf(sizes()->scale)); // TODO: unify
		auto div_color = theme()->outline;
		div_color.a = static_cast<uint8_t>(div_color.a * 0.25f);
		CLAY(CLAY_IDI("SettingsDivider", settings_divider_counter++),
		     {
				   .layout =
						 {
							   .sizing = {CLAY_SIZING_GROW(0),
		                                  CLAY_SIZING_FIXED(divider_h)},
						 },
				   .backgroundColor = div_color,
			 }) {}
	};

	auto draw_section_header = [&](Clay_ElementId id, StrView title) {
		CLAY(id,
		     {
				   .layout =
						 {
							   .sizing = {CLAY_SIZING_GROW(0),
		                                  CLAY_SIZING_FIT(0)},
							   .padding = {sizes()->space.lg, sizes()->space.lg,
		                                   sizes()->space.xxl,
		                                   sizes()->space.xs},
						 },
			 }) {
			draw_text(title, theme()->primary, sizes()->font.label_md,
			          FontID::MAIN, CLAY_TEXT_WRAP_NONE, CLAY_TEXT_ALIGN_LEFT);
		}
	};

	auto draw_switch_row = [&](Clay_ElementId row_id, Clay_ElementId switch_id,
	                           StrView label, bool value, auto on_change) {
		CLAY(row_id, {
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
			CLAY(CLAY_IDI("LabelCol", row_id.id),
			     {
					   .layout = {.sizing = {CLAY_SIZING_GROW(0),
			                                 CLAY_SIZING_FIT(0)}},
				 }) {
				draw_text(label, theme()->onSurface, title_font_size,
				          trans_font, CLAY_TEXT_WRAP_WORDS,
				          CLAY_TEXT_ALIGN_LEFT);
			}

			if (switch_button(ctx, switch_id, value)) {
				on_change(!value);
				ctx->settings.save(ctx->arena_frame);
				ctx->push_one_frame();
			}
		}
	};

	CLAY(CLAY_ID("SettingsScreenRoot"),
	     {
			   .layout =
					 {
						   .sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)},
						   .padding = sizes()->pad.screen,
						   .childAlignment = {CLAY_ALIGN_X_CENTER,
	                                          CLAY_ALIGN_Y_TOP},
						   .layoutDirection = CLAY_TOP_TO_BOTTOM,
					 },
			   .backgroundColor = theme()->surface,
			   .clip = {.vertical = true,
	                    .childOffset = Clay_GetScrollOffset()}, // TODO: change
	                                                            // to list?
		 }) {

		draw_section_header(CLAY_ID("SecApp"),
		                    tr()->screen_settings_sec_appearance);

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
				draw_text(tr()->screen_settings_dark_theme, theme()->onSurface,
				          title_font_size, trans_font);
			}

			if (switch_button(ctx, CLAY_ID("ThemeSwitch"), is_dark)) {
				auto new_theme = !is_dark ? Theme::Dark : Theme::Light;
				theme_set(new_theme);
				ctx->settings.theme_type = new_theme;
				ctx->settings.save(ctx->arena_frame);
				ctx->push_one_frame();
			}
		}

		draw_setting_divider();

		CLAY(CLAY_ID("FontScaleRow"),
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
			CLAY(CLAY_ID("LabelFontCol"),
			     {
					   .layout = {.sizing = {CLAY_SIZING_GROW(0),
			                                 CLAY_SIZING_FIT(0)}},
				 }) {
				draw_text(tr()->screen_settings_text_size, theme()->onSurface,
				          title_font_size, trans_font, CLAY_TEXT_WRAP_WORDS,
				          CLAY_TEXT_ALIGN_LEFT);
			}

			CLAY(CLAY_ID("FontStepperGroup"),
			     {
					   .layout =
							 {
								   .childGap = sizes()->space.md,
								   .childAlignment = {CLAY_ALIGN_X_CENTER,
			                                          CLAY_ALIGN_Y_CENTER},
								   .layoutDirection = CLAY_LEFT_TO_RIGHT,
							 },
				 }) {
				auto btn_style = mobile_button_style_surface_container_high();
				btn_style.background.a = 0.f;
				btn_style.background_pressed.a = 0.f;
				btn_style.border_width = 0;
				btn_style.border = {};
				btn_style.font_size = sizes()->font.title_md;

				auto current_level =
					  std::to_underlying(ctx->settings.font_scale);

				auto btn_minus = mobile_button(ctx, CLAY_ID("FontMinusBtn"),
				                               "A−"_v, btn_style);

				constexpr Arr<StrView, 5> FONT_LEVEL_NAMES = {
					  "85%"_v, "100%"_v, "115%"_v, "130%"_v, "145%"_v};

				draw_text(FONT_LEVEL_NAMES[current_level], theme()->primary,
				          sizes()->font.title_md, FontID::MAIN);

				auto btn_plus = mobile_button(ctx, CLAY_ID("FontPlusBtn"),
				                              "A+"_v, btn_style);

				if (btn_minus.activated() && current_level > 0) {
					--current_level;
					ctx->settings.font_scale =
						  static_cast<FontScaleLevel>(current_level);
					sizes_set_scale(ctx->scale, ctx->settings.density,
					                ctx->settings.font_scale);
					ctx->settings.save(ctx->arena_frame);
					ctx->push_one_frame();
				} else if (btn_plus.activated() && current_level < 4) {
					++current_level;
					ctx->settings.font_scale =
						  static_cast<FontScaleLevel>(current_level);
					sizes_set_scale(ctx->scale, ctx->settings.density,
					                ctx->settings.font_scale);
					ctx->settings.save(ctx->arena_frame);
					ctx->push_one_frame();
				}
			}
		}

		draw_setting_divider();

		CLAY(CLAY_ID("DensityRow"),
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
			CLAY(CLAY_ID("LabelDensityCol"),
			     {
					   .layout = {.sizing = {CLAY_SIZING_GROW(0),
			                                 CLAY_SIZING_FIT(0)}},
				 }) {
				draw_text(tr()->screen_settings_density, theme()->onSurface,
				          title_font_size, trans_font);
			}

			CLAY(CLAY_ID("DensityOptions"),
			     {
					   .layout =
							 {
								   .padding =
										 CLAY_PADDING_ALL(sizes()->space.xs),
								   .childGap = sizes()->space.xs,
								   .childAlignment = {CLAY_ALIGN_X_CENTER,
			                                          CLAY_ALIGN_Y_CENTER},
								   .layoutDirection = CLAY_LEFT_TO_RIGHT,
							 },
					   .backgroundColor = theme()->surfaceContainerHigh,
					   .cornerRadius = sizes()->radius.sm,
				 }) {

				auto current_density = ctx->settings.density;
				Arr<Pair<DensityMode, StrView>, 3> d_options{{
					  {DensityMode::Compact,
				       tr()->screen_settings_density_compact},
					  {DensityMode::Normal,
				       tr()->screen_settings_density_normal},
					  {DensityMode::Comfortable,
				       tr()->screen_settings_density_spacious},
				}};

				int counter = 0;
				for (auto &[mode, label] : d_options) {
					const bool is_selected = (mode == current_density);

					auto b_style = mobile_button_style_surface_container_high();
					b_style.border_width = 0;
					b_style.border = {};
					b_style.font_size = sizes()->font.label_md; // 12sp
					b_style.padding_x = sizes()->space.sm;      // 8dp
					b_style.padding_y = sizes()->space.xs;      // 4dp
					b_style.corner_radius = sizes()->radius.xs.topLeft;

					if (is_selected) {
						b_style.background = theme()->surface;
						b_style.text = theme()->primary;
					} else {
						b_style.background.a = 0.f;
						b_style.text = theme()->onSurfaceContainer;
					}

					auto btn =
						  mobile_button(ctx, CLAY_IDI("DensityOpt", counter++),
					                    label, b_style);
					if (btn.activated()) {
						ctx->settings.density = mode;
						sizes_set_scale(ctx->scale, ctx->settings.density,
						                ctx->settings.font_scale);
						ctx->settings.save(ctx->arena_frame);
						ctx->push_one_frame();
					}
				}
			}
		}

		draw_setting_divider();

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
				draw_text(tr()->screen_settings_default_screen,
				          theme()->onSurface, title_font_size, trans_font,
				          CLAY_TEXT_WRAP_WORDS, CLAY_TEXT_ALIGN_LEFT);
			}

			CLAY(CLAY_ID("ScreenOptions"),
			     {
					   .layout =
							 {
								   .childGap = sizes()->space.sm,
								   .childAlignment = {CLAY_ALIGN_X_CENTER,
			                                          CLAY_ALIGN_Y_CENTER},
								   .layoutDirection = CLAY_LEFT_TO_RIGHT,
							 },
				 }) {

				auto current_default =
					  static_cast<Screen>(ctx->settings.default_screen);
				Arr<Pair<Screen, StrView>, 2> options{
					  {{Screen::Dictionary,
				        tr()->screen_settings_screen_wortschatz},
				       {Screen::Trainer,
				        tr()->screen_settings_screen_trainer}}};

				int counter = 0;
				for (auto &[screen, label] : options) {
					const bool is_selected = (screen == current_default);

					auto b_style = mobile_button_style_surface_container_high();
					b_style.border_width = 0;
					b_style.border = {};
					b_style.border_pressed = {};
					b_style.font_size = sizes()->font.body_sm;
					b_style.padding_x = sizes()->space.md;
					b_style.padding_y = sizes()->space.xs;
					b_style.corner_radius = sizes()->radius.sm.topLeft;

					if (is_selected) {
						b_style.background = theme()->surfaceContainerHigh;
						b_style.text = theme()->primary;
					} else {
						b_style.background.a = 0.f;
						b_style.text = theme()->onSurfaceContainer;
					}

					auto btn =
						  mobile_button(ctx, CLAY_IDI("ScreenOpt", counter++),
					                    label, b_style);
					if (btn.activated()) {
						ctx->settings.default_screen =
							  std::to_underlying(screen);
						ctx->settings.save(ctx->arena_frame);
					}
				}
			}
		}

		draw_section_header(CLAY_ID("SecTrainer"),
		                    tr()->screen_settings_sec_trainer);

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
								   .childGap = sizes()->space.md,
								   .childAlignment = {CLAY_ALIGN_X_CENTER,
			                                          CLAY_ALIGN_Y_CENTER},
								   .layoutDirection = CLAY_LEFT_TO_RIGHT,
							 },
				 }) {

				auto btn_style = mobile_button_style_surface_container_high();
				btn_style.background.a = 0.f;
				btn_style.background_pressed.a = 0.f;
				btn_style.border_width = 0;
				btn_style.border = {};
				btn_style.border_pressed = {};
				btn_style.font_size = sizes()->font.title_md;
				btn_style.font_id = FontID::MAIN;

				auto current_value = ctx->settings.exercise_round_size;

				auto btn_minus =
					  mobile_button(ctx, CLAY_ID("MinusBtn"), "−"_v, btn_style);

				draw_text(StrView::from_number(ctx->arena_frame, current_value),
				          theme()->primary, btn_style.font_size, FontID::MAIN);

				auto btn_plus =
					  mobile_button(ctx, CLAY_ID("PlusBtn"), "+"_v, btn_style);

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

		draw_setting_divider();

		draw_switch_row(CLAY_ID("SuggestionsRow"), CLAY_ID("SuggestionsSwitch"),
		                tr()->screen_settings_suggestions,
		                ctx->settings.is_using_suggestions, [&](bool v) {
							ctx->settings.is_using_suggestions = v;
						});

		draw_section_header(CLAY_ID("SecWordCard"),
		                    tr()->screen_settings_sec_dictionary);
		draw_switch_row(CLAY_ID("MarkSeinRow"), CLAY_ID("MarkSeinSwitch"),
		                tr()->screen_settings_mark_aux_sein,
		                ctx->settings.is_mark_verb_aux_sein, [&](bool v) {
							ctx->settings.is_mark_verb_aux_sein = v;
						});

		draw_setting_divider();

		draw_switch_row(CLAY_ID("MarkIrrRow"), CLAY_ID("MarkIrrSwitch"),
		                tr()->screen_settings_mark_irregular,
		                ctx->settings.is_mark_verb_irregular, [&](bool v) {
							ctx->settings.is_mark_verb_irregular = v;
						});

		draw_setting_divider();

		draw_switch_row(
			  CLAY_ID("MarkSepPrefRow"), CLAY_ID("MarkSepPrefSwitch"),
			  tr()->screen_settings_mark_verb_separable_prefix,
			  ctx->settings.is_mark_verb_separable_prefix,
			  [&](bool v) { ctx->settings.is_mark_verb_separable_prefix = v; });

		draw_setting_divider();

		draw_switch_row(CLAY_ID("MarkVerbTypeRow"),
		                CLAY_ID("MarkVerbTypeSwitch"),
		                tr()->screen_settings_mark_verb_type,
		                ctx->settings.is_mark_verb_type,
		                [&](bool v) { ctx->settings.is_mark_verb_type = v; });

		draw_setting_divider();

		draw_switch_row(CLAY_ID("MarkAdjTypeRow"), CLAY_ID("MarkAdjTypeSwitch"),
		                tr()->screen_settings_mark_adj_type,
		                ctx->settings.is_mark_adj_type,
		                [&](bool v) { ctx->settings.is_mark_adj_type = v; });

		draw_switch_row(
			  CLAY_ID("ShowDictSearchOnlyTransRow"),
			  CLAY_ID("ShowDictSearchOnlyTransSwitch"),
			  tr()->screen_settings_show_dict_search_only_translations,
			  ctx->settings.is_show_dictionary_search_only_translations_button,
			  [&](bool v) {
				  ctx->settings
						.is_show_dictionary_search_only_translations_button = v;
			  });

		draw_section_header(CLAY_ID("SecWordView"),
		                    tr()->screen_settings_sec_word_details);

		draw_switch_row(CLAY_ID("ShowIPARow"), CLAY_ID("ShowIPASwitch"),
		                tr()->screen_settings_show_ipa,
		                ctx->settings.is_show_ipa,
		                [&](bool v) { ctx->settings.is_show_ipa = v; });

		draw_setting_divider();

		draw_switch_row(CLAY_ID("ShowOriginRow"), CLAY_ID("ShowOriginSwitch"),
		                tr()->screen_settings_show_origin,
		                ctx->settings.is_show_origin,
		                [&](bool v) { ctx->settings.is_show_origin = v; });

		draw_setting_divider();

		draw_switch_row(
			  CLAY_ID("NounPluralSuffixRow"), CLAY_ID("NounPluralSuffixSwitch"),
			  tr()->screen_settings_noun_plural_suffix,
			  ctx->settings.is_show_noun_plural_as_suffix,
			  [&](bool v) { ctx->settings.is_show_noun_plural_as_suffix = v; });

		CLAY(CLAY_ID("AboutFooter"),
		     {
				   .layout =
						 {
							   .sizing = {CLAY_SIZING_GROW(0),
		                                  CLAY_SIZING_FIT(0)},
							   .padding =
									 {
										   .left = sizes()->space.lg,
										   .right = sizes()->space.lg,
										   .top = sizes()->space.lg,
										   .bottom = sizes()->space.xl,
									 },
							   .childGap = sizes()->space.xs,
							   .childAlignment = {CLAY_ALIGN_X_CENTER,
		                                          CLAY_ALIGN_Y_BOTTOM},
							   .layoutDirection = CLAY_TOP_TO_BOTTOM,
						 },
			 }) {

			auto sub_color = theme()->onSurface;
			sub_color.a = static_cast<uint8_t>(sub_color.a * 0.5f);
			const uint16_t note_font_size = sizes()->font.label_sm;

			auto app_version_str = StrView::lit(KLAPPT_VERSION);

			draw_text(StrView::concat(ctx->arena_frame, "klappt version "_v,
			                          app_version_str),
			          sub_color, note_font_size, FontID::MAIN,
			          CLAY_TEXT_WRAP_WORDS, CLAY_TEXT_ALIGN_CENTER);

			draw_text(tr()->screen_settings_about_wiktionary, sub_color,
			          note_font_size, FontID::MAIN, CLAY_TEXT_WRAP_WORDS,
			          CLAY_TEXT_ALIGN_CENTER);

			draw_text(tr()->screen_settings_about_wiktextract, sub_color,
			          note_font_size, FontID::MAIN, CLAY_TEXT_WRAP_WORDS,
			          CLAY_TEXT_ALIGN_CENTER);
		}
	}
}
