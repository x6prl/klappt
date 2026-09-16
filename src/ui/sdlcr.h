#pragma once

#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <clay/clay.h>

#include "base/profiler.h"

#include "ui/textcache.h"

typedef struct {
	SDL_Renderer *renderer;
	TextCache *text;
	uint64_t ticks;
} Clay_SDL3RendererData;

namespace {
constexpr int MAX_QUADS = 4096;
SDL_Vertex g_v[4 * MAX_QUADS];
int g_i[6 * MAX_QUADS];
int g_nv = 0, g_ni = 0;

void flush_text(Clay_SDL3RendererData *rd) {
	if (g_ni == 0)
		return;
	SDL_RenderGeometry(rd->renderer, rd->text->atlas.tex, g_v, g_nv, g_i, g_ni);
	g_nv = 0;
	g_ni = 0;
}

void emit_glyph_quad(Clay_SDL3RendererData *rd, const TextCache::GlyphEntry *g,
                     float x, float y, SDL_FColor c) {
	if (g_nv + 4 > 4 * MAX_QUADS) {
		flush_text(rd);
	}
	constexpr float PW = static_cast<float>(TextCache::ATLAS_SIZE);
	const float u0 = static_cast<float>(g->rx) / PW;
	const float v0 = static_cast<float>(g->ry) / PW;
	const float u1 = static_cast<float>(g->rx + g->w) / PW;
	const float v1 = static_cast<float>(g->ry + g->h) / PW;

	x = SDL_roundf(x);
	y = SDL_roundf(y);

	const int b = g_nv;
	g_v[g_nv++] = SDL_Vertex{{x, y}, c, {u0, v0}};
	g_v[g_nv++] = SDL_Vertex{{x + g->w, y}, c, {u1, v0}};
	g_v[g_nv++] = SDL_Vertex{{x + g->w, y + g->h}, c, {u1, v1}};
	g_v[g_nv++] = SDL_Vertex{{x, y + g->h}, c, {u0, v1}};

	g_i[g_ni++] = b;
	g_i[g_ni++] = b + 1;
	g_i[g_ni++] = b + 2;
	g_i[g_ni++] = b;
	g_i[g_ni++] = b + 2;
	g_i[g_ni++] = b + 3;
}

void draw_text_clay(Clay_SDL3RendererData *rd, StrView str, uint16_t font_id,
                    uint16_t font_size, float x, float y, SDL_FColor color) {
	if (str.size == 0)
		return;

	float pen_x = SDL_roundf(x);
	const float line_y = SDL_roundf(y); // top-of-line reference

	const char *ptr = str.data;
	size_t remaining = str.size;

	while (remaining > 0) {
		const Uint32 cp = SDL_StepUTF8(&ptr, &remaining);
		if (!cp)
			break;
		const auto *g = rd->text->get_glyph(cp, font_id, font_size);
		if (!g)
			continue;

		if (g->w > 0 && g->h > 0 && g->page != TextCache::PAGE_NONE) {
			const float gx =
				  pen_x + (g->minx < 0 ? static_cast<float>(g->minx) : 0.0f);
			emit_glyph_quad(rd, g, gx, line_y, color);
		}
		pen_x += static_cast<float>(g->advance);
	}
}

constexpr int QUADRANT_STEPS = 16;

static inline void init_trig_table(float *c, float *s, int steps) {
	const float step = (SDL_PI_F * 0.5f) / static_cast<float>(steps);
	for (int i = 0; i <= steps; ++i) {
		const float angle = static_cast<float>(i) * step;
		c[i] = SDL_cosf(angle);
		s[i] = SDL_sinf(angle);
	}
}

static float g_cos[QUADRANT_STEPS + 1]{};
static float g_sin[QUADRANT_STEPS + 1]{};
static bool g_trig_initialized = []() {
	init_trig_table(g_cos, g_sin, QUADRANT_STEPS);
	return true;
}();

static inline float SDL_Clay_Snap(float value) { return SDL_roundf(value); }

static inline SDL_FRect SDL_Clay_SnapRect(const SDL_FRect rect) {
	const float x0 = SDL_Clay_Snap(rect.x);
	const float y0 = SDL_Clay_Snap(rect.y);
	const float x1 = SDL_Clay_Snap(rect.x + rect.w);
	const float y1 = SDL_Clay_Snap(rect.y + rect.h);
	return SDL_FRect{
		  x0,
		  y0,
		  SDL_max(0.0f, x1 - x0),
		  SDL_max(0.0f, y1 - y0),
	};
}
} // namespace

