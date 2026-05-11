#pragma once

#include <cmath>

#include "SDL3/SDL_rect.h"
#include "SDL3/SDL_render.h"

namespace {

enum CornerFlags {
	CORNER_NONE = 0,
	CORNER_TOP_LEFT = 1 << 0,
	CORNER_TOP_RIGHT = 1 << 1,
	CORNER_BOTTOM_RIGHT = 1 << 2,
	CORNER_BOTTOM_LEFT = 1 << 3,
	CORNER_ALL = 0xF
};

}

inline bool rect_contains(const SDL_FRect &rect, float x, float y) {
	return x >= rect.x && x < rect.x + rect.w && y >= rect.y &&
	       y < rect.y + rect.h;
}

inline SDL_FColor blend_over(const SDL_FColor &fg, const SDL_FColor &bg) {
	return SDL_FColor{(fg.r * fg.a + bg.r * bg.a * (1.0f - fg.a)),
	                  (fg.g * fg.a + bg.g * bg.a * (1.0f - fg.a)),
	                  (fg.b * fg.a + bg.b * bg.a * (1.0f - fg.a)), 1.f};
}

// Internal helper to calculate the perimeter points of the rectangle
static void generate_rounded_rect_perimeter(const SDL_FRect *rect,
                                            float radius, int corner_mask,
                                            int segments,
                                            SDL_FPoint *out_points,
                                            int *out_count) {
	// Clamp the radius so corners don't overlap if the radius is too large
	float max_radius = fminf(rect->w / 2.0f, rect->h / 2.0f);
	if (radius > max_radius) {
		radius = max_radius;
	}

	int v_count = 0;

	// Short-circuit for standard rectangle
	if (radius <= 0.0f || corner_mask == CORNER_NONE) {
		out_points[v_count].x = rect->x + rect->w;
		out_points[v_count++].y = rect->y;
		out_points[v_count].x = rect->x + rect->w;
		out_points[v_count++].y = rect->y + rect->h;
		out_points[v_count].x = rect->x;
		out_points[v_count++].y = rect->y + rect->h;
		out_points[v_count].x = rect->x;
		out_points[v_count++].y = rect->y;
		*out_count = v_count;
		return;
	}

	const float pi = 3.14159265359f;

	// 1. Top-Right Corner
	if (corner_mask & CORNER_TOP_RIGHT) {
		float cx = rect->x + rect->w - radius;
		float cy = rect->y + radius;
		for (int i = 0; i <= segments; i++) {
			float angle = -pi / 2.0f + (i * (pi / 2.0f) / segments);
			out_points[v_count].x = cx + cosf(angle) * radius;
			out_points[v_count].y = cy + sinf(angle) * radius;
			v_count++;
		}
	} else {
		out_points[v_count].x = rect->x + rect->w;
		out_points[v_count].y = rect->y;
		v_count++;
	}

	// 2. Bottom-Right Corner
	if (corner_mask & CORNER_BOTTOM_RIGHT) {
		float cx = rect->x + rect->w - radius;
		float cy = rect->y + rect->h - radius;
		for (int i = 0; i <= segments; i++) {
			float angle = 0.0f + (i * (pi / 2.0f) / segments);
			out_points[v_count].x = cx + cosf(angle) * radius;
			out_points[v_count].y = cy + sinf(angle) * radius;
			v_count++;
		}
	} else {
		out_points[v_count].x = rect->x + rect->w;
		out_points[v_count].y = rect->y + rect->h;
		v_count++;
	}

	// 3. Bottom-Left Corner
	if (corner_mask & CORNER_BOTTOM_LEFT) {
		float cx = rect->x + radius;
		float cy = rect->y + rect->h - radius;
		for (int i = 0; i <= segments; i++) {
			float angle = pi / 2.0f + (i * (pi / 2.0f) / segments);
			out_points[v_count].x = cx + cosf(angle) * radius;
			out_points[v_count].y = cy + sinf(angle) * radius;
			v_count++;
		}
	} else {
		out_points[v_count].x = rect->x;
		out_points[v_count].y = rect->y + rect->h;
		v_count++;
	}

	// 4. Top-Left Corner
	if (corner_mask & CORNER_TOP_LEFT) {
		float cx = rect->x + radius;
		float cy = rect->y + radius;
		for (int i = 0; i <= segments; i++) {
			float angle = pi + (i * (pi / 2.0f) / segments);
			out_points[v_count].x = cx + cosf(angle) * radius;
			out_points[v_count].y = cy + sinf(angle) * radius;
			v_count++;
		}
	} else {
		out_points[v_count].x = rect->x;
		out_points[v_count].y = rect->y;
		v_count++;
	}

	*out_count = v_count;
}

// Draw a FILLED rounded rectangle
inline bool render_filled_rounded_rect(SDL_Renderer *renderer,
                                       const SDL_FRect *rect, float radius,
                                       int corner_mask, SDL_FColor color) {
	const int segments = 15;   // Smoothness of the curve
	SDL_FPoint perimeter[128]; // Max perimeter points: 4 * (15 + 1) = 64
	int p_count = 0;

	generate_rounded_rect_perimeter(rect, radius, corner_mask, segments,
	                                perimeter, &p_count);

	// We need 1 center point + p_count perimeter points to build a Triangle Fan
	SDL_Vertex vertices[128 + 1];
	int indices[(128) * 3];

	// Setup the Center vertex
	vertices[0].position.x = rect->x + rect->w / 2.0f;
	vertices[0].position.y = rect->y + rect->h / 2.0f;
	vertices[0].color = color;
	vertices[0].tex_coord.x = 0.0f;
	vertices[0].tex_coord.y = 0.0f;

	// Setup the perimeter vertices
	for (int i = 0; i < p_count; i++) {
		vertices[i + 1].position = perimeter[i];
		vertices[i + 1].color = color;
		vertices[i + 1].tex_coord.x = 0.0f;
		vertices[i + 1].tex_coord.y = 0.0f;
	}

	// Generate indices array to form triangles (0, 1, 2), (0, 2, 3), etc.
	int i_count = 0;
	for (int i = 1; i <= p_count; i++) {
		indices[i_count++] = 0;
		indices[i_count++] = i;
		indices[i_count++] =
			  (i == p_count) ? 1
							 : i + 1; // loop back to the first perimeter point
	}

	return SDL_RenderGeometry(renderer, NULL, vertices, p_count + 1, indices,
	                          i_count);
}
