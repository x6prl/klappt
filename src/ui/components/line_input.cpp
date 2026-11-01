#include "line_input.h"

#if defined(ANDROID)
#include <SDL3/SDL_system.h>
#include <jni.h>
#endif

#include <SDL3/SDL_clipboard.h>
#include <SDL3/SDL_log.h>
#include <SDL3/SDL_stdinc.h>

#include "app/sizes.h"
#include "app/tslt.h"
#include "app/worker.h"
#include "base/arena.h"
#include "base/str_view.h"

#include "ui/components/button.h"
#include "ui/components/line_input_state.h"
#include "ui/screen_helpers.h"

// #define LINE_INPUT_DEBUG

namespace {

float get_glyph_width(LineInputState *state, UTF32Codepoint cp) {
	auto g = state->text_cache->get_glyph(cp, state->font_id, state->font_size);
	return g->advance;
}

int my_insert_chars(LineInputState *input, int pos, const uint32_t *new_chars,
                    int n) {
	constexpr int ERROR = 0;
	constexpr int SUCCESS = 1;
	if (input->length + n > input->CAPACITY) {
		return ERROR;
	}
	memmove(&input->data[pos + n], &input->data[pos],
	        (input->length - pos) * sizeof(uint32_t));
	memcpy(&input->data[pos], new_chars, n * sizeof(uint32_t));
	input->length += n;
	return SUCCESS;
}

void my_delete_chars(LineInputState *input, int pos, int n) {
	memmove(&input->data[pos], &input->data[pos + n],
	        (input->length - (pos + n)) * sizeof(UTF32Codepoint));
	input->length -= n;
}

void my_layout_row(auto *r, LineInputState *input, int line_start_index) {
	(void)line_start_index;
	float total_width = 0.0f;
	for (LineInputPosition i = 0; i < input->length; ++i) {
		total_width += get_glyph_width(input, input->data[i]);
	}
	r->num_chars = input->length;
	r->x0 = 0.0f;
	r->x1 = total_width;
	r->baseline_y_delta = static_cast<float>(input->font_size);
	r->ymin = 0.0f;
	r->ymax = static_cast<float>(input->font_size);
}

void copy_selection_to_clipboard(Arena &scratch, LineInputState *input,
                                 auto stb_state) {
	if (stb_state->select_start == stb_state->select_end)
		return;
	int sel_min = std::min(stb_state->select_start, stb_state->select_end);
	int sel_max = std::max(stb_state->select_start, stb_state->select_end);
	auto str_length = sel_max - sel_min;

	char *buf = scratch.pushN<char>(str_length * sizeof(UTF32Codepoint) + 1);
	char *ptr = buf;
	for (Size i = 0; i < str_length; ++i) {
		ptr += UTF32::encode_to_utf8(input->data[sel_min + i], ptr);
	}
	*ptr = '\0';
	SDL_SetClipboardText(buf);
}
void copy_selection_to_primary(Arena &scratch, LineInputState *input,
                               auto stb_state) {
	(void)scratch;
	(void)input;
	(void)stb_state;
#ifndef ANDROID
	if (stb_state->select_start == stb_state->select_end)
		return;
	int sel_min = std::min(stb_state->select_start, stb_state->select_end);
	int sel_max = std::max(stb_state->select_start, stb_state->select_end);
	auto str_length = sel_max - sel_min;

	char *buf = scratch.pushN<char>(str_length * sizeof(UTF32Codepoint) + 1);
	char *ptr = buf;
	for (Size i = 0; i < str_length; ++i) {
		ptr += UTF32::encode_to_utf8(input->data[sel_min + i], ptr);
	}
	*ptr = '\0';
	SDL_SetPrimarySelectionText(buf);
#endif // !ANDROID
}

} // namespace

#define STB_TEXTEDIT_CHARTYPE UTF32Codepoint
#define STB_TEXTEDIT_POSITIONTYPE LineInputPosition
#define STB_TEXTEDIT_UNDOSTATECOUNT LineInputState::UNDO_STATE_COUNT
#define STB_TEXTEDIT_UNDOCHARCOUNT LineInputState::UNDO_CHAR_COUNT

//
// Symbols you must define for implementation mode:
//
#define STB_TEXTEDIT_STRING LineInputState
#define STB_TEXTEDIT_STRINGLEN(obj) ((STB_TEXTEDIT_POSITIONTYPE)(obj)->length)
#define STB_TEXTEDIT_LAYOUTROW(r, obj, n) my_layout_row(r, obj, n)

// returns the pixel delta from the xpos of the i'th character
// to the xpos of the i+1'th char for a line of characters
// starting at character #n (i.e. accounts for kerning
// with previous char)
#define STB_TEXTEDIT_GETWIDTH(obj, n, i) get_glyph_width((obj), (obj)->data[i])

// maps a keyboard input to an insertable character (return type is int, -1
// means not valid to insert)
#define STB_TEXTEDIT_KEYTOTEXT(k) (((k) > 0 && (k) <= 0x10FFFF) ? (int)(k) : -1)

#define STB_TEXTEDIT_GETCHAR(obj, i) ((obj)->data[i])
#define STB_TEXTEDIT_NEWLINE ((STB_TEXTEDIT_CHARTYPE)'\n')
#define STB_TEXTEDIT_DELETECHARS(obj, i, n) my_delete_chars(obj, i, n)
#define STB_TEXTEDIT_INSERTCHARS(obj, i, src, n) my_insert_chars(obj, i, src, n)

