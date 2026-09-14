#include "screen_helpers.h"

#include "ui/components/switch_button.h"
#include "ui/components/text_input.h"

#include "ui/sizes.h"

namespace {

static StrView word_type_name(WordType type) {
	switch (type) {
	case WordType::Noun:
		return "Noun"_v;
	case WordType::Verb:
		return "Verb"_v;
	case WordType::Adj:
		return "Adj"_v;
	case WordType::Phrase:
		return "Phrase"_v;
	case WordType::Nil:
		return "Nil"_v;
	}
	return "Unknown"_v;
}

static StrView mode_name(Engine::Mode mode) {
	switch (mode) {
	case Engine::Mode::Entire:
		return "Entire"_v;
	case Engine::Mode::Gaps:
		return "Gaps"_v;
	case Engine::Mode::Chunks:
		return "Chunks"_v;
	case Engine::Mode::Compose:
		return "Compose"_v;
	case Engine::Mode::Count:
		return "Count"_v;
	}
	return "Unknown"_v;
}

static StrView gender_name(Gender gender) {
	switch (gender) {
	case Gender::m:
		return "der"_v;
	case Gender::f:
		return "die"_v;
	case Gender::n:
		return "das"_v;
	case Gender::none:
		return "-"_v;
	case Gender::unknown:
		return "?"_v;
	}
	return "?"_v;
}

static void assign_buffer(MobileTextInputBuffer &dst, StrView src) {
	MobileTextInputBuffer next{};
	Size copy_size = src.size;
	if (copy_size > MobileTextInputBuffer::MAX_SIZE - 1) {
		copy_size = MobileTextInputBuffer::MAX_SIZE - 1;
	}
	if (copy_size > 0) {
		SDL_memcpy(next.data, src.data, copy_size);
	}
	next.size = copy_size;
	dst = next;
}

static bool is_blank(StrView text) {
	text.mut_trim();
	return !text;
}

static StrView lemma_required_error(WordType type) {
	switch (type) {
	case WordType::Noun:
		return "Noun lemma is required"_v;
	case WordType::Verb:
		return "Verb infinitive is required"_v;
	case WordType::Adj:
		return "Adjective lemma is required"_v;
	case WordType::Phrase:
		return "Phrase text is required"_v;
	case WordType::Nil:
		return "Choose word type"_v;
	}
	return "Choose word type"_v;
}

static void validate(WordEditState &edit) {
	edit.valid = false;
	edit.validation_error = {};
	if (edit.type == WordType::Nil || is_blank(edit.lemma.view())) {
		edit.validation_error = lemma_required_error(edit.type);
		return;
	}
	if (edit.type == WordType::Noun && edit.gender == Gender::unknown) {
		edit.validation_error = "Noun gender is required"_v;
		return;
	}

	edit.valid = true;
}

static void init_edit_from_view(AppContext *ctx) {
	auto &view = *ctx->word_view_state;
	auto &edit = *ctx->word_edit_state;
	const auto &word = view.word_copy;

	edit = {};
	edit.word_id = view.word_id;
	edit.type = word.type;
	edit.in_learning_list = word.in_learning_list;
	edit.was_learned = word.was_learned;
	edit.has_learning_state = view.has_learning_state;
	if (view.has_learning_state) {
		edit.mode = view.learning_state_copy.mode;
	}
	assign_buffer(edit.translations_raw, word.translations_raw);

	switch (word.type) {
	case WordType::Noun:
		edit.gender = word.n.gender;
		assign_buffer(edit.lemma, word.n.lemma);
		assign_buffer(edit.plural_suffix, word.n.plural_suffix);
		break;
	case WordType::Verb:
		assign_buffer(edit.lemma, word.v.infinitive);
		assign_buffer(edit.third_person, word.v.third_person);
		assign_buffer(edit.praeteritum, word.v.praeteritum);
		assign_buffer(edit.auxv_and_past_participle,
		              word.v.auxv_and_past_participle);
		break;
	case WordType::Adj:
		edit.adjective_indeclinable = word.a.is_indeclinable;
		assign_buffer(edit.lemma, word.a.lemma);
		assign_buffer(edit.comparative, word.a.comparative);
		assign_buffer(edit.superlative, word.a.superlative);
		break;
	case WordType::Phrase:
		assign_buffer(edit.lemma, word.p.text);
		break;
	case WordType::Nil:
		break;
	}

	validate(edit);
}

static Word build_word_from_edit(Arena &arena, const WordEditState &edit) {
	Word word{};
	word.word_id = edit.word_id;
	word.type = edit.type;
	word.in_learning_list = edit.in_learning_list;
	word.was_learned = edit.was_learned;
	word.translations_raw = edit.translations_raw.view().copy(arena);

	switch (edit.type) {
	case WordType::Noun:
		word.n.gender = edit.gender;
		word.n.lemma = edit.lemma.view().copy(arena);
		word.n.plural_suffix = edit.plural_suffix.view().copy(arena);
		break;
	case WordType::Verb:
		word.v.infinitive = edit.lemma.view().copy(arena);
		word.v.third_person = edit.third_person.view().copy(arena);
		word.v.praeteritum = edit.praeteritum.view().copy(arena);
		word.v.auxv_and_past_participle =
			  edit.auxv_and_past_participle.view().copy(arena);
		break;
	case WordType::Adj:
		word.a.lemma = edit.lemma.view().copy(arena);
		word.a.comparative = edit.comparative.view().copy(arena);
		word.a.superlative = edit.superlative.view().copy(arena);
		word.a.is_indeclinable = edit.adjective_indeclinable;
		break;
	case WordType::Phrase:
		word.p.text = edit.lemma.view().copy(arena);
		break;
	case WordType::Nil:
		break;
	}

	return word;
}

static void update_learning_list_copy(AppContext *ctx, const Word &word) {
	if (!ctx->words) {
		return;
	}
	for (auto ref = ctx->words->begin(); ref < ctx->words->end();
	     ref.advance(ctx->words)) {
		if ((*ctx->words)[ref].word_id == word.word_id) {
			(*ctx->words)[ref] = word;
			save_words_dat(ctx->arena_frame, ctx->settings, *ctx->words);
			return;
		}
	}
}

static void save_edit(AppContext *ctx) {
	auto &edit = *ctx->word_edit_state;
	validate(edit);
	if (!edit.valid) {
		return;
	}

	Word word = build_word_from_edit(ctx->arena, edit);
	ctx->word_store.save(ctx->arena_frame, word);
	update_learning_list_copy(ctx, word);

	auto &view = *ctx->word_view_state;
	view.word_copy = word;
	if (view.has_learning_state) {
		view.learning_state_copy.mode = edit.mode;
		if (!ctx->states.set(view.word_id, view.learning_state_copy)) {
			ctx->app_status.push_error("Cannot save learning state"_v);
		}
	}

	ctx->pop();
}

static MobileButtonStyle enum_button_style(bool selected) {
	auto style = selected ? mobile_button_style_primary()
	                      : mobile_button_style_surface_container_high();
	// CHANGED: compact enum button dimensions mapped to sizes() tokens
	style.height =
		  static_cast<float>(sizes()->space.xxl + sizes()->space.sm); // 40dp
	style.padding_x = sizes()->space.md;
	style.padding_y = sizes()->space.sm;
	style.corner_radius = sizes()->radius.md.topLeft;
	style.font_size = sizes()->font.body_sm; // 14sp
	return style;
}

static void draw_row_label(AppContext *ctx, Clay_ElementId id, StrView label) {
	CLAY(id,
	     {.layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0)}}}) {
		// CHANGED: replaced udpi(16) with sizes()->font.body_md (16sp)
		draw_text(label, theme()->onSurface, sizes()->font.body_md,
		          FontID::MAIN, CLAY_TEXT_WRAP_WORDS, CLAY_TEXT_ALIGN_LEFT);
	}
}

