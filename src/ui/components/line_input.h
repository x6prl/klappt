#pragma once

#include "SDL3/SDL_keyboard.h"
#include "app/app_context.h"

struct LineInputStyle {
	bool fill_width{true};
	float min_width{0.0f};
	float height{
		  0.0f}; // Resolved in factory from sizes()->dim.min_touch_target
	uint16_t padding_x{0};     // Resolved in factory from sizes()->space.lg
	uint16_t padding_y{0};     // Resolved in factory from sizes()->space.sm
	float corner_radius{0.0f}; // Resolved in factory from sizes()->radius.md
	uint16_t font_size{0};     // Resolved in factory from sizes()->font.body_md
	uint16_t border_width{1};
	float caret_width{2.0f};
	float caret_height{0.0f}; // Resolved in factory from sizes()->font.title_md
	uint16_t font_id{FontID::MAIN};
	SDL_TextInputType text_type{SDL_TEXTINPUT_TYPE_TEXT};
	SDL_Capitalization capitalization{SDL_CAPITALIZE_NONE};
	bool autocorrect{false};
	// bool rtl{false};
	bool clearable{true};
	Clay_Color background{};
	Clay_Color background_focused{};
	Clay_Color border{};
	Clay_Color border_focused{};
	Clay_Color text{};
	Clay_Color selection{};
	Clay_Color placeholder{};
	// Clay_Color composition{};
	Clay_Color caret{};
};

inline LineInputStyle line_input_style_default() {
	const Theme *t = theme();
	const Sizes *s = sizes();

	auto color_mix = [](Clay_Color lhs, Clay_Color rhs, float t) -> Clay_Color {
		if (t < 0.0f) {
			t = 0.0f;
		} else if (t > 1.0f) {
			t = 1.0f;
		}

		auto mix = [t](float a, float b) -> float { return a + (b - a) * t; };
		return {mix(lhs.r, rhs.r), mix(lhs.g, rhs.g), mix(lhs.b, rhs.b),
		        mix(lhs.a, rhs.a)};
	};

	return {
		  .height = s->dim.min_touch_target,
		  .padding_x = s->space.lg,
		  .padding_y = s->space.sm,
		  .corner_radius = s->radius.md.topLeft,
		  .font_size = s->font.body_md,
		  .border_width = 1,
		  .caret_width = 2.0f * s->scale,
		  .caret_height = static_cast<float>(s->font.title_md),
		  .font_id = FontID::MAIN,
		  .background = t->surface,
		  .background_focused = color_mix(t->surface, t->primary, 0.08f),
		  .border = color_mix(t->outline, t->surface, 0.18f),
		  .border_focused = t->primary,
		  .text = t->onSurface,
		  .selection = color_mix(t->primary, {0.f, 0.f, 0.f, 0.f}, 0.4f),
		  .placeholder = color_mix(t->onSurface, t->surface, 0.58f),
		  // .composition = color_mix(t->primary, t->onSurface, 0.35f),
		  .caret = t->primary,
	};
}

namespace LineInput {
// void handle_sdl3_event(Arena &scratch, LineInputState *input,
//                        const SDL_Event &event, TapSwipeLongTap *tslt,
//                        float widget_screen_x, float widget_screen_y);

[[nodiscard]] bool handle_keyboard_event(Arena &scratch, LineInputState *input,
                                         const SDL_Event *event);

// Focus is applied the next time this field is drawn.
void request_focus(LineInputState *input);

void ui_line_input(AppContext *ctx, Clay_ElementId id, LineInputState *state,
                   StrView placeholder, const LineInputStyle &style,
                   Clay_ElementId focus_preserving_element = {});

// inline void focus_input(AppContext *ctx, LineInputState *input,
//                         Clay_ElementId id, const Clay_BoundingBox &box);
} // namespace LineInput
