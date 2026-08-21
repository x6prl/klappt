#include <SDL3/SDL_log.h>

#include "screen_helpers.h"

#include "app/worker.h"
#include "base/str_view.h"
#include "ui/components/button.h"
#include "ui/components/lists.h"
#include "ui/dpi.h"
#include "ui/themes.h"

void screen_start_go(AppContext *ctx) {
	ctx->go(Screen::Start);

	// TODO: purge
	// if (ctx->words->size == 0 && !seed_default_learning_list(*ctx)) {
	// 	SDL_LogError(SDL_LOG_CATEGORY_ERROR,
	// 	             "Seeding default learning list failed");
	// 	exit(-1);
	// }
}

void screen_start_draw(AppContext *ctx) {
	CLAY(CLAY_ID("ScreenStart"),
	     {
			   .layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)},
	                      .padding = CLAY_PADDING_ALL(udpi(16.0f)),
	                      .childGap = udpi(14.0f),
	                      .childAlignment = {CLAY_ALIGN_X_CENTER,
	                                         CLAY_ALIGN_Y_CENTER},
	                      .layoutDirection = CLAY_TOP_TO_BOTTOM},
		 }) {
		auto go = mobile_button(ctx, CLAY_ID("go"), "Go"_v);
		if (go.activated()) {
			screen_exercise_go(ctx, false);
		}
	}
	auto eheight = [](Size i) -> float { return 50.f; };
	auto edraw = [](AppContext *ctx, Size i, Clay_ElementId item_id) {
		CLAY(item_id, {
							.layout = {.sizing = {CLAY_SIZING_GROW(0),
		                                          CLAY_SIZING_FIXED(50)},
		                               .padding = CLAY_PADDING_ALL(udpi(16.0f)),
		                               .childGap = udpi(14.0f),
		                               .childAlignment = {CLAY_ALIGN_X_CENTER,
		                                                  CLAY_ALIGN_Y_CENTER},
		                               .layoutDirection = CLAY_TOP_TO_BOTTOM},
							.backgroundColor = theme()->onError,
							.cornerRadius = CLAY_CORNER_RADIUS(dpi(10.f)),
							.border = {.color = theme()->secondary,
		                               .width = CLAY_BORDER_OUTSIDE(udpi(1.f))},
					  }) {
			draw_text(StrView::from_number(ctx->arena_frame, i),
			          theme()->error);
			for (auto j = 0; false && j < i; ++j) {
				draw_text(StrView::concat(
								ctx->arena_frame, "draw i: "_v,
								StrView::from_number(ctx->arena_frame, j)),
				          theme()->error);
			}
		}
	};
	// list::vertical_uniform(ctx, CLAY_ID("List"), 2, 50, edraw);
	// list::vertical_uniform(ctx, CLAY_ID("List2"), 20, 50, edraw);
	// list::vertical_uniform_w(ctx, CLAY_ID("List2"), 20, 50,
	//                          [edraw](AppContext *ctx, list::ItemsRange r) {
	// 							 for (Size i = r.first; i < r.last_exclusive;
	// 	                              ++i) {
	// 								 edraw(ctx, i, CLAY_IDI("ListItem", i));
	// 							 }
	// 						 });
	// list::vertical_dynamic(ctx, CLAY_ID("List2"), 20, edraw);
	// list::vertical_fixed(ctx, CLAY_ID("List"), 2, eheight, edraw);
	// list::vertical_fixed(ctx, CLAY_ID("List2"), 20, eheight, edraw);
	// auto logClayScrollContainerData = [](const char *tag,
	//                                      Clay_ScrollContainerData data) {
	// 	if (!data.found) {
	// 		SDL_Log("[%s] Clay_ScrollContainerData: NOT FOUND (found = "
	// 		        "false)",
	// 		        tag ? tag : "ScrollData");
	// 		return;
	// 	}
	//
	// 	// scrollPosition is a pointer, so check for NULL before
	// 	// dereferencing
	// 	float scrollX = data.scrollPosition ? data.scrollPosition->x : 0.0f;
	// 	float scrollY = data.scrollPosition ? data.scrollPosition->y : 0.0f;
	//
	// 	SDL_Log("[%s] Clay_ScrollContainerData:\n"
	// 	        "  - found: true\n"
	// 	        "  - scrollPosition: (x: %.2f, y: %.2f) [ptr: %p]\n"
	// 	        "  - containerDimensions: (w: %.2f, h: %.2f)\n"
	// 	        "  - contentDimensions:   (w: %.2f, h: %.2f)\n"
	// 	        "  - config (clip): [horizontal: %s, vertical: %s, "
	// 	        "childOffset: (%.2f, %.2f)]",
	// 	        tag ? tag : "ScrollData", scrollX, scrollY,
	// 	        (void *)data.scrollPosition,
	// 	        data.scrollContainerDimensions.width,
	// 	        data.scrollContainerDimensions.height,
	// 	        data.contentDimensions.width, data.contentDimensions.height,
	// 	        data.config.horizontal ? "true" : "false",
	// 	        data.config.vertical ? "true" : "false",
	// 	        data.config.childOffset.x, data.config.childOffset.y);
	// };
	//
	// auto get_item_height = [](Size i) -> float {
	// 	// Ensure your edraw() function uses CLAY_IDI("ListItem", i) for its
	// 	// root element!
	// 	auto data = Clay_GetElementData(CLAY_IDI("ListItem", i));
	// 	if (data.found && data.boundingBox.height > 0) {
	// 		return data.boundingBox.height;
	// 	}
	// 	// Fallback to a default estimated height for items that haven't been
	// 	// rendered yet
	// 	return 50.0f;
	// };
	// CLAY(CLAY_ID("List"),
	//      {
	// 		   .layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)},
	//                       // .padding = CLAY_PADDING_ALL(udpi(16.0f)),
	//                       // .childGap = udpi(14.0f),
	//                       .childAlignment = {CLAY_ALIGN_X_CENTER,
	//                                          CLAY_ALIGN_Y_CENTER},
	//                       .layoutDirection = CLAY_TOP_TO_BOTTOM},
	// 		   .backgroundColor = theme()->error,
	// 		   .clip =
	// 				 {
	// 					   .vertical = true,
	// 					   .childOffset = Clay_GetScrollOffset(),
	// 				 },
	// 	 }) {
	// 	auto scroll_container_data =
	// Clay_GetScrollContainerData(CLAY_ID("List"));
	// 	logClayScrollContainerData(">", scroll_container_data);
	//
	// 	Size items_count = 20; // Total number of items
	// 	Size index_first = 0;
	// 	auto scroll_position_y = scroll_container_data.scrollPosition->y;
	// 	auto spacer_top_height = 0.f;
	//
	// 	// 1. Calculate top spacer and the first visible item index
	// 	for (;; ++index_first) {
	// 		if (index_first >= items_count)
	// 			break; // Safety break if scrolled past the end
	// 		auto h = get_item_height(index_first);
	// 		if (scroll_position_y + h > 0) {
	// 			break;
	// 		} else {
	// 			scroll_position_y += h;
	// 			spacer_top_height += h;
	// 			continue;
	// 		}
	// 	}
	//
	// 	Size index_last = index_first;
	// 	auto should_fill_height =
	// 		  scroll_position_y * -1.f +
	// scroll_container_data.scrollContainerDimensions.height;
	//
	// 	// 2. Calculate the last visible item index
	// 	if (index_first < items_count) {
	// 		for (;; ++index_last) {
	// 			if (index_last + 1 >= items_count)
	// 				break; // Safety break
	// 			if (should_fill_height - get_item_height(index_last + 1) > 0) {
	// 				should_fill_height -= get_item_height(index_last + 1);
	// 				continue;
	// 			} else {
	// 				SDL_Log("ilast=%d", index_last);
	// 				break;
	// 			}
	// 		}
	// 		index_last = index_last + 1 > items_count ? items_count : index_last
	// + 1;
	// 	}
	//
	// 	// 3. Calculate bottom spacer
	// 	auto spacer_bottom_height = 0.f;
	// 	for (auto i = index_last + 1; i < items_count; ++i) {
	// 		spacer_bottom_height += get_item_height(i);
	// 	}
	//
	// 	// 4. Render Top Spacer
	// 	CLAY(CLAY_ID("TopSpacer"),
	// 	     {
	// 			   .layout = {.sizing = {CLAY_SIZING_GROW(0),
	// 	                                 CLAY_SIZING_FIXED(spacer_top_height)}},
	// 			   .backgroundColor = theme()->primary,
	// 		 }) {}
	//
	// 	// 5. Render visible items
	// 	for (Size i{index_first}; i <= index_last && i < items_count; ++i) {
	// 		// IMPORTANT: Ensure your `edraw` implementation wraps the item in a
	// 		// CLAY macro using CLAY_IDI("ListItem", i). Without this ID,
	// 		// Clay_GetElementData cannot track the bounding box, and the
	// 		// virtualization will fall back to the 50px default.
	// 		edraw(ctx, i);
	// 	}
	//
	// 	// 6. Render Bottom Spacer
	// 	CLAY(CLAY_ID("BottomSpacer"),
	// 	     {
	// 			   .layout = {.sizing = {CLAY_SIZING_GROW(0),
	// 	                                 CLAY_SIZING_FIXED(spacer_bottom_height)}},
	// 			   .backgroundColor = theme()->primary,
	// 		 }) {}
	//
	// 	SDL_Log("ts=%f,bs=%f", spacer_top_height, spacer_bottom_height);
	// 	ctx->anim();
	// }
	// auto eheight = [](Size i) -> float { return 50.f; };
	// Size d = 0;
	// auto edraw = [&](AppContext *ctx, Size i) {
	// 	CLAY(CLAY_IDI("ListElement", i),
	// 	     {
	// 			   .layout = {.sizing = {CLAY_SIZING_GROW(0),
	// 	                                 CLAY_SIZING_FIXED(eheight(i))},
	// 	                      .padding = CLAY_PADDING_ALL(udpi(16.0f)),
	// 	                      .childGap = udpi(14.0f),
	// 	                      .childAlignment = {CLAY_ALIGN_X_CENTER,
	// 	                                         CLAY_ALIGN_Y_CENTER},
	// 	                      .layoutDirection = CLAY_LEFT_TO_RIGHT},
	// 			   .backgroundColor = theme()->onError,
	// 			   .cornerRadius = CLAY_CORNER_RADIUS(dpi(10.f)),
	// 			   .border = {.color = theme()->secondary,
	// 	                      .width = CLAY_BORDER_OUTSIDE(udpi(1.f))},
	// 		 }) {
	// 		draw_text(StrView::from_number(ctx->arena_frame, i),
	// 		          theme()->error);
	// 		draw_text(
	// 			  StrView::concat(ctx->arena_frame, "draw i: "_v,
	// 		                      StrView::from_number(ctx->arena_frame, d++)),
	// 			  theme()->error);
	// 	}
	// };
	// CLAY(CLAY_ID("List"),
	//      {
	// 		   .layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)},
	//                       // .padding = CLAY_PADDING_ALL(udpi(16.0f)),
	//                       // .childGap = udpi(14.0f),
	//                       .childAlignment = {CLAY_ALIGN_X_CENTER,
	//                                          CLAY_ALIGN_Y_CENTER},
	//                       .layoutDirection = CLAY_TOP_TO_BOTTOM},
	// 		   .backgroundColor = theme()->error,
	// 		   .clip =
	// 				 {
	// 					   .vertical = true,
	// 					   .childOffset = Clay_GetScrollOffset(),
	// 				 },
	// 	 }) {
	// 	auto scd = Clay_GetScrollContainerData(CLAY_ID("List"));
	// 	logClayScrollContainerData(">", scd);
	// 	const Size imax = 20;
	// 	Size ifirst = 0;
	// 	auto sp_y = scd.scrollPosition->y;
	// 	auto spacer0h = 0.f;
	// 	for (;; ++ifirst) {
	// 		auto h = eheight(ifirst);
	// 		if (sp_y + h > 0) {
	// 			break;
	// 		} else {
	// 			sp_y += h;
	// 			spacer0h += h;
	// 			continue;
	// 		}
	// 	}
	// 	Size ilast = ifirst;
	// 	auto have_h = sp_y * -1.f + scd.scrollContainerDimensions.height;
	// 	// +eheight(ilast);
	// 	for (;; ++ilast) {
	// 		if (have_h - eheight(ilast + 1) > 0) {
	// 			have_h -= eheight(ilast + 1);
	// 			continue;
	// 		} else {
	// 			SDL_Log("ilast=%d", ilast);
	// 			break;
	// 		}
	// 	}
	// 	auto spacer1h = 0.f;
	// 	for (auto i = ilast; i < imax; ++i) {
	// 		spacer1h += eheight(i);
	// 	}
	//
	// 	CLAY(CLAY_ID("TopSpacer"),
	// 	     {
	// 			   .layout = {.sizing = {CLAY_SIZING_GROW(0),
	// 	                                 CLAY_SIZING_FIXED(spacer0h)}},
	// 			   .backgroundColor = theme()->primary,
	// 		 }) {}
	// 	for (Size i{ifirst}; i <= ilast; ++i) {
	// 		edraw(ctx, i);
	// 	}
	// 	CLAY(CLAY_ID("BottomSpacer"),
	// 	     {
	// 			   .layout = {.sizing = {CLAY_SIZING_GROW(0),
	// 	                                 CLAY_SIZING_FIXED(spacer1h)}},
	// 			   .backgroundColor = theme()->primary,
	// 		 }) {}
	// 	SDL_Log("ts=%f,bs=%f",spacer0h,spacer1h);
	// 	ctx->anim();
	// }
}
