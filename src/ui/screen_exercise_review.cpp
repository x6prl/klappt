#include "app/app_context.h"
#include "app/words_init.h"
#include "app/worker.h"
#include "base/str_builder.h"
#include "base/str_view.h"
#include "domain/exercises.h"
#include "platform/neuro.h"

#include "ui/components/button.h"
#include "ui/dpi.h"
#include "ui/themes.h"

#include "screen_helpers.h"
#include <SDL3/SDL_log.h>

void screen_exercise_review_push(AppContext *ctx) {
	ctx->push(Screen::ExerciseReview);
}

namespace {

bool promote_current_review_word_mode(AppContext *ctx) {
	const auto &review = ctx->exercises.current_result_review();

	Engine::State state{};
	auto [success, found] = ctx->states.get(review.word_id, state);
	if (!success || !found) {
		ctx->app_status.push_error("Word learning state error"_v);
		return false;
	}
	if (state.mode >= Engine::Mode::Compose) {
		return true;
	}

	const int current_mode_index = Engine::modei(state.mode);
	const auto next_mode = Engine::imode(current_mode_index + 1);
	const auto &current = state.memory[current_mode_index];
	auto &next = state.memory[Engine::modei(next_mode)];

	const double source_stability = std::max(
		  current.stability_days,
		  Engine::State::p.promotion_stability_days[current_mode_index]);
	const double source_quality =
		  std::max(current.quality_ewma, Engine::State::p.promotion_quality);

	next.stability_days = std::max(
		  next.stability_days,
		  source_stability *
				Engine::State::p.promotion_transfer[current_mode_index]);
	next.quality_ewma = std::max(next.quality_ewma, source_quality * 0.80);

	state.mode = next_mode;
	state.refresh_due(std::time(nullptr));

	if (!ctx->states.set(review.word_id, state)) {
		ctx->app_status.push_error("Cannot save learning state"_v);
		return false;
	}

	return true;
}

struct CoalescedDiff {
	DynArr<StrView> parts;
	DynArr<int> is_right;
};

static inline CoalescedDiff coalesce_diff_parts(Arena &a,
                                                const DynArr<StrView> &parts,
                                                const DynArr<int> &is_right) {
	CoalescedDiff out{};
	if (parts.size == 0)
		return out;

	Size i = 0;
	while (i < parts.size) {
		auto current_token = parts[i];
		int current_status = (i < is_right.size) ? is_right[i] : 1;

		if (current_token == " "_v) {
			out.parts.push(a, " "_v);
			out.is_right.push(a, 1);
			++i;
			continue;
		}

		StrBuilder sb{};
		sb.push(a, current_token);
		Size j = i + 1;
		while (j < parts.size) {
			if (parts[j] == " "_v)
				break;
			int next_status = (j < is_right.size) ? is_right[j] : 1;
			if (next_status != current_status)
				break;

			sb.push(a, parts[j]);
			++j;
		}

		out.parts.push(a, sb.join(a));
		out.is_right.push(a, current_status);
		i = j;
	}

	return out;
}

static void draw_wrapped_diff_tokens(AppContext *ctx,
                                     const DynArr<StrView> &raw_parts,
                                     const DynArr<int> &raw_is_right,
                                     bool is_actual, float font_size) {
	if (raw_parts.size == 0)
		return;

	auto merged =
		  coalesce_diff_parts(ctx->arena_frame, raw_parts, raw_is_right);
	const auto &parts = merged.parts;
	const auto &is_right = merged.is_right;

	const float max_row_width =
		  std::max(100.0f, ctx->display_width - udpi(72.0f));

	auto get_token_width = [&](StrView token) -> float {
		if (!token || token.size == 0)
			return 0.0f;
		auto slice = CLAY__INIT(Clay_StringSlice){
			  .length = static_cast<int32_t>(token.size),
			  .chars = token.data,
			  .baseChars = token.data,
		};
		return ctx->text
		      ->measure_text(slice,
		                     CLAY_TEXT_CONFIG({
								   .fontId = FontID::MONOSPACE_REGULAR,
								   .fontSize = static_cast<uint16_t>(font_size),
								   .wrapMode = CLAY_TEXT_WRAP_NONE,
							 }))
		      .width;
	};

	CLAY(CLAY_IDI("WrappedTokensBlock", is_actual ? 1 : 0),
	     {
			   .layout =
					 {
						   .sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0)},
						   .childGap = udpi(4.0f),
						   .childAlignment = {CLAY_ALIGN_X_LEFT,
	                                          CLAY_ALIGN_Y_TOP},
						   .layoutDirection = CLAY_TOP_TO_BOTTOM,
					 },
		 }) {

		Size line_start = 0;
		float current_line_w = 0.0f;
		int line_index = 0;

		auto draw_single_line = [&](Size from, Size to, int l_idx) {
			CLAY(CLAY_IDI("DiffLine", (is_actual ? 5000 : 6000) + l_idx),
			     {
					   .layout =
							 {
								   .sizing = {CLAY_SIZING_GROW(0),
			                                  CLAY_SIZING_FIT(0)},
								   .childGap = 0,
								   .childAlignment = {CLAY_ALIGN_X_LEFT,
			                                          CLAY_ALIGN_Y_CENTER},
								   .layoutDirection = CLAY_LEFT_TO_RIGHT,
							 },
				 }) {
				for (Size i = from; i < to; ++i) {
					const auto &token = parts[i];
					if (!token || token.size == 0)
						continue;

					const bool is_space = (token == " "_v);
					const bool is_correct_part =
						  (i < is_right.size && is_right[i] != 0);

					Clay_Color bg{};
					Clay_Color fg = theme()->onSurfaceContainer;

					if (!is_space) {
						if (is_actual && !is_correct_part) {
							bg = theme()->wrongContainer;
							fg = theme()->onWrongContainer;
						} else if (!is_actual && !is_correct_part) {
							bg = theme()->rightContainer;
							fg = theme()->onRightContainer;
						}
					}

					const bool has_bg = (bg.a > 0);

					CLAY(CLAY_IDI("TokenPill", (is_actual ? 10000 : 20000) +
					                                 static_cast<int>(i)),
					     {
							   .layout =
									 {
										   .padding =
												 has_bg
													   ? Clay_Padding{udpi(1.f),
					                                                  udpi(1.f),
					                                                  udpi(3.f),
					                                                  udpi(3.f)}
													   : Clay_Padding{0, 0, 0,
					                                                  0},
									 },
							   .backgroundColor = bg,
							   .cornerRadius = CLAY_CORNER_RADIUS(dpi(3.f)),
						 }) {
						draw_text(token, fg, static_cast<uint16_t>(font_size),
						          FontID::MONOSPACE_REGULAR);
					}
				}
			}
		};

		for (Size i = 0; i < parts.size; ++i) {
			float tw = get_token_width(parts[i]);
			if (i > line_start && (current_line_w + tw > max_row_width)) {
				draw_single_line(line_start, i, line_index++);
				line_start = i;
				current_line_w = 0.0f;
			}
			current_line_w += tw;
		}

		if (line_start < parts.size) {
			draw_single_line(line_start, parts.size, line_index++);
		}
	}
}

} // namespace

