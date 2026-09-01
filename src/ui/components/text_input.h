#pragma once

#include <SDL3/SDL_keyboard.h>
#include <SDL3/SDL_properties.h>

#include "ui/components/button.h"
#include "ui/dpi.h"
#include "ui/textcache.h"
#include "ui/translations/langs.h"

#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
extern "C" void mobile_text_input_web_wakeup();
#endif

#include "../themes.h"
#include "app/app_context.h"
#include "base/str_view.h"
#include "text_input_state.h"
#include <clay/clay.h>

struct MobileTextInputStyle {
	bool fill_width{true};
	float min_width{0.0f};
	float height{56.0f};
	float padding_x{16.0f};
	float padding_y{14.0f};
	float corner_radius{18.0f};
	float font_size{18.0f};
	float border_width{1.0f};
	float caret_width{2.0f};
	float caret_height{24.0f};
	uint16_t font_id{FontID::MAIN};
	SDL_TextInputType text_type{SDL_TEXTINPUT_TYPE_TEXT};
	SDL_Capitalization capitalization{SDL_CAPITALIZE_SENTENCES};
	bool autocorrect{true};
	bool rtl{false};
	bool clearable{true};
	Clay_Color background{};
	Clay_Color background_focused{};
	Clay_Color border{};
	Clay_Color border_focused{};
	Clay_Color text{};
	Clay_Color placeholder{};
	Clay_Color composition{};
	Clay_Color caret{};
};

struct MobileTextInputResult {
	bool focused{};
	bool tapped{};
	bool changed{};
	bool submitted{};
	bool blurred{};
};

inline Clay_Color mobile_text_input_mix(Clay_Color lhs, Clay_Color rhs,
                                        float t) {
	if (t < 0.0f) {
		t = 0.0f;
	} else if (t > 1.0f) {
		t = 1.0f;
	}

	auto mix = [t](float a, float b) -> float { return a + (b - a) * t; };
	return {mix(lhs.r, rhs.r), mix(lhs.g, rhs.g), mix(lhs.b, rhs.b),
	        mix(lhs.a, rhs.a)};
}

inline MobileTextInputStyle mobile_text_input_style_default() {
	const Theme *t = theme();
	return {
		  .background = t->surface,
		  .background_focused =
				mobile_text_input_mix(t->surface, t->primary, 0.08f),
		  .border = mobile_text_input_mix(t->outline, t->surface, 0.18f),
		  .border_focused = t->primary,
		  .text = t->onSurface,
		  .placeholder = mobile_text_input_mix(t->onSurface, t->surface, 0.58f),
		  .composition = mobile_text_input_mix(t->primary, t->onSurface, 0.35f),
		  .caret = t->primary,
	};
}

inline StrView mobile_text_input_view(const MobileTextInputBuffer &value) {
	return value.view();
}

inline bool mobile_text_input_is_utf8_continuation(unsigned char ch) {
	return (ch & 0xC0u) == 0x80u;
}

inline Size mobile_text_input_utf8_codepoint_bytes(unsigned char lead) {
	if ((lead & 0x80u) == 0u) {
		return 1;
	}
	if ((lead & 0xE0u) == 0xC0u) {
		return 2;
	}
	if ((lead & 0xF0u) == 0xE0u) {
		return 3;
	}
	if ((lead & 0xF8u) == 0xF0u) {
		return 4;
	}
	return 1;
}

inline void mobile_text_input_clear_composition(AppContext *ctx) {
	ctx->mobile_text_input.composition.clear();
}

inline float mobile_text_input_measure_text(AppContext *ctx, StrView text,
                                            uint16_t font_id,
                                            uint16_t font_size) {
	if (!text.size || !text.data || !ctx || !ctx->text) {
		return 0.0f;
	}

	auto slice = CLAY__INIT(Clay_StringSlice){
		  .length = static_cast<int32_t>(text.size),
		  .chars = text.data,
		  .baseChars = text.data,
	};
	return ctx->text
	      ->measure_text(slice, CLAY_TEXT_CONFIG({
									  .fontId = font_id,
									  .fontSize = font_size,
									  .wrapMode = CLAY_TEXT_WRAP_NONE,
								}))
	      .width;
}

