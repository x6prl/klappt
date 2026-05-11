#include "themes.h"
namespace {
static Theme::Type current_theme{Theme::Light};
constexpr static Theme app_themes[Theme::Themes_COUNT] =
	  { // ------------------------------------
        // LIGHT THEME
        // ------------------------------------
			{
				  .theme = Theme::Light,

				  .primary = {103, 80, 164, 255},    // Deep Purple
				  .onPrimary = {255, 255, 255, 255}, // White

				  .secondary = {98, 91, 113, 255},     // Muted Purple
				  .onSecondary = {255, 255, 255, 255}, // White

				  .surface = {255, 251, 254, 255}, // Very light pink/white
				  .onSurface = {28, 27, 31, 255},  // Almost Black
				  .surfaceContainerLow = {251, 247, 252, 255},  // Low elevated surface
				  .onSurfaceContainerLow = {28, 27, 31, 255},   // Almost Black
				  .surfaceContainer = {247, 242, 250, 255},     // Light elevated surface
				  .onSurfaceContainer = {28, 27, 31, 255},      // Almost Black
				  .surfaceContainerHigh = {238, 232, 244, 255}, // Higher elevated surface
				  .onSurfaceContainerHigh = {28, 27, 31, 255},  // Almost Black

				  .error = {179, 38, 30, 255},     // Red
				  .onError = {255, 255, 255, 255}, // White
				  .rightContainer = {219, 239, 223, 255},   // Soft green
				  .onRightContainer = {21, 74, 35, 255},    // Deep green
				  .wrongContainer = {245, 225, 227, 255},   // Soft rose
				  .onWrongContainer = {104, 55, 61, 255},   // Muted plum

				  .outline = {121, 116, 126, 255}, // Grey
				  .shadow = {0, 0, 0, 64}          // 25% Black for shadows
			},

			// ------------------------------------
            // DARK THEME
            // ------------------------------------
			{
				  .theme = Theme::Dark,

				  .primary = {208, 188, 255, 255}, // Light Purple
				  .onPrimary = {56, 30, 114, 255}, // Dark Purple

				  .secondary = {204, 194, 220, 255}, // Light Muted Purple
				  .onSecondary = {51, 45, 65, 255},  // Dark Muted Purple

				  .surface = {28, 27, 31, 255},      // Almost Black
				  .onSurface = {230, 225, 229, 255}, // Light Grey
				  .surfaceContainerLow = {31, 30, 35, 255},      // Low elevated surface
				  .onSurfaceContainerLow = {230, 225, 229, 255}, // Light Grey
				  .surfaceContainer = {36, 35, 40, 255},     // Slightly elevated surface
				  .onSurfaceContainer = {230, 225, 229, 255}, // Light Grey
				  .surfaceContainerHigh = {46, 45, 51, 255},  // Higher elevated surface
				  .onSurfaceContainerHigh = {230, 225, 229, 255}, // Light Grey

				  .error = {242, 184, 181, 255}, // Light Red
				  .onError = {96, 20, 16, 255},  // Dark Red
				  .rightContainer = {51, 82, 60, 255},     // Muted green
				  .onRightContainer = {220, 245, 224, 255}, // Pale green
				  .wrongContainer = {91, 58, 64, 255},      // Soft rose
				  .onWrongContainer = {248, 223, 227, 255}, // Pale rose

				  .outline = {147, 143, 153, 255}, // Lighter Grey
				  .shadow = {0, 0, 0, 128}         // 50% Black
			}};
} // namespace

const Theme *theme() { return &app_themes[current_theme]; }
void theme_set(Theme::Type type) { current_theme = type; };
Theme::Type theme_next() {
	current_theme = static_cast<Theme::Type>(
		  (static_cast<int>(current_theme) + 1) % Theme::Themes_COUNT);
	return current_theme;
}
