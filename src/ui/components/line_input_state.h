#pragma once

#include "app/textcache.h"
#include "base/arena.h"
#include "base/str_view.h"

using LineInputPosition = int32_t;

struct LineInputState {
	constexpr static LineInputPosition CAPACITY = 256;
	constexpr static LineInputPosition UNDO_STATE_COUNT = 16;
	constexpr static LineInputPosition UNDO_CHAR_COUNT = 256;
	static constexpr LineInputPosition COMPOSITION_CAPACITY = 8;

	uint16_t font_size{0};
	uint16_t font_id{0};
	TextCache *text_cache{nullptr};

	UTF32Codepoint *data{nullptr};
	LineInputPosition length{0};
	bool focus_requested{false};
	bool is_show_popup_menu{false};
	bool is_dragging{false};
	float scroll_x{0.0f};
	float padding_x{0.0f};
	float padding_y{0.0f};

	void *stb_state{nullptr};

	UTF32Codepoint composition_data[COMPOSITION_CAPACITY]{};
	LineInputPosition composition_length{0};
	LineInputPosition composition_cursor{0};
	LineInputPosition composition_sel_length{0};

	[[nodiscard]]
	bool is_composing() const {
		return composition_length > 0;
	}
	void clear();
	[[nodiscard]]
	StrView to_utf8(Arena &a) const {
		return StrView::from_utf32(a, data, length);
	}
};

namespace LineInput {
void init(LineInputState *input, Arena &a, TextCache *text = nullptr);
void clear(LineInputState *input);
} // namespace LineInput
