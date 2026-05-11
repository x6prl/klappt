#pragma once

#include <SDL3/SDL.h>
#include <SDL3/SDL_iostream.h>
#include <string>

#include "base/arena.h"
#include "base/str_view.h"
#include "platform/web_persist.h"

constexpr auto ORG = "lexi";
constexpr auto APP = "lexi.sdl";

inline bool file_save(StrView file_name, const void *data, Size size) {
#ifdef __EMSCRIPTEN__
	std::string path = web_persist_path_for(file_name);
	const bool ok = SDL_SaveFile(path.c_str(), data, size);
	if (ok) {
		web_persist_sync();
	}
	return ok;
#else
	char *pref = SDL_GetPrefPath(ORG, APP);
	if (!pref) {
		SDL_Log("SDL_GetPrefPath failed: %s", SDL_GetError());
		return false;
	}

	std::string path =
		  std::string(pref) + std::string(file_name.data, file_name.size);
	SDL_free(pref);

	return SDL_SaveFile(path.c_str(), data, size);
#endif
}

struct FileLoader {
	void *data{nullptr};
	Size size{};

	static std::string path_for(StrView file_name) {
#ifdef __EMSCRIPTEN__
		return web_persist_path_for(file_name);
#else
		char *pref = SDL_GetPrefPath(ORG, APP);
		if (!pref) {
			SDL_Log("SDL_GetPrefPath failed: %s", SDL_GetError());
			return {};
		}
		std::string path =
			  std::string(pref) + std::string(file_name.data, file_name.size);
		SDL_free(pref);
		return path;
#endif
	}

	bool load(StrView file_name) {
		auto path = path_for(file_name);
		if (!path.size()) {
			return false;
		}

		size_t _size{};
		data = SDL_LoadFile(path.c_str(), &_size);
		size = static_cast<Size>(_size);
		if (!data) {
			return false;
		}
		return true;
	}

	~FileLoader() {
		if (data) {
			SDL_free(data);
		}
	}
};
