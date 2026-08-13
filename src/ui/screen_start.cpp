#include <SDL3/SDL_log.h>

#include "screen_helpers.h"

#include "base/str_view.h"
#include "app/worker.h"
#include "ui/components/button.h"
#include "ui/dpi.h"

void screen_start_go(AppContext *ctx) {
	ctx->go(Screen::Start);

	// TODO: purge
	// if (ctx->words->size == 0 && !seed_default_learning_list(*ctx)) {
	// 	SDL_LogError(SDL_LOG_CATEGORY_ERROR,
	// 	             "Seeding default learning list failed");
	// 	exit(-1);
	// }
}

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
		if (go.activated()) {
			// auto on_zip_downloaded = [](Size slot_index, int32_t request_id,
			//                             int status, StrView file_name,
			//                             DynArr<uint8_t> memory_buffer) {
			// 	Measure m{};
			// 	SDL_Log("file loaded %d, status %d", request_id, status);
			// 	std::filesystem::path out =
			// 		  std::string_view(file_name.data, file_name.size);
			// 	SDL_Log("unzipping %s", out.c_str());
			// 	out = out.parent_path();
			// 	unpack(file_name, StrView::lit(out.c_str()));
			// 	SDL_Log("unzipped to %s", out.c_str());
			// 	m.lap().print();
			// };
			// auto on_f_mem = [](Size slot_index, int32_t request_id, int
			// status,
			//                    StrView file_name,
			//                    DynArr<uint8_t> memory_buffer) {
			// 	SDL_Log("mem loaded %d, status %d", request_id, status);
			// 	StrView v{(char *)memory_buffer.data, memory_buffer.size};
			// 	SDL_Log(StrView_Fmt, StrView_Arg(v));
			// };
			// req_pool_index_1 = Worker::net_download_file(
			// 	  ctx,
			// 	  "http://0.0.0.0:8000/sherpa-onnx-whisper-base/base-decoder.onnx"_v,
			// 	  "/tmp/get.html"_v, on_zip_downloaded);
			// download_track(ctx, req_pool_index_1, "onnx"_v);
			// auto r2 = Worker::net_download_memory(
			// 	  ctx, "https://curl.se/ca/cacert.pem"_v,
			// 	  //
			// 	  //
			// "https://www.openthesaurus.de/synonyme/search?q=test&format=application/json"_v,
			// 	  {
			// 			.data = ctx->arena_screen().pushN<uint8_t>(190000),
			// 			.size = 0,
			// 			.reserved = 190000,
			// 	  },
			// 	  on_f_mem);
			//
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
		// 	download_row(ctx, dl);
		// }
	}
}
