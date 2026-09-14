#pragma once

#include "app/app_context.h"

TapSwipeLongTap::State word_card_for_words_list(AppContext *ctx, Clay_ElementId id, const Word &w);
bool word_card_tap(AppContext *ctx, Clay_ElementId id, const Word &w);
TapSwipeLongTap::State word_card_with_due(AppContext *ctx, Clay_ElementId id, const Word &w, int due_mark);
