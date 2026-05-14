#pragma once

#include "app/app_context.h"
#include "ui/tslt.h"

static constexpr float WORD_CARD_HEIGHT = 30.f;
static constexpr float WORD_CARD_ROW_HEIGHT = 34.f;

TapSwipeLongTap::State word_card_words_list(AppContext *ctx, Clay_ElementId id, const Word &w);
bool word_card_tap(AppContext *ctx, Clay_ElementId id, const Word &w);
bool word_card_with_due(AppContext *ctx, Clay_ElementId id, const Word &w, int due_mark);
