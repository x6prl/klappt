#pragma once

#include "base/str_view.h"
#include <clay/clay.h>

struct MobileTextInputBuffer {
	static constexpr Size max_size{256};

	char data[max_size]{};
	Size size{0};

	void clear() {
		size = 0;
		data[0] = '\0';
	}

	const char *c_str() {
		data[size] = '\0';
		return data;
	}

	StrView view() const { return {data, size}; }
};

struct MobileTextInputState {
	uint32_t focused_id{};
	uint32_t changed_id{};
	uint32_t submitted_id{};
	uint32_t blurred_id{};
	Clay_ElementId focused_element{};
	MobileTextInputBuffer *focused_value{};
	Clay_BoundingBox focused_bounds{};
	float scroll_offset_px{0.0f};
	float cursor_offset_px{0.0f};
	bool focused_bounds_valid{false};
	bool focused_drawn_this_frame{false};
	bool rtl{false};
	bool activate_text_input{false}; // used to activate text input when going to a screen
	uint16_t padding_left{};
	uint16_t padding_right{};
	uint16_t padding_top{};
	uint16_t padding_bottom{};
	MobileTextInputBuffer composition{};
};
