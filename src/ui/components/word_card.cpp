#include "word_card.h"

#include "../dpi.h"
#include "../themes.h"
#include "base/profiler.h"
#include "base/str_view.h"
#include "ui/tslt.h"
#include <charconv>

namespace {

void word_main(Clay_ElementId id, StrView pre, StrView main, StrView post,
               Clay_Color color, uint16_t font_size, uint16_t gap) {
	CLAY(id, {
				   .layout =
						 {
							   .sizing = {CLAY_SIZING_PERCENT(0.5f),
	                                      CLAY_SIZING_GROW(0)},
							   .childGap = gap,
							   .childAlignment = {CLAY_ALIGN_X_LEFT,
	                                              CLAY_ALIGN_Y_CENTER},
						 },
				   .clip = {.horizontal = true},
			 }) {
		auto pcolor = color;
		pcolor.a *= 0.5f;
		if (pre)
			CLAY_TEXT(pre.to_clay_string(),
			          CLAY_TEXT_CONFIG({
							.textColor = pcolor,
							.fontId = FontID::MAIN,
							.fontSize = font_size,
							.wrapMode = CLAY_TEXT_WRAP_NONE,
					  }));
		if (main)
			CLAY_TEXT(main.to_clay_string(),
			          CLAY_TEXT_CONFIG({
							.textColor = color,
							.fontId = FontID::MONOSPACE_REGULAR,
							.fontSize = font_size,
							.wrapMode = CLAY_TEXT_WRAP_NONE,
					  }));
		if (post)
			CLAY_TEXT(post.to_clay_string(),
			          CLAY_TEXT_CONFIG({
							.textColor = pcolor,
							.fontId = FontID::MAIN,
							.fontSize = font_size,
							.wrapMode = CLAY_TEXT_WRAP_NONE,
					  }));
	}
}

uint16_t translation_font_id(const AppContext *ctx) {
	return ctx->settings.tr_language == Settings::TranslationLanguage::Arabic
	             ? FontID::ARABIC_MAIN
	             : FontID::MAIN;
}

void word_second_col(Clay_ElementId id, StrView text, Clay_Color color,
                     uint16_t font_size, uint16_t font_id) {
	CLAY(id,
	     {
			   .layout =
					 {
						   .sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)},
						   .childAlignment = {CLAY_ALIGN_X_LEFT,
	                                          CLAY_ALIGN_Y_CENTER},
					 },
			   .clip = {.horizontal = true},
		 }) {
		auto first_translation = text.split_by(';').first;
		auto visible_text = first_translation.split_by('{').first;
		CLAY_TEXT(visible_text.to_clay_string(),
		          CLAY_TEXT_CONFIG({
						.textColor = color,
						.fontId = font_id,
						.fontSize = font_size,
						.wrapMode = CLAY_TEXT_WRAP_NONE,
				  }));
	}
}

} // namespace