void screen_exercise_review_draw(AppContext *ctx) {
	if (ctx->exercises.results.is_empty()) {
		CLAY(CLAY_ID("ExDiffEmpty"),
		     {
				   .layout =
						 {
							   .sizing = {CLAY_SIZING_GROW(0),
		                                  CLAY_SIZING_GROW(0)},
							   .padding = CLAY_PADDING_ALL(udpi(20.0f)),
							   .childGap = udpi(16.0f),
							   .childAlignment = {CLAY_ALIGN_X_CENTER,
		                                          CLAY_ALIGN_Y_CENTER},
							   .layoutDirection = CLAY_TOP_TO_BOTTOM,
						 },
				   .backgroundColor = theme()->surface,
			 }) {
			draw_text("All answers correct!"_v, theme()->onSurface,
			          static_cast<uint16_t>(udpi(24.f)));
		}
		return;
	}

	const auto &diff = ctx->exercises.current_result_review();
	const bool is_correct = (diff.actual == diff.expected);
	const auto padding = udpi(20.0f);

	const Size max_str_len =
		  std::max({diff.source.utf8_length(), diff.expected.utf8_length(),
	                diff.actual.utf8_length()});
	const bool is_phrase = (max_str_len > 35);

	CLAY(CLAY_ID("ExDiffRoot"),
	     {
			   .layout =
					 {
						   .sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)},
						   .padding =
								 {
									   .left = padding,
									   .right = padding,
									   .top = udpi(10.f),
									   .bottom = udpi(16.f),
								 },
						   .childGap = udpi(12.0f),
						   .childAlignment = {CLAY_ALIGN_X_CENTER,
	                                          CLAY_ALIGN_Y_TOP},
						   .layoutDirection = CLAY_TOP_TO_BOTTOM,
					 },
			   .backgroundColor = theme()->surface,
		 }) {

		const float source_font_size =
			  is_phrase ? get_font_size_based_on_str_size(
								ctx->display_width, ctx->scale,
								diff.source.utf8_length(), 15.f, 20.f)
						: get_font_size_based_on_str_size(
								ctx->display_width, ctx->scale,
								diff.source.utf8_length(), 20.f, 30.f);
		const auto font_id = translation_font_id(ctx);

		CLAY(CLAY_ID("ReviewTaskArea"),
		     {
				   .layout =
						 {
							   .sizing = {CLAY_SIZING_GROW(0),
		                                  CLAY_SIZING_FIT(0)},
							   .padding =
									 {
										   .left = udpi(8.f),
										   .right = udpi(8.f),
										   .top = udpi(4.f),
										   .bottom = udpi(6.f),
									 },
							   .childGap = udpi(4.f),
							   .childAlignment = {CLAY_ALIGN_X_CENTER,
		                                          CLAY_ALIGN_Y_CENTER},
							   .layoutDirection = CLAY_TOP_TO_BOTTOM,
						 },
				   .backgroundColor = theme()->surface,
			 }) {

			draw_text(diff.source, theme()->onSurface,
			          static_cast<uint16_t>(source_font_size), font_id,
			          CLAY_TEXT_WRAP_WORDS, CLAY_TEXT_ALIGN_CENTER);

			if (diff.source_sub0.size > 0) {
				auto sub_col = theme()->onSurface;
				sub_col.a = static_cast<uint8_t>(sub_col.a * 0.6f);
				draw_text(diff.source_sub0, sub_col,
				          static_cast<uint16_t>(udpi(12.f)), font_id,
				          CLAY_TEXT_WRAP_WORDS, CLAY_TEXT_ALIGN_CENTER);
			}

			if (diff.source_sub1.size > 0) {
				CLAY(CLAY_ID("ValencyChip"),
				     {
						   .layout = {.padding = {udpi(6.f), udpi(6.f),
				                                  udpi(2.f), udpi(2.f)}},
						   .backgroundColor = theme()->surfaceContainerHigh,
						   .cornerRadius = CLAY_CORNER_RADIUS(dpi(4.f)),
					 }) {
					draw_text(diff.source_sub1, theme()->onSurfaceContainerHigh,
					          static_cast<uint16_t>(udpi(11.f)));
				}
			}
		}

		CLAY(CLAY_ID("DiffScrollArea"),
		     {
				   .layout =
						 {
							   .sizing = {CLAY_SIZING_GROW(0),
		                                  CLAY_SIZING_GROW(0)},
							   .padding = CLAY_PADDING_ALL(udpi(16.0f)),
							   .childGap = udpi(12.0f),
							   .childAlignment = {CLAY_ALIGN_X_LEFT,
		                                          CLAY_ALIGN_Y_TOP},
							   .layoutDirection = CLAY_TOP_TO_BOTTOM,
						 },
				   .backgroundColor = theme()->surfaceContainerLow,
				   .cornerRadius = CLAY_CORNER_RADIUS(dpi(20.f)),
				   .clip =
						 {
							   .vertical = true,
							   .childOffset = Clay_GetScrollOffset(),
						 },
			 }) {

			auto label_color = theme()->onSurfaceContainer;
			label_color.a = static_cast<uint8_t>(label_color.a * 0.6f);
			const float diff_font_size = is_phrase ? udpi(15.f) : udpi(18.f);

			if (is_correct) {
				CLAY(CLAY_ID("CorrectHeaderRow"),
				     {
						   .layout =
								 {
									   .sizing = {CLAY_SIZING_GROW(0),
				                                  CLAY_SIZING_FIT(0)},
									   .childAlignment = {CLAY_ALIGN_X_LEFT,
				                                          CLAY_ALIGN_Y_CENTER},
									   .layoutDirection = CLAY_LEFT_TO_RIGHT,
								 },
					 }) {
					CLAY(CLAY_ID("SuccessBadge"),
					     {
							   .layout = {.padding = {udpi(8.f), udpi(8.f),
					                                  udpi(3.f), udpi(3.f)}},
							   .backgroundColor = theme()->rightContainer,
							   .cornerRadius = CLAY_CORNER_RADIUS(dpi(6.f)),
						 }) {
						draw_text("✓ Correct answer"_v,
						          theme()->onRightContainer,
						          static_cast<uint16_t>(udpi(12.f)));
					}
				}

				CLAY(CLAY_ID("CorrectAnswerBox"),
				     {
						   .layout =
								 {
									   .sizing = {CLAY_SIZING_GROW(0),
				                                  CLAY_SIZING_FIT(0)},
									   .padding = CLAY_PADDING_ALL(udpi(12.f)),
								 },
						   .backgroundColor = theme()->surfaceContainer,
						   .cornerRadius = CLAY_CORNER_RADIUS(dpi(12.f)),
					 }) {
					draw_text(diff.expected, theme()->onSurface,
					          static_cast<uint16_t>(diff_font_size),
					          FontID::MONOSPACE_REGULAR, CLAY_TEXT_WRAP_WORDS,
					          CLAY_TEXT_ALIGN_LEFT);
				}

			} else {
				draw_text("Expected answer"_v, label_color,
				          static_cast<uint16_t>(udpi(12.f)));
				CLAY(CLAY_ID("ExpectedBox"),
				     {
						   .layout =
								 {
									   .sizing = {CLAY_SIZING_GROW(0),
				                                  CLAY_SIZING_FIT(0)},
									   .padding = CLAY_PADDING_ALL(udpi(12.f)),
								 },
						   .backgroundColor = theme()->surfaceContainer,
						   .cornerRadius = CLAY_CORNER_RADIUS(dpi(12.f)),
					 }) {
					if (diff.expected_parts.size > 0) {
						draw_wrapped_diff_tokens(ctx, diff.expected_parts,
						                         diff.is_right_actual_part,
						                         false, diff_font_size);
					} else {
						draw_text(diff.expected, theme()->onSurface,
						          static_cast<uint16_t>(diff_font_size),
						          FontID::MONOSPACE_REGULAR,
						          CLAY_TEXT_WRAP_WORDS, CLAY_TEXT_ALIGN_LEFT);
					}
				}

				draw_text("Your answer"_v, label_color,
				          static_cast<uint16_t>(udpi(12.f)));
				CLAY(CLAY_ID("ActualBox"),
				     {
						   .layout =
								 {
									   .sizing = {CLAY_SIZING_GROW(0),
				                                  CLAY_SIZING_FIT(0)},
									   .padding = CLAY_PADDING_ALL(udpi(12.f)),
								 },
						   .backgroundColor = theme()->surfaceContainer,
						   .cornerRadius = CLAY_CORNER_RADIUS(dpi(12.f)),
					 }) {
					if (diff.actual_parts.size > 0) {
						draw_wrapped_diff_tokens(ctx, diff.actual_parts,
						                         diff.is_right_actual_part,
						                         true, diff_font_size);
					} else {
						draw_text(diff.actual, theme()->onSurface,
						          static_cast<uint16_t>(diff_font_size),
						          FontID::MONOSPACE_REGULAR,
						          CLAY_TEXT_WRAP_WORDS, CLAY_TEXT_ALIGN_LEFT);
					}
				}
			}

			CLAY(CLAY_ID("NextButtonContainer"),
			     {
					   .layout =
							 {
								   .sizing = {CLAY_SIZING_GROW(0),
			                                  CLAY_SIZING_GROW(0)},
								   .padding =
										 {
											   .top = udpi(4.f),
											   .bottom = udpi(4.f),
										 },
								   .childGap = udpi(14.0f),
								   .childAlignment = {CLAY_ALIGN_X_CENTER,
			                                          CLAY_ALIGN_Y_BOTTOM},
								   .layoutDirection = CLAY_LEFT_TO_RIGHT,
							 },
				 }) {
				// auto s = mobile_button_style_primary();
				// s.min_width = 100.f;
				// s.font_id = FontID::ICONS;
				// auto next_btn =
				// 	  mobile_button(ctx, CLAY_ID("NextButton"), Icons::NEXT, s);
				// // auto next_btn = mobile_icon_button<true>(ctx,
				// // CLAY_ID("NextButton"),
				// //                                          Icons::NEXT);
				// if (next_btn.activated()) {
				// 	ctx->exercises.next_result();
				// }
				draw_text("click anywhere to review next"_v, theme()->outline,
				          udpi(16.f));
			}
			if (Clay_Hovered() && (ctx->tslt.is_tap() ||
			    ctx->tslt.is_longtap())) {
				ctx->exercises.next_result();
			}
		}

		CLAY(CLAY_ID("ReviewActionsBar"),
		     {
				   .layout =
						 {
							   .sizing = {CLAY_SIZING_GROW(0),
		                                  CLAY_SIZING_FIT(0)},
							   .padding =
									 {
										   .top = udpi(4.f),
										   .bottom = udpi(4.f),
									 },
							   .childGap = udpi(14.0f),
							   .childAlignment = {CLAY_ALIGN_X_CENTER,
		                                          CLAY_ALIGN_Y_CENTER},
							   .layoutDirection = CLAY_LEFT_TO_RIGHT,
						 },
			 }) {

			auto remove_btn = mobile_icon_button<false>(
				  ctx, CLAY_ID("RemoveButton"), Icons::REMOVE);
			if (remove_btn.activated()) {
				auto word_ref = diff.word_ref;
				auto &word = (*ctx->words)[word_ref];
				remove_word_from_learning_list(ctx->arena_frame, &word,
				                               ctx->words, &ctx->word_store);
				save_words_dat(ctx->arena_frame, ctx->settings, *ctx->words);
				ctx->exercises.next_result();
			}

			auto view_btn = mobile_icon_button<false>(
				  ctx, CLAY_ID("EditButton"), Icons::EDIT);
			if (view_btn.activated()) {
				screen_word_view_push(ctx, diff.word_id);
			}

			if (is_correct) {
				auto promote_btn = mobile_icon_button<false>(
					  ctx, CLAY_ID("PromoteModeButton"), Icons::CHEVRON_UP);
				if (promote_btn.activated()) {
					promote_current_review_word_mode(ctx);
				}
			}
		}
	}
}
