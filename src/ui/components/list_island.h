#pragma once

#include "app/app_context.h"
#include "base/str_view.h"

#include "keypad_island.h"

void list_island_invalidate();
void list_island(AppContext *ctx, Clay_ElementId id, const StrView *labels,
                 Size label_count, const IslandStyle &style,
                 IslandTapCallback on_tap);

void list_island_commit(AppContext *ctx);
