#include "keypad_island.h"

#include <cstdint>

#include "SDL3/SDL_log.h"
#include "SDL3/SDL_pixels.h"
#include "SDL3_ttf/SDL_ttf.h"

#include "../dpi.h"
#include "../textcache.h"
#include "../themes.h"
#include "base/measure.h"
#include "base/profiler.h"
#include "base/stats.h"
#include "render_helpers.h"

namespace {

using KeypadIslandMask = Arr<uint8_t, 6>;

constexpr KeypadIslandMask masks[4] = {
	  {{1, 1, 0, 1, 0, 0}},
	  {{1, 1, 0, 1, 1, 0}},
	  {{1, 1, 1, 1, 1, 0}},
	  {{1, 1, 1, 1, 1, 1}},
};

constexpr const KeypadIslandMask &mask(Size size) { return masks[size - 3]; };

struct KeypadIslandCell {
	int idx = -1;
	StrView label{};
	SDL_FRect rect{};
	uint8_t corners = CORNER_NONE;
};

struct KeypadIslandPending {
	bool active{};
	Clay_ElementId id{};
	StrView labels[6]{};
	Size label_count{};
	SDL_FRect cells[6]{};
	IslandStyle style{};
	IslandTapCallback on_tap{};
};

struct KeypadIslandState {
	KeypadIslandPending pending{};
	SDL_Texture *texture{};
	bool redraw{true};
};

struct KeypadIslandCommitStats {
	Stats element_data{};
	Stats redraw_setup{};
	Stats layout{};
	Stats shadows{};
	Stats cell_bg{};
	Stats text_cache_get{};
	Stats text_measure{};
	Stats text_draw{};
	Stats dividers{};
	Stats redraw_finish{};
	Stats render_texture{};
	Stats tap_scan{};
	int commits{};

	void print_stage(const char *name, const Stats &stats) {
		SDL_Log("keypad_island_commit[%s]: avg=%d us total=%d top=%d", name,
		        stats.avg(), stats.n, stats.top10_max[0]);
	}

	void maybe_print() {
		if (commits == 0 || (commits % 200) != 0) {
			return;
		}
		print_stage("element_data", element_data);
		print_stage("redraw_setup", redraw_setup);
		print_stage("layout", layout);
		print_stage("shadows", shadows);
		print_stage("cell_bg", cell_bg);
		print_stage("text_cache_get", text_cache_get);
		print_stage("text_measure", text_measure);
		print_stage("text_draw", text_draw);
		print_stage("dividers", dividers);
		print_stage("redraw_finish", redraw_finish);
		print_stage("render_texture", render_texture);
		print_stage("tap_scan", tap_scan);
	}
};

KeypadIslandState g_keypad_island{};
KeypadIslandCommitStats g_keypad_island_commit_stats{};

constexpr bool keypad_island_has(const KeypadIslandMask &mask, int c, int r) {
	return c >= 0 && c < 3 && r >= 0 && r < 2 && mask[(r * 3) + c] != 0;
}

constexpr uint8_t keypad_island_corners(const KeypadIslandMask &mask, int c,
                                        int r) {
	uint8_t corners = CORNER_NONE;
	if (!keypad_island_has(mask, c - 1, r) &&
	    !keypad_island_has(mask, c, r - 1)) {
		corners |= CORNER_TOP_LEFT;
	}
	if (!keypad_island_has(mask, c + 1, r) &&
	    !keypad_island_has(mask, c, r - 1)) {
		corners |= CORNER_TOP_RIGHT;
	}
	if (!keypad_island_has(mask, c - 1, r) &&
	    !keypad_island_has(mask, c, r + 1)) {
		corners |= CORNER_BOTTOM_LEFT;
	}
	if (!keypad_island_has(mask, c + 1, r) &&
	    !keypad_island_has(mask, c, r + 1)) {
		corners |= CORNER_BOTTOM_RIGHT;
	}
	return corners;
}
int keypad_island_max_col(const KeypadIslandMask &mask) {
	for (int c = 2; c >= 0; --c) {
		for (int r = 0; r < 2; ++r) {
			if (keypad_island_has(mask, c, r)) {
				return c;
			}
		}
	}
	return 0;
}
// int keypad_island_max_row(const KeypadIslandMask &mask) {
// 	for (int r = 1; r >= 0; --r) {
// 		for (int c = 0; c < 3; ++c) {
// 			if (keypad_island_has(mask, c, r)) {
// 				return r;
// 			}
// 		}
// 	}
// 	return 0;
// }

} // namespace

