#pragma once

#include <SDL3/SDL.h>
#include <SDL3/SDL_filesystem.h>

#include "base/arena.h"
#include "base/str_builder.h"
#include "base/str_view.h"
#include "platform/fs.h"

#ifdef __EMSCRIPTEN__
#include "platform/web_persist.h"
#endif

inline StrView get_writable_file_path_for(Arena &a, StrView file_name,
                                          StrView suffix = {}) {
	auto w = get_writable_path();
	if (suffix) {
		StrBuilder strs{};
		strs.push(a, w);
		strs.push(a, file_name);
		strs.push(a, suffix);
		return strs.join(a);
	} else {
		return StrView::concat(a, w, file_name);
	}
}

inline bool file_save_relative(Arena &scratch, StrView file_name,
                               const void *data, Size size) {
	auto g = scratch.guard();
	auto path = get_writable_file_path_for(scratch, file_name).to_cstr(scratch);
	const bool ok = SDL_SaveFile(path, data, size);
#ifdef __EMSCRIPTEN__
	if (ok) {
		web_persist_sync();
	}
#endif
	return ok;
}

struct FileLoader {
	void *data{nullptr};
	Size size{0};

	bool load_from_path(Arena &scratch, StrView path_to_file) {
		auto g = scratch.guard();
		if (!path_to_file) {
			return false;
		}

		size_t _size{};
		data = SDL_LoadFile(path_to_file.to_cstr(scratch), &_size);
		size = static_cast<Size>(_size);
		if (!data) {
			return false;
		}
		return true;
	}
	bool load_from_writable(Arena &scratch, StrView file_name) {
		auto g = scratch.guard();
		auto path = get_writable_file_path_for(scratch, file_name);
		return load_from_path(scratch, path);
	}

	~FileLoader() {
		if (data) {
			SDL_free(data);
		}
	}
};
