#pragma once

#include <SDL3/SDL_log.h>

#include <clay/clay.h>

#include "app/app_context.h"

namespace list {
struct ItemsRange {
	Size first{};
	Size last_exclusive{}; // exclusive
};

// vertical
template <typename TDrawFunc>
void vertical_uniform(AppContext *ctx, Clay_ElementId list_id, Size item_count,
                      float item_height, TDrawFunc draw_item);
template <typename TDrawItemsFunc>
void vertical_uniform_w(AppContext *ctx, Clay_ElementId list_id,
                        Size item_count, float item_height,
                        TDrawItemsFunc draw_items);
template <typename TDrawFunc, typename THeightFunc>
void vertical_fixed(AppContext *ctx, Clay_ElementId list_id, Size item_count,
                    THeightFunc get_item_height, TDrawFunc draw_item);
template <typename TDrawFunc, typename THeightFunc>
void vertical_fixed_rich(AppContext *ctx, Clay_ElementId list_id, uint16_t gap,
                         Clay_Padding padding, Size item_count,
                         THeightFunc get_item_height, TDrawFunc draw_item);
template <typename TDrawFunc>
void vertical_dynamic(AppContext *ctx, Clay_ElementId list_id, Size item_count,
                      TDrawFunc draw_item);
template <typename TDrawFunc>
void vertical_dynamic_rich(AppContext *ctx, Clay_ElementId list_id,
                           uint16_t gap, Clay_Padding padding, Size item_count,
                           TDrawFunc draw_item);

namespace {
inline void log_clay_scroll_container_data(const char *tag,
                                           Clay_ScrollContainerData data) {
	if (!data.found) {
		SDL_Log("[%s] Clay_ScrollContainerData: NOT FOUND (found = "
		        "false)",
		        tag ? tag : "ScrollData");
		return;
	}

	float scrollX = data.scrollPosition ? data.scrollPosition->x : 0.0f;
	float scrollY = data.scrollPosition ? data.scrollPosition->y : 0.0f;

	SDL_Log("[%s] Clay_ScrollContainerData:\n"
	        "  - found: true\n"
	        "  - scrollPosition: (x: %.2f, y: %.2f) [ptr: %p]\n"
	        "  - containerDimensions: (w: %.2f, h: %.2f)\n"
	        "  - contentDimensions:   (w: %.2f, h: %.2f)\n"
	        "  - config (clip): [horizontal: %s, vertical: %s, "
	        "childOffset: (%.2f, %.2f)]",
	        tag ? tag : "ScrollData", scrollX, scrollY,
	        (void *)data.scrollPosition, data.scrollContainerDimensions.width,
	        data.scrollContainerDimensions.height, data.contentDimensions.width,
	        data.contentDimensions.height,
	        data.config.horizontal ? "true" : "false",
	        data.config.vertical ? "true" : "false", data.config.childOffset.x,
	        data.config.childOffset.y);
};

inline float clay_get_element_height(uint32_t i) {
	auto data = Clay_GetElementData(CLAY_IDI_LOCAL("ListItem", i));
	return (data.found && data.boundingBox.height > 0.0f)
	             ? data.boundingBox.height
	             : 50.0f;
}

inline ItemsRange calculate_uniform_window(Clay_ElementId list_id,
                                           Size item_count, float item_size,
                                           bool is_horizontal) {
	ItemsRange w{0, 0};
	if (item_count <= 0 || item_size <= 0.0f) {
		return w;
	}

	auto scd = Clay_GetScrollContainerData(list_id);
	float scroll_pos = 0.0f;
	float container_size = item_size * 10.0f;

	if (scd.found) {
		if (scd.scrollPosition) {
			scroll_pos = is_horizontal ? -scd.scrollPosition->x
			                           : -scd.scrollPosition->y;
		}
		container_size = is_horizontal ? scd.scrollContainerDimensions.width
		                               : scd.scrollContainerDimensions.height;
	}

	if (scroll_pos < 0.0f) {
		scroll_pos = 0.0f;
	}

	w.first = (Size)(scroll_pos / item_size);
	if (w.first >= item_count) {
		w.first = item_count;
	}

	Size visible_count = (Size)(container_size / item_size) + 1;
	w.last_exclusive = w.first + visible_count;
	if (w.last_exclusive > item_count) {
		w.last_exclusive = item_count;
	}

	return w;
}

} // namespace

template <typename TDrawFunc>
void vertical_uniform(AppContext *ctx, Clay_ElementId list_id, Size item_count,
                      float item_height, TDrawFunc draw_item) {
	if (item_count == 0) {
		return;
	}
	auto window =
		  calculate_uniform_window(list_id, item_count, item_height, false);
	float spacer_top = window.first * item_height;
	float spacer_bottom = (item_count - window.last_exclusive) * item_height;

	CLAY(list_id,
	     {.layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)},
	                 .layoutDirection = CLAY_TOP_TO_BOTTOM},
	      .clip = {.vertical = true, .childOffset = Clay_GetScrollOffset()}}) {
		if (spacer_top > 0.0f) {
			CLAY(CLAY_ID_LOCAL("TopSpacer"),
			     {.layout = {.sizing = {CLAY_SIZING_GROW(0),
			                            CLAY_SIZING_FIXED(spacer_top)}}}) {}
		}
		for (Size i = window.first; i < window.last_exclusive; ++i) {
			draw_item(ctx, i, CLAY_IDI_LOCAL("ListItem", i));
		}
		if (spacer_bottom > 0.0f) {
			CLAY(CLAY_ID_LOCAL("BottomSpacer"),
			     {.layout = {.sizing = {CLAY_SIZING_GROW(0),
			                            CLAY_SIZING_FIXED(spacer_bottom)}}}) {}
		}
	}
}