inline bool mobile_text_input_insert(MobileTextInputBuffer &dst,
                                     Size &cursor_offset, const char *utf8) {
	if (!utf8 || !utf8[0])
		return false;

	if (cursor_offset > dst.size) {
		cursor_offset = dst.size;
	}

	Size insert_len = 0;
	while (utf8[insert_len] && utf8[insert_len] != '\r' &&
	       utf8[insert_len] != '\n') {
		++insert_len;
	}
	if (insert_len == 0)
		return false;

	if (dst.size + insert_len >= MobileTextInputBuffer::MAX_SIZE) {
		insert_len = MobileTextInputBuffer::MAX_SIZE - 1 - dst.size;
		if (insert_len <= 0)
			return false;
	}

	std::memmove(dst.data + cursor_offset + insert_len,
	             dst.data + cursor_offset, dst.size - cursor_offset);

	std::memcpy(dst.data + cursor_offset, utf8, insert_len);

	dst.size += insert_len;
	cursor_offset += insert_len;
	dst.data[dst.size] = '\0';
	return true;
}

inline bool mobile_text_input_backspace_at_cursor(MobileTextInputBuffer &dst,
                                                  Size &cursor_offset) {
	if (!dst.size || cursor_offset == 0)
		return false;

	if (cursor_offset > dst.size) {
		cursor_offset = dst.size;
	}

	Size prev_index = cursor_offset - 1;
	while (prev_index > 0 &&
	       mobile_text_input_is_utf8_continuation(
				 static_cast<unsigned char>(dst.data[prev_index]))) {
		--prev_index;
	}

	Size bytes_to_remove = cursor_offset - prev_index;

	std::memmove(dst.data + prev_index, dst.data + cursor_offset,
	             dst.size - cursor_offset);

	dst.size -= bytes_to_remove;
	cursor_offset = prev_index;
	dst.data[dst.size] = '\0';
	return true;
}

inline void mobile_text_input_clear(AppContext *ctx,
                                    MobileTextInputBuffer *value) {
	if (value) {
		value->clear();
	}
	auto &runtime = ctx->mobile_text_input;
	runtime.cursor_byte_offset = 0;
	runtime.scroll_offset_px = 0.0f;
	runtime.cursor_offset_px = 0.0f;
	runtime.composition.clear();

#ifdef __EMSCRIPTEN__
	EM_ASM({
		const state = Module.lexiSDLTextInput;
		if (state && state.input) {
			state.input.value = "";
		}
	});
#endif
}

inline void mobile_text_input_deactivate(AppContext *ctx,
                                         bool notify_blur = false);

inline void
mobile_text_input_native_activate(SDL_Window *window,
                                  const MobileTextInputStyle &style) {
	SDL_PropertiesID props = SDL_CreateProperties();
	if (props) {
		SDL_SetNumberProperty(props, SDL_PROP_TEXTINPUT_TYPE_NUMBER,
		                      style.text_type);
		SDL_SetNumberProperty(props, SDL_PROP_TEXTINPUT_CAPITALIZATION_NUMBER,
		                      style.capitalization);
		SDL_SetBooleanProperty(props, SDL_PROP_TEXTINPUT_AUTOCORRECT_BOOLEAN,
		                       style.autocorrect);
		SDL_SetBooleanProperty(props, SDL_PROP_TEXTINPUT_MULTILINE_BOOLEAN,
		                       false);
		SDL_StartTextInputWithProperties(window, props);
		SDL_DestroyProperties(props);
	} else {
		SDL_StartTextInput(window);
	}
}

inline void mobile_text_input_native_deactivate(SDL_Window *window) {
	SDL_ClearComposition(window);
	if (SDL_TextInputActive(window)) {
		SDL_StopTextInput(window);
	}
	SDL_SetTextInputArea(window, nullptr, 0);
}

