#pragma once

#include "../themes.h"
#include "app/app_context.h"
#include "base/profiler.h"
#include "base/str_view.h"
#include "ui/dpi.h"
#include <clay/clay.h>
#include <cstdint>

namespace Icons {
constexpr auto EDIT = ""_v;
constexpr auto REMOVE = ""_v;
constexpr auto NEXT = ""_v;
constexpr auto SAVE = ""_v;
constexpr auto CHEVRON_UP = ""_v;
constexpr auto CHEVRON_DOWN = ""_v;
} // namespace Icons

struct MobileButtonStyle {
	bool fill_width{false};
	float min_width{0.0f};
	float height{48.0f};
	float padding_x{18.0f};
	float padding_y{12.0f};
	float corner_radius{16.0f};
	float font_size{16.0f};
	uint16_t font_id{FontID::MAIN};
	float border_width{0.0f};
	Clay_LayoutAlignmentX text_align{CLAY_ALIGN_X_CENTER};
	Clay_Color background{};
	Clay_Color background_pressed{};
	Clay_Color border{};
	Clay_Color border_pressed{};
	Clay_Color text{};
	Clay_Color text_pressed{};
};

struct MobileButtonResult {
	bool held{};
	bool tapped{};
	bool long_tapped{};

	bool activated() const { return tapped || long_tapped; }
};

inline Clay_Color mobile_button_mix(Clay_Color lhs, Clay_Color rhs, float t) {
	if (t < 0.0f) {
		t = 0.0f;
	} else if (t > 1.0f) {
		t = 1.0f;
	}

	auto mix = [t](float a, float b) -> float { return a + (b - a) * t; };

	return {mix(lhs.r, rhs.r), mix(lhs.g, rhs.g), mix(lhs.b, rhs.b),
	        mix(lhs.a, rhs.a)};
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
inline MobileButtonStyle mobile_button_style_primary() {
	const Theme *t = theme();
	return {
		  .background = t->primary,
		  .background_pressed = mobile_button_mix(t->primary, t->shadow, 0.24f),
		  .border = t->primary,
		  .border_pressed = mobile_button_mix(t->primary, t->shadow, 0.32f),
		  .text = t->onPrimary,
		  .text_pressed = t->onPrimary,
	};
}

inline MobileButtonStyle mobile_button_style_surface_container_high() {
	const Theme *t = theme();
	return {
		  .background = t->surfaceContainerHigh,
		  .background_pressed = mobile_button_mix(
				t->surfaceContainerHigh, t->onSurfaceContainerHigh, 0.12f),
		  .border = t->outline,
		  .border_pressed =
				mobile_button_mix(t->outline, t->onSurfaceContainerHigh, 0.24f),
		  .text = t->onSurfaceContainerHigh,
		  .text_pressed = t->onSurfaceContainerHigh,
	};
}

inline MobileButtonStyle mobile_button_style_app_bar() {
	const Theme *t = theme();
	return {
		  .min_width = 44.0f,
		  .height = 44.0f,
		  .padding_x = 12.0f,
		  .padding_y = 12.0f,
		  .corner_radius = 14.0f,
		  .font_size = 24,
		  .font_id = FontID::ICONS,
		  .border_width = 0,
		  .background = {0, 0, 0, 0},
		  .background_pressed =
				mobile_button_mix(t->onPrimary, t->primary, 0.82f),
		  .border = {0, 0, 0, 0},
		  .border_pressed = {0, 0, 0, 0},
		  .text = t->onSurfaceContainerLow,
		  .text_pressed = t->onSurfaceContainerLow,
	};
}

inline MobileButtonResult mobile_button_state(const AppContext *ctx,
                                              Clay_ElementId id) {
	const bool hit = Clay_PointerOver(id);
	const bool held = hit && (ctx->tslt.state == TapSwipeLongTap::KeyDown ||
	                          ctx->tslt.state == TapSwipeLongTap::LongTap);
	return {
		  .held = held,
		  .tapped = hit && ctx->tslt.is_tap(),
		  .long_tapped = hit && ctx->tslt.is_longtap(),
	};
}

inline MobileButtonResult
mobile_button(AppContext *ctx, Clay_ElementId id, StrView label,
              const MobileButtonStyle &style =
                    mobile_button_style_surface_container_high()) {
	KLAPPT_PROFILE_SCOPE_N("mobile_button");
	const uint16_t padding_x = udpi(style.padding_x);
	const uint16_t padding_y = udpi(style.padding_y);
	const uint16_t border_width = udpi(style.border_width);
	const uint16_t font_size = udpi(style.font_size);
	const float min_width = udpi(style.min_width);
	const float height = udpi(style.height);
	const float corner_radius = udpi(style.corner_radius);

	const auto state = mobile_button_state(ctx, id);
	if (label.data && label.size > 0) {
		KLAPPT_PROFILE_NAME(label.data, static_cast<size_t>(label.size));
	}
	if (state.activated()) {
		ctx->anim(); // TODO: just push one frame
	}
	CLAY(id, {
				   .layout =
						 {
							   .sizing =
									 {
										   style.fill_width
												 ? CLAY_SIZING_GROW(min_width)
												 : CLAY_SIZING_FIT(min_width),
										   CLAY_SIZING_FIXED(height),
									 },
							   .padding = {padding_x, padding_x, padding_y,
	                                       padding_y},
							   .childAlignment = {style.text_align,
	                                              CLAY_ALIGN_Y_CENTER},
						 },
				   .backgroundColor = state.held ? style.background_pressed
	                                             : style.background,
				   .cornerRadius = CLAY_CORNER_RADIUS(corner_radius),
				   .border =
						 {
							   .color = state.held ? style.border_pressed
	                                               : style.border,
							   .width = {border_width, border_width,
	                                     border_width, border_width, 0},
						 },
			 }) {
		CLAY_TEXT(
			  label.to_clay_string(),
			  CLAY_TEXT_CONFIG({
					.textColor = state.held ? style.text_pressed : style.text,
					.fontId = style.font_id,
					.fontSize = font_size,
			  }));
	}

	return state;
}

template <bool primary = true>
inline MobileButtonResult mobile_icon_button(AppContext *ctx, Clay_ElementId id,
                                             StrView icon) {
	MobileButtonStyle style;
	if constexpr (primary) {
		style = mobile_button_style_primary();
	} else {
		style = mobile_button_style_surface_container_high();
	}
	style.font_id = FontID::ICONS;
	return mobile_button(ctx, id, icon, style);
}
#pragma GCC diagnostic pop
