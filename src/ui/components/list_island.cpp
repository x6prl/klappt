#include "list_island.h"

#include <cstring>

#include "SDL3/SDL_log.h"
#include "SDL3/SDL_pixels.h"
#include "SDL3/SDL_rect.h"
#include "SDL3/SDL_render.h"
#include "SDL3_ttf/SDL_ttf.h"

#include "../dpi.h"
#include "../textcache.h"
#include "../themes.h"
#include "base/profiler.h"
#include "render_helpers.h"

namespace {

constexpr Size LIST_ISLAND_MAX_ITEMS = 5;

struct ListIslandPending {
	bool active{};
	Clay_ElementId id{};
	StrView labels[LIST_ISLAND_MAX_ITEMS]{};
	Size label_count{};
	SDL_FRect cells[LIST_ISLAND_MAX_ITEMS]{};
	IslandStyle style{};
	IslandTapCallback on_tap{};
};

struct ListIslandState {
	ListIslandPending pending{};
	SDL_Texture *texture{};
	int texture_w{};
	int texture_h{};
	bool redraw{true};
};

ListIslandState g_list_island{};

bool list_labels_equal(const StrView *lhs, const StrView *rhs, Size n) {
	for (Size i{0}; i < n; ++i) {
		if (lhs[i] != rhs[i]) {
			return false;
		}
	}
	return true;
}

} // namespace

void list_island_invalidate() { g_list_island = {}; }

void prect(const char *s, SDL_FRect r) {
	SDL_Log("%s\t>> x %f,\ty %f\t| w %f,\th %f", s, r.x, r.y, r.w, r.h);
}

void list_island(AppContext *ctx, Clay_ElementId id, const StrView *labels,
                 Size label_count, const IslandStyle &style,
                 IslandTapCallback on_tap) {
	KLAPPT_PROFILE_SCOPE_N("list_island");
	(void)ctx;
	CLAY(id,
	     {.layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)}}}) {}

	if (label_count > LIST_ISLAND_MAX_ITEMS) {
		label_count = LIST_ISLAND_MAX_ITEMS;
	}

	if (g_list_island.pending.label_count == label_count &&
	    list_labels_equal(g_list_island.pending.labels, labels, label_count) &&
	    0 == memcmp(&style, &g_list_island.pending.style,
	                sizeof(IslandStyle)) &&
	    g_list_island.pending.on_tap == on_tap &&
	    0 == memcmp(&g_list_island.pending.id, &id, sizeof(Clay_ElementId))) {
		g_list_island.pending.active = true;
	} else {
		g_list_island.redraw = true;
		g_list_island.pending = {
			  .active = true,
			  .id = id,
			  .label_count = label_count,
			  .style = style,
			  .on_tap = on_tap,
		};
		for (Size i = 0; i < label_count; ++i) {
			g_list_island.pending.labels[i] = labels[i];
		}
	}
}

