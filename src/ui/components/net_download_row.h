#pragma once

#include <SDL3/SDL_log.h>

#include "app/app_context.h"
#include "app/net_context.h"
#include "app/sizes.h"
#include "base/str_view.h"
#include "platform/net_worker.h"

#include "ui/components/button.h"
#include "ui/components/download_data.h"

#include "ui/screen_helpers.h"

inline void download_track(AppContext *ctx, Size req_pool_index,
                           StrView title) {
	NetContext::NetRequestSlot &slot = ctx->net->requests_pool[req_pool_index];
	for (auto &dl : ctx->downloads) {
		if (dl.tracking_req_id == slot.req.request_id) {
			return;
		}
	}

	ctx->downloads.push(
		  ctx->arena, {
							.title = title,
							.status = DownloadData::TRACKING,
							.tracking_req_pool_index = req_pool_index,
							.tracking_req_id = slot.req.request_id,
							.copy_url = slot.req.url,
							.copy_file_name = slot.req.file_name,
							.copy_memory_buffer = slot.req.memory_buffer,
							.copy_on_finished_func = slot.req.on_finished_func,
					  });
}

inline void download_update_tracking(AppContext *ctx, DownloadData &dl,
                                     Size req_pool_index) {
	NetContext::NetRequestSlot &slot = ctx->net->requests_pool[req_pool_index];
	dl.status = DownloadData::TRACKING;
	dl.tracking_req_pool_index = req_pool_index;
	dl.tracking_req_id = slot.req.request_id;
	// dl.copy_url = slot.req.url;
	// dl.copy_file_name = slot.req.file_name;
	// dl.copy_memory_buffer = slot.req.memory_buffer;
	// dl.copy_on_finished_func = slot.req.on_finished_func;
	dl.bytes_total = 0;
	dl.speed_last_dlnow = 0;
	dl.speed_kibs = 0.f;
	dl.speed_last_ticks = 0;

	return;
}
// inline void download_update_tracking(AppContext *ctx, int32_t old_req_id,
//                                      Size req_pool_index) {
// 	NetContext::NetRequestSlot &slot = ctx->net->requests_pool[req_pool_index];
//
// 	for (auto &dl : ctx->downloads) {
// 		if (dl.tracking_req_id == old_req_id) {
// 			dl.status = DownloadData::TRACKING;
// 			dl.tracking_req_pool_index = req_pool_index;
// 			dl.tracking_req_id = slot.req.request_id;
// 			dl.copy_url = slot.req.url;
// 			dl.copy_file_name = slot.req.file_name;
// 			dl.copy_memory_buffer = slot.req.memory_buffer;
// 			dl.copy_on_finished_func = slot.req.on_finished_func;
// 			return;
// 		}
// 	}
// }