void keypad_island_invalidate() { g_keypad_island = {}; }

void keypad_island(AppContext *ctx, Clay_ElementId id, const StrView *labels,
                   Size label_count, const IslandStyle &style,
                   IslandTapCallback on_tap) {
	KLAPPT_PROFILE_SCOPE_N("keypad_island");
	(void)ctx;
	// const float tile = ctx->scale * style.tile * 3.0f;
	// const float height = ctx->scale * style.tile * 2.0f;
	CLAY(id,
	     {.layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)}}}) {}

	auto comp_labels = [](const StrView *w0, const StrView *w1, Size n) {
		for (Size i{0}; i < n; ++i) {
			if (w0[i] != w1[i]) {
				return false;
			}
		}
		return true;
	};

	if (g_keypad_island.pending.label_count == label_count &&
	    comp_labels(g_keypad_island.pending.labels, labels, label_count) &&
	    0 == memcmp(&style, &g_keypad_island.pending.style,
	                sizeof(IslandStyle)) &&
	    g_keypad_island.pending.on_tap == on_tap &&
	    0 == memcmp(&g_keypad_island.pending.id, &id,
	                sizeof(Clay_ElementId)) // ---------------------------
	) {
		g_keypad_island.pending.active = true;
		// skip
	} else {
		auto b = [](bool v) { return v ? "true" : "false"; };
		SDL_Log("count %s",
		        b(g_keypad_island.pending.label_count == label_count));
		if (label_count == g_keypad_island.pending.label_count)
			SDL_Log("labels %s", b(comp_labels(g_keypad_island.pending.labels,
			                                   labels, label_count)));
		SDL_Log("style %s",
		        b(0 == memcmp(&style, &g_keypad_island.pending.style,
		                      sizeof(IslandStyle))));
		SDL_Log("id %s", b(0 == memcmp(&g_keypad_island.pending.id, &id,
		                               sizeof(Clay_ElementId))));
		g_keypad_island.redraw = true;
		g_keypad_island.pending = {
			  .active = true,
			  .id = id,
			  .label_count = label_count,
			  .style = style,
			  .on_tap = on_tap,
		};
		for (Size i = 0; i < label_count; ++i) {
			g_keypad_island.pending.labels[i] = labels[i];
		}
	}
}

