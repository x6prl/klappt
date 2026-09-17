#pragma once

#include "button.h"

inline bool switch_button(AppContext *ctx, Clay_ElementId id, bool is_turned_on,
                          float height = 0.0f) {
	constexpr auto ICON_TOGGLE_OFF = ""_v;
	constexpr auto ICON_TOGGLE_ON = ""_v;

	if (height <= 0.0f) {
		height = sizes()->dim.icon_md;
	}

	MobileButtonStyle style{
		  .height = height,
		  .padding_x = 0,
		  .padding_y = 0,
		  .corner_radius = sizes()->radius.sm.topLeft,
		  .font_size = static_cast<uint16_t>(height),
		  .font_id = FontID::ICONS,
		  .border_width = 0,
		  .background_pressed = theme()->wrongContainer,
		  .text = is_turned_on ? theme()->primary : theme()->onSurface,
		  .text_pressed = is_turned_on ? theme()->primary : theme()->onSurface,
	};

	auto res = mobile_button(
		  ctx, id, is_turned_on ? ICON_TOGGLE_ON : ICON_TOGGLE_OFF, style);

	return res.activated();
}