inline bool mobile_text_input_native_handle_key_down(AppContext *ctx,
                                                     SDL_Event *event) {
	auto &runtime = ctx->mobile_text_input;
	if (!runtime.focused_value)
		return false;

	auto &buf = *runtime.focused_value;
	if (runtime.cursor_byte_offset > buf.size) {
		runtime.cursor_byte_offset = buf.size;
	}

	if (runtime.composition.size > 0) {
		return false;
	}

	if (event->key.key == SDLK_LEFT) {
		if (runtime.cursor_byte_offset > 0) {
			--runtime.cursor_byte_offset;
			while (runtime.cursor_byte_offset > 0 &&
			       mobile_text_input_is_utf8_continuation(
						 static_cast<unsigned char>(
							   buf.data[runtime.cursor_byte_offset]))) {
				--runtime.cursor_byte_offset;
			}
		}
		return true;
	}
	if (event->key.key == SDLK_RIGHT) {
		if (runtime.cursor_byte_offset < buf.size) {
			const unsigned char lead = static_cast<unsigned char>(
				  buf.data[runtime.cursor_byte_offset]);
			runtime.cursor_byte_offset +=
				  mobile_text_input_utf8_codepoint_bytes(lead);
			if (runtime.cursor_byte_offset > buf.size) {
				runtime.cursor_byte_offset = buf.size;
			}
		}
		return true;
	}
	if (event->key.key == SDLK_HOME) {
		runtime.cursor_byte_offset = 0;
		return true;
	}
	if (event->key.key == SDLK_END) {
		runtime.cursor_byte_offset = buf.size;
		return true;
	}
	if (event->key.key == SDLK_BACKSPACE) {
		mobile_text_input_clear_composition(ctx);
		if (mobile_text_input_backspace_at_cursor(buf,
		                                          runtime.cursor_byte_offset)) {
			runtime.changed_id = runtime.focused_id;
		}
		return true;
	}
	if (event->key.key == SDLK_RETURN || event->key.key == SDLK_KP_ENTER) {
		runtime.submitted_id = runtime.focused_id;
		mobile_text_input_deactivate(ctx, false);
		return true;
	}
	return false;
}

#ifdef __EMSCRIPTEN__
inline const char *
mobile_text_input_web_input_mode(SDL_TextInputType text_type) {
	switch (text_type) {
	case SDL_TEXTINPUT_TYPE_TEXT_EMAIL:
		return "email";
	case SDL_TEXTINPUT_TYPE_NUMBER:
	case SDL_TEXTINPUT_TYPE_NUMBER_PASSWORD_HIDDEN:
	case SDL_TEXTINPUT_TYPE_NUMBER_PASSWORD_VISIBLE:
		return "numeric";
	case SDL_TEXTINPUT_TYPE_TEXT:
	case SDL_TEXTINPUT_TYPE_TEXT_NAME:
	case SDL_TEXTINPUT_TYPE_TEXT_USERNAME:
	case SDL_TEXTINPUT_TYPE_TEXT_PASSWORD_HIDDEN:
	case SDL_TEXTINPUT_TYPE_TEXT_PASSWORD_VISIBLE:
	default:
		return "text";
	}
}

inline const char *mobile_text_input_web_type(SDL_TextInputType text_type) {
	switch (text_type) {
	case SDL_TEXTINPUT_TYPE_TEXT_EMAIL:
		return "email";
	case SDL_TEXTINPUT_TYPE_TEXT_PASSWORD_HIDDEN:
	case SDL_TEXTINPUT_TYPE_NUMBER_PASSWORD_HIDDEN:
		return "password";
	default:
		return "text";
	}
}

inline const char *
mobile_text_input_web_capitalization(SDL_Capitalization capitalization) {
	switch (capitalization) {
	case SDL_CAPITALIZE_WORDS:
		return "words";
	case SDL_CAPITALIZE_SENTENCES:
		return "sentences";
	case SDL_CAPITALIZE_LETTERS:
		return "characters";
	case SDL_CAPITALIZE_NONE:
	default:
		return "none";
	}
}

inline bool mobile_text_input_web_should_use() {
	return EM_ASM_INT({
			   if (typeof navigator == 'undefined' ||
		           typeof window == 'undefined') {
				   return 0;
			   }
			   const ua = navigator.userAgent || "";
			   const platform = navigator.platform || "";
			   const mobilePattern = new RegExp(
					 'Android|webOS|iPhone|iPad|iPod|BlackBerry|IEMobile|Opera Mini',
					 'i');
			   const mobileUA = mobilePattern.test(ua);
			   const ipadDesktopMode = platform == 'MacIntel' &&
		                               (navigator.maxTouchPoints || 0) > 1;
			   const coarsePointer =
					 window.matchMedia &&
					 window.matchMedia('(pointer: coarse)').matches;
			   return (mobileUA || ipadDesktopMode || coarsePointer) ? 1 : 0;
		   }) != 0;
}

