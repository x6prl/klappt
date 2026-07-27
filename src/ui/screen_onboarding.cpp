#include "SDL3/SDL_log.h"
#include "app/words_init.h"
#include "app/worker.h"
#include "base/atomic.h"
#include "base/str_view.h"
#include "domain/settings.h"
#include "platform/net_worker.h"
#include "ui/components/button.h"
#include "ui/components/download_data.h"
#include "ui/components/net_download_row.h"
#include "ui/components/switch_button.h"
#include "ui/dpi.h"
#include "ui/screen_helpers.h"
#include <utility>

namespace {
void draw_option_row(AppContext *ctx, Clay_ElementId id, StrView label,
                     StrView sub_text, bool is_turned_on, auto on_switched) {
	auto label_text_size = udpi(16.f);
	auto sub_text_size = udpi(12.f);

	CLAY(id,
	     {
			   .layout =
					 {
						   .sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0)},
						   .padding = CLAY_PADDING_ALL(udpi(16.0f)),
						   .childGap = udpi(14.0f),
						   .childAlignment = {CLAY_ALIGN_X_CENTER,
	                                          CLAY_ALIGN_Y_CENTER},
					 },
			   .backgroundColor = theme()->surfaceContainer,
			   .cornerRadius = CLAY_CORNER_RADIUS(dpi(16)),

			   // .border = {.color = theme()->outline,
	           //                  .width = {.bottom = udpi(1.f)}},
		 }) {
		const bool is_dark = theme()->theme == Theme::Dark;
		CLAY(CLAY_IDI("Label", id.id),
		     {
				   .layout =
						 {
							   .sizing = {CLAY_SIZING_GROW(0),
		                                  CLAY_SIZING_FIT(0)},
							   .layoutDirection = CLAY_TOP_TO_BOTTOM,
						 },

			 }) {
			draw_text(label, theme()->onSurface, label_text_size,
			          translation_font_id(ctx), CLAY_TEXT_WRAP_WORDS,
			          CLAY_TEXT_ALIGN_LEFT);
			draw_text(sub_text, theme()->onSurface, sub_text_size,
			          translation_font_id(ctx), CLAY_TEXT_WRAP_WORDS,
			          CLAY_TEXT_ALIGN_LEFT);
		}
		if (switch_button(ctx, CLAY_IDI("Switch", id.id), is_turned_on, 42.f)) {
			on_switched(!is_turned_on);
			ctx->push_one_frame();
		}
	}
}

bool check_and_run_download_and_unpack(AppContext *ctx, StrView label,
                                       AssetsDL::Type t,
                                       auto should_be_downloaded_f) {
	auto &s = ctx->settings;
	auto &r = s.asset(t);
	auto es = StrView::from_number(ctx->arena_frame, r.expected_size);
	SDL_Log("TYPE (%d) %d %d %d " StrView_Fmt, std::to_underlying(t),
	        (int)r.is_zip_ready_to_unpack, (int)r.is_unpacked,
	        (int)r.is_zip_removed, StrView_Arg(es));
	if (should_be_downloaded_f(ctx) && !r.is_unpacked && !r.is_zip_removed) {
		auto pool_index = Worker::net_download_and_unpack_asset(ctx, t);
		if (pool_index >= 0) {
			download_track(ctx, pool_index, label);
		} else {
			SDL_LogError(SDL_LOG_CATEGORY_ERROR, "unxepected index %d",
			             pool_index);
			if (pool_index == -2) {
				return false;
			} else if (pool_index == -1) {
				SDL_Log("Was already downloaded, will run unpack...");
			}
		}
	}
	return true;
};

bool run_download_and_unpack_tr_asset(AppContext *ctx) {
	return check_and_run_download_and_unpack(
		  ctx, "Main dictionary"_v, AssetsDL::Type::XAPIAN_TR,
		  [](AppContext *_) { return true; });
}

