#include "screen_helpers.h"
#include "ui/components/button.h"
#include "ui/dpi.h"

void screen_start_go(AppContext *ctx) { ctx->go(Screen::Start); }

void screen_start_draw(AppContext *ctx) {
	CLAY(CLAY_ID("ScreenStart"),
	     {
			   .layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)},
	                      .padding = CLAY_PADDING_ALL(udpi(16.0f)),
	                      .childGap = udpi(14.0f),
	                      .childAlignment = {CLAY_ALIGN_X_CENTER,
	                                         CLAY_ALIGN_Y_CENTER},
	                      .layoutDirection = CLAY_TOP_TO_BOTTOM},
		 }) {
		auto go = mobile_button(ctx, CLAY_ID("go"), "Go"_v);
		if (go.activated()) {
			screen_exercise_go(ctx, false);
		}
	}
}
