#include "screen_helpers.h"
#include "ui/dpi.h"

void screen_word_edit_push(AppContext *ctx, WordId word_id) {
	// TODO: implement
	(void)word_id;
	ctx->push(Screen::WordEdit);
}

void screen_word_edit_draw(AppContext *ctx) {
	draw_text("word edit screen"_v, theme()->onSurface, udpi(20));
}