inline void mobile_text_input_web_activate(const MobileTextInputBuffer *value,
                                           const MobileTextInputStyle &style) {
	const char *initial = value ? value->data : "";
	const char *input_mode = mobile_text_input_web_input_mode(style.text_type);
	const char *input_type = mobile_text_input_web_type(style.text_type);
	const char *capitalization =
		  mobile_text_input_web_capitalization(style.capitalization);
	EM_ASM(
		  {
			  let state = Module.lexiSDLTextInput;
			  if (!state) {
				  state = {};
				  state.input = null;
				  state.submitted = false;
				  state.blurred = false;
				  state.blurPendingAt = 0;
				  state.closingByApp = false;
				  state.active = false;
				  Module.lexiSDLTextInput = state;
			  }

			  let input = state.input;
			  if (!input) {
				  input = document.createElement('input');
				  input.id = '__lexi_sdl_text_input';
				  input.autocomplete = 'off';
				  input.style.position = 'fixed';
				  input.style.left = '0px';
				  input.style.top = '0px';
				  input.style.width = '1px';
				  input.style.height = '1px';
				  input.style.opacity = '0.01';
				  input.style.pointerEvents = 'auto';
				  input.style.border = '0';
				  input.style.padding = '0';
				  input.style.margin = '0';
				  input.style.background = 'transparent';
				  input.style.color = 'transparent';
				  input.style.caretColor = 'transparent';
				  input.style.outline = 'none';
				  input.style.zIndex = '2147483647';
				  input.style.fontSize = '16px';
				  input.addEventListener(
						'input',
						function() { _mobile_text_input_web_wakeup(); });
				  input.addEventListener(
						'keydown', function(event) {
							if (event.key == 'Enter') {
								state.submitted = true;
								state.closingByApp = true;
								state.active = false;
								_mobile_text_input_web_wakeup();
								event.preventDefault();
								input.blur();
							}
						});
				  input.addEventListener(
						'blur', function() {
							if (state.closingByApp) {
								state.closingByApp = false;
								state.blurPendingAt = 0;
								state.blurred = false;
							} else if (state.active) {
								state.blurPendingAt =
									  (typeof performance != 'undefined')
											? performance.now()
											: Date.now();
								setTimeout(
									  function() {
										  const current =
												Module.lexiSDLTextInput;
										  if (current &&
						                      current.blurPendingAt) {
											  _mobile_text_input_web_wakeup();
										  }
									  },
									  140);
							}
							state.active = false;
							_mobile_text_input_web_wakeup();
						});
				  document.body.appendChild(input);
				  state.input = input;
			  }

			  input.style.display = 'block';
			  input.type = UTF8ToString($1);
			  input.inputMode = UTF8ToString($2);
			  input.autocapitalize = UTF8ToString($3);
			  input.autocorrect = $4 ? 'on' : 'off';
			  input.spellcheck = !!$4;
			  input.dir = $5 ? 'rtl' : 'ltr';
			  input.style.textAlign = $5 ? 'right' : 'left';
			  input.value = UTF8ToString($0);
			  state.submitted = false;
			  state.blurred = false;
			  state.blurPendingAt = 0;
			  state.closingByApp = false;
			  state.active = true;
			  try {
				  input.focus({preventScroll : true});
				  const end = input.value.length;
				  input.setSelectionRange(end, end);
			  } catch (e) {
			  }
		  },
		  initial, input_type, input_mode, capitalization,
		  style.autocorrect ? 1 : 0, style.rtl ? 1 : 0);
}

inline void mobile_text_input_web_deactivate() {
	EM_ASM({
		const state = Module.lexiSDLTextInput;
		if (!state || !state.input) {
			return;
		}
		state.submitted = false;
		state.blurred = false;
		state.blurPendingAt = 0;
		state.closingByApp = true;
		state.active = false;
		try {
			state.input.blur();
		} catch (e) {
		}
		state.input.style.display = 'none';
	});
}

inline void mobile_text_input_web_update_rect(const SDL_Rect &rect) {
	EM_ASM(
		  {
			  const state = Module.lexiSDLTextInput;
			  if (!state || !state.input) {
				  return;
			  }
			  state.input.style.left = $0 + 'px';
			  state.input.style.top = $1 + 'px';
			  state.input.style.width = $2 + 'px';
			  state.input.style.height = $3 + 'px';
			  state.input.style.pointerEvents = state.active ? 'auto' : 'none';
		  },
		  rect.x, rect.y, rect.w, rect.h);
}

inline void mobile_text_input_web_copy_value(char *dst, Size capacity) {
	if (!dst || capacity < 1) {
		return;
	}
	dst[0] = '\0';
	EM_ASM(
		  {
			  const state = Module.lexiSDLTextInput;
			  if (!state || !state.input) {
				  return;
			  }
			  stringToUTF8(state.input.value || "", $0, $1);
		  },
		  dst, static_cast<int>(capacity));
}