TapSwipeLongTap::State word_card_words_list(AppContext *ctx, Clay_ElementId id,
                                            const Word &w) {
	KLAPPT_PROFILE_SCOPE_N("word_card_longtap");
	const auto padding = udpi(6.f);
	const auto height = dpi(WORD_CARD_HEIGHT);
	TapSwipeLongTap::State ret{TapSwipeLongTap::State::KeyUp};
	auto is_usual_card = w.in_learning_list == 0;

	CLAY(id,
	     {
			   .layout =
					 {
						   .sizing = {CLAY_SIZING_GROW(0),
	                                  CLAY_SIZING_FIXED(height)},
						   .padding = CLAY_PADDING_ALL(padding),
						   .childGap = udpi(8.f),
						   .childAlignment = {CLAY_ALIGN_X_LEFT,
	                                          CLAY_ALIGN_Y_CENTER},
					 },
			   .backgroundColor = is_usual_card ? theme()->surfaceContainer
	                                            : theme()->surfaceContainerHigh,
			   .cornerRadius = CLAY_CORNER_RADIUS(dpi(10.f)),
			   .border = {.color = is_usual_card ? theme()->surfaceContainer
	                                             : theme()->secondary,
	                      .width = CLAY_BORDER_OUTSIDE(udpi(1.f))},
		 }) {
		StrView pre_main{};
		StrView main;
		StrView post_main{};
		switch (w.type) {
		case WordType::Noun:
			if (w.n.gender != Gender::none)
				pre_main = gender_to_article_nominative_strview(w.n.gender);
			main = w.n.lemma;
			post_main = w.n.plural_suffix;
			break;
		case WordType::Verb:
			main = w.v.infinitive;
			break;
		case WordType::Adj:
			main = w.a.lemma;
			break;
		case WordType::Phrase:
			main = w.p.text;
			break;
		default:
			break;
		}
		auto col = is_usual_card ? theme()->onSurfaceContainer
		                         : theme()->onSurfaceContainerHigh;
		word_main(CLAY_IDI("Main", w.word_id.value), pre_main, main, post_main,
		          col, udpi(13), udpi(3));

		auto tr = w.translations_raw;
		word_second_col(CLAY_IDI("SecondCol", id.id), tr, col, udpi(13),
		                translation_font_id(ctx));

		bool is_tap_or_longtap = (ctx->tslt.state == TapSwipeLongTap::Tap ||
		                          ctx->tslt.state == TapSwipeLongTap::LongTap);
		if (Clay_Hovered() && is_tap_or_longtap) {
			ret = ctx->tslt.state;
		}
	}
	return ret;
}

bool word_card_tap(AppContext *ctx, Clay_ElementId id, const Word &w) {
	KLAPPT_PROFILE_SCOPE_N("word_card_tap");
	const auto padding = udpi(6.f);
	const auto height = dpi(WORD_CARD_HEIGHT);
	bool ret{false};
	auto is_usual_card = w.in_learning_list == 0;

	CLAY(id,
	     {
			   .layout =
					 {
						   .sizing = {CLAY_SIZING_GROW(0),
	                                  CLAY_SIZING_FIXED(height)},
						   .padding = CLAY_PADDING_ALL(padding),
						   .childGap = udpi(8.f),
						   .childAlignment = {CLAY_ALIGN_X_LEFT,
	                                          CLAY_ALIGN_Y_CENTER},
					 },
			   .backgroundColor = is_usual_card ? theme()->surfaceContainer
	                                            : theme()->surfaceContainerHigh,
			   .cornerRadius = CLAY_CORNER_RADIUS(dpi(10.f)),
			   .border = {.color = is_usual_card ? theme()->surfaceContainer
	                                             : theme()->secondary,
	                      .width = CLAY_BORDER_OUTSIDE(udpi(1.f))},
		 }) {
		StrView pre_main{};
		StrView main;
		StrView post_main{};
		switch (w.type) {
		case WordType::Noun:
			if (w.n.gender != Gender::none)
				pre_main = gender_to_article_nominative_strview(w.n.gender);
			main = w.n.lemma;
			post_main = w.n.plural_suffix;
			break;
		case WordType::Verb:
			main = w.v.infinitive;
			break;
		case WordType::Adj:
			main = w.a.lemma;
			break;
		case WordType::Phrase:
			main = w.p.text;
			break;
		default:
			break;
		}
		auto col = is_usual_card ? theme()->onSurfaceContainer
		                         : theme()->onSurfaceContainerHigh;
		word_main(CLAY_IDI("Main", w.word_id.value), pre_main, main, post_main,
		          col, udpi(13), udpi(4));

		auto tr = w.translations_raw;
		word_second_col(CLAY_IDI("SecondCol", id.id), tr, col, udpi(13),
		                translation_font_id(ctx));

		if (ctx->tslt.is_tap() && Clay_Hovered()) {
			ret = true;
		}
	}
	return ret;
}

