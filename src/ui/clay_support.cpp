#include "clay_support.h"

#include <cstdlib>

#include "SDL3/SDL_log.h"
#include "SDL3/SDL_mouse.h"
#include "SDL3/SDL_video.h"

#include <clay/clay.h>

#include "app/app_context.h"

namespace {

Clay_Dimensions clay_layout_dimensions{};
Clay_Vector2 pending_scroll_delta{};

void clay_handle_errors(Clay_ErrorData errorData) {
	// See the Clay_ErrorData struct for more information.
	SDL_Log("%s", errorData.errorText.chars);
	switch (errorData.errorType) {
		// etc
	default:;
	}
}

Clay_Dimensions get_window_sizef(SDL_Window *window) {
	int window_size_w{}, window_size_h{};
	SDL_GetWindowSizeInPixels(window, &window_size_w, &window_size_h);
	return {static_cast<float>(window_size_w),
	        static_cast<float>(window_size_h)};
}

} // namespace

void clay_handle_event(SDL_Event *event) {
	static float pointer_x = 0.f;
	static float pointer_y = 0.f;
	static bool pointer_down = false;

	switch (event->type) {
	case SDL_EVENT_MOUSE_MOTION: {
		pointer_x = event->motion.x;
		pointer_y = event->motion.y;
		SDL_MouseButtonFlags buttons = SDL_GetMouseState(nullptr, nullptr);
		pointer_down = (buttons & SDL_BUTTON_LMASK) != 0;
		break;
	}
	case SDL_EVENT_MOUSE_BUTTON_DOWN:
	case SDL_EVENT_MOUSE_BUTTON_UP:
		pointer_x = event->button.x;
		pointer_y = event->button.y;
		if (event->button.button == SDL_BUTTON_LEFT) {
			pointer_down = event->type == SDL_EVENT_MOUSE_BUTTON_DOWN;
		}
		break;
	case SDL_EVENT_FINGER_MOTION:
	case SDL_EVENT_FINGER_DOWN:
	case SDL_EVENT_FINGER_UP:
	case SDL_EVENT_FINGER_CANCELED: {
		pointer_x = event->tfinger.x * clay_layout_dimensions.width;
		pointer_y = event->tfinger.y * clay_layout_dimensions.height;
		pointer_down = event->type == SDL_EVENT_FINGER_MOTION ||
		               event->type == SDL_EVENT_FINGER_DOWN;
		break;
	}
	}

	Clay_SetPointerState({pointer_x, pointer_y}, pointer_down);

	switch (event->type) {
	case SDL_EVENT_WINDOW_RESIZED:
		clay_layout_dimensions =
			  get_window_sizef(SDL_GetWindowFromID(event->window.windowID));
		Clay_SetLayoutDimensions(clay_layout_dimensions);
		break;
	case SDL_EVENT_MOUSE_WHEEL:
		pending_scroll_delta.x += event->wheel.x;
		pending_scroll_delta.y += event->wheel.y;
		break;
	}
}

void clay_update_scroll(float delta_time_seconds) {
	Clay_UpdateScrollContainers(true, pending_scroll_delta, delta_time_seconds);
	pending_scroll_delta = {};
}

void clay_init(AppContext *ctx) {
	if (ctx->clay_arena.memory) {
		free(ctx->clay_arena.memory);
		Clay_SetCurrentContext(nullptr);
	}

	Clay_SetMaxElementCount(40000);
	Clay_SetMaxMeasureTextCacheWordCount(100000);
	auto clay_memory_footprint = Clay_MinMemorySize();
	auto clay_arena = Clay_CreateArenaWithCapacityAndMemory(
		  clay_memory_footprint, malloc(clay_memory_footprint));
	ctx->clay_arena = clay_arena;
	auto window_size = get_window_sizef(ctx->window);
	clay_layout_dimensions = window_size;
	Clay_Initialize(ctx->clay_arena, clay_layout_dimensions,
	                {.errorHandlerFunction = clay_handle_errors,
	                 .userData = nullptr});
	Clay_SetDpiScale(ctx->scale);
	Clay_SetCullingEnabled(true);
}
