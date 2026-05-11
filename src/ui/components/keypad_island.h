#pragma once

#include "app/app_context.h"
#include "base/str_view.h"

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