static void draw_field(AppContext *ctx, Clay_ElementId id,
                       Clay_ElementId input_id, MobileTextInputBuffer *buffer,
                       StrView placeholder) {
	CLAY(id, {.layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0)},
	                     // CHANGED: replaced udpi(4.f) with sizes()->space.xs
	                     .childGap = sizes()->space.xs,
	                     .layoutDirection = CLAY_TOP_TO_BOTTOM}}) {
		// CHANGED: replaced udpi(14) with sizes()->font.body_sm (14sp)
		draw_text(placeholder, theme()->onSurface, sizes()->font.body_sm,
		          FontID::MAIN, CLAY_TEXT_WRAP_WORDS, CLAY_TEXT_ALIGN_LEFT);
		if (auto input = mobile_text_input(ctx, input_id, buffer, placeholder);
		    input.changed || input.submitted || input.blurred) {
			validate(*ctx->word_edit_state);
			ctx->push_one_frame();
		}
	}
}

template <typename T, typename NameFn, typename IdFn, typename SelectFn>
static void draw_enum_row(AppContext *ctx, Clay_ElementId row_id,
                          Clay_ElementId label_id, StrView label,
                          const T *values, Size value_count, T selected,
                          NameFn name, IdFn id, SelectFn select) {
	CLAY(row_id,
	     {.layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0)},
	                 // CHANGED: replaced udpi(4.f) with sizes()->space.xs
	                 .padding = CLAY_PADDING_ALL(sizes()->space.xs),
	                 // CHANGED: replaced udpi(6.f) with sizes()->space.xs
	                 .childGap = sizes()->space.xs,
	                 .childAlignment = {CLAY_ALIGN_X_CENTER,
	                                    CLAY_ALIGN_Y_CENTER}}}) {
		draw_row_label(ctx, label_id, label);
		for (Size i = 0; i < value_count; ++i) {
			auto btn = mobile_button(ctx, id(i), name(values[i]),
			                         enum_button_style(selected == values[i]));
			if (btn.activated()) {
				select(values[i]);
			}
		}
	}
}