bool word_card_with_due(AppContext *ctx, Clay_ElementId id, const Word &w,
                        int due_mark) {
	KLAPPT_PROFILE_SCOPE_N("word_card_with_due");
	const auto padding = udpi(6.f);
	const auto height = dpi(WORD_CARD_HEIGHT);
	bool ret{false};

	CLAY(id,
	     {
			   .layout =
					 {
						   .sizing = {CLAY_SIZING_GROW(0),
	                                  CLAY_SIZING_FIXED(height)},
						   .padding = CLAY_PADDING_ALL(padding),
						   .childGap = udpi(8.f),
						   .childAlignment = {CLAY_ALIGN_X_LEFT,
	                                          CLAY_ALIGN_Y_CENTER},
					 },
			   .backgroundColor = theme()->surfaceContainer,

			   .cornerRadius = CLAY_CORNER_RADIUS(dpi(10.f)),
			   // .border = {.color = is_usual_card ? theme()->surfaceContainer
	           //                                         : theme()->secondary,
	           // .width = CLAY_BORDER_OUTSIDE(udpi(1.f))},
		 }) {
		StrView pre_main{};
		StrView main;
		StrView post_main{};
		switch (w.type) {
		case WordType::Noun:
			if (w.n.gender != Gender::none)
				pre_main = gender_to_article_nominative_strview(w.n.gender);
			main = w.n.lemma;
			post_main = w.n.plural_suffix;
			break;
		case WordType::Verb:
			main = w.v.infinitive;
			break;
		case WordType::Adj:
			main = w.a.lemma;
			break;
		case WordType::Phrase:
			main = w.p.text;
			break;
		default:
			break;
		}
		auto col = theme()->onSurfaceContainer;
		word_main(CLAY_IDI("Main", w.word_id.value), pre_main, main, post_main,
		          col, udpi(13), udpi(4));

		CLAY(CLAY_IDI("SecondColWrapper", id.id),
		     {
				   .layout =
						 {
							   .sizing = {CLAY_SIZING_GROW(0),
		                                  CLAY_SIZING_FIXED(height)},
							   .padding = {.right = padding},
							   .childAlignment = {CLAY_ALIGN_X_LEFT,
		                                          CLAY_ALIGN_Y_CENTER},
						 },
				   .backgroundColor = theme()->surfaceContainer,
			 }) {
			auto tr = w.translations_raw;
			word_second_col(CLAY_IDI("SecondCol", id.id), tr, col, udpi(13),
			                translation_font_id(ctx));
		}
		StrView already = ""_v;
		if (due_mark < 0) {
			auto color_due = theme()->primary;
			color_due.a = 255.f;
			CLAY_TEXT(already.to_clay_string(),
			          CLAY_TEXT_CONFIG({
							.textColor = color_due,
							.fontId = FontID::ICONS,
							.fontSize = udpi(13),
							.wrapMode = CLAY_TEXT_WRAP_NONE,
					  }));
		} else {
			auto color_wait = theme()->secondary;
			color_wait.a = 255.f;
			// TODO: cache it
			char str_buf[16];
			auto min = due_mark / 60;
			auto hours = due_mark / (60 * 60);
			auto days = due_mark / (60 * 60 * 24);
			auto weeks = due_mark / (60 * 60 * 24 * 7);
			auto to_draw = min;
			char time_ch = 'm';
			if (!weeks) {
				if (!days) {
					if (hours) {
						to_draw = hours;
						time_ch = 'h';
					} else {
						if (!min) {
							to_draw = due_mark;
							time_ch = 's';
						}
					}
				} else {
					to_draw = days;
					time_ch = 'd';
				}
			} else {
				to_draw = weeks;
				time_ch = 'w';
			}
			auto res = std::to_chars(str_buf, str_buf + sizeof(str_buf) - 1,
			                         to_draw);
			*res.ptr = time_ch;
			already =
				  StrView::from_chars(ctx->tmparena, str_buf,
			                          static_cast<Size>(res.ptr - str_buf + 1));

			// SDL_Log("due_mark %d %d %d", due_mark, min, hours);
			// SDL_Log("already " StrView_Fmt, StrView_Arg(already));
			CLAY_TEXT(already.to_clay_string(),
			          CLAY_TEXT_CONFIG({
							.textColor = color_wait,
							.fontId = FontID::MAIN,
							// .fontId = FontID::ICONS,
							.fontSize = udpi(13),
							.wrapMode = CLAY_TEXT_WRAP_NONE,
					  }));
		}

		if (Clay_Hovered() && ctx->tslt.state == TapSwipeLongTap::Tap) {
			ret = true;
		}
	}
	return ret;
}
