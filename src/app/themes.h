#pragma once

#include <clay/clay.h>

struct Theme {
	enum Type { Light, Dark, Themes_COUNT } theme;

	// Main colors
	Clay_Color primary;
	Clay_Color onPrimary;

	Clay_Color secondary;
	Clay_Color onSecondary;

	// Surface
	Clay_Color surface;
	Clay_Color onSurface;
	Clay_Color surfaceContainerLow;
	Clay_Color onSurfaceContainerLow;
	Clay_Color surfaceContainer;
	Clay_Color onSurfaceContainer;
	Clay_Color surfaceContainerHigh;
	Clay_Color onSurfaceContainerHigh;

	// Status/Errors
	Clay_Color error;
	Clay_Color onError;
	Clay_Color rightContainer;
	Clay_Color onRightContainer;
	Clay_Color wrongContainer;
	Clay_Color onWrongContainer;

	// Borders and shadows
	Clay_Color outline;
	Clay_Color shadow;
};

const Theme *theme();
void theme_set(Theme::Type type);
Theme::Type theme_next();