inline bool download_row(AppContext *ctx, DownloadData &dl,
                         bool is_cancelable = false) {
	bool is_retry_pressed = false;
	const bool is_active = (dl.tracking_req_pool_index >= 0 &&
	                        dl.status == DownloadData::TRACKING);

	int percentage = -1;
	StrView status_icon{};
	Clay_Color status_icon_color{};
	StrView status_text{};

	if (is_active) {
		auto &slot = ctx->net->requests_pool[dl.tracking_req_pool_index];
		auto &req = slot.req;

		int64_t dlnow = Atomic::get(&req.bytes_downloaded);
		int64_t dltotal = Atomic::get(&req.bytes_total);

		auto t_diff = (float)(ctx->ticks - dl.speed_last_ticks) / 1000.f;
		if (dl.speed_last_ticks > 0 && t_diff > 1.f) {
			auto dl_diff = (float)(dlnow - dl.speed_last_dlnow);
			dl.speed_kibs = dl_diff / t_diff;
			dl.speed_last_ticks = ctx->ticks;
			dl.speed_last_dlnow = dlnow;
		} else if (dl.speed_last_ticks == 0) {
			dl.speed_last_ticks = ctx->ticks;
		}

		if (dltotal > 0) {
			percentage = (int)((dlnow * 100) / dltotal);
			StrBuilder strs{};
			if (dl.speed_kibs > 0.f) {
				strs.push(ctx->arena_frame, dl.speed_kibs / 1024.f);
				strs.push(ctx->arena_frame, "KiB/s "_v);
			}
			strs.push_formatted_bytes(ctx->arena_frame, dlnow);
			strs.push(ctx->arena_frame, "/"_v);
			strs.push_formatted_bytes(ctx->arena_frame, dltotal);
			status_text = strs.join(ctx->arena_frame);
		} else {
			status_icon = Icons::CIRCLE_HALF;
			status_icon_color = theme()->primary;
		}
	}

	switch (dl.status) {
	case DownloadData::UNPACKING:
		percentage = 100;
		status_text = "unpacking..."_v;
		status_icon = Icons::ROTATE_LEFT;
		status_icon_color = theme()->primary;
		break;
	case DownloadData::FINISHED_OK:
		status_icon = Icons::CIRCLE_CHECK;
		status_icon_color = theme()->onRightContainer;
		break;
	case DownloadData::FINISHED_CANCELLED:
		status_icon = Icons::CIRCLE_EXCLAMATION;
		status_icon_color = theme()->onWrongContainer;
		break;
	case DownloadData::FINISHED_ERROR:
		status_icon = Icons::CIRCLE_EXCLAMATION;
		status_icon_color = theme()->onWrongContainer;
		status_text = dl.error.view();
		break;
	default:
		break;
	}

	CLAY(CLAY_IDI("DownloadRow", dl.tracking_req_id),
	     {
			   .layout =
					 {
						   .sizing = {CLAY_SIZING_GROW(0),
	                                  CLAY_SIZING_FIXED(0)},
						   .padding = sizes()->pad.card,
						   .childAlignment = {CLAY_ALIGN_X_LEFT,
	                                          CLAY_ALIGN_Y_CENTER},
						   .layoutDirection = CLAY_LEFT_TO_RIGHT,
					 },
			   .backgroundColor = theme()->surfaceContainer,
			   .cornerRadius = sizes()->radius.md,
		 }) {
		CLAY(CLAY_IDI("TextColumn", dl.tracking_req_id),
		     {
				   .layout =
						 {
							   .sizing = {CLAY_SIZING_GROW(0),
		                                  CLAY_SIZING_FIXED(0)},
							   .childAlignment = {CLAY_ALIGN_X_LEFT,
		                                          CLAY_ALIGN_Y_CENTER},
							   .layoutDirection = CLAY_TOP_TO_BOTTOM,
						 },
				   .backgroundColor = theme()->surfaceContainer,
			 }) {
			CLAY(CLAY_IDI("Title", dl.tracking_req_id),
			     {
					   .layout =
							 {
								   .sizing = {CLAY_SIZING_GROW(0),
			                                  CLAY_SIZING_FIXED(0)},
								   .childGap = sizes()->space.md,
								   .childAlignment = {CLAY_ALIGN_X_LEFT,
			                                          CLAY_ALIGN_Y_CENTER},
								   .layoutDirection = CLAY_LEFT_TO_RIGHT,
							 },
				 }) {
				auto title_text = dl.title ? dl.title : "download"_v;
				draw_text(title_text, theme()->onSurfaceContainer);

				CLAY(CLAY_IDI("Spacer", dl.tracking_req_id),
				     {.layout = {.sizing = {CLAY_SIZING_GROW(0),
				                            CLAY_SIZING_FIXED(0)}}}) {}

				if (status_text) {
					draw_text(status_text, theme()->onSurface,
					          sizes()->font.label_sm, FontID::MONOSPACE_REGULAR,
					          CLAY_TEXT_WRAP_NONE);
				} else if (!is_cancelable) {
					draw_text(status_icon, status_icon_color,
					          sizes()->font.label_sm, FontID::ICONS);
				}
				CLAY(CLAY_IDI("Spacer", dl.tracking_req_id),
				     {.layout = {.sizing = {CLAY_SIZING_FIXED((float)sizes()->space.xs),
				                            CLAY_SIZING_FIXED(0)}}}) {}
			}

			const bool has_percentage = (percentage >= 0);
			const float progress_height = static_cast<float>(sizes()->space.xs);
			CLAY(CLAY_IDI("ProgressBar", dl.tracking_req_id),
			     {
					   .layout =
							 {
								   .sizing = {CLAY_SIZING_GROW(0),
			                                  CLAY_SIZING_FIXED(
													progress_height)},
								   .layoutDirection = CLAY_LEFT_TO_RIGHT,
							 },
					   .backgroundColor = has_percentage
			                                    ? theme()->wrongContainer
			                                    : Clay_Color{},
					   .cornerRadius = sizes()->radius.xs,
				 }) {
				if (has_percentage) {
					CLAY(CLAY_IDI("Percentage", dl.tracking_req_id),
					     {
							   .layout =
									 {
										   .sizing = {CLAY_SIZING_PERCENT(
															(float)percentage *
															0.01f),
					                                  CLAY_SIZING_FIXED(
															progress_height)},
										   .layoutDirection =
												 CLAY_LEFT_TO_RIGHT,
									 },
							   .backgroundColor =
									 (dl.status == DownloadData::UNPACKING)
										   ? theme()->primary
										   : theme()->onRightContainer,
							   .cornerRadius = sizes()->radius.xs,
						 }) {}
				}
			}
		}
		const bool is_failed = (dl.status == DownloadData::FINISHED_ERROR ||
		                        dl.status == DownloadData::FINISHED_CANCELLED);
		if (is_cancelable || is_failed) {
			auto style = mobile_button_style_surface_container_high();
			style.font_id = FontID::ICONS;
			style.font_size = static_cast<uint16_t>(sizes()->dim.icon_md);
			style.height = sizes()->dim.action_btn_size;
			style.min_width = sizes()->dim.action_btn_size;
			style.padding_x = sizes()->space.xs;
			style.padding_y = sizes()->space.xs;
			style.corner_radius = sizes()->radius.sm.topLeft;
			style.background = {};
			style.text = theme()->onWrongContainer;
			auto icon = Icons::STOP;
			if (!is_active) {
				style.background_pressed =
					  is_failed ? theme()->surfaceContainerLow : Clay_Color{};
				style.text_pressed = style.text;
				style.text = (dl.status == DownloadData::FINISHED_OK)
				                   ? theme()->onRightContainer
				             : is_failed ? theme()->onWrongContainer
				                         : theme()->primary;

				icon = (dl.status == DownloadData::FINISHED_OK)
				             ? Icons::CIRCLE_CHECK
				             : Icons::ROTATE_LEFT;
			}
			auto c = mobile_button(ctx,
			                       CLAY_IDI("CancelButton", dl.tracking_req_id),
			                       icon, style);
			if (c.activated()) {
				if (is_active) {
					Worker::net_cancel_request(ctx, dl.tracking_req_pool_index);
					dl.status = DownloadData::FINISHED_CANCELLED;
					dl.tracking_req_pool_index = -1;
				} else {
					is_retry_pressed = true;
				}
			}
		}
	}

	if (is_retry_pressed) {
		Worker::download_retry(ctx, dl);
	}

	return is_retry_pressed;
}