template <typename TDrawItemsFunc>
void vertical_uniform_w(AppContext *ctx, Clay_ElementId list_id,
                        Size item_count, float item_height,
                        TDrawItemsFunc draw_items) {
	if (item_count == 0) {
		return;
	}
	auto window =
		  calculate_uniform_window(list_id, item_count, item_height, false);
	float spacer_top = window.first * item_height;
	float spacer_bottom = (item_count - window.last_exclusive) * item_height;

	CLAY(list_id,
	     {.layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)},
	                 .layoutDirection = CLAY_TOP_TO_BOTTOM},
	      .clip = {.vertical = true, .childOffset = Clay_GetScrollOffset()}}) {
		if (spacer_top > 0.0f) {
			CLAY(CLAY_ID_LOCAL("TopSpacer"),
			     {.layout = {.sizing = {CLAY_SIZING_GROW(0),
			                            CLAY_SIZING_FIXED(spacer_top)}}}) {}
		}
		draw_items(ctx, window);
		if (spacer_bottom > 0.0f) {
			CLAY(CLAY_ID_LOCAL("BottomSpacer"),
			     {.layout = {.sizing = {CLAY_SIZING_GROW(0),
			                            CLAY_SIZING_FIXED(spacer_bottom)}}}) {}
		}
	}
}

template <typename TDrawFunc, typename THeightFunc>
void vertical_fixed(AppContext *ctx, Clay_ElementId list_id, Size item_count,
                    THeightFunc get_item_height, TDrawFunc draw_item) {
	CLAY(list_id,
	     {.layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)},
	                 .layoutDirection = CLAY_TOP_TO_BOTTOM},
	      .clip = {.vertical = true, .childOffset = Clay_GetScrollOffset()}}) {
		auto scd = Clay_GetScrollContainerData(list_id);
		if (!scd.found) {
			return;
		}

		// log_clay_scroll_container_data(">", scd);

		float scroll_pos_y = scd.scrollPosition ? scd.scrollPosition->y : 0.0f;

		float spacer_top = 0.0f;
		Size index_first = 0;
		for (; index_first < item_count - 1; ++index_first) {
			float h = get_item_height(index_first);
			if (scroll_pos_y + h > 0.0f) {
				break;
			}
			scroll_pos_y += h;
			spacer_top += h;
		}

		float height_to_fill =
			  -scroll_pos_y + scd.scrollContainerDimensions.height;
		Size index_last = index_first;
		for (; index_last < item_count - 1; ++index_last) {
			float h = get_item_height(index_last);
			if (height_to_fill - h > 0) {
				height_to_fill -= h;
			} else {
				break;
			}
		}
		// SDL_Log("ifirst=%d", index_first);
		// SDL_Log("ilast=%d", index_last);

		float spacer_bottom = 0.0f;
		for (Size i = index_last + 1; i < item_count; ++i) {
			float h = get_item_height(i);
			spacer_bottom += h;
		}

		if (spacer_top > 0.0f) {
			CLAY(CLAY_ID_LOCAL("TopSpacer"),
			     {.layout = {.sizing = {CLAY_SIZING_GROW(0),
			                            CLAY_SIZING_FIXED(spacer_top)}}}) {}
		}
		for (Size i = index_first; i <= index_last; ++i) {
			draw_item(ctx, i, CLAY_IDI_LOCAL("ListItem", i));
		}
		if (spacer_bottom > 0.0f) {
			CLAY(CLAY_ID_LOCAL("BottomSpacer"),
			     {.layout = {.sizing = {CLAY_SIZING_GROW(0),
			                            CLAY_SIZING_FIXED(spacer_bottom)}}}) {}
		}
	}
}

