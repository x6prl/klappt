#include "trs.h"

#include "translations/screen_settings.h"

namespace {
Lang current_lang = lang_ru;
}

UiTranslation *tr() { return &trs[current_lang]; }

void set_language(Lang lang) {
	if (lang < lang_COUNT) {
		current_lang = lang;
	}
}
