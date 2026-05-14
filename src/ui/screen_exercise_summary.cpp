#include "screen_helpers.h"
#include "ui/components/button.h"
#include "ui/dpi.h"

void screen_exercise_summary_go(AppContext *ctx) {
	ctx->go(Screen::ExerciceResultSummary);
}

void screen_exercise_summary_draw(AppContext *ctx) {
	CLAY(CLAY_ID("ExerciseSummaryContainer"),
	     {
			   .layout =
					 {
						   .sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)},
						   .padding = CLAY_PADDING_ALL(udpi(16.0f)),
						   .childGap = udpi(14.0f),
						   .childAlignment = {CLAY_ALIGN_X_CENTER,
	                                          CLAY_ALIGN_Y_CENTER},
						   .layoutDirection = CLAY_TOP_TO_BOTTOM,

					 },
			   .backgroundColor = theme()->surface,
		 }) {
		StrViewArray strs{};
		{
			strs.push(ctx->tmparena,
			          StrView::from_number(ctx->tmparena,
			                           ctx->exercises.correct_exercise_count));
			strs.push(ctx->tmparena, " of "_v);
			strs.push(ctx->tmparena,
			          StrView::from_number(ctx->tmparena,
			                           ctx->exercises.exercise_total()));
		}
		auto result_str = strs.join(ctx->tmparena);
		CLAY(CLAY_ID("Result"),
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
				   .backgroundColor = theme()->surfaceContainer,
				   .cornerRadius = CLAY_CORNER_RADIUS(dpi(18.f)),
			 }) {
			draw_text(result_str, theme()->onSurfaceContainer, udpi(30));
		}
		CLAY(CLAY_ID("Buttons"),
		     {.layout = {

					.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)},
					.padding = CLAY_PADDING_ALL(udpi(16.0f)),
					.childGap = udpi(14.0f),
					.childAlignment = {CLAY_ALIGN_X_CENTER,
		                               CLAY_ALIGN_Y_CENTER},

			  }}) {
			auto res_all = mobile_button(ctx, CLAY_ID("All"), "Show all"_v);
			if (res_all.activated()) {
				ctx->exercises.build_result_reviews(ctx->tmparena, false);
				if (!ctx->exercises.results.empty()) {
					screen_exercise_review_push(ctx);
				}
			}
			bool is_result_with_errors =
				  ctx->exercises.correct_exercise_count !=
				  ctx->exercises.exercise_total();
			auto res_errors_only = mobile_button(
				  ctx, CLAY_ID("Basic"),
				  is_result_with_errors ? "Errors only"_v : "Next exercises"_v,
				  mobile_button_style_primary());
			if (res_errors_only.activated()) {
				if (is_result_with_errors) {
					ctx->exercises.build_result_reviews(ctx->tmparena, true);
					if (!ctx->exercises.results.empty()) {
						screen_exercise_review_push(ctx);
					}
				} else {
					screen_exercise_go(ctx, true);
				}
			}
		}
	}
}