bool run_download_and_unpack_optional_assets(AppContext *ctx) {
	auto ret = true;
	ret = ret &&
	      check_and_run_download_and_unpack(
				ctx, "German Wiktionary"_v, AssetsDL::Type::OPTIONAL_XAPIAN_DE,
				[](AppContext *ctx) { return ctx->settings.is_using_also_de; });
#if NEURO
	ret = ret && check_and_run_download_and_unpack(
					   ctx, "Text-to-speech"_v, AssetsDL::Type::OPTIONAL_TTS,
					   [](AppContext *ctx) {
						   SDL_Log("checking tts %d %d %d",
		                           (int)ctx->settings.is_using_also_de,
		                           (int)ctx->settings.is_using_tts,
		                           (int)ctx->settings.is_using_asr);
						   return ctx->settings.is_using_tts;
					   });
	ret = ret &&
	      check_and_run_download_and_unpack(
				ctx, "Voice recognition"_v, AssetsDL::Type::OPTIONAL_ASR,
				[](AppContext *ctx) { return ctx->settings.is_using_asr; });
#endif
	return ret;
}

// bool is_resources_were_downloaded_and_unpacked(AppContext *ctx) {
// 	// TODO: add integrity check
// 	auto &s = ctx->settings;
// 	auto ret = true;
// 	if (s.is_using_also_de) {
// 		ret = ret && s.resource(Settings::AssetsDL::Type::OPTIONAL_XAPIAN_DE)
// 		                   .is_unpacked;
// 	}
// 	if (s.is_using_tts) {
// 		ret = ret &&
// 		      s.resource(Settings::AssetsDL::Type::OPTIONAL_TTS).is_unpacked;
// 	}
// 	if (s.is_using_asr) {
// 		ret = ret &&
// 		      s.resource(Settings::AssetsDL::Type::OPTIONAL_ASR).is_unpacked;
// 	}
// 	return ret;
// }
} // namespace

void screen_onboarding_go(AppContext *ctx) { ctx->go(Screen::Onboarding); }

