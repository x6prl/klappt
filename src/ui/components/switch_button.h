#pragma once

#include "button.h"

// TODO: implement
inline bool switch_button(AppContext *ctx, Clay_ElementId id, bool turned_on,
                          float height = 24.f) {
	constexpr auto ICON_TOGGLE_OFF = ""_v;
	constexpr auto ICON_TOGGLE_ON = ""_v;

	MobileButtonStyle style{
		  .height = height,
		  .padding_x = 0.f,
		  .padding_y = 0.f,
		  .corner_radius = 0.f,
		  .font_size = height,
		  .font_id = FontID::ICONS,
		  .border_width = 0.f,
		  // .background = theme()->wrongContainer,
		  .text = theme()->onSurface,
	};

	auto res = mobile_button(
		  ctx, id, turned_on ? ICON_TOGGLE_ON : ICON_TOGGLE_OFF, style);

	return res.activated();
}
