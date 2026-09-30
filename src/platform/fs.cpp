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
constexpr auto ORG = "klappt";
constexpr auto APP = "klappt.sdl";

inline std::filesystem::path to_fs_path(StrView sv) {
	return std::filesystem::path{
		  std::string_view{sv.data, static_cast<size_t>(sv.size)}};
}
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

bool fs_exists(StrView path) {
	std::error_code ec;
	return std::filesystem::exists(to_fs_path(path), ec);
}

bool fs_is_regular_file(StrView path) {
	std::error_code ec;
	return std::filesystem::is_regular_file(to_fs_path(path), ec);
}

bool fs_is_directory(StrView path) {
	std::error_code ec;
	return std::filesystem::is_directory(to_fs_path(path), ec);
}

bool fs_remove(StrView path) {
	std::error_code ec;
	return std::filesystem::remove(to_fs_path(path), ec);
}

bool fs_remove_all(StrView path) {
	std::error_code ec;
	return std::filesystem::remove_all(to_fs_path(path), ec) > 0;
}

bool fs_rename(StrView from, StrView to) {
	std::error_code ec;
	std::filesystem::rename(to_fs_path(from), to_fs_path(to), ec);
	return !ec;
}

bool fs_touch(StrView path) {
	auto p = to_fs_path(path);
	std::error_code ec;
	if (p.has_parent_path()) {
		std::filesystem::create_directories(p.parent_path(), ec);
	}
	FILE *f = fopen(p.string().c_str(), "ab");
	if (f) {
		fclose(f);
		return true;
	}
	return false;
}

Size fs_file_size(StrView path) {
	auto p = to_fs_path(path);
	std::error_code ec;
	if (std::filesystem::exists(p, ec) &&
	    std::filesystem::is_regular_file(p, ec)) {
		return static_cast<Size>(std::filesystem::file_size(p, ec));
	}
	return -1;
}