void screen_onboarding_draw(AppContext *const ctx) {
	auto text_size = dpi(24.f);
	auto &settings = ctx->settings;

	auto next_stage = [&ctx](bool is_should_save) {
		ctx->settings.onboarding_stage += 1;
		if (is_should_save) {
			ctx->settings.save(ctx->arena);
		}
	};

	CLAY(CLAY_ID("OnboardingContainer"),
	     {
			   .layout =
					 {
						   .sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)},
						   .padding = CLAY_PADDING_ALL(udpi(4.0f)),
						   .childGap = udpi(160.f),
						   .childAlignment = {CLAY_ALIGN_X_CENTER,
	                                          CLAY_ALIGN_Y_CENTER},
						   .layoutDirection = CLAY_TOP_TO_BOTTOM,
					 },
		 }) {
		switch (settings.onboarding_stage) {
		case 0: {
			CLAY(CLAY_ID("LanguageLabel"),
			     {
					   .layout =
							 {
								   .sizing = {CLAY_SIZING_GROW(0),
			                                  CLAY_SIZING_FIT(0)},
								   .padding = CLAY_PADDING_ALL(udpi(4.0f)),
								   .childAlignment = {CLAY_ALIGN_X_CENTER,
			                                          CLAY_ALIGN_Y_CENTER},
							 },
				 }) {
				draw_text("Chose language"_v, theme()->onSurface, text_size);
			}

			CLAY(CLAY_ID("LanguageRow"),
			     {
					   .layout =
							 {
								   .sizing = {CLAY_SIZING_GROW(0),
			                                  CLAY_SIZING_FIT(0)},
								   .padding = CLAY_PADDING_ALL(udpi(4.0f)),
								   .childGap = udpi(14.0f),
								   .childAlignment = {CLAY_ALIGN_X_CENTER,
			                                          CLAY_ALIGN_Y_CENTER},
							 },
				 }) {
				auto button_style =
					  mobile_button_style_surface_container_high();
				Settings::for_every_lang([&](int i, Lang lang) {
					auto btn = mobile_button(ctx, CLAY_IDI("LangButton", i),
					                         lang_code(lang), button_style);
					if (btn.activated()) {
						settings.tr_language = lang;
						settings.save(ctx->arena_frame);
						next_stage(true);
					}
				});
			}
		} break;
		case 1:
			CLAY(CLAY_ID("ResourcesAndOptions"),
			     {
					   .layout =
							 {
								   .sizing = {CLAY_SIZING_GROW(0),
			                                  CLAY_SIZING_GROW(0)},
								   .padding = CLAY_PADDING_ALL(udpi(16.0f)),
								   .childGap = udpi(56.0f),
								   .childAlignment = {CLAY_ALIGN_X_CENTER,
			                                          CLAY_ALIGN_Y_CENTER},
								   .layoutDirection = CLAY_TOP_TO_BOTTOM,
							 },
				 }) {
				if (ctx->downloads.is_empty()) { // ask user
					draw_text(
						  "We have to download and prepare the dictionaries"_v,
						  theme()->onSurface, text_size);
					auto dlbtn =
						  mobile_button(ctx, CLAY_ID("DLButton"), "Download"_v);
					if (dlbtn.activated()) {
						(void)run_download_and_unpack_tr_asset(ctx);
					}
				} else {
					draw_option_row(
						  ctx, CLAY_ID("DEWiki"), "German Wiktionary"_v,
						  "Usable, if you already understand something. "
						  "Glossary "
						  "information, without translation."_v,
						  settings.is_using_also_de, [ctx](bool new_val) {
							  ctx->settings.is_using_also_de = new_val;
							  ctx->settings.save(ctx->arena_frame);
						  });
#if NEURO
					draw_option_row(
						  ctx, CLAY_ID("TTS"), "Text-to-speech"_v,
						  "Allows you to hear the pronounciation of a word or a phrase, even when there is no audio in Wiktionary. Used for offline audio generation. May be very slow on old devices. ~80MB"_v,
						  settings.is_using_tts, [ctx](bool new_val) {
							  ctx->settings.is_using_tts = new_val;
							  ctx->settings.save(ctx->arena_frame);
						  });
					draw_option_row(
						  ctx, CLAY_ID("ASR"), "Speech recognition"_v,
						  "Allows you to say something and see how ASR engine transcribes it. Currently NOT very usefull. May be REALLY slow on old devices. ~160MB"_v,
						  settings.is_using_asr, [ctx](bool new_val) {
							  ctx->settings.is_using_asr = new_val;
							  ctx->settings.save(ctx->arena_frame);
						  });
#endif

					auto next_btn =
						  mobile_button(ctx, CLAY_ID("NextButton"), "Next"_v,
					                    mobile_button_style_primary());
					if (next_btn.activated()) {
						run_download_and_unpack_optional_assets(ctx);
						next_stage(true);
					}
				}
			}
			break;
		case 2:
			CLAY(CLAY_ID("WaitingForDownloads"),
			     {
					   .layout =
							 {
								   .sizing = {CLAY_SIZING_GROW(0),
			                                  CLAY_SIZING_GROW(0)},
								   .padding = CLAY_PADDING_ALL(udpi(8.0f)),
								   .childGap = udpi(16.0f),
								   .childAlignment = {CLAY_ALIGN_X_CENTER,
			                                          CLAY_ALIGN_Y_CENTER},
								   .layoutDirection = CLAY_TOP_TO_BOTTOM,
							 },
				 }) {
				// NOTE: assests are NOT ready

				// NOTE: case 0: no downloads were registered (app
				// restarted)
				if (ctx->downloads.is_empty()) {
					if (ctx->net) { // if net subsystem ready
						SDL_Log("RERUN all downloads");
						run_download_and_unpack_tr_asset(ctx);
						run_download_and_unpack_optional_assets(ctx);
					} else {
						SDL_Log("No net ctx, waiting to rerun downloads...");
						ctx->anim();
					}
				}
				// NOTE: case 1: downloads are registered
				else {
					// NOTE: waking up NetThread, because it will go to
					// sleep due empty job queue :c
					bool is_any_download_tracked_and_in_some_kind_of_progress =
						  false;
					for (auto &dl : ctx->downloads) {
						bool is_tracked_but_finished =
							  dl.status ==
									DownloadData::Status::FINISHED_ERROR ||
							  dl.status == DownloadData::Status::FINISHED_OK ||
							  dl.status ==
									DownloadData::Status::FINISHED_CANCELLED;
						auto req_in_real_progress =
							  [ctx](DownloadData dl) -> bool {
							if (!ctx->net) {
								return false;
							}
							if (dl.tracking_req_pool_index < 0) {
								return false;
							}
							return Atomic::get(
										 &ctx->net
												->requests_pool
													  [dl.tracking_req_pool_index]
												.req.status) ==
							       NetRequest::STATUS_IN_PROGRESS;
						};
						if (is_tracked_but_finished ||
						    req_in_real_progress(dl)) {
							is_any_download_tracked_and_in_some_kind_of_progress =
								  true;
							break;
						}
					}
					if (!is_any_download_tracked_and_in_some_kind_of_progress) {
						SDL_SignalCondition(ctx->net_worker_job_queue.cond);
					}
				}

				draw_text("Downloading resources..."_v, theme()->onSurface,
				          text_size);
				draw_text("Please wait and do not close the app"_v,
				          theme()->onSurface, text_size);

				Size finished_count = 0;
				for (auto &dl : ctx->downloads) {
					if (dl.status == DownloadData::Status::FINISHED_OK) {
						++finished_count;
					}
					bool should_retry = download_row(ctx, dl);
					// if (should_retry) {
					// 	Worker::net_request_retry(ctx,
					// 	                          dl.tracking_req_pool_index);
					// 	download_update_tracking(ctx,
					// 	                         dl.tracking_req_pool_index);
					// }
				}
				// resetting dl tracking
				if (finished_count > 0 &&
				    finished_count == ctx->downloads.size) {
					SDL_Log("all %d downloads are successful", finished_count);
					ctx->downloads.size = 0;
					next_stage(true);
				}
			}
			break;
		case 3:
			CLAY(CLAY_ID("WaitingForUnpacking"),
			     {
					   .layout =
							 {
								   .sizing = {CLAY_SIZING_GROW(0),
			                                  CLAY_SIZING_GROW(0)},
								   .padding = CLAY_PADDING_ALL(udpi(8.0f)),
								   .childGap = udpi(16.0f),
								   .childAlignment = {CLAY_ALIGN_X_CENTER,
			                                          CLAY_ALIGN_Y_CENTER},
								   .layoutDirection = CLAY_TOP_TO_BOTTOM,
							 },
				 }) {
				bool is_unpacking_in_progress = false;
				{
					using AType = AssetsDL::Type;
					is_unpacking_in_progress =
						  settings.asset(AType::XAPIAN_TR)
								.is_zip_ready_to_unpack &&
						  !settings.asset(AType::XAPIAN_TR).is_unpacked;
					settings.assets.for_each_optional(
						  [&is_unpacking_in_progress](
								AssetsDL::RemoteAsset &asset, auto t) {
							  auto g = tctx()->a.guard();
							  auto es = StrView::from_number(
									tctx()->a, asset.expected_size);
							  // SDL_Log("TYPE (%d) %d %d %d " StrView_Fmt,
						      //            std::to_underlying(t),
						      //            (int)asset.is_zip_ready_to_unpack,
						      //            (int)asset.is_unpacked,
						      //            (int)asset.is_zip_removed,
						      //            StrView_Arg(es));
							  is_unpacking_in_progress =
									is_unpacking_in_progress ||
									asset.is_zip_ready_to_unpack &&
										  !asset.is_unpacked;
						  });
				}

				if (is_unpacking_in_progress) {
					draw_text("Unpacking resources…"_v, theme()->onSurface,
					          text_size);
					draw_text("Please wait and do not close the app"_v,
					          theme()->onSurface, text_size);
				} else {
					next_stage(true);
				}
			}
			break;
		default:
			settings.onboarding_stage = -1;
			settings.save(ctx->arena_frame);
			if (!init_runtime_data(*ctx)) {
				SDL_LogError(SDL_LOG_CATEGORY_ERROR,
				             "failed to init runtime data");
			}
			screen_start_go(ctx);
		}
	}
}
