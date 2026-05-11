#pragma once

#include "app/app_context.h"
#include "base/pair.h"
#include "ui/entry.h"

#if HOTRELOAD
#include <cstdio>
#include <dlfcn.h>
#include <filesystem>
#include <string>
#include <sys/stat.h>

#define X(NAME, ARGS, RETURN_TYPE) typedef RETURN_TYPE(*NAME##_t) ARGS;
UI_ENTRY_FUNCTIONS
#undef X
#define X(NAME, args, rettype) static NAME##_t NAME = NULL;
UI_ENTRY_FUNCTIONS
#undef X

#ifndef HOTRELOAD_MODULE_PATH
#define HOTRELOAD_MODULE_PATH "build/Debug/libapp_hotreload.so"
#endif

static Pair<bool, bool> hotreload(const char *path) {
	static struct stat statbuf;
	static time_t last_mtime = 0;
	static long last_mtime_nsec = 0;
	static void *handle = nullptr;

	if (0 == stat(path, &statbuf)) {
		if (statbuf.st_size == 0) {
			return {true, false};
		}
		if (handle != nullptr && last_mtime == statbuf.st_mtime &&
		    last_mtime_nsec == statbuf.st_mtim.tv_nsec) {
			return {true, false};
		}
		last_mtime = statbuf.st_mtime;
		last_mtime_nsec = statbuf.st_mtim.tv_nsec;
	} else if (handle != nullptr) {
		// Missing path after at least one successful load is not fatal.
		return {true, false};
	}

#if !__ANDROID__
	std::string load_path = path;
	{
		std::error_code ec;
		namespace fs = std::filesystem;
		const fs::path src(path);
		const fs::path shadow_dir = src.parent_path() / ".hotreload-cache";
		fs::create_directories(shadow_dir, ec);
		if (!ec) {
			const fs::path shadow_path =
				  shadow_dir / (src.filename().string() + "." +
			                    std::to_string((long long)last_mtime) + "." +
			                    std::to_string(last_mtime_nsec));
			fs::copy_file(src, shadow_path,
			              fs::copy_options::overwrite_existing, ec);
			if (!ec) {
				load_path = shadow_path.string();
			}
		}
	}
	const char *load_cstr = load_path.c_str();
#else
	const char *load_cstr = path;
#endif

	printf("loading hotreload module: %s\t", load_cstr);
	void *old_handle = handle;
	handle = dlopen(load_cstr, RTLD_LAZY);
	if (!handle) {
		fprintf(stderr, "dlopen() failed: %s\n", dlerror());
		handle = old_handle;
		return {false, false};
	}
	if (old_handle) {
		dlclose(old_handle);
	}

#define X(NAME, ARGS, RETURN_TYPE)                                             \
	NAME = (NAME##_t)(dlsym(handle, #NAME));                                   \
	if (!NAME)                                                                 \
		fprintf(stderr, "dlsym() failed: %s\n", dlerror());
	UI_ENTRY_FUNCTIONS
#undef X

	printf("DONE\n");
	return {true, true};
}

#endif // HOTRELOAD