#define STB_TEXTEDIT_K_SHIFT 0x400000
// #define STB_TEXTEDIT_K_SHIFT     // a power of two that is or'd in to a
// keyboard
//                                  // input to represent the shift key
#define STB_TEXTEDIT_K_LEFT 0x200001  // keyboard input to move cursor left
#define STB_TEXTEDIT_K_RIGHT 0x200002 // keyboard input to move cursor right
#define STB_TEXTEDIT_K_UP 0x200003    // keyboard input to move cursor up
#define STB_TEXTEDIT_K_DOWN 0x200004  // keyboard input to move cursor down
#define STB_TEXTEDIT_K_PGUP 0x20000F  // keyboard input to move cursor up a page
#define STB_TEXTEDIT_K_PGDOWN                                                  \
	0x200010 // keyboard input to move cursor down a page
#define STB_TEXTEDIT_K_LINESTART 0x200005 // Home
#define STB_TEXTEDIT_K_LINEEND 0x200006   // End
#define STB_TEXTEDIT_K_TEXTSTART 0x200007 // Ctrl+Home
#define STB_TEXTEDIT_K_TEXTEND 0x200008   // Ctrl+End
#define STB_TEXTEDIT_K_DELETE                                                  \
	0x200009 // keyboard input to delete selection or character under cursor
#define STB_TEXTEDIT_K_BACKSPACE                                               \
	0x20000A // keyboard input to delete selection or character left of cursor
#define STB_TEXTEDIT_K_UNDO 0x20000B // keyboard input to perform undo
#define STB_TEXTEDIT_K_REDO 0x20000C // keyboard input to perform redo
// Optional:
//    STB_TEXTEDIT_K_INSERT              keyboard input to toggle insert mode
#define STB_TEXTEDIT_IS_SPACE(ch)                                              \
	((ch) == ' ' || (ch) == '\t' || (ch) == '\n' || (ch) == '\r' ||            \
	 (ch) == 0x00A0)
//          true if character is whitespace (e.g.
//    'isspace'),
//                                          required for default
//                                          WORDLEFT/WORDRIGHT handlers
#define STB_TEXTEDIT_K_WORDLEFT 0x20000D
#define STB_TEXTEDIT_K_WORDRIGHT 0x20000E
//    STB_TEXTEDIT_MOVEWORDLEFT(obj,i)   custom handler for WORDLEFT, returns
//    index to move cursor to STB_TEXTEDIT_MOVEWORDRIGHT(obj,i)  custom handler
//    for WORDRIGHT, returns index to move cursor to STB_TEXTEDIT_K_WORDLEFT
//    keyboard input to move cursor left one word // e.g. ctrl-LEFT
//    STB_TEXTEDIT_K_WORDRIGHT           keyboard input to move cursor right one
//    word // e.g. ctrl-RIGHT STB_TEXTEDIT_K_LINESTART2          secondary
//    keyboard input to move cursor to start of line STB_TEXTEDIT_K_LINEEND2
//    secondary keyboard input to move cursor to end of line
//    STB_TEXTEDIT_K_TEXTSTART2          secondary keyboard input to move cursor
//    to start of text STB_TEXTEDIT_K_TEXTEND2            secondary keyboard
//    input to move cursor to end of text

#define STB_TEXTEDIT_IMPLEMENTATION
#include "stb_textedit.h"

