#include "fs.h"

#include <filesystem>
#include <string>

#include <SDL3/SDL_error.h>
#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_log.h>

#include "base/str_view.h"

namespace {
#ifdef __EMSCRIPTEN__
constexpr char WEB_PERSIST_ROOT[] = "/persist/";
#endif
constexpr auto ORG = "lexi";
constexpr auto APP = "lexi.sdl";
} // namespace

StrView get_app_base_path() {
	thread_local static std::string path{};
	if (path.empty()) {
#if __ANDROID__
		path = ""; // on Android we do not want to use basepath. Instead, assets
		           // are available at the root directory.
#else
		auto basePathPtr = SDL_GetBasePath();
		if (!basePathPtr) {
			SDL_LogError(SDL_LOG_CATEGORY_ERROR, "SDL_GetBasePath error: %s",
			             SDL_GetError());
		} else {
			path = basePathPtr;
			if (*path.rbegin() != '/') {
				path += '/';
			}
		}
#endif
	}
	return {path.c_str(), (Size)path.size()};
}

StrView get_writable_path() {
	thread_local static std::string path{};
	if (path.empty()) {
#ifdef __EMSCRIPTEN__
		path = WEB_PERSIST_ROOT;
#else
		char *pref = SDL_GetPrefPath(ORG, APP);
		if (!pref) {
			SDL_Log("SDL_GetPrefPath failed: %s", SDL_GetError());
		}

		path = pref;

		if (*path.rbegin() != '/') {
			path += '/';
		}
		SDL_free(pref);
#endif
	}
	return {path.c_str(), (Size)path.size()};
}

Size get_regular_file_size(const char *path) {
	std::filesystem::path f(path);
	if (std::filesystem::exists(f) && std::filesystem::is_regular_file(f)) {
		return std::filesystem::file_size(f);
	}
	return -1;
}