inline SDL_FRect dividerv(float x, float y, float tile, int c, int r, float w) {
	return {roundf(x + tile * (c + 1) - w / 2.f), roundf(y + tile * r), w,
	        tile};
}
inline SDL_FRect dividerh(float x, float y, float tile, int c, float h) {
	return {roundf(x + tile * c), roundf(y + tile - h / 2.f), tile, h};
}
void keypad_island_commit(AppContext *ctx) {
	KLAPPT_PROFILE_SCOPE_N("keypad_island_commit");
	Measure perf;
	auto sample_lap = [&perf](Stats &stats) {
		perf.lap();
		stats.push(static_cast<int>(perf.tlap / 1000));
		perf.start();
	};
	auto &commit_stats = g_keypad_island_commit_stats;
	++commit_stats.commits;

	auto &pending = g_keypad_island.pending;
	if (!pending.active) {
		return;
	}

	const auto data = Clay_GetElementData(pending.id);
	sample_lap(commit_stats.element_data);
	if (!data.found) {
		pending.active = false;
		commit_stats.maybe_print();
		return;
	}

	auto renderer = ctx->renderer;

	SDL_FRect box{data.boundingBox.x, data.boundingBox.y,
	              data.boundingBox.width, data.boundingBox.height};

	if (rect_contains(box, ctx->tslt.x, ctx->tslt.y) && ctx->tslt.is_tap()) {
		g_keypad_island.redraw = true;
	}
	auto &tex = g_keypad_island.texture;
	if (g_keypad_island.redraw) {
		KLAPPT_PROFILE_SCOPE_N("keypad_island_commit.redraw");
		g_keypad_island.redraw = false;
		SDL_Log("redrawing island...");
		if (!tex) {
			// texture size is not changable the whole life
			tex = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32,
			                        SDL_TEXTUREACCESS_TARGET, box.w, box.h);
			// SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_);
		}
		SDL_SetRenderTarget(ctx->renderer, tex);
		sample_lap(commit_stats.redraw_setup);

		auto surface_color = clay_color_to_SDL_FColor_norm(
			  g_keypad_island.pending.style.surface);
		auto shadow = clay_color_to_SDL_FColor_norm(
			  g_keypad_island.pending.style.shadow_color);
		auto background = clay_color_to_SDL_FColor_norm(
			  g_keypad_island.pending.style.background);
		auto divider_color =
			  blend_over(clay_color_to_SDL_FColor_norm(
							   g_keypad_island.pending.style.divider_color),
		                 background);
		// clear texture
		SDL_SetRenderDrawColorFloat(renderer, surface_color.r, surface_color.g,
		                            surface_color.b, surface_color.a);
		SDL_RenderClear(renderer);

		const Size cells_count = g_keypad_island.pending.label_count;
		const auto &island_mask = mask(cells_count);
		const float island_columns =
			  static_cast<float>(keypad_island_max_col(island_mask) + 1);
		// const float island_rows =
		// 	  static_cast<float>(keypad_island_max_row(island_mask) + 1);

		float size_w_percent = 0.6f;
		float tile = (box.w * size_w_percent) / 3.f;
		float x = (box.w - tile * island_columns) * 0.5f;
		float y = box.h / 15.f;
		float divider_thickness = roundf(dpi(1.f));

		// calculate boxes for buttons
		// SDL_FRect cells[6]{};
		auto &cells = g_keypad_island.pending.cells;
		SDL_FRect dividers[7]{
			  dividerh(x, y, tile, 0, divider_thickness),
			  dividerv(x, y, tile, 0, 0, divider_thickness), // 3
			  dividerh(x, y, tile, 1, divider_thickness),
			  dividerv(x, y, tile, 0, 1, divider_thickness), // 4
			  dividerv(x, y, tile, 1, 0, divider_thickness), // 5
			  dividerh(x, y, tile, 2, divider_thickness),
			  dividerv(x, y, tile, 2, 1, divider_thickness), // 6

		};
		uint8_t corners[6]{};
		auto _cell_i{0};
		for (Size r = 0; r < 2; ++r) {
			for (Size c = 0; c < 3; ++c) {
				const Size slot = r * 3 + c;
				if (island_mask[slot]) {
					cells[_cell_i] = {x + tile * c, y + tile * r, tile, tile};
					corners[_cell_i] = keypad_island_corners(island_mask, c, r);
					++_cell_i;
				}
			}
		}
		sample_lap(commit_stats.layout);

		/*
		 * DRAW SHADOWS
		 */
		const auto SHADOW_LAYERS = 12;
		const auto shadow_layer_dt = dpi(1.f);
		const auto radius = dpi(16.f);
		for (Size l{0}; l < SHADOW_LAYERS; ++l) {
			auto layer_color = shadow;
			layer_color.a = shadow.a / 12.f * l;
			// layer_color = {1.f,0,0,1.f};
			auto color = blend_over(layer_color, surface_color);
			for (Size i{0}; i < cells_count; ++i) {
				auto rect = cells[i];
				rect.x -= shadow_layer_dt * (SHADOW_LAYERS - l);
				rect.y -= shadow_layer_dt * (SHADOW_LAYERS - l);
				rect.w += shadow_layer_dt * (SHADOW_LAYERS - l) * 2.f;
				rect.h += shadow_layer_dt * (SHADOW_LAYERS - l) * 2.f;
				auto r = radius + shadow_layer_dt * (SHADOW_LAYERS - l);
				render_filled_rounded_rect(renderer, &rect, r, corners[i],
				                           color);
			}
		}
		sample_lap(commit_stats.shadows);
		/*
		 * DRAW BG and TEXT
		 */
		auto color_text = g_keypad_island.pending.style.text;
		auto labels = g_keypad_island.pending.labels;
		auto font_size = udpi(30.f);
		auto ticks = ctx->ticks;
		TTF_Font *font{};
		{
			KLAPPT_PROFILE_SCOPE_N("keypad_island.get_font");
			font = ctx->text->get_font(FontID::MAIN, font_size);
		}
		for (Size i{0}; i < cells_count; ++i) {
			auto &cell = cells[i];

			const float local_x = ctx->tslt.x - box.x;
			const float local_y = ctx->tslt.y - box.y;
			auto cell_pressed =
				  rect_contains(cell, local_x, local_y) && ctx->tslt.is_tap();
			auto bgc = !cell_pressed
			                 ? background
			                 : clay_color_to_SDL_FColor_norm(theme()->primary);
			// BG
			render_filled_rounded_rect(renderer, cells + i, radius, corners[i],
			                           bgc);
			sample_lap(commit_stats.cell_bg);

			// TEXT
			auto label = labels[i];
			TTF_Text *text{};
			{
				KLAPPT_PROFILE_SCOPE_N("keypad_island.text_cache_get");
				text = ctx->text->get(label, FontID::MAIN, font_size,
				                      color_text, ticks);
			}
			sample_lap(commit_stats.text_cache_get);
			int width = 0;
			int height = 0;
			TTF_GetStringSize(font, label.data, label.size, &width, &height);
			sample_lap(commit_stats.text_measure);
			float tw = static_cast<float>(width);
			float th = static_cast<float>(height);
			float tx = round(cell.x + (cell.w - tw) / 2.f);
			float ty = round(cell.y + (cell.h - th) / 2.f);
			TTF_DrawRendererText(text, tx, ty);
			sample_lap(commit_stats.text_draw);
		}
		/*
		 * DRAW DIVIDERS
		 */
		auto dividers_count{0};
		switch (cells_count) {
		case 3:
			dividers_count = 2;
			break;
		case 4:
			dividers_count = 4;
			break;
		case 5:
			dividers_count = 5;
			break;
		case 6:
			dividers_count = 7;
			break;
		default:;
		}
		SDL_SetRenderDrawColorFloat(renderer, divider_color.r, divider_color.g,
		                            divider_color.b, divider_color.a);
		for (Size i{0}; i < dividers_count; ++i) {
			SDL_RenderFillRect(renderer, dividers + i);
		}
		sample_lap(commit_stats.dividers);
		// set renderer back to window
		SDL_SetRenderTarget(renderer, nullptr);
		sample_lap(commit_stats.redraw_finish);
	}
	if (false) {
	}
	SDL_FRect srcrect{0, 0, box.w, box.h};
	SDL_RenderTexture(renderer, tex, &srcrect, &box);
	sample_lap(commit_stats.render_texture);

	const float local_x = ctx->tslt.x - box.x;
	const float local_y = ctx->tslt.y - box.y;
	for (Size i{0}; i < g_keypad_island.pending.label_count; ++i) {
		auto &cell = g_keypad_island.pending.cells[i];
		auto cell_pressed =
			  rect_contains(cell, local_x, local_y) && ctx->tslt.is_tap();
		if (cell_pressed) {
			if (pending.on_tap) {
				pending.on_tap(ctx, static_cast<int>(i));
			}
			break;
		}
	}
	sample_lap(commit_stats.tap_scan);
	commit_stats.maybe_print();
	pending.active = false;
}