static void SDL_Clay_RenderFillRoundedRect(Clay_SDL3RendererData *rendererData,
                                           const SDL_FRect rect,
                                           const Clay_CornerRadius cornerRadius,
                                           const Clay_Color _color) {
	KLAPPT_PROFILE_SCOPE_N("sdlcr::fill_rounded_geometry");
	const SDL_FColor color = {_color.r / 255.0f, _color.g / 255.0f,
	                          _color.b / 255.0f, _color.a / 255.0f};
	const SDL_FRect snappedRect = SDL_Clay_SnapRect(rect);
	const float halfW = snappedRect.w * 0.5f;
	const float halfH = snappedRect.h * 0.5f;

	const float rTL = SDL_min(SDL_min(cornerRadius.topLeft, halfW), halfH);
	const float rTR = SDL_min(SDL_min(cornerRadius.topRight, halfW), halfH);
	const float rBR = SDL_min(SDL_min(cornerRadius.bottomRight, halfW), halfH);
	const float rBL = SDL_min(SDL_min(cornerRadius.bottomLeft, halfW), halfH);

	constexpr int MAX_VERTS = 4 + (4 * (QUADRANT_STEPS * 2)) + 8;
	constexpr int MAX_INDS = 6 + (4 * (QUADRANT_STEPS * 3)) + 24;

	SDL_Vertex vertices[MAX_VERTS];
	int indices[MAX_INDS];
	int vertexCount = 0;
	int indexCount = 0;

	// Center Quad
	vertices[vertexCount++] = {
		  {snappedRect.x + rTL, snappedRect.y + rTL},
		  color,
		  {0, 0},
	};
	vertices[vertexCount++] = {
		  {snappedRect.x + snappedRect.w - rTR, snappedRect.y + rTR},
		  color,
		  {1, 0},
	};
	vertices[vertexCount++] = {
		  {snappedRect.x + snappedRect.w - rBR,
	       snappedRect.y + snappedRect.h - rBR},
		  color,
		  {1, 1},
	};
	vertices[vertexCount++] = {
		  {snappedRect.x + rBL, snappedRect.y + snappedRect.h - rBL},
		  color,
		  {0, 1},
	};

	indices[indexCount++] = 0;
	indices[indexCount++] = 1;
	indices[indexCount++] = 3;
	indices[indexCount++] = 1;
	indices[indexCount++] = 2;
	indices[indexCount++] = 3;

	const float radii[4] = {rTL, rTR, rBR, rBL};
	const float signs[4][2] = {
		  {-1.0f, -1.0f},
		  {1.0f, -1.0f},
		  {1.0f, 1.0f},
		  {-1.0f, 1.0f},
	};
	const SDL_FPoint centers[4] = {
		  {snappedRect.x + rTL, snappedRect.y + rTL},
		  {snappedRect.x + snappedRect.w - rTR, snappedRect.y + rTR},
		  {snappedRect.x + snappedRect.w - rBR,
	       snappedRect.y + snappedRect.h - rBR},
		  {snappedRect.x + rBL, snappedRect.y + snappedRect.h - rBL},
	};

	for (int j = 0; j < 4; ++j) {
		const float rad = radii[j];
		if (rad <= 0.0f)
			continue;

		const float sx = signs[j][0];
		const float sy = signs[j][1];
		const SDL_FPoint c = centers[j];

		for (int i = 0; i < QUADRANT_STEPS; ++i) {
			const float c1 = g_cos[i] * rad;
			const float s1 = g_sin[i] * rad;
			const float c2 = g_cos[i + 1] * rad;
			const float s2 = g_sin[i + 1] * rad;

			vertices[vertexCount++] = {
				  {c.x + c1 * sx, c.y + s1 * sy}, color, {0, 0}};
			vertices[vertexCount++] = {
				  {c.x + c2 * sx, c.y + s2 * sy}, color, {0, 0}};

			indices[indexCount++] = j;
			indices[indexCount++] = vertexCount - 2;
			indices[indexCount++] = vertexCount - 1;
		}
	}

	// 4 Edge Rectangles
	vertices[vertexCount++] = {
		  {snappedRect.x + rTL, snappedRect.y},
		  color,
		  {0, 0},
	};
	vertices[vertexCount++] = {
		  {snappedRect.x + snappedRect.w - rTR, snappedRect.y},
		  color,
		  {1, 0},
	};
	indices[indexCount++] = 0;
	indices[indexCount++] = vertexCount - 2;
	indices[indexCount++] = vertexCount - 1;
	indices[indexCount++] = 1;
	indices[indexCount++] = 0;
	indices[indexCount++] = vertexCount - 1;

	vertices[vertexCount++] = {
		  {snappedRect.x + snappedRect.w, snappedRect.y + rTR},
		  color,
		  {1, 0},
	};
	vertices[vertexCount++] = {
		  {snappedRect.x + snappedRect.w, snappedRect.y + snappedRect.h - rBR},
		  color,
		  {1, 1},
	};
	indices[indexCount++] = 1;
	indices[indexCount++] = vertexCount - 2;
	indices[indexCount++] = vertexCount - 1;
	indices[indexCount++] = 2;
	indices[indexCount++] = 1;
	indices[indexCount++] = vertexCount - 1;

	vertices[vertexCount++] = {
		  {snappedRect.x + snappedRect.w - rBR, snappedRect.y + snappedRect.h},
		  color,
		  {1, 1},
	};
	vertices[vertexCount++] = {
		  {snappedRect.x + rBL, snappedRect.y + snappedRect.h},
		  color,
		  {0, 1},
	};
	indices[indexCount++] = 2;
	indices[indexCount++] = vertexCount - 2;
	indices[indexCount++] = vertexCount - 1;
	indices[indexCount++] = 3;
	indices[indexCount++] = 2;
	indices[indexCount++] = vertexCount - 1;

	vertices[vertexCount++] = {
		  {snappedRect.x, snappedRect.y + snappedRect.h - rBL},
		  color,
		  {0, 1},
	};
	vertices[vertexCount++] = {
		  {snappedRect.x, snappedRect.y + rTL},
		  color,
		  {0, 0},
	};
	indices[indexCount++] = 3;
	indices[indexCount++] = vertexCount - 2;
	indices[indexCount++] = vertexCount - 1;
	indices[indexCount++] = 0;
	indices[indexCount++] = 3;
	indices[indexCount++] = vertexCount - 1;

	{
		KLAPPT_PROFILE_SCOPE_N("sdlcr::render_geometry");
		SDL_RenderGeometry(rendererData->renderer, NULL, vertices, vertexCount,
		                   indices, indexCount);
	}
}

