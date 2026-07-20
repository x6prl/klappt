#include "app/worker.h"
#include "base/str_view.h"
#include "screen_helpers.h"
#include "ui/components/button.h"
#include "ui/dpi.h"
#include <SDL3/SDL_log.h>

void screen_start_go(AppContext *ctx) { ctx->go(Screen::Start); }

void screen_start_draw(AppContext *ctx) {
	CLAY(CLAY_ID("ScreenStart"),
	     {
			   .layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)},
	                      .padding = CLAY_PADDING_ALL(udpi(16.0f)),
	                      .childGap = udpi(14.0f),
	                      .childAlignment = {CLAY_ALIGN_X_CENTER,
	                                         CLAY_ALIGN_Y_CENTER},
	                      .layoutDirection = CLAY_TOP_TO_BOTTOM},
		 }) {
		auto go = mobile_button(ctx, CLAY_ID("go"), "Go"_v);
		static Size req_pool_index_1 = -1, r2 = -1;
		if (go.activated()) {
			// auto on_f_file = [](Size slot_index, int32_t request_id, int status,
			//                     DynArr<uint8_t> memory_buffer) {
			// 	SDL_Log("file loaded %d, status %d", request_id, status);
			// };
			// auto on_f_mem = [](Size slot_index, int32_t request_id, int status,
			//                    DynArr<uint8_t> memory_buffer) {
			// 	SDL_Log("mem loaded %d, status %d", request_id, status);
			// };
			// req_pool_index_1 = Worker::net_download_file(
			// 	  ctx,
			// 	  "http://0.0.0.0:8000/sherpa-onnx-whisper-base/base-decoder.onnx"_v,
			// 	  "/tmp/get.html"_v, on_f_file);
			// r2 = Worker::net_download_memory(
			// 	  ctx,
			// 	  "https://www.openthesaurus.de/synonyme/search?q=test&format=application/json"_v,
			// 	  {
			// 			.data = ctx->arena_screen().pushN<uint8_t>(2048),
			// 			.size = 0,
			// 			.reserved = 2048,
			// 	  },
			// 	  on_f_mem);
			// track_download(ctx, req_pool_index_1, "onnx"_v);
			// track_download(ctx, r2, "very long api call"_v);

			screen_exercise_go(ctx, false);

			// job_push(
			// 	  ctx,
			// 	  {.type = Job::Type::TTS,
			//        .str =
			//              "Das war eine kleine, gemütliche Stadt, gefüllt mit
			//              bescheidenen, aber freundlichen Menschen."_v});
			// job_push(
			// 	  ctx,
			// 	  {.type = Job::Type::ASR,
			//        .str =
			//              "rrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrr"_v});
		}
		// for (auto&dl: ctx->downloads) {
		// 	net_download_row(ctx, dl);
		// }
	}
}
