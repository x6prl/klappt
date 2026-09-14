#pragma once

#include <cstdint>

#include <clay/clay.h>

enum class DensityMode : uint8_t { Compact = 0, Normal, Comfortable };
enum class FontScaleLevel : uint8_t { Small = 0, Normal, Large, XL, XXL };

struct Sizes {
	using u_t = uint16_t;
	using f_t = float;

	// 1. Spacing grid based on 4dp unit
	struct {
		u_t zero; // 0dp
		u_t xs;   // 4dp  - micro gaps (progress bars, inline accents)
		u_t sm;   // 8dp  - standard row element spacing
		u_t md;   // 12dp - block separation
		u_t lg;   // 16dp - screen & card margins
		u_t xl;   // 24dp - major section separators
		u_t xxl;  // 32dp - hero spacers
	} space;

	// 2. Typography scale
	struct {
		u_t label_sm; // 11sp - metadata, IPA, due timestamps, grammar tags
		u_t label_md; // 12sp - secondary text, translations in dense lists
		u_t body_sm;  // 14sp - card translations, examples, dictionary text
		u_t body_md;  // 16sp - primary content, form values, exercise choices
		u_t title_md; // 20sp - section headers (Examples), app bar title
		u_t title_lg; // 24sp - card title, exercise prompts
		u_t display;  // 56sp - hero review counters
	} font;

	// 3. Shape corner radii
	struct {
		Clay_CornerRadius xs; // 4dp  - keyword pills, valency tags
		Clay_CornerRadius sm; // 8dp  - badges, status markers, chips
		Clay_CornerRadius
			  md; // 12dp - input fields, list items, nested form blocks
		Clay_CornerRadius lg;   // 16dp - word cards, hero containers
		Clay_CornerRadius full; // 9999 - pill buttons, circular counters
	} radius;

	// 4. Paddings
	struct {
		Clay_Padding screen; // 16dp horizontal, 12dp vertical
		Clay_Padding card;   // 16dp uniform
		Clay_Padding
			  card_compact;     // 12dp horizontal, 8dp vertical (forms block)
		Clay_Padding list_item; // 12dp horizontal, 8dp vertical
		Clay_Padding badge;     // 8dp horizontal, 4dp vertical
		Clay_Padding badge_compact; // 6dp horizontal, 2dp vertical
		Clay_Padding example_quote; // 12dp left, 8dp right, 8dp vertical
	} pad;

	// 5. Component dimensions
	struct {
		f_t min_touch_target;    // 48dp - accessibility requirement (WCAG / M3)
		f_t app_bar_height;      // 64dp - top app bar
		f_t bottom_bar_height;   // 64dp - bottom navigation bar
		f_t word_card_height;    // 50dp - single item row in dictionary list
		f_t action_btn_size;     // 44dp - square action buttons (copy, back,
		                         // refresh)
		f_t accent_border_width; // 3dp  - example card vertical border
		f_t form_label_width;    // 100dp- fixed width column for "er/sie/es:"
		f_t icon_sm;             // 16dp
		f_t icon_md;             // 24dp
	} dim;

	f_t scale{1.0f};
	DensityMode density{DensityMode::Normal};
	FontScaleLevel font_scale{1};
};

const Sizes *sizes();
void sizes_set_scale(float scale, DensityMode density,
                     FontScaleLevel font_scale);
