#include "app/app_context.h"
#include "base/str_builder.h"
#include "base/str_view.h"
#include "domain/exercises.h"

#include "ui/components/button.h"
#include "ui/trs.h"

#include "screen_helpers.h"

void screen_exercise_summary_go(AppContext *ctx) {
	ctx->go(Screen::ExerciceResultSummary);
}

namespace {

static inline StrView get_motivation_title(float ratio, uint64_t rnd_seed) {
	auto seed = random_num(0, 500, &rnd_seed);
	if (ratio >= 0.99f) {
		static constexpr Arr<StrView, 5> titles = {
			  "Ausgezeichnet!"_v, "Perfekt!"_v,     "Hervorragend!"_v,
			  "Meisterhaft!"_v,   "Fantastisch!"_v,
		};
		return titles[seed % titles.size()];
	}
	if (ratio >= 0.70f) {
		static constexpr Arr<StrView, 5> titles = {
			  "Gut gemacht!"_v,    "Sehr gut!"_v, "Klasse!"_v,
			  "Tolle Leistung!"_v, "Stark!"_v,
		};
		return titles[seed % titles.size()];
	}
	if (ratio >= 0.40f) {
		static constexpr Arr<StrView, 4> titles = {
			  "Guter Fortschritt!"_v,
			  "Weiter so!"_v,
			  "Auf gutem Weg!"_v,
			  "Gute Übung!"_v,
		};
		return titles[seed % titles.size()];
	}
	static constexpr Arr<StrView, 4> titles = {
		  "Dranbleiben!"_v,
		  "Übung macht den Meister!"_v,
		  "Jeder Schritt zählt!"_v,
		  "Nicht aufgeben!"_v,
	};
	return titles[seed % titles.size()];
}

} // namespace

