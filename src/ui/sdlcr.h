#pragma once

#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <clay/clay.h>
#include <vector>

#include "ui/textcache.h"

typedef struct {
	SDL_Renderer *renderer;
	TextCache *text;
	uint64_t ticks;
} Clay_SDL3RendererData;

/* Global for convenience. Even in 4K this is enough for smooth curves (low
 * radius or rect size coupled with no AA or low resolution might make it appear
 * as jagged curves) */
static int NUM_CIRCLE_SEGMENTS = 16;

static float SDL_Clay_Snap(float value) { return SDL_roundf(value); }

static SDL_FRect SDL_Clay_SnapRect(const SDL_FRect rect) {
	const float x0 = SDL_Clay_Snap(rect.x);
	const float y0 = SDL_Clay_Snap(rect.y);
	const float x1 = SDL_Clay_Snap(rect.x + rect.w);
	const float y1 = SDL_Clay_Snap(rect.y + rect.h);
	return (SDL_FRect){
		  x0,
		  y0,
		  SDL_max(0.0f, x1 - x0),
		  SDL_max(0.0f, y1 - y0),
	};
}

static void SDL_Clay_RenderFillRoundedRect(Clay_SDL3RendererData *rendererData,
                                           const SDL_FRect rect,
                                           const float cornerRadius,
                                           const Clay_Color _color) {
	const SDL_FColor color = {_color.r / 255, _color.g / 255, _color.b / 255,
	                          _color.a / 255};
	const SDL_FRect snappedRect = SDL_Clay_SnapRect(rect);

	int indexCount = 0, vertexCount = 0;

	const float minRadius = SDL_min(snappedRect.w, snappedRect.h) / 2.0f;
	const float clampedRadius = SDL_min(SDL_Clay_Snap(cornerRadius), minRadius);

	constexpr int MAX_SEGMENTS = 24;
	constexpr int MAX_VERTICES = 4 + (4 * (MAX_SEGMENTS * 2)) + 8;
	constexpr int MAX_INDICES = 6 + (4 * (MAX_SEGMENTS * 3)) + 24;

	// Clamp to MAX_SEGMENTS so it can never exceed static buffer limits
	const int numCircleSegments = SDL_clamp(
		  (int)SDL_max((float)NUM_CIRCLE_SEGMENTS, clampedRadius * 0.5f), 4,
		  MAX_SEGMENTS);

	static SDL_Vertex vertices[MAX_VERTICES];
	static int indices[MAX_INDICES];

	// define center rectangle
	vertices[vertexCount++] = (SDL_Vertex){
		  {snappedRect.x + clampedRadius, snappedRect.y + clampedRadius},
		  color,
		  {0, 0}}; // 0 center TL
	vertices[vertexCount++] =
		  (SDL_Vertex){{snappedRect.x + snappedRect.w - clampedRadius,
	                    snappedRect.y + clampedRadius},
	                   color,
	                   {1, 0}}; // 1 center TR
	vertices[vertexCount++] =
		  (SDL_Vertex){{snappedRect.x + snappedRect.w - clampedRadius,
	                    snappedRect.y + snappedRect.h - clampedRadius},
	                   color,
	                   {1, 1}}; // 2 center BR
	vertices[vertexCount++] =
		  (SDL_Vertex){{snappedRect.x + clampedRadius,
	                    snappedRect.y + snappedRect.h - clampedRadius},
	                   color,
	                   {0, 1}}; // 3 center BL

	indices[indexCount++] = 0;
	indices[indexCount++] = 1;
	indices[indexCount++] = 3;
	indices[indexCount++] = 1;
	indices[indexCount++] = 2;
	indices[indexCount++] = 3;

	// define rounded corners as triangle fans

	const float step =
		  (SDL_PI_F * 0.5f) / static_cast<float>(numCircleSegments);

	for (int i = 0; i < numCircleSegments; i++) {
		const float angle1 = (float)i * step;
		const float angle2 = (float)(i + 1) * step;

		// Compute ONCE per segment, not 4 times inside the corner loop
		const float c1 = SDL_cosf(angle1) * clampedRadius;
		const float s1 = SDL_sinf(angle1) * clampedRadius;
		const float c2 = SDL_cosf(angle2) * clampedRadius;
		const float s2 = SDL_sinf(angle2) * clampedRadius;

		// Corner 0: Top-Left (-X, -Y)
		// Corner 1: Top-Right (+X, -Y)
		// Corner 2: Bottom-Right (+X, +Y)
		// Corner 3: Bottom-Left (-X, +Y)
		const float signs[4][2] = {
			  {-1.0f, -1.0f}, {1.0f, -1.0f}, {1.0f, 1.0f}, {-1.0f, 1.0f}};
		const SDL_FPoint centers[4] = {
			  {snappedRect.x + clampedRadius, snappedRect.y + clampedRadius},
			  {snappedRect.x + snappedRect.w - clampedRadius,
		       snappedRect.y + clampedRadius},
			  {snappedRect.x + snappedRect.w - clampedRadius,
		       snappedRect.y + snappedRect.h - clampedRadius},
			  {snappedRect.x + clampedRadius,
		       snappedRect.y + snappedRect.h - clampedRadius}};

		for (int j = 0; j < 4; j++) {
			const float sx = signs[j][0];
			const float sy = signs[j][1];
			const SDL_FPoint c = centers[j];

			vertices[vertexCount++] =
				  (SDL_Vertex){{c.x + c1 * sx, c.y + s1 * sy}, color, {0, 0}};
			vertices[vertexCount++] =
				  (SDL_Vertex){{c.x + c2 * sx, c.y + s2 * sy}, color, {0, 0}};

			indices[indexCount++] = j;
			indices[indexCount++] = vertexCount - 2;
			indices[indexCount++] = vertexCount - 1;
		}
	}
	// Define edge rectangles
	//  Top edge
	vertices[vertexCount++] = (SDL_Vertex){
		  {snappedRect.x + clampedRadius, snappedRect.y}, color, {0, 0}}; // TL
	vertices[vertexCount++] = (SDL_Vertex){
		  {snappedRect.x + snappedRect.w - clampedRadius, snappedRect.y},
		  color,
		  {1, 0}}; // TR

	indices[indexCount++] = 0;
	indices[indexCount++] = vertexCount - 2; // TL
	indices[indexCount++] = vertexCount - 1; // TR
	indices[indexCount++] = 1;
	indices[indexCount++] = 0;
	indices[indexCount++] = vertexCount - 1; // TR
	// Right edge
	vertices[vertexCount++] = (SDL_Vertex){
		  {snappedRect.x + snappedRect.w, snappedRect.y + clampedRadius},
		  color,
		  {1, 0}}; // RT
	vertices[vertexCount++] =
		  (SDL_Vertex){{snappedRect.x + snappedRect.w,
	                    snappedRect.y + snappedRect.h - clampedRadius},
	                   color,
	                   {1, 1}}; // RB

	indices[indexCount++] = 1;
	indices[indexCount++] = vertexCount - 2; // RT
	indices[indexCount++] = vertexCount - 1; // RB
	indices[indexCount++] = 2;
	indices[indexCount++] = 1;
	indices[indexCount++] = vertexCount - 1; // RB
	// Bottom edge
	vertices[vertexCount++] =
		  (SDL_Vertex){{snappedRect.x + snappedRect.w - clampedRadius,
	                    snappedRect.y + snappedRect.h},
	                   color,
	                   {1, 1}}; // BR
	vertices[vertexCount++] = (SDL_Vertex){
		  {snappedRect.x + clampedRadius, snappedRect.y + snappedRect.h},
		  color,
		  {0, 1}}; // BL

	indices[indexCount++] = 2;
	indices[indexCount++] = vertexCount - 2; // BR
	indices[indexCount++] = vertexCount - 1; // BL
	indices[indexCount++] = 3;
	indices[indexCount++] = 2;
	indices[indexCount++] = vertexCount - 1; // BL
	// Left edge
	vertices[vertexCount++] = (SDL_Vertex){
		  {snappedRect.x, snappedRect.y + snappedRect.h - clampedRadius},
		  color,
		  {0, 1}}; // LB
	vertices[vertexCount++] = (SDL_Vertex){
		  {snappedRect.x, snappedRect.y + clampedRadius}, color, {0, 0}}; // LT

	indices[indexCount++] = 3;
	indices[indexCount++] = vertexCount - 2; // LB
	indices[indexCount++] = vertexCount - 1; // LT
	indices[indexCount++] = 0;
	indices[indexCount++] = 3;
	indices[indexCount++] = vertexCount - 1; // LT

	// Render everything
	SDL_RenderGeometry(rendererData->renderer, NULL, vertices, vertexCount,
	                   indices, indexCount);
}

