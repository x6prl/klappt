#include "screen_helpers.h"
#include "ui/dpi.h"
#include "ui/components/button.h"
#include "app/words_init.h"

void screen_exercise_review_push(AppContext *ctx) {
	ctx->push(Screen::ExerciseReview);
}

void screen_exercise_review_draw(AppContext *ctx) {
	if (ctx->exercises.results.empty()) {
		CLAY(CLAY_ID("ExDiffEmpty"),
		     {.layout = {
					.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)},
					.padding = CLAY_PADDING_ALL(udpi(16.0f)),
					.childGap = udpi(14.0f),
					.childAlignment = {CLAY_ALIGN_X_CENTER,
		                               CLAY_ALIGN_Y_CENTER},
					.layoutDirection = CLAY_TOP_TO_BOTTOM,
			  }}) {
			draw_text("No wrong answers"_v, theme()->onSurface, udpi(28));
		}
		return;
	}

	CLAY(CLAY_ID("ExDiff"),
	     {
			   .layout =
					 {
						   .sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)},
						   .childAlignment = {CLAY_ALIGN_X_CENTER,
	                                          CLAY_ALIGN_Y_CENTER},
						   .layoutDirection = CLAY_TOP_TO_BOTTOM,
					 },
		 }) {
		auto child_gap = 14.f;
		auto label_size = 14.f;
		auto label_color = theme()->onSurfaceContainer;
		label_color.a *= 0.666f;

		CLAY(CLAY_ID("Translation"),
		     {
				   .layout =
						 {

							   .sizing = {CLAY_SIZING_GROW(0),
		                                  CLAY_SIZING_GROW(0)},
							   .padding = CLAY_PADDING_ALL(udpi(16.0f)),
							   .childGap = udpi(child_gap),
							   .childAlignment = {CLAY_ALIGN_X_CENTER,
		                                          CLAY_ALIGN_Y_CENTER},
							   .layoutDirection = CLAY_TOP_TO_BOTTOM,
						 },
			 }) {
			auto &diff = ctx->exercises.current_result_review();
			const auto source_font_id = translation_font_id(ctx);
			draw_text(diff.source, theme()->onSurface, udpi(20),
			          source_font_id);
			draw_text(diff.source_sub0, theme()->onSurface, udpi(20),
			          source_font_id, CLAY_TEXT_WRAP_NEWLINES);
			draw_text(diff.source_sub1, theme()->onSurface, udpi(20),
			          source_font_id);
		}
		CLAY(CLAY_ID("DiffBlock"),
		     {.layout = {
					.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)},
					.padding = CLAY_PADDING_ALL(udpi(16.0f)),
					.childAlignment = {CLAY_ALIGN_X_CENTER,
		                               CLAY_ALIGN_Y_CENTER},
			  }}) {
			CLAY(CLAY_ID("DiffContainer"),
			     {
					   .layout =
							 {
								   .sizing = {CLAY_SIZING_GROW(0),
			                                  CLAY_SIZING_FIT(0)},
								   .padding = CLAY_PADDING_ALL(udpi(16.0f)),
								   .childGap = udpi(child_gap),
								   .childAlignment = {CLAY_ALIGN_X_CENTER,
			                                          CLAY_ALIGN_Y_CENTER},
								   .layoutDirection = CLAY_TOP_TO_BOTTOM,
							 },
					   .backgroundColor = theme()->surfaceContainer,
					   .cornerRadius = CLAY_CORNER_RADIUS(dpi(14.f)),
				 }) {
				auto &diff = ctx->exercises.current_result_review();
				auto response_length = static_cast<float>(
					  std::max(diff.actual.size, diff.expected.size));
				auto font_size = get_font_size_based_on_str_size(
					  ctx->display_width, ctx->scale, response_length);
				auto max_chars_per_line = static_cast<Size>(std::max(
					  12.0f, (ctx->display_width * 0.78f) / font_size));

				auto draw_part_line = [&](Clay_ElementId id, Size start,
				                          Size end, bool is_actual) {
					CLAY(id, {.layout = {.sizing = {CLAY_SIZING_FIT(0),
					                                CLAY_SIZING_FIT(0)}}}) {
						for (Size i = start; i < end; ++i) {
							auto part_id =
								  is_actual ? CLAY_IDI("DiffActualPart", i)
											: CLAY_IDI("DiffExpectedPart", i);
							auto eq = diff.is_right_actual_part[i] != 0;
							auto bg = eq ? theme()->rightContainer
							             : theme()->wrongContainer;
							auto col = eq ? theme()->onRightContainer
							              : theme()->onWrongContainer;
							auto text = is_actual ? diff.actual_parts[i]
							                      : diff.expected_parts[i];
							CLAY(part_id,
							     {
									   .layout =
											 {
												   .sizing =
														 {CLAY_SIZING_FIT(0),
							                              CLAY_SIZING_FIT(0)},
												   .childAlignment =
														 {CLAY_ALIGN_X_CENTER,
							                              CLAY_ALIGN_Y_CENTER},
											 },
									   .backgroundColor =
											 is_actual && text != " "_v
												   ? bg
												   : Clay_Color{},
								 }) {
								draw_text(text,
								          is_actual
								                ? col
								                : theme()->onSurfaceContainer,
								          font_size, FontID::MONOSPACE_REGULAR);
							}
						}
					}
				};

				auto draw_wrapped_parts = [&](StrView label, bool is_actual) {
					auto block_id = is_actual ? CLAY_ID("DiffActual")
					                          : CLAY_ID("DiffExpected");
					draw_text(label, label_color, udpi(label_size));
					CLAY(block_id,
					     {
							   .layout =
									 {
										   .sizing = {CLAY_SIZING_GROW(0),
					                                  CLAY_SIZING_FIT(0)},
										   .childGap = udpi(8.0f),
										   .childAlignment =
												 {CLAY_ALIGN_X_CENTER,
					                              CLAY_ALIGN_Y_CENTER},
										   .layoutDirection =
												 CLAY_TOP_TO_BOTTOM,
									 },
						 }) {
						Size line_start = 0;
						Size line_len = 0;
						for (Size i = 0; i < diff.actual_parts.size; ++i) {
							auto part_len =
								  std::max(diff.actual_parts[i].size,
							               diff.expected_parts[i].size);
							if (i > line_start &&
							    line_len + part_len > max_chars_per_line) {
								auto line_id =
									  is_actual ? CLAY_IDI("DiffActualLine",
								                           static_cast<int>(
																 line_start))
												: CLAY_IDI("DiffExpectedLine",
								                           static_cast<int>(
																 line_start));
								draw_part_line(line_id, line_start, i,
								               is_actual);
								line_start = i;
								line_len = 0;
							}
							line_len += part_len;
						}
						if (line_start < diff.actual_parts.size) {
							auto line_id =
								  is_actual
										? CLAY_IDI("DiffActualLine",
							                       static_cast<int>(line_start))
										: CLAY_IDI(
												"DiffExpectedLine",
												static_cast<int>(line_start));
							draw_part_line(line_id, line_start,
							               diff.actual_parts.size, is_actual);
						}
					}
				};

				draw_wrapped_parts("Correct answer"_v, false);
				draw_wrapped_parts("Your answer"_v, true);
			}
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
			auto surface_style = mobile_button_style_surface_container_high();
			surface_style.font_id = FontID::ICONS;

			auto edit_this_word = mobile_button(ctx, CLAY_ID("EditButton"),
			                                    ""_v, surface_style);
			if (edit_this_word.activated()) {
				screen_word_edit_push(
					  ctx, ctx->exercises.current_result_review().word_id);
			}

			auto remove_this_word_from_learning_list = mobile_button(
				  ctx, CLAY_ID("RemoveButton"), ""_v, surface_style);
			if (remove_this_word_from_learning_list.activated()) {
				auto word_ref = ctx->exercises.current_result_review().word_ref;
				auto &word = (*ctx->words)[word_ref];
				remove_word_from_learning_list(ctx->tmparena, &word, ctx->words,
				                               &ctx->word_store);
				save_words_dat(ctx->tmparena, ctx->settings, *ctx->words);
			}

			auto next_style = mobile_button_style_primary();
			next_style.font_id = FontID::ICONS;
			auto next = mobile_button(ctx, CLAY_ID("NextButton"), ""_v,
			                          next_style);
			if (next.activated()) {
				ctx->exercises.next_result();
			}
		}
	}
}