void screen_exercise_summary_draw(AppContext *ctx) {
	const auto &es = ctx->exercises;
	const Size total = es.exercise_total();
	const Size correct = es.correct_exercise_count;
	const Size to_review = (total > correct) ? (total - correct) : 0;
	const float ratio =
		  total > 0 ? (static_cast<float>(correct) / static_cast<float>(total))
					: 0.0f;
	const int percent = static_cast<int>(ratio * 100.0f + 0.5f);

	const uint64_t title_rnd_seed = static_cast<uint64_t>(
		  correct * 10 + total * 20 + es.exercises.first().points_earned * 30);

	CLAY(CLAY_ID("ExerciseSummaryRoot"),
	     {
			   .layout =
					 {
						   .sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)},
						   .padding =
								 {
									   .left = sizes()->space.lg,
									   .right = sizes()->space.lg,
									   .top = sizes()->space.md,
									   .bottom = sizes()->space.lg,
								 },
						   .childGap = sizes()->space.lg,
						   .childAlignment = {CLAY_ALIGN_X_CENTER,
	                                          CLAY_ALIGN_Y_TOP},
						   .layoutDirection = CLAY_TOP_TO_BOTTOM,
					 },
			   .backgroundColor = theme()->surface,
		 }) {

		CLAY(CLAY_ID("SummaryResultCard"),
		     {
				   .layout =
						 {
							   .sizing = {CLAY_SIZING_GROW(0),
		                                  CLAY_SIZING_PERCENT(0.6)},
							   .padding = sizes()->pad.card,
							   .childGap = sizes()->space.md,
							   .childAlignment = {CLAY_ALIGN_X_CENTER,
		                                          CLAY_ALIGN_Y_CENTER},
							   .layoutDirection = CLAY_TOP_TO_BOTTOM,
						 },
				   .backgroundColor = theme()->surfaceContainerLow,
				   .cornerRadius = sizes()->radius.lg,
			 }) {

			draw_text(get_motivation_title(ratio, title_rnd_seed),
			          theme()->onSurface, sizes()->font.title_lg, FontID::MAIN,
			          CLAY_TEXT_WRAP_WORDS, CLAY_TEXT_ALIGN_CENTER);

			StrBuilder percent_str{};
			percent_str.push(ctx->arena_frame,
			                 StrView::from_number(ctx->arena_frame, percent));
			percent_str.push(ctx->arena_frame, "%"_v);

			draw_text(percent_str.join(ctx->arena_frame), theme()->primary,
			          sizes()->font.display, FontID::MAIN, CLAY_TEXT_WRAP_WORDS,
			          CLAY_TEXT_ALIGN_CENTER);

			// StrBuilder score_str{};
			// score_str.push(ctx->arena_frame,
			//                StrView::from_number(ctx->arena_frame, correct));
			// score_str.push(ctx->arena_frame, " / "_v);
			// score_str.push(ctx->arena_frame,
			//                StrView::from_number(ctx->arena_frame, total));
			// score_str.push(ctx->arena_frame, " correct"_v);
			//
			// draw_text(score_str.join(ctx->arena_frame),
			//           theme()->onSurfaceContainer,
			//           static_cast<uint16_t>(udpi(16.f)), FontID::MAIN,
			//           CLAY_TEXT_WRAP_WORDS, CLAY_TEXT_ALIGN_CENTER);

			CLAY(CLAY_ID("StatBadgesRow"),
			     {
					   .layout =
							 {
								   .sizing = {CLAY_SIZING_GROW(0),
			                                  CLAY_SIZING_FIT(0)},
								   .childGap = sizes()->space.sm,
								   .childAlignment = {CLAY_ALIGN_X_CENTER,
			                                          CLAY_ALIGN_Y_CENTER},
								   .layoutDirection = CLAY_LEFT_TO_RIGHT,
							 },
				 }) {
				Clay_ElementDeclaration badge_template = {
					  .layout =
							{
								  .padding = sizes()->pad.badge_compact,
								  .childGap = sizes()->space.xs,
								  .childAlignment = {CLAY_ALIGN_X_CENTER,
				                                     CLAY_ALIGN_Y_CENTER},
								  .layoutDirection = CLAY_LEFT_TO_RIGHT,
							},
					  .backgroundColor = theme()->rightContainer,
					  .cornerRadius = sizes()->radius.sm,
				};
				if (correct > 0) {
					CLAY(CLAY_ID("CorrectBadge"), badge_template) {
						draw_text("✓"_v, theme()->onRightContainer,
						          sizes()->font.label_md, FontID::MAIN);
						draw_text(
							  StrView::from_number(ctx->arena_frame, correct),
							  theme()->onRightContainer, sizes()->font.label_md,
							  FontID::MAIN);
					}
				}

				if (to_review > 0) {
					badge_template.backgroundColor =
						  theme()->surfaceContainerHigh;

					CLAY(CLAY_ID("ReviewBadge"), badge_template) {

						StrBuilder r_text{};
						r_text.push(ctx->arena_frame,
						            StrView::from_number(ctx->arena_frame,
						                                 to_review));
						r_text.push(ctx->arena_frame, tr()->screen_exercise_summary_to_review);

						draw_text(r_text.join(ctx->arena_frame),
						          theme()->onSurfaceContainerHigh,
						          sizes()->font.label_md, FontID::MAIN);
					}
				}
			}
		}

		CLAY(CLAY_ID("SummaryActions"),
		     {
				   .layout =
						 {
							   .sizing = {CLAY_SIZING_GROW(0),
		                                  CLAY_SIZING_GROW(0)},
							   .childGap = sizes()->space.md,
							   .childAlignment = {CLAY_ALIGN_X_CENTER,
		                                          CLAY_ALIGN_Y_CENTER},
							   .layoutDirection = CLAY_LEFT_TO_RIGHT,
						 },
			 }) {

			const bool has_errors = (to_review > 0);

			auto sec_btn =
				  mobile_button(ctx, CLAY_ID("SummarySecBtn"), tr()->screen_exercise_summary_action_review_all);

			if (sec_btn.activated()) {
				ctx->exercises.build_result_reviews(ctx->arena_frame, false);
				if (!ctx->exercises.results.is_empty()) {
					screen_exercise_review_push(ctx);
				}
			}

			auto main_btn =
				  mobile_button(ctx, CLAY_ID("SummaryMainBtn"),
			                    has_errors ? tr()->screen_exercise_summary_action_review_errors : tr()->screen_exercise_summary_action_next_round,
			                    mobile_button_style_primary());

			if (main_btn.activated()) {
				if (has_errors) {
					ctx->exercises.build_result_reviews(ctx->arena_frame, true);
					if (!ctx->exercises.results.is_empty()) {
						screen_exercise_review_push(ctx);
					}
				} else {
					screen_exercise_go(ctx, true);
				}
			}
		}
	}
}
