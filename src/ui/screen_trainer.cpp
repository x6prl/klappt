#include <SDL3/SDL_log.h>

#include "screen_helpers.h"

#include "app/worker.h"
#include "base/str_view.h"
#include "ui/components/button.h"
#include "ui/dpi.h"
#include "ui/themes.h"

void screen_trainer_go(AppContext *ctx) {
	ctx->go(Screen::Trainer);
}

void screen_trainer_draw(AppContext *ctx) {
	CLAY(CLAY_ID("ScreenStart"),
	     {
			   .layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)},
	                      .padding = CLAY_PADDING_ALL(udpi(16.0f)),
	                      .childGap = udpi(14.0f),
	                      .childAlignment = {CLAY_ALIGN_X_CENTER,
	                                         CLAY_ALIGN_Y_CENTER},
	                      .layoutDirection = CLAY_TOP_TO_BOTTOM},
		 }) {
		auto go = mobile_button(ctx, CLAY_ID("TrainButton"), "Train"_v);
		if (go.activated()) {
			screen_exercise_go(ctx, false);
		}
	}
	auto eheight = [](Size i) -> float { return 50.f; };
	auto edraw = [](AppContext *ctx, Size i, Clay_ElementId item_id) {
		CLAY(item_id, {
							.layout = {.sizing = {CLAY_SIZING_GROW(0),
		                                          CLAY_SIZING_FIXED(50)},
		                               .padding = CLAY_PADDING_ALL(udpi(16.0f)),
		                               .childGap = udpi(14.0f),
		                               .childAlignment = {CLAY_ALIGN_X_CENTER,
		                                                  CLAY_ALIGN_Y_CENTER},
		                               .layoutDirection = CLAY_TOP_TO_BOTTOM},
							.backgroundColor = theme()->onError,
							.cornerRadius = CLAY_CORNER_RADIUS(dpi(10.f)),
							.border = {.color = theme()->secondary,
		                               .width = CLAY_BORDER_OUTSIDE(udpi(1.f))},
					  }) {
			draw_text(StrView::from_number(ctx->arena_frame, i),
			          theme()->error);
			for (auto j = 0; false && j < i; ++j) {
				draw_text(StrView::concat(
								ctx->arena_frame, "draw i: "_v,
								StrView::from_number(ctx->arena_frame, j)),
				          theme()->error);
			}
		}
	};
}
