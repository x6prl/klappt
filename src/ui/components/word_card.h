#pragma once

#include "app/app_context.h"
#include "domain/words.h"

static constexpr float WORD_CARD_HEIGHT = 30.f;
static constexpr float WORD_CARD_ROW_HEIGHT = 34.f;

bool word_card_longtap(AppContext *ctx, Clay_ElementId id, const Word &w);
bool word_card_tap(AppContext *ctx, Clay_ElementId id, const Word &w);
void word_card_with_due(AppContext *ctx, Clay_ElementId id, const Word &w, int due_mark);