static void SDL_Clay_RenderArc(Clay_SDL3RendererData *rendererData,
                               const SDL_FPoint center, const float radius,
                               const float startAngle, const float endAngle,
                               const float thickness, const Clay_Color _color) {
	if (radius <= 0.0f || thickness <= 0.0f) {
		return;
	}

	const SDL_FColor color = {_color.r / 255, _color.g / 255, _color.b / 255,
	                          _color.a / 255};
	const float outerRadius = radius;
	const float innerRadius = SDL_max(radius - thickness, 0.0f);
	const float radStart = startAngle * (SDL_PI_F / 180.0f);
	const float radEnd = endAngle * (SDL_PI_F / 180.0f);
	const int numCircleSegments =
		  SDL_max(NUM_CIRCLE_SEGMENTS, (int)(radius * 1.5f));

	static std::vector<SDL_Vertex> vertices;
	static std::vector<int> indices;
	vertices.resize((numCircleSegments + 1) * 2);
	indices.resize(numCircleSegments * 6);
	int vertexCount = 0;
	int indexCount = 0;

	for (int i = 0; i <= numCircleSegments; i++) {
		const float t = (float)i / (float)numCircleSegments;
		const float angle = radStart + ((radEnd - radStart) * t);
		const float c = SDL_cosf(angle);
		const float s = SDL_sinf(angle);
		vertices[vertexCount++] = (SDL_Vertex){
			  {center.x + c * outerRadius, center.y + s * outerRadius},
			  color,
			  {0, 0}};
		vertices[vertexCount++] = (SDL_Vertex){
			  {center.x + c * innerRadius, center.y + s * innerRadius},
			  color,
			  {0, 0}};
	}

	for (int i = 0; i < numCircleSegments; i++) {
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

	SDL_RenderGeometry(rendererData->renderer, NULL, vertices.data(),
	                   vertexCount, indices.data(), indexCount);
}

static void SDL_Clay_RenderRoundedBorder(Clay_SDL3RendererData *rendererData,
                                         const SDL_FRect rect,
                                         const Clay_CornerRadius radii,
                                         const Clay_BorderWidth widths,
                                         const Clay_Color color) {
	const SDL_FRect snappedRect = SDL_Clay_SnapRect(rect);
	const float left = static_cast<float>(widths.left);
	const float right = static_cast<float>(widths.right);
	const float top = static_cast<float>(widths.top);
	const float bottom = static_cast<float>(widths.bottom);

	if (left <= 0.0f && right <= 0.0f && top <= 0.0f && bottom <= 0.0f) {
		return;
	}

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
		SDL_FRect line = {snappedRect.x, snappedRect.y + radii.topLeft, left,
		                  verticalLeft};
		SDL_RenderFillRect(rendererData->renderer, &line);
	}
	if (right > 0.0f) {
		SDL_FRect line = {snappedRect.x + snappedRect.w - right,
		                  snappedRect.y + radii.topRight, right, verticalRight};
		SDL_RenderFillRect(rendererData->renderer, &line);
	}
	if (top > 0.0f) {
		SDL_FRect line = {snappedRect.x + radii.topLeft, snappedRect.y,
		                  horizontalTop, top};
		SDL_RenderFillRect(rendererData->renderer, &line);
	}
	if (bottom > 0.0f) {
		SDL_FRect line = {snappedRect.x + radii.bottomLeft,
		                  snappedRect.y + snappedRect.h - bottom,
		                  horizontalBottom, bottom};
		SDL_RenderFillRect(rendererData->renderer, &line);
	}

	if (radii.topLeft > 0.0f) {
		SDL_Clay_RenderArc(rendererData,
		                   (SDL_FPoint){snappedRect.x + radii.topLeft,
		                                snappedRect.y + radii.topLeft},
		                   radii.topLeft, 180.0f, 270.0f, SDL_max(top, left),
		                   color);
	}
	if (radii.topRight > 0.0f) {
		SDL_Clay_RenderArc(
			  rendererData,
			  (SDL_FPoint){snappedRect.x + snappedRect.w - radii.topRight,
		                   snappedRect.y + radii.topRight},
			  radii.topRight, 270.0f, 360.0f, SDL_max(top, right), color);
	}
	if (radii.bottomLeft > 0.0f) {
		SDL_Clay_RenderArc(
			  rendererData,
			  (SDL_FPoint){snappedRect.x + radii.bottomLeft,
		                   snappedRect.y + snappedRect.h - radii.bottomLeft},
			  radii.bottomLeft, 90.0f, 180.0f, SDL_max(bottom, left), color);
	}
	if (radii.bottomRight > 0.0f) {
		SDL_Clay_RenderArc(
			  rendererData,
			  (SDL_FPoint){snappedRect.x + snappedRect.w - radii.bottomRight,
		                   snappedRect.y + snappedRect.h - radii.bottomRight},
			  radii.bottomRight, 0.0f, 90.0f, SDL_max(bottom, right), color);
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
	SDL_Rect clipStack[16];
	int clipStackSize = 0;

	for (int32_t i = 0; i < rcommands->length; i++) {
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
			Clay_RectangleRenderData *config = &rcmd->renderData.rectangle;
			SDL_SetRenderDrawBlendMode(rendererData->renderer,
			                           SDL_BLENDMODE_BLEND);
			SDL_SetRenderDrawColor(
				  rendererData->renderer, config->backgroundColor.r,
				  config->backgroundColor.g, config->backgroundColor.b,
				  config->backgroundColor.a);
			if (config->cornerRadius.topLeft > 0) {
				SDL_Clay_RenderFillRoundedRect(rendererData, rect,
				                               config->cornerRadius.topLeft,
				                               config->backgroundColor);
			} else {
				SDL_RenderFillRect(rendererData->renderer, &rect);
			}
		} break;
		case CLAY_RENDER_COMMAND_TYPE_TEXT: {
			Clay_TextRenderData *config = &rcmd->renderData.text;
			auto text = rendererData->text->get(
				  {config->stringContents.chars, config->stringContents.length},
				  config->fontId, config->fontSize, config->textColor,
				  rendererData->ticks);
			// TTF_Font *font = rendererData->fonts[config->fontId];
			// TTF_SetFontSize(font, config->fontSize);
			// TTF_Text *text = TTF_CreateText(rendererData->textEngine, font,
			// config->stringContents.chars, config->stringContents.length);
			// TTF_SetTextColor(text, config->textColor.r, config->textColor.g,
			// config->textColor.b, config->textColor.a);
			TTF_DrawRendererText(text, rect.x, rect.y);
			// TTF_DestroyText(text);
		} break;
		case CLAY_RENDER_COMMAND_TYPE_BORDER: {
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
			break;
		}
		case CLAY_RENDER_COMMAND_TYPE_SCISSOR_END: {
			if (clipStackSize > 0) {
				clipStackSize--;
			}
			const SDL_Rect *restoreClip =
				  (clipStackSize > 0)
						? &clipStack[SDL_min(clipStackSize - 1, 15)]
						: nullptr;
			SDL_SetRenderClipRect(rendererData->renderer, restoreClip);
			break;
		}
		case CLAY_RENDER_COMMAND_TYPE_IMAGE: {
			SDL_Texture *texture =
				  (SDL_Texture *)rcmd->renderData.image.imageData;
			const SDL_FRect dest = {rect.x, rect.y, rect.w, rect.h};
			SDL_RenderTexture(rendererData->renderer, texture, NULL, &dest);
			break;
		}
		case CLAY_RENDER_COMMAND_TYPE_NONE:
			break;
		default:
			SDL_Log("Unknown render command type: %d", rcmd->commandType);
		}
	}
}