static void SDL_Clay_RenderArcFixed(Clay_SDL3RendererData *rendererData,
                                    const SDL_FPoint center, const float radius,
                                    const float startAngleDeg,
                                    const float endAngleDeg,
                                    const float thickness,
                                    const SDL_FColor color) {
	if (radius <= 0.0f || thickness <= 0.0f)
		return;

	constexpr int SEGMENTS = 8;
	constexpr int VERTS = (SEGMENTS + 1) * 2;
	constexpr int INDS = SEGMENTS * 6;

	SDL_Vertex vertices[VERTS];
	int indices[INDS];
	int vertexCount = 0;
	int indexCount = 0;

	const float outerRadius = radius;
	const float innerRadius = SDL_max(radius - thickness, 0.0f);
	const float radStart = startAngleDeg * (SDL_PI_F / 180.0f);
	const float radEnd = endAngleDeg * (SDL_PI_F / 180.0f);

	for (int i = 0; i <= SEGMENTS; ++i) {
		const float t = static_cast<float>(i) / static_cast<float>(SEGMENTS);
		const float angle = radStart + ((radEnd - radStart) * t);
		const float c = SDL_cosf(angle);
		const float s = SDL_sinf(angle);

		vertices[vertexCount++] = {
			  {center.x + c * outerRadius, center.y + s * outerRadius},
			  color,
			  {0, 0}};
		vertices[vertexCount++] = {
			  {center.x + c * innerRadius, center.y + s * innerRadius},
			  color,
			  {0, 0}};
	}

	for (int i = 0; i < SEGMENTS; ++i) {
		const int outer0 = i * 2;
		const int inner0 = outer0 + 1;
		const int outer1 = outer0 + 2;
		const int inner1 = outer0 + 3;

		indices[indexCount++] = outer0;
		indices[indexCount++] = outer1;
		indices[indexCount++] = inner0;
		indices[indexCount++] = inner0;
		indices[indexCount++] = outer1;
		indices[indexCount++] = inner1;
	}

	SDL_RenderGeometry(rendererData->renderer, NULL, vertices, vertexCount,
	                   indices, indexCount);
}

