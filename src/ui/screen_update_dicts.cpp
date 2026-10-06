#include "app/assets_dl.h"
#include "screen_helpers.h"

#include "app/app_context.h"
#include "app/words_init.h"
#include "app/worker.h"
#include "base/str_view.h"
#include "platform/net_worker.h"

#include "ui/components/button.h"
#include "ui/components/net_download_row.h"

namespace {

static bool was_updating = false;

static inline StrView get_asset_display_name(AssetsDL::AssetId id) {
	switch (id) {
	case AssetsDL::AssetId::DICT_EN:
		return "English"_v;
	case AssetsDL::AssetId::DICT_RU:
		return "Русский"_v;
	case AssetsDL::AssetId::DICT_TR:
		return "Türkçe"_v;
	case AssetsDL::AssetId::DICT_AR:
		return "العربية"_v;
	case AssetsDL::AssetId::DICT_DE:
		return "German Dictionary (de)"_v;
	case AssetsDL::AssetId::OPT_TTS:
		return "Speech Synthesis (TTS)"_v;
	case AssetsDL::AssetId::OPT_ASR:
		return "Speech Recognition (ASR)"_v;
	default:
		return "Asset"_v;
	}
}

static bool should_show_asset(AppContext *ctx, AssetsDL::AssetId id) {
	// 1. If already installed, always show it so user can update it
	if (AssetsDL::is_installed(ctx->arena_frame, id)) {
		return true;
	}

	// 2. The active language dictionary MUST be shown (to download if missing)
	const auto active_dict_id =
		  AssetsDL::dict_asset_id_for_lang(ctx->settings.tr_language);
	if (id == active_dict_id) {
		return true;
	}

	// 3. Optional features enabled in Settings
	switch (id) {
	case AssetsDL::AssetId::DICT_DE:
		return ctx->settings.is_subdict_de;
	case AssetsDL::AssetId::DICT_EN:
		return ctx->settings.is_subdict_en;
	case AssetsDL::AssetId::OPT_TTS:
		return ctx->settings.is_module_tts;
	case AssetsDL::AssetId::OPT_ASR:
		return ctx->settings.is_module_asr;
	default:
		return false;
	}
}

static void trigger_asset_download_or_update(AppContext *ctx,
                                             AssetsDL::AssetId id) {
	AssetsDL::clean_asset(ctx->arena_frame, id);

	auto pool_index = Worker::net_download_and_unpack_asset(ctx, id);
	if (pool_index >= 0) {
		download_track(ctx, pool_index, get_asset_display_name(id));
		was_updating = true;
	}

	if (ctx->net) {
		SDL_SignalCondition(ctx->net_worker_job_queue.cond);
	}
	ctx->push_one_frame();
}

} // namespace

void screen_update_dicts_push(AppContext *ctx) {
	ctx->push(Screen::UpdateDicts);
}