void sync_state_to_android_ime(LineInputState *input);
namespace {
#if defined(ANDROID) && defined(LINE_INPUT_DEBUG)

void log_line_input_event(const SDL_Event &event) {
	switch (event.type) {
	case SDL_EVENT_KEY_DOWN:
	case SDL_EVENT_KEY_UP: {
		const auto &k = event.key;

		SDL_Log("[LineInput] %s "
		        "key=%s (0x%08x) "
		        "scancode=%s (%u) raw=0x%04x "
		        "mod=0x%04x repeat=%d down=%d "
		        "which=%u timestamp=%llu",
		        event.type == SDL_EVENT_KEY_DOWN ? "KEY_DOWN" : "KEY_UP",
		        SDL_GetKeyName(k.key), static_cast<unsigned>(k.key),
		        SDL_GetScancodeName(k.scancode),
		        static_cast<unsigned>(k.scancode), static_cast<unsigned>(k.raw),
		        static_cast<unsigned>(k.mod), static_cast<int>(k.repeat),
		        static_cast<int>(k.down), static_cast<unsigned>(k.which),
		        static_cast<unsigned long long>(k.timestamp));
		break;
	}

	case SDL_EVENT_TEXT_EDITING:
		SDL_Log("[LineInput] TEXT_EDITING "
		        "bytes=%zu start=%d edit_length=%d text=|%s|",
		        event.edit.text ? strlen(event.edit.text) : 0, event.edit.start,
		        event.edit.length, event.edit.text);
		break;

	case SDL_EVENT_TEXT_INPUT:
		SDL_Log("[LineInput] TEXT_INPUT bytes=%zu, string |%s|",
		        event.text.text ? strlen(event.text.text) : 0, event.text.text);
		break;

	default:
		// SDL_Log("[LineInput] unknown event |%d|", event.type);
		break;
	}
}

void log_line_input_state(const char *stage, const LineInputState *input,
                          const STB_TexteditState *stb) {

	SDL_Log("[LineInput] %s "
	        "cursor=%d selection=(%d,%d) length=%d "
	        "composition=(length=%d cursor=%d selection=%d) "
	        "dragging=%d popup=%d scroll_x=%.2f",
	        stage, static_cast<int>(stb->cursor),
	        static_cast<int>(stb->select_start),
	        static_cast<int>(stb->select_end), static_cast<int>(input->length),
	        static_cast<int>(input->composition_length),
	        static_cast<int>(input->composition_cursor),
	        static_cast<int>(input->composition_sel_length),
	        static_cast<int>(input->is_dragging),
	        static_cast<int>(input->is_show_popup_menu), input->scroll_x);
}
#endif

void clear_composition(LineInputState *input, bool notify_ime = true) {
	const bool had_composition = input->is_composing();

	// SDL_Log("[LineInput] clear_composition?");
	input->composition_length = 0;
	input->composition_cursor = 0;
	input->composition_sel_length = 0;

	if (had_composition && notify_ime) {
		SDL_Log("[LineInput] clear_composition for window");
		SDL_ClearComposition(tctx()->app_ctx->window);
	}
}

void paste_from_clipboard(Arena &scratch, LineInputState *input) {
	auto g = scratch.guard();
	auto stb_state = static_cast<STB_TexteditState *>(input->stb_state);

	char *clip_cstr = SDL_GetClipboardText();

	if (clip_cstr) {
		auto clip_cstr_size_bytes = strlen(clip_cstr);

		LineInputPosition selection_length =
			  std::abs(stb_state->select_end - stb_state->select_start);

		LineInputPosition available =
			  LineInputState::CAPACITY - input->length + selection_length;

		// new lines filtering
		for (size_t i = 0; i < clip_cstr_size_bytes; ++i) {
			auto &ch = clip_cstr[i];
			if (ch == '\n' || ch == '\r') {
				ch = ' ';
			}
		}

		StrView utf8_str{clip_cstr, (Size)clip_cstr_size_bytes};
		auto [utf32_codepoints, length] =
			  UTF32::decode_from_utf8(scratch, utf8_str);
		length = std::min(length, (Size)available);
		if (length <= available) {
			clear_composition(input);
			stb_textedit_paste(input, stb_state, utf32_codepoints, length);
		}

		SDL_free(clip_cstr);
	}
}

void select_all(LineInputState *input) {
	auto stb_state = static_cast<STB_TexteditState *>(input->stb_state);

	stb_state->select_start = 0;
	stb_state->select_end = input->length;
	stb_state->cursor = input->length;
}

void select_word(LineInputState *input) {
	auto is_word_boundary = [](UTF32Codepoint cp) {
		// ASCII letters and digits
		if ((cp >= 'a' && cp <= 'z') || (cp >= 'A' && cp <= 'Z') ||
		    (cp >= '0' && cp <= '9') || cp == '_')
			return false;

		// Latin-1 Supplement
		if ((cp >= 0x00C0 && cp <= 0x00D6) || (cp >= 0x00D8 && cp <= 0x00F6) ||
		    (cp >= 0x00F8 && cp <= 0x00FF))
			return false;

		// Cyrillic
		if ((cp >= 0x0400 && cp <= 0x0481) || (cp >= 0x048A && cp <= 0x052F))
			return false;

		// Combining diacritical marks
		if ((cp >= 0x0300 && cp <= 0x036F))
			return false;

		return true;
	};

	auto stb_state = static_cast<STB_TexteditState *>(input->stb_state);
	int start = stb_state->cursor;
	int end = stb_state->cursor;
	auto data = input->data;

	while (start > 0 && !is_word_boundary(data[start - 1]))
		--start;

	while (end < input->length && !is_word_boundary(data[end]))
		++end;

	stb_state->select_start = start;
	stb_state->select_end = end;
	stb_state->cursor = end;
}

inline void native_input_activate(SDL_Window *window,
                                  const LineInputStyle &style) {
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

inline void focus_input(AppContext *ctx, LineInputState *input,
                        Clay_ElementId id, const Clay_BoundingBox &box,
                        const LineInputStyle &style) {
	ctx->focused_input = input;
	ctx->focused_input_id = id.id;
	native_input_activate(ctx->window, style);
	sync_state_to_android_ime(input);

	SDL_Rect rect{
		  .x = static_cast<int>(box.x),
		  .y = static_cast<int>(box.y),
		  .w = static_cast<int>(box.width),
		  .h = static_cast<int>(box.height),
	};
	SDL_SetTextInputArea(ctx->window, &rect, 0);
}

inline void native_input_deactivate(SDL_Window *window) {
	if (SDL_TextInputActive(window)) {
		SDL_StopTextInput(window);
	}
	SDL_SetTextInputArea(window, nullptr, 0);
}

} // namespace

void sync_state_to_android_ime(LineInputState *input) {
#if defined(ANDROID)
	auto *stb_state = static_cast<STB_TexteditState *>(input->stb_state);
	JNIEnv *env = static_cast<JNIEnv *>(SDL_GetAndroidJNIEnv());
	if (!env)
		return;

	// JNI's NewStringUTF expects modified UTF-8, which cannot represent a
	// supplementary-plane code point in normal UTF-8. Build UTF-16 directly.
	jchar utf16_buf[LineInputState::CAPACITY * 2];
	jsize utf16_length = 0;
	for (LineInputPosition i = 0; i < input->length; ++i) {
		const UTF32Codepoint cp = input->data[i];
		if (cp <= 0xFFFF) {
			utf16_buf[utf16_length++] = static_cast<jchar>(cp);
		} else if (cp <= 0x10FFFF) {
			const UTF32Codepoint value = cp - 0x10000;
			utf16_buf[utf16_length++] =
				  static_cast<jchar>(0xD800 + (value >> 10));
			utf16_buf[utf16_length++] =
				  static_cast<jchar>(0xDC00 + (value & 0x3FF));
		}
	}

	jstring jtext = env->NewString(utf16_buf, utf16_length);
	if (!jtext)
		return;
	jclass cls = env->FindClass("org/libsdl/app/SDLInputConnection");
	if (cls) {
		jmethodID mid = env->GetStaticMethodID(cls, "setNativeBufferState",
		                                       "(Ljava/lang/String;II)V");
		if (mid) {
			int sel_start = stb_state->select_start;
			int sel_end = stb_state->select_end;
			env->CallStaticVoidMethod(cls, mid, jtext, sel_start, sel_end);
		}
		env->DeleteLocalRef(cls);
	}
	env->DeleteLocalRef(jtext);
#else
	(void)input;
#endif
}

namespace LineInput {

void init(LineInputState *input, Arena &a, TextCache *text) {
	input->text_cache = text;
	input->data = a.pushN<UTF32Codepoint>(LineInputState::CAPACITY);
	input->stb_state = a.push(sizeof(STB_TexteditState));
	input->length = 0;
	input->scroll_x = 0.0f;
	clear_composition(input);

	int SINGLE_LINE = 1;
	stb_textedit_initialize_state(
		  static_cast<STB_TexteditState *>(input->stb_state), SINGLE_LINE);
}

void clear(LineInputState *input) {
	input->length = 0;
	input->scroll_x = 0.0f;
	input->is_show_popup_menu = false;
	if (input->stb_state) {
		stb_textedit_initialize_state(
			  static_cast<STB_TexteditState *>(input->stb_state), 1);
	}
	clear_composition(input);
	input->is_dragging = false;
	sync_state_to_android_ime(input); // <--- Keep this
}

bool handle_keyboard_event(Arena &scratch, LineInputState *input,
                           const SDL_Event *event) {
	auto g = scratch.guard();
	auto *stb_state = static_cast<STB_TexteditState *>(input->stb_state);
	bool handled = false;

#if defined(ANDROID) && defined(LINE_INPUT_DEBUG)
	log_line_input_event(*event);
	// log_line_input_state("BEFORE", input, stb_state);
#endif

	switch (event->type) {
	case SDL_EVENT_WINDOW_FOCUS_LOST:
		// native_input_deactivate(tctx()->app_ctx->window);
		break;
	case SDL_EVENT_TEXT_EDITING: {
		handled = true;
		const char *text = event->edit.text;
		input->composition_length = 0;
		input->composition_cursor = 0;
		input->composition_sel_length = 0;

		if (!text || text[0] == '\0') {
			if (event->edit.start >= 0 && event->edit.start <= input->length) {
				stb_state->select_start = event->edit.start;
				stb_state->select_end =
					  std::clamp(event->edit.start + event->edit.length, 0,
				                 static_cast<int>(input->length));
				// Android uses a signed selection length. The cursor belongs at
				// the active end, which is especially important for a backwards
				// spacebar-swipe selection (negative length).
				stb_state->cursor = stb_state->select_end;
			}
		} else {
			auto text_len = strlen(text);
			StrView utf8_str{text, (Size)text_len};
			auto [utf32_codepoints, length] =
				  UTF32::decode_from_utf8(scratch, utf8_str);

			input->composition_length = std::min(
				  static_cast<int>(length),
				  static_cast<int>(LineInputState::COMPOSITION_CAPACITY));

			for (int i = 0; i < input->composition_length; ++i) {
				input->composition_data[i] = utf32_codepoints[i];
			}

			input->composition_cursor = (event->edit.start >= 0)
			                                  ? event->edit.start
			                                  : input->composition_length;
			input->composition_sel_length =
				  (event->edit.length >= 0) ? event->edit.length : 0;
		}
		input->is_show_popup_menu = false;
		break;
	}
	case SDL_EVENT_TEXT_INPUT: {
		const bool had_composition = input->is_composing();
		input->composition_length = 0;
		input->composition_cursor = 0;
		input->composition_sel_length = 0;
		handled = had_composition;

		const char *p = event->text.text;
		if (!p || !p[0]) {
			break;
		}

		auto text_len = strlen(p);
		StrView utf8_str{p, (Size)text_len};
		auto [utf32_codepoints, length] =
			  UTF32::decode_from_utf8(scratch, utf8_str);

		if (length > 0) {
			Size valid_len = 0;
			for (Size i = 0; i < length; ++i) {
				if (utf32_codepoints[i] != '\n' &&
				    utf32_codepoints[i] != '\r') {
					utf32_codepoints[valid_len++] = utf32_codepoints[i];
				}
			}

			if (valid_len > 0) {
				stb_textedit_paste(input, stb_state, utf32_codepoints,
				                   (int)valid_len);
				handled = true;
			}
		}

		input->is_show_popup_menu = false;
		break;
	}
	case SDL_EVENT_KEY_DOWN: {
		bool is_shift = (event->key.mod & SDL_KMOD_SHIFT) != 0;
		// macOS/iOS uses GUI (Cmd), others use Ctrl
		bool is_ctrl = (event->key.mod & SDL_KMOD_CTRL) ||
		               (event->key.mod & SDL_KMOD_GUI);
		int key = 0;

		switch (event->key.key) {
		case SDLK_LEFT:
			key = is_ctrl ? STB_TEXTEDIT_K_WORDLEFT : STB_TEXTEDIT_K_LEFT;
			break;
		case SDLK_RIGHT:
			key = is_ctrl ? STB_TEXTEDIT_K_WORDRIGHT : STB_TEXTEDIT_K_RIGHT;
			break;
		case SDLK_HOME:
			key = is_ctrl ? STB_TEXTEDIT_K_TEXTSTART : STB_TEXTEDIT_K_LINESTART;
			break;
		case SDLK_END:
			key = is_ctrl ? STB_TEXTEDIT_K_TEXTEND : STB_TEXTEDIT_K_LINEEND;
			break;
		case SDLK_BACKSPACE:
			key = STB_TEXTEDIT_K_BACKSPACE;
			break;
		case SDLK_DELETE:
			key = STB_TEXTEDIT_K_DELETE;
			break;
		case SDLK_Z:
			if (is_ctrl)
				key = is_shift ? STB_TEXTEDIT_K_REDO : STB_TEXTEDIT_K_UNDO;
			break;
		case SDLK_Y:
			if (is_ctrl)
				key = STB_TEXTEDIT_K_REDO;
			break;
		case SDLK_C:
			if (is_ctrl && (stb_state->select_start != stb_state->select_end)) {
				copy_selection_to_clipboard(scratch, input, stb_state);
				handled = true;
			}
			break;
		case SDLK_V:
			if (is_ctrl && SDL_HasClipboardText()) {
				paste_from_clipboard(scratch, input);
				handled = true;
			}
			break;
		case SDLK_X:
			if (is_ctrl) {
				copy_selection_to_clipboard(scratch, input, stb_state);
				stb_textedit_cut(input, stb_state);
				handled = true;
			}
			break;
		case SDLK_A:
			if (is_ctrl) {
				stb_state->select_start = 0;
				stb_state->select_end = input->length;
				stb_state->cursor = input->length;
				handled = true;
			}
			break;
		case SDLK_RETURN:
		case SDLK_KP_ENTER:
			native_input_deactivate(tctx()->app_ctx->window);
			handled = true;
			break;
		}

		if (key != 0) {
			if (is_shift && key != STB_TEXTEDIT_K_REDO) {
				key |= STB_TEXTEDIT_K_SHIFT;
			}
			stb_textedit_key(input, stb_state, key);
			if (!is_shift) {
				stb_state->select_start = stb_state->cursor;
				stb_state->select_end = stb_state->cursor;
			}
			input->is_show_popup_menu = false;
			handled = true;
		}
		break;
	}
	}
#if defined(ANDROID) && defined(LINE_INPUT_DEBUG)
	// log_line_input_state("AFTER", input, stb_state);
#endif
	return handled;
}

void request_focus(LineInputState *input) { input->focus_requested = true; }

void ui_line_input(AppContext *ctx, Clay_ElementId id, LineInputState *input,
                   StrView placeholder, const LineInputStyle &style,
                   Clay_ElementId focus_preserving_element) {
	input->font_id = style.font_id;
	input->font_size = style.font_size;
	input->padding_x = style.padding_x;
	input->padding_y = style.padding_y;

	auto &tslt = ctx->tslt;
	auto elem_data = Clay_GetElementData(id);
	const Clay_BoundingBox &widget_bounding_box = elem_data.boundingBox;
	bool is_focused = (ctx->focused_input == input);
	if (input->focus_requested) {
		focus_input(ctx, input, id, widget_bounding_box, style);
		input->focus_requested = false;
		is_focused = true;
	}

	const bool has_value = (input->length > 0 || input->is_composing());

	auto clear_btn_container_id = CLAY_IDI("LineInputClearBtnContainer", id.id);
	auto clear_data = Clay_GetElementData(clear_btn_container_id);

	float clear_btn_width = 0.0f;
	if (style.clearable && has_value) {
		clear_btn_width =
			  clear_data.found
					? clear_data.boundingBox.width
					: (sizes()->dim.icon_sm + sizes()->space.sm * 2.0f);
	}

	const float right_padding =
		  (style.clearable && has_value) ? clear_btn_width : style.padding_x;

	bool is_touch_on_clear_btn = false;
	if (style.clearable && has_value && elem_data.found) {
		Clay_BoundingBox clear_box =
			  clear_data.found
					? clear_data.boundingBox
					: Clay_BoundingBox{
							.x = widget_bounding_box.x +
		                         widget_bounding_box.width - clear_btn_width,
							.y = widget_bounding_box.y,
							.width = clear_btn_width,
							.height = widget_bounding_box.height,
					  };
		is_touch_on_clear_btn = tslt.is_started_and_ended_inside(clear_box);
	}

	bool is_touch_inside =
		  tslt.is_started_and_ended_inside(widget_bounding_box) &&
		  !is_touch_on_clear_btn;
	const bool is_touch_on_focus_preserving_element =
		  focus_preserving_element.id != 0 &&
		  Clay_PointerOver(focus_preserving_element);
	auto *stb_state = static_cast<STB_TexteditState *>(input->stb_state);
	auto popup_menu_clay_id = CLAY_ID("LineEditPopUpMenu");

	if (is_touch_inside && tslt.is_tap_or_longtap()) {
		clear_composition(input);

		float local_x = (tslt.x - (widget_bounding_box.x + style.padding_x)) +
		                input->scroll_x;
		float local_y = (tslt.y - (widget_bounding_box.y + style.padding_y));

		SDL_Log("[LineInput] TSLT "
		        "started_inside=%d scroll=%d "
		        "x=%.2f y=%.2f motion=(%.2f,%.2f)",
		        tslt.is_started_inside(widget_bounding_box), tslt.is_scroll(),
		        tslt.x, tslt.y, tslt.motion_x, tslt.motion_y);
		stb_textedit_click(input, stb_state, local_x, local_y);
		if (tslt.consume_longtap()) {
			if (!is_focused) {
				focus_input(ctx, input, id, widget_bounding_box, style);
				is_focused = true;
			}
			if (stb_state->select_end == stb_state->select_start) {
				select_word(input);
				sync_state_to_android_ime(input);
			}
			input->is_show_popup_menu = true;
			ctx->push_one_frame();
		} else if (tslt.consume_tap()) {
			if (!is_focused) {
				focus_input(ctx, input, id, widget_bounding_box, style);
				is_focused = true;
			}

			stb_textedit_click(input, stb_state, local_x, local_y);
			ctx->push_one_frame();
			if (tslt.tap_count == 2) {
				select_word(input);
				sync_state_to_android_ime(input);
				copy_selection_to_primary(ctx->arena_frame, input, stb_state);
				input->is_show_popup_menu = true;
			} else if (tslt.tap_count >= 3) {
				select_all(input);
				sync_state_to_android_ime(input);
				copy_selection_to_primary(ctx->arena_frame, input, stb_state);
				input->is_show_popup_menu = true;
			} else {
				sync_state_to_android_ime(input);
				input->is_show_popup_menu = false;
			}
		}
	} else if (is_focused) {
		if (tslt.is_started_inside(widget_bounding_box) &&
		    !is_touch_on_clear_btn) {
			// dragging
			if (tslt.is_scroll()) {
				clear_composition(input);
				input->is_show_popup_menu = false;
				float local_motion_x = (tslt.motion_x - (widget_bounding_box.x +
				                                         style.padding_x)) +
				                       input->scroll_x;
				float local_motion_y = (tslt.motion_y - (widget_bounding_box.y +
				                                         style.padding_y));
				if (!input->is_dragging) {
					input->is_dragging = true;
					float local_x =
						  (tslt.x - (widget_bounding_box.x + style.padding_x)) +
						  input->scroll_x;
					float local_y =
						  (tslt.y - (widget_bounding_box.y + style.padding_y));
					stb_textedit_click(input, stb_state, local_x, local_y);
				}
				stb_textedit_drag(input, stb_state, local_motion_x,
				                  local_motion_y);
			} else {
				input->is_dragging = false;
				copy_selection_to_primary(ctx->arena_frame, input, stb_state);
				sync_state_to_android_ime(input); // <--- Add this
			}
		} else if (!is_touch_on_clear_btn &&
		           !is_touch_on_focus_preserving_element &&
		           tslt.is_tap_or_longtap_or_scroll()) {
			// outside click
			auto popup_menu_data = Clay_GetElementData(popup_menu_clay_id);
			bool is_touching_popup_menu =
				  input->is_show_popup_menu && popup_menu_data.found &&
				  tslt.is_started_inside(popup_menu_data.boundingBox);

			if (!is_touching_popup_menu && tslt.is_tap_or_longtap_or_scroll()) {
				ctx->focused_input = nullptr;
				ctx->focused_input_id = 0;
				native_input_deactivate(ctx->window);
				clear_composition(input);
				is_focused = false;
				ctx->push_one_frame();
				input->is_show_popup_menu = false;
			}
		}
	}

	LineInputPosition comp_replace_start =
		  std::min(stb_state->select_start, stb_state->select_end);
	LineInputPosition comp_replace_end =
		  std::max(stb_state->select_start, stb_state->select_end);
	LineInputPosition total_glyphs = input->length + input->composition_length;
	UTF32Codepoint *display_data = input->data;
	LineInputPosition comp_insert_idx = stb_state->cursor;

	if (input->is_composing()) {
		total_glyphs -= comp_replace_end - comp_replace_start;
		comp_insert_idx = comp_replace_start;
		display_data = ctx->arena_frame.pushN<UTF32Codepoint>(total_glyphs);
		memcpy(display_data, input->data,
		       comp_replace_start * sizeof(UTF32Codepoint));
		memcpy(display_data + comp_insert_idx, input->composition_data,
		       input->composition_length * sizeof(UTF32Codepoint));
		memcpy(display_data + comp_insert_idx + input->composition_length,
		       input->data + comp_replace_end,
		       (input->length - comp_replace_end) * sizeof(UTF32Codepoint));
	}
	float render_cursor_x{0.0f};
	float render_sel_x1{0.0f};
	float render_sel_x2{0.0f};
	float render_comp_x1{0.0f};
	float render_comp_x2{0.0f};
	bool render_is_has_selection{false};

	float current_x = 0.0f;

	{ // selection and cursor pos calculations
		LineInputPosition sel_min =
			  std::min(stb_state->select_start, stb_state->select_end);
		LineInputPosition sel_max =
			  std::max(stb_state->select_start, stb_state->select_end);
		render_is_has_selection = (sel_min != sel_max);

		LineInputPosition target_cursor_idx =
			  input->is_composing()
					? (comp_insert_idx + input->composition_cursor)
					: stb_state->cursor;

		for (LineInputPosition i = 0; i < total_glyphs; ++i) {
			if (i == target_cursor_idx)
				render_cursor_x = current_x;

			if (input->is_composing()) {
				if (i == comp_insert_idx)
					render_comp_x1 = current_x;
				if (i == comp_insert_idx + input->composition_length)
					render_comp_x2 = current_x;
			}

			if (!input->is_composing()) {
				if (i == sel_min)
					render_sel_x1 = current_x;
				if (i == sel_max)
					render_sel_x2 = current_x;
			}

			current_x += get_glyph_width(input, display_data[i]);
		}

		if (target_cursor_idx == total_glyphs)
			render_cursor_x = current_x;
		if (input->is_composing() &&
		    (comp_insert_idx + input->composition_length) == total_glyphs)
			render_comp_x2 = current_x;
		if (!input->is_composing()) {
			if (sel_min == total_glyphs)
				render_sel_x1 = current_x;
			if (sel_max == total_glyphs)
				render_sel_x2 = current_x;
		}
	}

	// cursor autoscroll (accounting for clear button width)
	if (elem_data.found) {
		float visible_width =
			  elem_data.boundingBox.width - style.padding_x - right_padding;
		if (visible_width > 0.0f) {
			float caret_margin = style.caret_width + 4.0f;
			if (render_cursor_x < input->scroll_x) {
				input->scroll_x = std::max(0.0f, render_cursor_x - 4.0f);
			} else if (render_cursor_x + caret_margin >
			           input->scroll_x + visible_width) {
				input->scroll_x =
					  (render_cursor_x + caret_margin) - visible_width;
			}

			float max_scroll = std::max(0.0f, current_x - visible_width);
			if (input->scroll_x > max_scroll) {
				input->scroll_x = max_scroll;
			}
		}
	}
	if (input->scroll_x < 0.0f)
		input->scroll_x = 0.0f;

	if (is_focused && elem_data.found) {
		SDL_Rect ime_rect{
			  .x = static_cast<int>(widget_bounding_box.x),
			  .y = static_cast<int>(widget_bounding_box.y),
			  .w = static_cast<int>(widget_bounding_box.width),
			  .h = static_cast<int>(widget_bounding_box.height),
		};
		int cursor_offset_px = static_cast<int>(
			  style.padding_x + render_cursor_x - input->scroll_x);

		SDL_SetTextInputArea(ctx->window, &ime_rect, cursor_offset_px);
	}

	CLAY(id, {.layout =
	                {
						  .sizing = {.width = CLAY_SIZING_GROW(),
	                                 .height = CLAY_SIZING_FIXED(style.height)},
						  .padding = {.left = style.padding_x,
	                                  .right = (uint16_t)right_padding,
	                                  .top = style.padding_y,
	                                  .bottom = style.padding_y},
						  .childAlignment = {.y = CLAY_ALIGN_Y_CENTER},
					},
	          .backgroundColor = style.background,
	          .cornerRadius = CLAY_CORNER_RADIUS(style.corner_radius),
	          .border = {
					.color = is_focused ? style.border_focused : style.border,
					.width = {style.border_width, style.border_width,
	                          style.border_width, style.border_width},
			  }}) {

		CLAY(CLAY_ID_LOCAL("Clipper"),
		     {.layout =
		            {
						  .sizing = {CLAY_SIZING_GROW(),
		                             CLAY_SIZING_FIXED(style.height)},
						  .childAlignment = {.y = CLAY_ALIGN_Y_CENTER},
					},
		      .clip = {
					.horizontal = true,
					.childOffset = {-input->scroll_x, 0.0f},
			  }}) {

			if (render_is_has_selection && !input->is_composing()) {
				CLAY(CLAY_ID_LOCAL("SelectionRec"),
				     {
						   .layout = {.sizing =
				                            {
												  .width = CLAY_SIZING_FIXED(
														render_sel_x2 -
														render_sel_x1),
												  .height = CLAY_SIZING_FIXED(
														style.caret_height),
											}},
						   .backgroundColor = style.selection,
						   .floating =
								 {
									   .offset = {render_sel_x1 -
				                                        input->scroll_x,
				                                  0.0f},
									   .zIndex = 0,
									   .attachPoints =
											 {.element =
				                                    CLAY_ATTACH_POINT_LEFT_CENTER,
				                              .parent =
				                                    CLAY_ATTACH_POINT_LEFT_CENTER},
									   .attachTo = CLAY_ATTACH_TO_PARENT,
									   .clipTo = CLAY_CLIP_TO_ATTACHED_PARENT,
								 },
					 }) {}
			}

			if (input->is_composing() && (render_comp_x2 > render_comp_x1)) {
				CLAY(CLAY_ID_LOCAL("CompositionUnderline"),
				     {
						   .layout = {.sizing =
				                            {
												  .width = CLAY_SIZING_FIXED(
														render_comp_x2 -
														render_comp_x1),
												  .height =
														CLAY_SIZING_FIXED(2.0f),
											}},
						   .backgroundColor = style.caret,
						   .floating =
								 {
									   .offset = {render_comp_x1 -
				                                        input->scroll_x,
				                                  (style.caret_height * 0.5f) -
				                                        1.0f},
									   .zIndex = 1,
									   .attachPoints =
											 {.element =
				                                    CLAY_ATTACH_POINT_LEFT_CENTER,
				                              .parent =
				                                    CLAY_ATTACH_POINT_LEFT_CENTER},
									   .attachTo = CLAY_ATTACH_TO_PARENT,
									   .clipTo = CLAY_CLIP_TO_ATTACHED_PARENT,
								 },
					 }) {}
			}

			if (total_glyphs > 0) {
				StrView str = StrView::from_utf32(ctx->arena_frame,
				                                  display_data, total_glyphs);
				draw_text(str, style.text, input->font_size, input->font_id,
				          CLAY_TEXT_WRAP_NONE, CLAY_TEXT_ALIGN_LEFT);
			} else if (!is_focused) {
				draw_text(placeholder, style.placeholder, input->font_size,
				          input->font_id, CLAY_TEXT_WRAP_NONE,
				          CLAY_TEXT_ALIGN_LEFT);
			}

			if (is_focused) {
				CLAY(CLAY_ID_LOCAL("Cursor"),
				     {
						   .layout = {.sizing = {.width = CLAY_SIZING_FIXED(
													   style.caret_width),
				                                 .height = CLAY_SIZING_FIXED(
													   style.caret_height)}},
						   .backgroundColor = style.caret,
						   .floating =
								 {
									   .offset = {render_cursor_x -
				                                        input->scroll_x,
				                                  0.0f},
									   .zIndex = 2,
									   .attachPoints =
											 {.element =
				                                    CLAY_ATTACH_POINT_LEFT_CENTER,
				                              .parent =
				                                    CLAY_ATTACH_POINT_LEFT_CENTER},
									   .attachTo = CLAY_ATTACH_TO_PARENT,
								 },
					 }) {

					if (input->is_show_popup_menu && !input->is_composing()) {
						CLAY(popup_menu_clay_id,
						     {
								   .layout =
										 {
											   .sizing = {CLAY_SIZING_FIT(0),
						                                  CLAY_SIZING_FIT(0)},
											   .childGap = sizes()->space.xs,
											   .childAlignment =
													 {CLAY_ALIGN_X_CENTER,
						                              CLAY_ALIGN_Y_CENTER},
										 },
								   .floating =
										 {
											   .offset = {0, (float)sizes()
						                                           ->space.xs},
											   .zIndex = 3,
											   .attachPoints =
													 {.element =
						                                    CLAY_ATTACH_POINT_CENTER_TOP,
						                              .parent =
						                                    CLAY_ATTACH_POINT_CENTER_BOTTOM},
											   .attachTo =
													 CLAY_ATTACH_TO_PARENT,
										 },
							 }) {
							auto btn_style =
								  mobile_button_style_surface_container_high();
							btn_style.border_width = 0;
							btn_style.font_size = sizes()->font.body_sm;
							btn_style.height =
								  2.f * sizes()->space.sm + btn_style.font_size;

							if (stb_state->select_end !=
							    stb_state->select_start) {
								auto copy_btn =
									  mobile_button(ctx, CLAY_ID("CopyBtn"),
								                    "Copy"_v, btn_style);
								if (copy_btn.activated() &&
								    tslt.consume_tap()) {
									input->is_show_popup_menu = false;
									copy_selection_to_clipboard(
										  ctx->arena_frame, input, stb_state);
								}
								auto cut_btn =
									  mobile_button(ctx, CLAY_ID("CutBtn"),
								                    "Cut"_v, btn_style);
								if (cut_btn.activated() && tslt.consume_tap()) {
									input->is_show_popup_menu = false;
									copy_selection_to_clipboard(
										  ctx->arena_frame, input, stb_state);
									stb_textedit_cut(input, stb_state);
									ctx->push_one_frame();
								}
							} else if (SDL_HasClipboardText()) {
								auto paste_btn =
									  mobile_button(ctx, CLAY_ID("PasteBtn"),
								                    "Paste"_v, btn_style);
								if (paste_btn.activated() &&
								    tslt.consume_tap()) {
									input->is_show_popup_menu = false;
									paste_from_clipboard(ctx->arena_frame,
									                     input);
									ctx->push_one_frame();
								}
							}
						}
					}
				}
			}
		}

		if (style.clearable && has_value) {
			CLAY(clear_btn_container_id,
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
				btn_style.border_width = 0;
				btn_style.border = {};
				btn_style.border_pressed = {};
				btn_style.font_id = FontID::ICONS;
				btn_style.font_size =
					  static_cast<uint16_t>(sizes()->dim.icon_sm);
				btn_style.height = sizes()->dim.min_touch_target;
				btn_style.min_width = sizes()->dim.min_touch_target * 0.65f;
				btn_style.padding_x = sizes()->space.lg;
				btn_style.padding_y = 0;
				btn_style.text.a *= 0.5f;

				auto b = mobile_button(ctx, CLAY_ID_LOCAL("ClearButton"),
				                       Icons::CLEAR, btn_style);
				if (b.activated()) {
					LineInput::clear(input);
					if (!is_focused) {
						focus_input(ctx, input, id, widget_bounding_box, style);
					}
					ctx->push_one_frame();
				}
			}
		}
	}
}

} // namespace LineInput

void LineInputState::clear() { LineInput::clear(this); }