static void SDL_Clay_RenderRoundedBorder(Clay_SDL3RendererData *rendererData,
                                         const SDL_FRect rect,
                                         const Clay_CornerRadius radii,
                                         const Clay_BorderWidth widths,
                                         const Clay_Color color) {
	KLAPPT_PROFILE_SCOPE_N("sdlcr::border_geometry");
	const SDL_FRect snappedRect = SDL_Clay_SnapRect(rect);
	const float left = static_cast<float>(widths.left);
	const float right = static_cast<float>(widths.right);
	const float top = static_cast<float>(widths.top);
	const float bottom = static_cast<float>(widths.bottom);

	if (left <= 0.0f && right <= 0.0f && top <= 0.0f && bottom <= 0.0f) {
		return;
	}

	const SDL_FColor fcolor = {color.r / 255.0f, color.g / 255.0f,
	                           color.b / 255.0f, color.a / 255.0f};

	SDL_SetRenderDrawColor(rendererData->renderer, color.r, color.g, color.b,
	                       color.a);

	const float verticalLeft =
		  SDL_max(0.0f, snappedRect.h - radii.topLeft - radii.bottomLeft);
	const float verticalRight =
		  SDL_max(0.0f, snappedRect.h - radii.topRight - radii.bottomRight);
	const float horizontalTop =
		  SDL_max(0.0f, snappedRect.w - radii.topLeft - radii.topRight);
	const float horizontalBottom =
		  SDL_max(0.0f, snappedRect.w - radii.bottomLeft - radii.bottomRight);

	if (left > 0.0f) {
		const SDL_FRect line = {snappedRect.x, snappedRect.y + radii.topLeft,
		                        left, verticalLeft};
		SDL_RenderFillRect(rendererData->renderer, &line);
	}
	if (right > 0.0f) {
		const SDL_FRect line = {snappedRect.x + snappedRect.w - right,
		                        snappedRect.y + radii.topRight, right,
		                        verticalRight};
		SDL_RenderFillRect(rendererData->renderer, &line);
	}
	if (top > 0.0f) {
		const SDL_FRect line = {snappedRect.x + radii.topLeft, snappedRect.y,
		                        horizontalTop, top};
		SDL_RenderFillRect(rendererData->renderer, &line);
	}
	if (bottom > 0.0f) {
		const SDL_FRect line = {snappedRect.x + radii.bottomLeft,
		                        snappedRect.y + snappedRect.h - bottom,
		                        horizontalBottom, bottom};
		SDL_RenderFillRect(rendererData->renderer, &line);
	}

	if (radii.topLeft > 0.0f) {
		SDL_Clay_RenderArcFixed(
			  rendererData,
			  {snappedRect.x + radii.topLeft, snappedRect.y + radii.topLeft},
			  radii.topLeft, 180.0f, 270.0f, SDL_max(top, left), fcolor);
	}
	if (radii.topRight > 0.0f) {
		SDL_Clay_RenderArcFixed(rendererData,
		                        {snappedRect.x + snappedRect.w - radii.topRight,
		                         snappedRect.y + radii.topRight},
		                        radii.topRight, 270.0f, 360.0f,
		                        SDL_max(top, right), fcolor);
	}
	if (radii.bottomLeft > 0.0f) {
		SDL_Clay_RenderArcFixed(
			  rendererData,
			  {snappedRect.x + radii.bottomLeft,
		       snappedRect.y + snappedRect.h - radii.bottomLeft},
			  radii.bottomLeft, 90.0f, 180.0f, SDL_max(bottom, left), fcolor);
	}
	if (radii.bottomRight > 0.0f) {
		SDL_Clay_RenderArcFixed(
			  rendererData,
			  {snappedRect.x + snappedRect.w - radii.bottomRight,
		       snappedRect.y + snappedRect.h - radii.bottomRight},
			  radii.bottomRight, 0.0f, 90.0f, SDL_max(bottom, right), fcolor);
	}
}

