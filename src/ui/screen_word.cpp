#include "screen_helpers.h"
#include "ui/dpi.h"

void screen_word_push(AppContext *ctx, WordId word_id) {
	// TODO: implement
	(void)word_id;
	ctx->push(Screen::Word);
}

void screen_word_draw(AppContext *ctx) {
	draw_text("word screen"_v, theme()->onSurface, udpi(20));
}
