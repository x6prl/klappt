#pragma once

#include <clay/clay.h>

#include "app/app_context.h"

// TODO: rewrite

struct FastListWindow {
	Size first{};
	Size last{};
};

inline void fast_list_spacer(Clay_ElementId id, float height) {
	if (height <= 0.f) {
		return;
	}
	CLAY(id, {.layout = {.sizing = {CLAY_SIZING_GROW(0),
	                                CLAY_SIZING_FIXED(height)}}}) {}
}

inline FastListWindow fast_list_window(Clay_ElementId id, Size item_count,
                                       float item_height, Size overscan = 2) {
	if (item_count <= 0 || item_height <= 0.f) {
		return {};
	}

	const auto scroll = Clay_GetScrollContainerData(id);
	const float viewport_height =
		  scroll.found ? scroll.scrollContainerDimensions.height
					   : item_height * 30.f;
	float scroll_y = 0.f;
	if (scroll.found && scroll.scrollPosition) {
		scroll_y = -scroll.scrollPosition->y;
		if (scroll_y < 0.f) {
			scroll_y = 0.f;
		}
	}

	Size first = static_cast<Size>(scroll_y / item_height);
	if (first > overscan) {
		first -= overscan;
	} else {
		first = 0;
	}

	Size visible =
		  static_cast<Size>(viewport_height / item_height) + 1 + overscan * 2;
	if (visible > 150) {
		SDL_LogWarn(
			  SDL_LOG_CATEGORY_APPLICATION,
			  "fast_list: viewport is extremely large, clamping items to 150.");
		visible = 150;
	} else if (visible < 1) {
		visible = 1;
	}
	Size last = first + visible;
	if (last > item_count) {
		last = item_count;
	}
	if (last <= first) {
		last = first + 1;
		if (last > item_count) {
			last = item_count;
			first = item_count - 1;
			if (first < 0) {
				first = 0;
			}
		}
	}
	return {first, last};
}

template <class F>
inline void fast_list(AppContext *ctx, Clay_ElementId id, Size item_count,
                      float item_height, F &&render_items, Size overscan = 2) {
	(void)ctx;
	const auto window = fast_list_window(id, item_count, item_height, overscan);
	const float top_spacer = item_height * static_cast<float>(window.first);
	const float bottom_spacer =
		  item_height * static_cast<float>(item_count - window.last);

	CLAY(id,
	     {.layout =
	            {
					  .sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)},
					  .layoutDirection = CLAY_TOP_TO_BOTTOM,
				},
	      .clip = {.vertical = true, .childOffset = Clay_GetScrollOffset()}}) {
		fast_list_spacer(CLAY_IDI("FastListTopSpacer", id.id), top_spacer);
		render_items(window);
		fast_list_spacer(CLAY_IDI("FastListBottomSpacer", id.id),
		                 bottom_spacer);
	}
}
