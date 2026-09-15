#pragma once

#include "app/app_context.h"

TapSwipeLongTap::State word_card_dictionary(AppContext *ctx, Clay_ElementId id, const Word &w);
bool word_card_suggestions(AppContext *ctx, Clay_ElementId id, const Word &w);
TapSwipeLongTap::State word_card_learning_list(AppContext *ctx, Clay_ElementId id, const Word &w, int due_mark);