inline bool mobile_text_input_web_take_submitted() {
	return EM_ASM_INT({
			   const state = Module.lexiSDLTextInput;
			   if (!state) {
				   return 0;
			   }
			   const submitted = state.submitted ? 1 : 0;
			   state.submitted = false;
			   return submitted;
		   }) != 0;
}

inline bool mobile_text_input_web_take_blurred() {
	return EM_ASM_INT({
			   const state = Module.lexiSDLTextInput;
			   if (!state) {
				   return 0;
			   }
			   let blurred = state.blurred ? 1 : 0;
			   if (!blurred && state.blurPendingAt) {
				   const now = (typeof performance != 'undefined')
			                         ? performance.now()
			                         : Date.now();
				   const inputStillFocused =
						 state.input && document.activeElement == state.input;
				   if (inputStillFocused) {
					   state.blurPendingAt = 0;
				   } else if (now - state.blurPendingAt >= 120) {
					   blurred = 1;
					   state.blurPendingAt = 0;
				   }
			   }
			   state.blurred = false;
			   return blurred;
		   }) != 0;
}
#endif

inline void mobile_text_input_deactivate(AppContext *ctx, bool notify_blur) {
	auto &runtime = ctx->mobile_text_input;
	if (notify_blur && runtime.focused_id) {
		runtime.blurred_id = runtime.focused_id;
	}
	runtime.focused_id = 0;
	runtime.focused_element = {};
	runtime.focused_value = nullptr;
	runtime.focused_bounds = {};
	runtime.cursor_byte_offset = 0;
	runtime.scroll_offset_px = 0.0f;
	runtime.cursor_offset_px = 0.0f;
	runtime.focused_bounds_valid = false;
	runtime.focused_drawn_this_frame = false;
	runtime.rtl = false;
	runtime.padding_left = 0;
	runtime.padding_right = 0;
	runtime.padding_top = 0;
	runtime.padding_bottom = 0;
	mobile_text_input_clear_composition(ctx);
#ifdef __EMSCRIPTEN__
	if (mobile_text_input_web_should_use()) {
		mobile_text_input_web_deactivate();
		return;
	}
#endif
	mobile_text_input_native_deactivate(ctx->window);
}

inline void mobile_text_input_activate(AppContext *ctx, Clay_ElementId id,
                                       MobileTextInputBuffer *value,
                                       const MobileTextInputStyle &style) {
	if (!value) {
		return;
	}

	auto &runtime = ctx->mobile_text_input;
	if (runtime.focused_id == id.id && runtime.focused_value == value) {
		return;
	}

	mobile_text_input_deactivate(ctx, true);
	runtime.focused_id = id.id;
	runtime.focused_element = id;
	runtime.focused_value = value;
	runtime.cursor_byte_offset = value->size;
	runtime.rtl = style.rtl;
#ifdef __EMSCRIPTEN__
	if (mobile_text_input_web_should_use()) {
		runtime.focused_bounds_valid = false;
		mobile_text_input_web_activate(value, style);
		return;
	}
#endif
	mobile_text_input_native_activate(ctx->window, style);
}

inline bool mobile_text_input_handle_event(AppContext *ctx, SDL_Event *event) {
	auto &runtime = ctx->mobile_text_input;
	if (!runtime.focused_value) {
		return false;
	}

#ifdef __EMSCRIPTEN__
	const bool use_web_input = mobile_text_input_web_should_use();
#endif
	switch (event->type) {
	case SDL_EVENT_WINDOW_FOCUS_LOST:
		mobile_text_input_deactivate(ctx, true);
		return false;
	case SDL_EVENT_TEXT_EDITING:
#ifdef __EMSCRIPTEN__
		if (use_web_input) {
			return true;
		}
#endif
		runtime.composition.clear();
		mobile_text_input_insert(runtime.composition,
		                         runtime.cursor_byte_offset, event->edit.text);
		return true;
	case SDL_EVENT_TEXT_INPUT:
#ifdef __EMSCRIPTEN__
		if (use_web_input) {
			return true;
		}
#endif
		if (mobile_text_input_insert(*runtime.focused_value,
		                             runtime.cursor_byte_offset,
		                             event->text.text)) {
			runtime.changed_id = runtime.focused_id;
		}
		mobile_text_input_clear_composition(ctx);
		return true;
	case SDL_EVENT_KEY_DOWN:
#ifdef __EMSCRIPTEN__
		if (!use_web_input) {
			return mobile_text_input_native_handle_key_down(ctx, event);
		}
		if (event->key.key == SDLK_BACKSPACE || event->key.key == SDLK_RETURN ||
		    event->key.key == SDLK_KP_ENTER) {
			return true;
		}
		break;
#else
		return mobile_text_input_native_handle_key_down(ctx, event);
#endif
	default:
		break;
	}

	return false;
}

