#include "sizes.h"

#include <cmath>
#include <utility>

namespace {

static Sizes app_sizes{};

// 0 = Small, 1 = Normal, 2 = Large, 3 = XL, 4 = XXL
constexpr float FONT_MULTIPLIERS[5] = {
	  0.85f, 1.00f, 1.15f, 1.30f, 1.45f,
};

Sizes sizes_create(float scale, DensityMode density,
                   FontScaleLevel font_scale) {
	if (scale <= 0.1f) {
		scale = 1.0f;
	}

	const float font_mult = FONT_MULTIPLIERS[std::to_underlying(font_scale)];
	const float eff_font_scale = scale * font_mult;

	float base_screen_pad_h = 16.0f;
	float base_screen_pad_v = 12.0f;
	float base_card_h = 50.0f;
	float base_touch_target = 48.0f;
	float base_list_pad_v = 8.0f;
	float base_radius_md = 12.0f;

	switch (density) {
	case DensityMode::Compact:
		base_screen_pad_h = 12.0f;
		base_screen_pad_v = 8.0f;
		base_card_h = 44.0f;
		base_touch_target = 44.0f;
		base_list_pad_v = 6.0f;
		base_radius_md = 8.0f;
		break;
	case DensityMode::Normal:
		base_screen_pad_h = 16.0f;
		base_screen_pad_v = 12.0f;
		base_card_h = 50.0f;
		base_touch_target = 48.0f;
		base_list_pad_v = 8.0f;
		base_radius_md = 12.0f;
		break;
	case DensityMode::Comfortable:
		base_screen_pad_h = 20.0f;
		base_screen_pad_v = 16.0f;
		base_card_h = 58.0f;
		base_touch_target = 52.0f;
		base_list_pad_v = 10.0f;
		base_radius_md = 14.0f;
		break;
	}

	// compensate font growth for cards
	if (std::to_underlying(font_scale) >=
	    std::to_underlying(FontScaleLevel::Large)) {
		// Large ->  +4.5
		// XL    ->  +9.0
		// XXL   -> +13.5
		base_card_h +=
			  static_cast<float>(std::to_underlying(font_scale) - 1) * 4.5f;
	}

	// radii dempfing
	const float damped_radius_scale = 1.0f + (scale - 1.0f) * 0.5f;

	const auto to_u_t = [scale](float dp) -> Sizes::u_t {
		return static_cast<Sizes::u_t>(std::round(dp * scale));
	};

	const auto to_f_t = [scale](float dp) -> Sizes::f_t {
		return std::round(dp * scale);
	};

	const auto to_font_t = [eff_font_scale](float sp) -> Sizes::u_t {
		return static_cast<Sizes::u_t>(std::round(sp * eff_font_scale));
	};

	Sizes s{
		  .scale = scale,
		  .density = density,
		  .font_scale = font_scale,
	};

	// 1. Spacing
	s.space = {
		  .zero = 0,
		  .xs = to_u_t(4.0f),
		  .sm = to_u_t(8.0f),
		  .md = to_u_t(12.0f),
		  .lg = to_u_t(16.0f),
		  .xl = to_u_t(24.0f),
		  .xxl = to_u_t(32.0f),
	};

	// 2. Fonts (M3 scale)
	s.font = {
		  .label_sm = to_font_t(11.0f),
		  .label_md = to_font_t(12.0f),
		  .body_sm = to_font_t(14.0f),
		  .body_md = to_font_t(16.0f),
		  .title_md = to_font_t(20.0f),
		  .title_lg = to_font_t(24.0f),
		  .display = to_font_t(56.0f),
	};

	// 3. Radii
	s.radius = {
		  .xs = CLAY_CORNER_RADIUS(4.0f * damped_radius_scale),
		  .sm = CLAY_CORNER_RADIUS(8.0f * damped_radius_scale),
		  .md = CLAY_CORNER_RADIUS(base_radius_md * damped_radius_scale),
		  .lg = CLAY_CORNER_RADIUS(16.0f * damped_radius_scale),
		  .full = CLAY_CORNER_RADIUS(9999.0f),
	};

	// 4. Paddings
	s.pad = {
		  .screen = {.left = to_u_t(base_screen_pad_h),
	                 .right = to_u_t(base_screen_pad_h),
	                 .top = to_u_t(base_screen_pad_v),
	                 .bottom = to_u_t(base_screen_pad_v)},
		  .card = {.left = s.space.lg,
	               .right = s.space.lg,
	               .top = s.space.lg,
	               .bottom = s.space.lg},
		  .card_compact = {.left = s.space.md,
	                       .right = s.space.md,
	                       .top = s.space.sm,
	                       .bottom = s.space.sm},
		  .list_item = {.left = s.space.md,
	                    .right = s.space.md,
	                    .top = to_u_t(base_list_pad_v),
	                    .bottom = to_u_t(base_list_pad_v)},
		  .badge = {.left = s.space.sm,
	                .right = s.space.sm,
	                .top = to_u_t(4.0f),
	                .bottom = to_u_t(4.0f)},
		  .badge_compact = {.left = to_u_t(6.0f),
	                        .right = to_u_t(6.0f),
	                        .top = to_u_t(2.0f),
	                        .bottom = to_u_t(2.0f)},
		  .example_quote = {.left = s.space.md,
	                        .right = s.space.sm,
	                        .top = s.space.sm,
	                        .bottom = s.space.sm},
	};

	// 5. Dimensions
	s.dim = {
		  .min_touch_target = to_f_t(base_touch_target),
		  .app_bar_height = to_f_t(64.0f),
		  .bottom_bar_height = to_f_t(64.0f),
		  .word_card_height = to_f_t(base_card_h),
		  .action_btn_size = to_f_t(44.0f),
		  .accent_border_width = to_f_t(3.0f),
		  .form_label_width = to_f_t(100.0f),
		  .icon_sm = to_f_t(16.0f),
		  .icon_md = to_f_t(24.0f),
	};

	return s;
}
} // namespace

const Sizes *sizes() { return &app_sizes; }

void sizes_set_scale(float scale, DensityMode density,
                     FontScaleLevel font_scale) {
	app_sizes = sizes_create(scale, density, font_scale);
}