static void draw_word_type_row(AppContext *ctx) {
	auto &edit = *ctx->word_edit_state;
	const WordType types[] = {WordType::Noun, WordType::Verb, WordType::Adj,
	                          WordType::Phrase};
	draw_enum_row(
		  ctx, CLAY_ID("WordTypeRow"), CLAY_ID("TypeLabel"), "Type"_v, types, 4,
		  edit.type, word_type_name,
		  [](Size i) { return CLAY_IDI("TypeButton", i); },
		  [&](WordType type) {
			  edit.type = type;
			  validate(edit);
		  });
}

static void draw_gender_row(AppContext *ctx) {
	auto &edit = *ctx->word_edit_state;
	const Gender genders[] = {Gender::m, Gender::f, Gender::n, Gender::none};
	draw_enum_row(
		  ctx, CLAY_ID("GenderRow"), CLAY_ID("GenderLabel"), "Gender"_v,
		  genders, 4, edit.gender, gender_name,
		  [](Size i) { return CLAY_IDI("GenderButton", i); },
		  [&](Gender gender) {
			  edit.gender = gender;
			  validate(edit);
		  });
}

static void draw_mode_row(AppContext *ctx) {
	auto &edit = *ctx->word_edit_state;
	if (!edit.has_learning_state) {
		return;
	}
	Engine::Mode modes[Engine::MODE_COUNT]{};
	for (int i = 0; i < Engine::MODE_COUNT; ++i) {
		modes[i] = Engine::imode(i);
	}
	draw_enum_row(
		  ctx, CLAY_ID("ModeRow"), CLAY_ID("ModeLabel"), "Mode"_v, modes,
		  Engine::MODE_COUNT, edit.mode, mode_name,
		  [](Size i) { return CLAY_IDI("ModeButton", i); },
		  [&](Engine::Mode mode) {
			  edit.mode = mode;
			  validate(edit);
		  });
}

static void draw_type_fields(AppContext *ctx) {
	auto &edit = *ctx->word_edit_state;
	switch (edit.type) {
	case WordType::Noun:
		draw_gender_row(ctx);
		draw_field(ctx, CLAY_ID("LemmaField"), CLAY_ID("LemmaInput"),
		           &edit.lemma, "Lemma"_v);
		draw_field(ctx, CLAY_ID("PluralField"), CLAY_ID("PluralInput"),
		           &edit.plural_suffix, "Plural suffix"_v);
		break;
	case WordType::Verb:
		draw_field(ctx, CLAY_ID("InfinitiveField"), CLAY_ID("InfinitiveInput"),
		           &edit.lemma, "Infinitive"_v);
		draw_field(ctx, CLAY_ID("ThirdPersonField"),
		           CLAY_ID("ThirdPersonInput"), &edit.third_person,
		           "Third person"_v);
		draw_field(ctx, CLAY_ID("PraeteritumField"),
		           CLAY_ID("PraeteritumInput"), &edit.praeteritum,
		           "Praeteritum"_v);
		draw_field(ctx, CLAY_ID("ParticipleField"), CLAY_ID("ParticipleInput"),
		           &edit.auxv_and_past_participle,
		           "Auxiliary + past participle"_v);
		break;
	case WordType::Adj:
		draw_field(ctx, CLAY_ID("AdjectiveField"), CLAY_ID("AdjectiveInput"),
		           &edit.lemma, "Lemma"_v);
		CLAY(CLAY_ID("IndeclinableRow"),
		     {.layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0)},
		                 // CHANGED: replaced udpi(4.f) with sizes()->space.xs
		                 .padding = CLAY_PADDING_ALL(sizes()->space.xs),
		                 // CHANGED: replaced udpi(12.f) with sizes()->space.md
		                 .childGap = sizes()->space.md,
		                 .childAlignment = {CLAY_ALIGN_X_CENTER,
		                                    CLAY_ALIGN_Y_CENTER}}}) {
			draw_row_label(ctx, CLAY_ID("IndeclinableLabel"), "Indeclinable"_v);
			// CHANGED: switch uses sizes()->dim.icon_md automatically
			if (switch_button(ctx, CLAY_ID("IndeclinableSwitch"),
			                  edit.adjective_indeclinable)) {
				edit.adjective_indeclinable = !edit.adjective_indeclinable;
				validate(edit);
			}
		}
		if (!edit.adjective_indeclinable) {
			draw_field(ctx, CLAY_ID("ComparativeField"),
			           CLAY_ID("ComparativeInput"), &edit.comparative,
			           "Comparative"_v);
			draw_field(ctx, CLAY_ID("SuperlativeField"),
			           CLAY_ID("SuperlativeInput"), &edit.superlative,
			           "Superlative"_v);
		}
		break;
	case WordType::Phrase:
		draw_field(ctx, CLAY_ID("PhraseField"), CLAY_ID("PhraseInput"),
		           &edit.lemma, "Phrase"_v);
		break;
	case WordType::Nil:
		break;
	}
}

} // namespace