inline void mobile_text_input_begin_frame(AppContext *ctx) {
	ctx->mobile_text_input.focused_drawn_this_frame = false;
}

inline void mobile_text_input_sync(AppContext *ctx) {
	auto &runtime = ctx->mobile_text_input;
	if (!runtime.focused_id) {
		return;
	}
	if (!runtime.focused_drawn_this_frame || !runtime.focused_value) {
		mobile_text_input_deactivate(ctx);
		return;
	}

	const auto element = Clay_GetElementData(runtime.focused_element);
	if (!element.found) {
		return;
	}

	runtime.focused_bounds = element.boundingBox;
	runtime.focused_bounds_valid = true;

	SDL_Rect rect{
		  static_cast<int>(runtime.focused_bounds.x + runtime.padding_left),
		  static_cast<int>(runtime.focused_bounds.y + runtime.padding_top),
		  static_cast<int>(runtime.focused_bounds.width - runtime.padding_left -
	                       runtime.padding_right),
		  static_cast<int>(runtime.focused_bounds.height - runtime.padding_top -
	                       runtime.padding_bottom),
	};

	if (rect.w < 1) {
		rect.w = 1;
	}
	if (rect.h < 1) {
		rect.h = 1;
	}

	int cursor =
		  static_cast<int>(runtime.cursor_offset_px - runtime.scroll_offset_px);
	if (cursor < 0) {
		cursor = 0;
	} else if (cursor > rect.w) {
		cursor = rect.w;
	}
	if (runtime.rtl) {
		cursor = rect.w - cursor;
	}

#ifdef __EMSCRIPTEN__
	if (mobile_text_input_web_should_use()) {
		mobile_text_input_web_update_rect(rect);
		char web_value[MobileTextInputBuffer::max_size]{};
		mobile_text_input_web_copy_value(web_value,
		                                 MobileTextInputBuffer::max_size);
		if (std::strcmp(runtime.focused_value->data, web_value) != 0) {
			runtime.focused_value->clear();
			Size offset = 0;
			mobile_text_input_insert(*runtime.focused_value, offset, web_value);
			runtime.cursor_byte_offset = runtime.focused_value->size;
			runtime.changed_id = runtime.focused_id;
		}
		if (mobile_text_input_web_take_submitted()) {
			runtime.submitted_id = runtime.focused_id;
			mobile_text_input_deactivate(ctx);
			return;
		}
		if (mobile_text_input_web_take_blurred()) {
			mobile_text_input_deactivate(ctx, true);
			return;
		}
		return;
	}
#endif
	SDL_SetTextInputArea(ctx->window, &rect, cursor);
}

inline void mobile_text_input_end_frame(AppContext *ctx) {
	ctx->mobile_text_input.changed_id = 0;
	ctx->mobile_text_input.submitted_id = 0;
	ctx->mobile_text_input.blurred_id = 0;
}