void screen_update_dicts_draw(AppContext *const ctx) {
	const bool is_busy = !ctx->downloads.is_empty();

	// Count installed dictionaries
	int installed_dict_count = 0;
	for (int i = 0; i < (int)lang_COUNT; ++i) {
		auto lang = static_cast<Lang>(i);
		auto id = AssetsDL::dict_asset_id_for_lang(lang);
		if (AssetsDL::is_installed(ctx->arena_frame, id)) {
			++installed_dict_count;
		}
	}
	const bool has_installed_dict = (installed_dict_count > 0);

	if (is_busy) {
		ctx->anim();
		if (ctx->net) {
			SDL_SignalCondition(ctx->net_worker_job_queue.cond);
		}
	} else if (was_updating) {
		was_updating = false;
		if (init_runtime_data(*ctx)) {
			// If we started with 0 dicts (redirected on boot), auto-enter app
			// once installed
			if (!has_installed_dict) {
				screen_default_go(ctx);
				return;
			}
		} else {
			SDL_LogError(SDL_LOG_CATEGORY_ERROR,
			             "failed to reinit runtime data after asset download");
		}
	}

	CLAY(CLAY_ID("UpdateDictsRoot"),
	     {
			   .layout =
					 {
						   .sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)},
						   .padding = sizes()->pad.screen,
						   .childGap = sizes()->space.sm,
						   .childAlignment = {CLAY_ALIGN_X_CENTER,
	                                          CLAY_ALIGN_Y_TOP},
						   .layoutDirection = CLAY_TOP_TO_BOTTOM,
					 },
			   .backgroundColor = theme()->surface,
		 }) {

		CLAY(CLAY_ID("UpdateDictsScrollContainer"),
		     {
				   .layout =
						 {
							   .sizing = {CLAY_SIZING_GROW(0),
		                                  CLAY_SIZING_GROW(0)},
							   .padding = {.top = sizes()->space.xs,
		                                   .bottom = sizes()->space.md},
							   .childGap = sizes()->space.md,
							   .childAlignment = {CLAY_ALIGN_X_CENTER,
		                                          CLAY_ALIGN_Y_TOP},
							   .layoutDirection = CLAY_TOP_TO_BOTTOM,
						 },
				   .clip =
						 {
							   .vertical = true,
							   .childOffset = Clay_GetScrollOffset(),
						 },
			 }) {


			const auto active_dict_id =
				  AssetsDL::dict_asset_id_for_lang(ctx->settings.tr_language);

			for (Size i = 0; i < (Size)AssetsDL::AssetId::_COUNT; ++i) {
				auto id = static_cast<AssetsDL::AssetId>(i);
				if (!should_show_asset(ctx, id)) {
					continue;
				}

				const bool is_installed =
					  AssetsDL::is_installed(ctx->arena_frame, id);
				const bool is_active_lang = (id == active_dict_id);
				const auto title_font = (id == AssetsDL::AssetId::DICT_AR)
				                              ? FontID::ARABIC_MAIN
				                              : FontID::MAIN;

				CLAY(CLAY_IDI("AssetCard", i),
				     {
						   .layout =
								 {
									   .sizing = {CLAY_SIZING_GROW(0),
				                                  CLAY_SIZING_FIT(0)},
									   .padding = sizes()->pad.card_compact,
									   .childGap = sizes()->space.sm,
									   .childAlignment = {CLAY_ALIGN_X_LEFT,
				                                          CLAY_ALIGN_Y_CENTER},
									   .layoutDirection = CLAY_LEFT_TO_RIGHT,
								 },
						   .backgroundColor = theme()->surfaceContainer,
						   .cornerRadius = sizes()->radius.md,
					 }) {

					// Info Column
					CLAY(CLAY_IDI("AssetInfoCol", i),
					     {
							   .layout =
									 {
										   .sizing = {CLAY_SIZING_GROW(0),
					                                  CLAY_SIZING_FIT(0)},
										   .childGap = sizes()->space.xs,
										   .childAlignment =
												 {CLAY_ALIGN_X_LEFT,
					                              CLAY_ALIGN_Y_CENTER},
										   .layoutDirection =
												 CLAY_TOP_TO_BOTTOM,
									 },
						 }) {

						CLAY(CLAY_IDI("AssetTitleRow", i),
						     {
								   .layout =
										 {
											   .sizing = {CLAY_SIZING_GROW(0),
						                                  CLAY_SIZING_FIT(0)},
											   .childGap = sizes()->space.xs,
											   .childAlignment =
													 {CLAY_ALIGN_X_LEFT,
						                              CLAY_ALIGN_Y_CENTER},
											   .layoutDirection =
													 CLAY_LEFT_TO_RIGHT,
										 },
							 }) {

							draw_text(get_asset_display_name(id),
							          theme()->onSurfaceContainer,
							          sizes()->font.title_md, title_font);

							if (is_active_lang) {
								draw_text(" (active)"_v, theme()->primary,
								          sizes()->font.label_sm, FontID::MAIN);
							}
						}

						auto status_col = theme()->onSurfaceContainer;
						status_col.a =
							  static_cast<uint8_t>(status_col.a * 0.6f);
						draw_text(
							  is_installed ? "Installed"_v : "Not installed"_v,
							  status_col, sizes()->font.label_md, FontID::MAIN);
					}

					CLAY(CLAY_IDI("AssetCardSpacer", i),
					     {.layout = {.sizing = {CLAY_SIZING_GROW(0),
					                            CLAY_SIZING_FIXED(0)}}}) {}

					if (!is_busy) {
						if (is_installed) {
							auto update_btn = mobile_button(
								  ctx, CLAY_IDI("UpdateBtn", i), "Update"_v,
								  mobile_button_style_surface_container_high());
							if (update_btn.activated()) {
								trigger_asset_download_or_update(ctx, id);
							}
						} else {
							auto dl_btn = mobile_button(
								  ctx, CLAY_IDI("DownloadBtn", i), "Download"_v,
								  mobile_button_style_primary());
							if (dl_btn.activated()) {
								trigger_asset_download_or_update(ctx, id);
							}
						}
					}
				}
			}

			if (is_busy) {
				CLAY(CLAY_ID("ActiveDownloadsCard"),
				     {
						   .layout =
								 {
									   .sizing = {CLAY_SIZING_GROW(0),
				                                  CLAY_SIZING_FIT(0)},
									   .padding = sizes()->pad.card_compact,
									   .childGap = sizes()->space.sm,
									   .childAlignment = {CLAY_ALIGN_X_CENTER,
				                                          CLAY_ALIGN_Y_TOP},
									   .layoutDirection = CLAY_TOP_TO_BOTTOM,
								 },
						   .backgroundColor = theme()->surfaceContainerLow,
						   .cornerRadius = sizes()->radius.lg,
					 }) {

					draw_text("Downloading & Unpacking..."_v,
					          theme()->onSurface, sizes()->font.title_md,
					          FontID::MAIN);

					bool all_completed = true;
					for (auto &dl : ctx->downloads) {
						download_row(ctx, dl);
						if (dl.status != DownloadData::FINISHED_OK) {
							all_completed = false;
						}
					}

					if (all_completed) {
						ctx->downloads.pop(ctx->downloads.size);
						ctx->push_one_frame();
					}
				}
			}
		}
	}
}
