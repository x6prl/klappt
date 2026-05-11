#include "platform/web_persist.h"

#include <string>

#ifdef __EMSCRIPTEN__
#include <SDL3/SDL_log.h>
#include <emscripten.h>
#endif

namespace {

constexpr char WEB_PERSIST_ROOT[] = "/persist/";
#ifdef __EMSCRIPTEN__
int web_persist_batch_depth = 0;
bool web_persist_batch_pending = false;
#endif

}

std::string web_persist_path_for(StrView file_name) {
	return std::string(WEB_PERSIST_ROOT) +
	       std::string(file_name.data, static_cast<size_t>(file_name.size));
}

#ifdef __EMSCRIPTEN__
EM_JS(void, web_persist_sync_impl, (), {
	if (typeof FS === "undefined") {
		return;
	}
	try {
		const info = FS.analyzePath("/persist");
		if (!info.exists) {
			return;
		}
		FS.syncfs(false, function(err) {
			if (err) {
				console.error("FS.syncfs(false) failed", err);
			}
		});
	} catch (e) {
		console.error("FS.syncfs(false) threw", e);
	}
});
#endif

void web_persist_sync() {
#ifdef __EMSCRIPTEN__
	if (web_persist_batch_depth > 0) {
		web_persist_batch_pending = true;
		return;
	}
	web_persist_batch_pending = false;
	web_persist_sync_impl();
#endif
}

void web_persist_begin_batch() {
#ifdef __EMSCRIPTEN__
	++web_persist_batch_depth;
#endif
}

void web_persist_end_batch() {
#ifdef __EMSCRIPTEN__
	if (web_persist_batch_depth <= 0) {
		return;
	}
	--web_persist_batch_depth;
	if (web_persist_batch_depth == 0 && web_persist_batch_pending) {
		web_persist_sync();
	}
#endif
}