void screen_word_edit_push(AppContext *ctx) {
	init_edit_from_view(ctx);
	ctx->push(Screen::WordEdit);
}

void screen_word_edit_draw(AppContext *ctx) {
	KLAPPT_PROFILE_SCOPE_N("screen_word_edit_draw");
	auto &edit = *ctx->word_edit_state;
	validate(edit);

	CLAY(CLAY_ID("WordEditScreenShell"),
	     {.layout = {
				.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)},
				// CHANGED: replaced hardcoded udpi(8.f) with
	            // sizes()->pad.screen
				.padding = sizes()->pad.screen,
				// CHANGED: replaced udpi(8.f) with sizes()->space.sm (8dp)
				.childGap = sizes()->space.sm,
				.layoutDirection = CLAY_TOP_TO_BOTTOM,
		  }}) {
		CLAY(CLAY_ID("WordEditScroll"),
		     {.layout =
		            {
						  .sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)},
						  // CHANGED: replaced udpi(10.f) with sizes()->space.md
		                  // (12dp)
						  .childGap = sizes()->space.md,
						  .layoutDirection = CLAY_TOP_TO_BOTTOM,
					},
		      .clip = {.vertical = true,
		               .childOffset = Clay_GetScrollOffset()}}) {
			draw_word_type_row(ctx);
			draw_type_fields(ctx);
			draw_field(ctx, CLAY_ID("TranslationsField"),
			           CLAY_ID("TranslationsInput"), &edit.translations_raw,
			           "Translations"_v);
			draw_mode_row(ctx);
			if (!edit.valid) {
				// CHANGED: replaced udpi(15) with sizes()->font.body_sm (14sp)
				draw_text(edit.validation_error, theme()->error,
				          sizes()->font.body_sm, FontID::MAIN,
				          CLAY_TEXT_WRAP_WORDS, CLAY_TEXT_ALIGN_LEFT);
			}
		}

		CLAY(CLAY_ID("Buttons"),
		     {.layout = {
					.sizing =
						  {
								CLAY_SIZING_GROW(0),
								// CHANGED: replaced dpi(76.f) with standardized
		                        // bar height (64dp)
								CLAY_SIZING_FIXED(
									  sizes()->dim.bottom_bar_height),
						  },
					// CHANGED: replaced udpi(12.f) with sizes()->space.sm
					.padding = CLAY_PADDING_ALL(sizes()->space.sm),
					// CHANGED: replaced udpi(12.f) with sizes()->space.md
					.childGap = sizes()->space.md,
					.childAlignment = {CLAY_ALIGN_X_CENTER,
		                               CLAY_ALIGN_Y_CENTER},
			  }}) {
			auto style = edit.valid
			                   ? mobile_button_style_primary()
			                   : mobile_button_style_surface_container_high();
			style.font_id = FontID::ICONS;
			// CHANGED: icon font size bound to sizes()->dim.icon_md (24dp)
			style.font_size = static_cast<uint16_t>(sizes()->dim.icon_md);
			auto save =
				  mobile_button(ctx, CLAY_ID("SaveButton"), Icons::SAVE, style);
			if (save.activated() && edit.valid) {
				save_edit(ctx);
			}
		}
	}
}