void list_island_commit(AppContext *ctx) {
	KLAPPT_PROFILE_SCOPE_N("list_island_commit");
	auto &pending = g_list_island.pending;
	if (!pending.active || pending.label_count == 0) {
		return;
	}

	const auto data = Clay_GetElementData(pending.id);
	KLAPPT_PROFILE_ZONE_TEXT("Clay_GetElementData", 19);
	if (!data.found) {
		pending.active = false;
		return;
	}

	auto renderer = ctx->renderer;
	SDL_FRect box{data.boundingBox.x, data.boundingBox.y,
	              data.boundingBox.width, data.boundingBox.height};
	// prect("box", box);

	if (rect_contains(box, ctx->tslt.x, ctx->tslt.y) && ctx->tslt.is_tap()) {
		g_list_island.redraw = true;
	}

	auto &tex = g_list_island.texture;
	const int tex_w = static_cast<int>(box.w);
	const int tex_h = static_cast<int>(box.h);
	if (!tex || g_list_island.texture_w != tex_w ||
	    g_list_island.texture_h != tex_h) {
		if (tex) {
			SDL_DestroyTexture(tex);
		}
		tex = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32,
		                        SDL_TEXTUREACCESS_TARGET, tex_w, tex_h);
		g_list_island.texture_w = tex_w;
		g_list_island.texture_h = tex_h;
		g_list_island.redraw = true;
	}

	if (g_list_island.redraw) {
		KLAPPT_PROFILE_SCOPE_N("list_island_commit.redraw");
		g_list_island.redraw = false;

		auto surface_color =
			  clay_color_to_SDL_FColor_norm(pending.style.surface);
		auto shadow = clay_color_to_SDL_FColor_norm(pending.style.shadow_color);
		auto background =
			  clay_color_to_SDL_FColor_norm(pending.style.background);
		auto divider_color = blend_over(
			  clay_color_to_SDL_FColor_norm(pending.style.divider_color),
			  background);

		const float margin_h = 0.1f;
		const float margin_v = 0.1f;
		const float row_h = roundf((box.h * (1.f - margin_h * 2.f)) / 5.f);
		const float outer_w = roundf(box.w * (1.f - margin_h * 2.f));
		const float outer_x = roundf((box.w - outer_w) * 0.5f);
		const float outer_y = roundf(box.h * margin_v);
		const float outer_h = row_h * pending.label_count;

		const SDL_FRect outer{outer_x, outer_y, outer_w, outer_h};
		// prect("outer", outer);
		const float radius = dpi(16.f);
		const float divider_thickness = roundf(dpi(1.f));

		SDL_SetRenderTarget(renderer, tex);
		SDL_SetRenderDrawColorFloat(renderer, surface_color.r, surface_color.g,
		                            surface_color.b, surface_color.a);
		SDL_RenderClear(renderer);

		const auto shadow_layers = 12;
		const auto shadow_step = dpi(1.f);
		for (int layer = 0; layer < shadow_layers; ++layer) {
			auto layer_color = shadow;
			layer_color.a = shadow.a / 12.f * layer;
			auto color = blend_over(layer_color, surface_color);
			auto rect = outer;
			rect.x -= shadow_step * (shadow_layers - layer);
			rect.y -= shadow_step * (shadow_layers - layer);
			rect.w += shadow_step * (shadow_layers - layer) * 2.f;
			rect.h += shadow_step * (shadow_layers - layer) * 2.f;
			const float shadow_radius =
				  radius + shadow_step * (shadow_layers - layer);
			render_filled_rounded_rect(renderer, &rect, shadow_radius,
			                           CORNER_ALL, color);
		}

		render_filled_rounded_rect(renderer, &outer, radius, CORNER_ALL,
		                           background);

		auto font_size = udpi(30.f);
		TTF_Font *font{};
		{
			KLAPPT_PROFILE_SCOPE_N("list_island.get_font");
			font = ctx->text->get_font(FontID::MAIN, font_size);
		}
		auto ticks = ctx->ticks;
		const float local_x = ctx->tslt.x - box.x;
		const float local_y = ctx->tslt.y - box.y;
		for (Size i{0}; i < pending.label_count; ++i) {
			auto &cell = pending.cells[i];
			cell = {outer.x, roundf(outer.y + row_h * static_cast<float>(i)),
			        outer.w, row_h};
			// prect("cell", cell);
			if (i + 1 == pending.label_count) {
				cell.h = outer.y + outer.h - cell.y;
			}

			const bool cell_pressed =
				  rect_contains(cell, local_x, local_y) && ctx->tslt.is_tap();
			if (cell_pressed) {
				const auto pressed_color =
					  clay_color_to_SDL_FColor_norm(theme()->primary);
				int corners = CORNER_NONE;
				if (i == 0) {
					corners |= CORNER_TOP_LEFT | CORNER_TOP_RIGHT;
				}
				if (i + 1 == pending.label_count) {
					corners |= CORNER_BOTTOM_LEFT | CORNER_BOTTOM_RIGHT;
				}
				render_filled_rounded_rect(renderer, &cell, radius, corners,
				                           pressed_color);
			}

			TTF_Text *text{};
			{
				KLAPPT_PROFILE_SCOPE_N("list_island.text_cache_get");
				text = ctx->text->get(pending.labels[i], FontID::MAIN,
				                      font_size, pending.style.text, ticks);
			}
			int width = 0;
			int height = 0;
			{
				KLAPPT_PROFILE_SCOPE_N("list_island.text_measure");
				TTF_GetStringSize(font, pending.labels[i].data,
				                  pending.labels[i].size, &width, &height);
			}
			const float tx = roundf(cell.x + (cell.w - width) * 0.5f);
			const float ty = roundf(cell.y + (cell.h - height) * 0.5f);
			{
				KLAPPT_PROFILE_SCOPE_N("list_island.text_draw");
				TTF_DrawRendererText(text, tx, ty);
			}
		}

		SDL_SetRenderDrawColorFloat(renderer, divider_color.r, divider_color.g,
		                            divider_color.b, divider_color.a);
		for (Size i{0}; i + 1 < pending.label_count; ++i) {
			const float dy =
				  roundf(outer.y + row_h * static_cast<float>(i + 1) -
			             divider_thickness * 0.5f);
			const SDL_FRect divider{outer.x, dy, outer.w, divider_thickness};
			SDL_RenderFillRect(renderer, &divider);
		}

		SDL_SetRenderTarget(renderer, nullptr);
	}

	SDL_FRect srcrect{0, 0, box.w, box.h};
	SDL_RenderTexture(renderer, tex, &srcrect, &box);

	const float local_x = ctx->tslt.x - box.x;
	const float local_y = ctx->tslt.y - box.y;
	for (Size i{0}; i < pending.label_count; ++i) {
		if (rect_contains(pending.cells[i], local_x, local_y) &&
		    ctx->tslt.is_tap()) {
			if (pending.on_tap) {
				pending.on_tap(ctx, static_cast<int>(i));
			}
			break;
		}
	}

	pending.active = false;
}