inline MobileTextInputResult mobile_text_input(
	  AppContext *ctx, Clay_ElementId id, MobileTextInputBuffer *value,
	  StrView placeholder,
	  const MobileTextInputStyle &style = mobile_text_input_style_default(),
	  Clay_ElementId detached_clear_button_id =
			CLAY_ID("DetachedClearButton")) {

	const uint16_t padding_x = udpi(style.padding_x);
	const uint16_t padding_y = udpi(style.padding_y);
	const uint16_t border_width = udpi(style.border_width);
	const uint16_t font_size = udpi(style.font_size);
	const uint16_t caret_width = udpi(style.caret_width);
	const uint16_t caret_height = udpi(style.caret_height);
	const float min_width = dpi(style.min_width);
	const float height = dpi(style.height);
	const float corner_radius = dpi(style.corner_radius);

	const bool rtl = style.rtl || (ctx->settings.tr_language == lang_ar);
	MobileTextInputStyle effective_style = style;
	effective_style.rtl = rtl;
	if (rtl && effective_style.font_id == FontID::MAIN) {
		effective_style.font_id = FontID::ARABIC_MAIN;
	}

	const bool has_value = (value && value->size > 0);
	const float clear_btn_reserve =
		  (style.clearable && has_value) ? udpi(36.0f) : 0.0f;

	auto &runtime = ctx->mobile_text_input;
	const bool is_pointer_over =
		  Clay_PointerOver(id) ||
		  (style.clearable && Clay_PointerOver(detached_clear_button_id));

	const bool tapped = is_pointer_over && ctx->tslt.is_tap();
	if (tapped || runtime.activate_text_input) {
		mobile_text_input_activate(ctx, id, value, effective_style);
		runtime.activate_text_input = false;
	}

	if (runtime.focused_id == id.id && ctx->tslt.is_tap() && !is_pointer_over &&
	    !tapped) {
		mobile_text_input_deactivate(ctx, true);
	}

	const bool focused = (runtime.focused_id == id.id);
	const bool blurred = (runtime.blurred_id == id.id);
	if (focused) {
		runtime.focused_drawn_this_frame = true;
		runtime.focused_element = id;
		runtime.focused_value = value;
		runtime.rtl = rtl;
		runtime.padding_left = padding_x;
		runtime.padding_right =
			  padding_x + static_cast<uint16_t>(clear_btn_reserve);
		runtime.padding_top = padding_y;
		runtime.padding_bottom = padding_y;

		if (value && runtime.cursor_byte_offset > value->size) {
			runtime.cursor_byte_offset = value->size;
		}
	}

	const StrView committed =
		  value ? mobile_text_input_view(*value) : StrView{};
	const StrView composition =
		  focused ? mobile_text_input_view(runtime.composition) : StrView{};
	const bool has_committed = committed.size > 0;
	const bool has_composition = composition.size > 0;
	const bool show_placeholder = !has_committed && !has_composition;

	Size cursor_pos = focused ? runtime.cursor_byte_offset : 0;
	if (cursor_pos > committed.size)
		cursor_pos = committed.size;

	StrView left_part = (has_committed && cursor_pos > 0)
	                          ? committed.slice(0, cursor_pos)
	                          : StrView{};
	StrView right_part = (has_committed && cursor_pos < committed.size)
	                           ? committed.slice(cursor_pos)
	                           : StrView{};

	float scroll_offset_px = 0.0f;
	float cursor_offset_px =
		  mobile_text_input_measure_text(ctx, left_part,
	                                     effective_style.font_id, font_size) +
		  mobile_text_input_measure_text(ctx, composition,
	                                     effective_style.font_id, font_size);

	if (focused && runtime.focused_bounds_valid) {
		float inner_width = runtime.focused_bounds.width -
		                    static_cast<float>(padding_x * 2) -
		                    clear_btn_reserve;
		if (inner_width < 1.0f)
			inner_width = 1.0f;
		const float caret_extent =
			  cursor_offset_px + static_cast<float>(caret_width) + dpi(4.0f);
		if (caret_extent > inner_width) {
			scroll_offset_px = caret_extent - inner_width;
		}
	}

	if (focused) {
		runtime.scroll_offset_px = scroll_offset_px;
		runtime.cursor_offset_px = cursor_offset_px;
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
							   .padding = {padding_x,
	                                       static_cast<uint16_t>(
												 padding_x + clear_btn_reserve),
	                                       padding_y, padding_y},
							   .childAlignment = {rtl ? CLAY_ALIGN_X_RIGHT
	                                                  : CLAY_ALIGN_X_LEFT,
	                                              CLAY_ALIGN_Y_CENTER},
						 },
				   .backgroundColor =
						 focused ? style.background_focused : style.background,
				   .cornerRadius = CLAY_CORNER_RADIUS(corner_radius),
				   .border =
						 {
							   .color = focused ? style.border_focused
	                                            : style.border,
							   .width = {border_width, border_width,
	                                     border_width, border_width},
						 },
			 }) {

		CLAY(CLAY_ID_LOCAL("Viewport"),
		     {
				   .layout =
						 {
							   .sizing = {CLAY_SIZING_GROW(0),
		                                  CLAY_SIZING_GROW(0)},
							   .childAlignment = {rtl ? CLAY_ALIGN_X_RIGHT
		                                              : CLAY_ALIGN_X_LEFT,
		                                          CLAY_ALIGN_Y_CENTER},
						 },
				   .clip = {.horizontal = true,
		                    .vertical = false,
		                    .childOffset = {rtl ? scroll_offset_px
		                                        : -scroll_offset_px,
		                                    0}},
			 }) {

			CLAY(CLAY_ID_LOCAL("Content"),
			     {
					   .layout =
							 {
								   .sizing = {CLAY_SIZING_FIT(0),
			                                  CLAY_SIZING_GROW(0)},
								   .childGap = 0,
								   .childAlignment = {rtl ? CLAY_ALIGN_X_RIGHT
			                                              : CLAY_ALIGN_X_LEFT,
			                                          CLAY_ALIGN_Y_CENTER},
								   .layoutDirection = CLAY_LEFT_TO_RIGHT,
							 },
				 }) {

				if (show_placeholder) {
					if (focused) {
						CLAY(CLAY_ID_LOCAL("Caret"),
						     {
								   .layout =
										 {
											   .sizing =
													 {CLAY_SIZING_FIXED(
															static_cast<float>(
																  caret_width)),
						                              CLAY_SIZING_FIXED(
															static_cast<float>(
																  caret_height))},
										 },
								   .backgroundColor = style.caret,
							 }) {}
					}

					CLAY_TEXT(placeholder.to_clay_string(),
					          CLAY_TEXT_CONFIG({
									.textColor = style.placeholder,
									.fontId = effective_style.font_id,
									.fontSize = font_size,
									.wrapMode = CLAY_TEXT_WRAP_NONE,
							  }));
				} else {
					if (left_part.size > 0) {
						CLAY_TEXT(left_part.to_clay_string(),
						          CLAY_TEXT_CONFIG({
										.textColor = style.text,
										.fontId = effective_style.font_id,
										.fontSize = font_size,
										.wrapMode = CLAY_TEXT_WRAP_NONE,
								  }));
					}

					if (focused) {
						CLAY(CLAY_ID_LOCAL("Caret"),
						     {
								   .layout =
										 {
											   .sizing =
													 {CLAY_SIZING_FIXED(
															static_cast<float>(
																  caret_width)),
						                              CLAY_SIZING_FIXED(
															static_cast<float>(
																  caret_height))},
										 },
								   .backgroundColor = style.caret,
							 }) {}
					}

					if (has_composition) {
						CLAY_TEXT(composition.to_clay_string(),
						          CLAY_TEXT_CONFIG({
										.textColor = style.composition,
										.fontId = effective_style.font_id,
										.fontSize = font_size,
										.wrapMode = CLAY_TEXT_WRAP_NONE,
								  }));
					}

					if (right_part.size > 0) {
						CLAY_TEXT(right_part.to_clay_string(),
						          CLAY_TEXT_CONFIG({
										.textColor = style.text,
										.fontId = effective_style.font_id,
										.fontSize = font_size,
										.wrapMode = CLAY_TEXT_WRAP_NONE,
								  }));
					}
				}
			}
		}

		if (style.clearable && has_value) {
			CLAY(CLAY_ID_LOCAL("ClearButtonContainer"),
			     {
					   .floating =
							 {
								   .attachPoints =
										 {
											   .element =
													 CLAY_ATTACH_POINT_RIGHT_CENTER,
											   .parent =
													 CLAY_ATTACH_POINT_RIGHT_CENTER,
										 },
								   .pointerCaptureMode =
										 CLAY_POINTER_CAPTURE_MODE_PASSTHROUGH,
								   .attachTo = CLAY_ATTACH_TO_PARENT,
							 },
				 }) {

				auto btn_style = mobile_button_style_surface_container_high();
				btn_style.background.a = 0.f;
				btn_style.background_pressed.a = 0.f;
				btn_style.font_id = FontID::ICONS;
				btn_style.font_size *= 1.5f;
				btn_style.text.a *= 0.6f;

				auto b = mobile_button(ctx, CLAY_ID_LOCAL("ClearButton"),
				                       Icons::CLEAR, btn_style);
				if (b.activated()) {
					mobile_text_input_clear(ctx, value);
					runtime.changed_id = id.id;
				}
			}
		}
	}

	return {
		  .focused = focused,
		  .tapped = tapped,
		  .changed = runtime.changed_id == id.id,
		  .submitted = runtime.submitted_id == id.id,
		  .blurred = blurred,
	};
}
