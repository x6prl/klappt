#pragma once

#include <SDL3/SDL.h>
#include <SDL3/SDL_filesystem.h>

#include "base/arena.h"
#include "base/str_builder.h"
#include "base/str_view.h"
#include "platform/fs.h"

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

inline bool file_save(Arena &scratch, StrView file_name, const void *data,
                      Size size) {
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

	bool load(Arena &scratch, StrView file_name) {
		auto g = scratch.guard();
		auto path = get_writable_file_path_for(scratch, file_name);
		if (!path) {
			return false;
		}

		size_t _size{};
		data = SDL_LoadFile(path.to_cstr(scratch), &_size);
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
