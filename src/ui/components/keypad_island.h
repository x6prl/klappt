#pragma once

#include "app/app_context.h"
#include "base/str_view.h"

constexpr SDL_FColor clay_color_to_SDL_FColor_norm(Clay_Color c) {
	return {
		  (float)c.r / 255.0f,
		  (float)c.g / 255.0f,
		  (float)c.b / 255.0f,
		  (float)c.a / 255.0f,
	};
}

struct IslandStyle {
	Clay_Color surface{};
	Clay_Color background{};
	Clay_Color divider_color{};
	Clay_Color shadow_color{};
	Clay_Color text{};
};

using IslandTapCallback = void (*)(AppContext *ctx, int tapped);

void keypad_island_invalidate();
void keypad_island(AppContext *ctx, Clay_ElementId id, const StrView *labels,
                   Size label_count, const IslandStyle &style,
                   IslandTapCallback on_tap);

void keypad_island_commit(AppContext *ctx);