template <typename TDrawFunc, typename THeightFunc>
void vertical_fixed_rich(AppContext *ctx, Clay_ElementId list_id, uint16_t gap,
                         Clay_Padding padding, Size item_count,
                         THeightFunc get_item_height, TDrawFunc draw_item) {
	if (item_count == 0) {
		return;
	}

	CLAY(list_id,
	     {.layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)},
	                 .padding = padding,
	                 .childGap = gap,
	                 .layoutDirection = CLAY_TOP_TO_BOTTOM},
	      .clip = {.vertical = true, .childOffset = Clay_GetScrollOffset()}}) {
		auto scd = Clay_GetScrollContainerData(list_id);
		if (!scd.found) {
			return;
		}

		// log_clay_scroll_container_data(">", scd);

		float scroll_pos_y = scd.scrollPosition ? scd.scrollPosition->y : 0.0f;
		scroll_pos_y += padding.top;

		float spacer_top = 0.0f;
		Size index_first = 0;
		for (; index_first < item_count - 1; ++index_first) {
			float h = get_item_height(index_first);
			if (scroll_pos_y + h > 0.0f) {
				break;
			}
			scroll_pos_y += (h + gap);
			spacer_top += (h + gap);
		}

		float height_to_fill =
			  -scroll_pos_y + scd.scrollContainerDimensions.height;
		Size index_last = index_first;
		for (; index_last < item_count - 1; ++index_last) {
			float h = get_item_height(index_last);
			if (height_to_fill - h > 0) {
				height_to_fill -= (h + gap);
			} else {
				break;
			}
		}
		// SDL_Log("ifirst=%d", index_first);
		// SDL_Log("ilast=%d", index_last);

		float spacer_bottom = 0.0f;
		for (Size i = index_last + 1; i < item_count; ++i) {
			float h = get_item_height(i);
			spacer_bottom += (h + gap);
		}

		if (spacer_top > 0.0f) {
			CLAY(CLAY_ID_LOCAL("TopSpacer"),
			     {.layout = {
						.sizing = {CLAY_SIZING_GROW(0),
			                       CLAY_SIZING_FIXED(spacer_top - gap)}}}) {}
		}
		for (Size i = index_first; i <= index_last; ++i) {
			draw_item(ctx, i, CLAY_IDI_LOCAL("ListItem", i));
		}
		if (spacer_bottom > 0.0f) {
			CLAY(CLAY_ID_LOCAL("BottomSpacer"),
			     {.layout = {
						.sizing = {CLAY_SIZING_GROW(0),
			                       CLAY_SIZING_FIXED(spacer_bottom - gap)}}}) {}
		}
	}
}

template <typename TDrawFunc>
void vertical_dynamic(AppContext *ctx, Clay_ElementId list_id, Size item_count,
                      TDrawFunc draw_item) {
	vertical_fixed(ctx, list_id, item_count, &clay_get_element_height,
	               draw_item);
}

template <typename TDrawFunc>
void vertical_dynamic_rich(AppContext *ctx, Clay_ElementId list_id,
                           uint16_t gap, Clay_Padding padding, Size item_count,
                           TDrawFunc draw_item) {
	vertical_fixed_rich(ctx, list_id, gap, padding, item_count,
	                    &clay_get_element_height, draw_item);
}
} // namespace list