static inline SDL_Rect
SDL_Clay_RenderCommandClip(const Clay_RenderCommand *rcmd,
                           const SDL_Rect *parentClip) {
	const int x0 = static_cast<int>(SDL_roundf(rcmd->boundingBox.x));
	const int y0 = static_cast<int>(SDL_roundf(rcmd->boundingBox.y));
	const int x1 = static_cast<int>(
		  SDL_roundf(rcmd->boundingBox.x + rcmd->boundingBox.width));
	const int y1 = static_cast<int>(
		  SDL_roundf(rcmd->boundingBox.y + rcmd->boundingBox.height));

	if (parentClip) {
		const int px0 = parentClip->x;
		const int py0 = parentClip->y;
		const int px1 = parentClip->x + parentClip->w;
		const int py1 = parentClip->y + parentClip->h;

		const int ix0 = SDL_max(x0, px0);
		const int iy0 = SDL_max(y0, py0);
		const int ix1 = SDL_min(x1, px1);
		const int iy1 = SDL_min(y1, py1);

		return SDL_Rect{ix0, iy0, SDL_max(0, ix1 - ix0), SDL_max(0, iy1 - iy0)};
	}

	return SDL_Rect{x0, y0, SDL_max(0, x1 - x0), SDL_max(0, y1 - y0)};
}

static inline void
SDL_Clay_RenderClayCommands(Clay_SDL3RendererData *rendererData,
                            Clay_RenderCommandArray *rcommands) {
	KLAPPT_PROFILE_SCOPE_N("sdlcr");
	SDL_Rect clipStack[16];
	int clipStackSize = 0;

	for (int32_t i = 0; i < rcommands->length; ++i) {
		Clay_RenderCommand *rcmd = Clay_RenderCommandArray_Get(rcommands, i);
		const Clay_BoundingBox bounding_box = rcmd->boundingBox;
		const SDL_FRect rect = {
			  static_cast<float>(bounding_box.x),
			  static_cast<float>(bounding_box.y),
			  static_cast<float>(bounding_box.width),
			  static_cast<float>(bounding_box.height),
		};

		switch (rcmd->commandType) {
		case CLAY_RENDER_COMMAND_TYPE_RECTANGLE: {
			KLAPPT_PROFILE_SCOPE_N("sdlcr::rectangle");
			flush_text(rendererData);
			Clay_RectangleRenderData *config = &rcmd->renderData.rectangle;
			SDL_SetRenderDrawBlendMode(rendererData->renderer,
			                           SDL_BLENDMODE_BLEND);

			const bool hasRounding = (config->cornerRadius.topLeft > 0.0f ||
			                          config->cornerRadius.topRight > 0.0f ||
			                          config->cornerRadius.bottomLeft > 0.0f ||
			                          config->cornerRadius.bottomRight > 0.0f);

			if (hasRounding) {
				KLAPPT_PROFILE_SCOPE_N("sdlcr::rect_rounded");
				SDL_Clay_RenderFillRoundedRect(rendererData, rect,
				                               config->cornerRadius,
				                               config->backgroundColor);
			} else {
				KLAPPT_PROFILE_SCOPE_N("sdlcr::rect_flat");
				SDL_SetRenderDrawColor(
					  rendererData->renderer, config->backgroundColor.r,
					  config->backgroundColor.g, config->backgroundColor.b,
					  config->backgroundColor.a);
				SDL_RenderFillRect(rendererData->renderer, &rect);
			}
		} break;

		case CLAY_RENDER_COMMAND_TYPE_TEXT: {
			KLAPPT_PROFILE_SCOPE_N("sdlcr::text");
			auto *config = &rcmd->renderData.text;
			const SDL_FColor fc{
				  config->textColor.r / 255.0f, config->textColor.g / 255.0f,
				  config->textColor.b / 255.0f, config->textColor.a / 255.0f};
			draw_text_clay(
				  rendererData,
				  {config->stringContents.chars,
			       static_cast<TextCache::Size>(config->stringContents.length)},
				  config->fontId, config->fontSize, rect.x, rect.y, fc);
		} break;

		case CLAY_RENDER_COMMAND_TYPE_BORDER: {
			KLAPPT_PROFILE_SCOPE_N("sdlcr::border");
			Clay_BorderRenderData *config = &rcmd->renderData.border;
			const SDL_FRect snappedRect = SDL_Clay_SnapRect(rect);
			const float minRadius =
				  SDL_min(snappedRect.w, snappedRect.h) / 2.0f;
			const Clay_CornerRadius clampedRadii = {
				  .topLeft = SDL_min(
						SDL_Clay_Snap(config->cornerRadius.topLeft), minRadius),
				  .topRight =
						SDL_min(SDL_Clay_Snap(config->cornerRadius.topRight),
			                    minRadius),
				  .bottomLeft =
						SDL_min(SDL_Clay_Snap(config->cornerRadius.bottomLeft),
			                    minRadius),
				  .bottomRight =
						SDL_min(SDL_Clay_Snap(config->cornerRadius.bottomRight),
			                    minRadius)};

			SDL_Clay_RenderRoundedBorder(rendererData, snappedRect,
			                             clampedRadii, config->width,
			                             config->color);
		} break;

		case CLAY_RENDER_COMMAND_TYPE_SCISSOR_START: {
			KLAPPT_PROFILE_SCOPE_N("sdlcr::scissor_start");
			flush_text(rendererData);
			const SDL_Rect *parentClip =
				  (clipStackSize > 0)
						? &clipStack[SDL_min(clipStackSize - 1, 15)]
						: nullptr;
			SDL_Rect clip = SDL_Clay_RenderCommandClip(rcmd, parentClip);
			if (clipStackSize < 16) {
				clipStack[clipStackSize] = clip;
			}
			clipStackSize++;
			SDL_SetRenderClipRect(rendererData->renderer, &clip);
		} break;

		case CLAY_RENDER_COMMAND_TYPE_SCISSOR_END: {
			KLAPPT_PROFILE_SCOPE_N("sdlcr::scissor_end");
			flush_text(rendererData);
			if (clipStackSize > 0) {
				clipStackSize--;
			}
			const SDL_Rect *restoreClip =
				  (clipStackSize > 0)
						? &clipStack[SDL_min(clipStackSize - 1, 15)]
						: nullptr;
			SDL_SetRenderClipRect(rendererData->renderer, restoreClip);
		} break;

		case CLAY_RENDER_COMMAND_TYPE_IMAGE: {
			KLAPPT_PROFILE_SCOPE_N("sdlcr::image");
			SDL_Texture *texture =
				  static_cast<SDL_Texture *>(rcmd->renderData.image.imageData);
			const SDL_FRect dest = {rect.x, rect.y, rect.w, rect.h};
			SDL_RenderTexture(rendererData->renderer, texture, NULL, &dest);
		} break;

		case CLAY_RENDER_COMMAND_TYPE_NONE:
		default:
			break;
		}
	}
	flush_text(rendererData);
}
