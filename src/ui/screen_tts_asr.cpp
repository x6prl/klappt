#include "app/app_context.h"
#include "app/audio_context.h"
#include "app/worker.h"
#include "base/str_view.h"
#include "platform/neuro.h"
#include "screen_helpers.h"
#include "ui/components/button.h"
#include "ui/components/text_input.h"
#include "ui/dpi.h"
#include "ui/themes.h"
#include <SDL3/SDL_audio.h>
#include <SDL3/SDL_log.h>
#include <SDL3/SDL_stdinc.h>
#include <cstring>

void screen_tts_asr_go(AppContext *ctx) {
	ctx->mobile_text_input.activate_text_input = true;
	ctx->asr_result = {};
	ctx->go(Screen::TTS_ASR);
	record_init(ctx);
	ctx->push_one_frame();
}

void screen_tts_asr_draw(AppContext *ctx) {
	const auto header_height = dpi(40.0f);
	const auto input_height = dpi(56.0f);
	const auto btn_height = dpi(48.0f);

	CLAY(CLAY_ID("ScreenTTSAsr"),
	     {
			   .layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)},
	                      .padding = CLAY_PADDING_ALL(udpi(16.0f)),
	                      .childGap = udpi(14.0f),
	                      .layoutDirection = CLAY_TOP_TO_BOTTOM},
		 }) {

		// Header
		CLAY(CLAY_ID("TTSSectionHeader"),
		     {.layout = {.sizing = {CLAY_SIZING_GROW(0),
		                            CLAY_SIZING_FIXED(header_height)}}}) {
			draw_text("TTS - Text-to-Speech"_v, theme()->onSurface);
		}

		// TTS input field
		CLAY(CLAY_ID("TTSInputSlot"),
		     {.layout = {.sizing = {CLAY_SIZING_GROW(0),
		                            CLAY_SIZING_FIXED(input_height)}}}) {
			auto tts_input_result =
				  mobile_text_input(ctx, CLAY_ID("TTSInput"), &ctx->tts_input,
			                        "Enter text to speak..."_v);
		}

		// TTS play button
		CLAY(CLAY_ID("TTSSectionButtonSlot"),
		     {.layout = {.sizing = {CLAY_SIZING_GROW(0),
		                            CLAY_SIZING_FIXED(btn_height)}}}) {
			auto tts_btn =
				  mobile_button(ctx, CLAY_ID("TTSPlayButton"), "Play"_v);
			if (tts_btn.activated() && ctx->tts_input.size > 0) {
				run_tts(ctx, ctx->tts_input.view());
				// worker_job_push(ctx, Job{.type = Job::Type::TTS,
				//                   .tts_text = ctx->tts_input.view()});
			}
		}

		// Section dividers
		CLAY(CLAY_ID("Divider1"),
		     {.layout = {
					.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(1.0f)}}});

		// ASR header
		CLAY(CLAY_ID("ASRSectionHeader"),
		     {.layout = {.sizing = {CLAY_SIZING_GROW(0),
		                            CLAY_SIZING_FIXED(header_height)}}}) {
			draw_text("ASR - Speech Recognition"_v, theme()->onSurface);
		}

		bool is_asr_initialized = ctx->audio_asr_tts_status.is_asr_initialized;
		// = UIAudioContext::TRUE ==
		// SDL_GetAtomicInt(&ctx->sound_ctx->is_initialized);
		if (is_asr_initialized) {
			// ASR record button
			CLAY(CLAY_ID("ASRSectionButtonSlot"),
			     {.layout = {
						.sizing = {CLAY_SIZING_GROW(0),
			                       CLAY_SIZING_FIXED(btn_height)},
						.padding = CLAY_PADDING_ALL(udpi(16.0f)),
						.childGap = udpi(16.0f),
						.childAlignment = {CLAY_ALIGN_X_CENTER,
			                               CLAY_ALIGN_Y_CENTER},
				  }}) {
				auto btn_style = mobile_button_style_surface_container_high();
				bool is_rec = ctx->audio_asr_tts_status.is_recording;

				// = UIAudioContext::TRUE ==
				//              SDL_GetAtomicInt(&ctx->sound_ctx->is_recording);
				if (ctx->audio_asr_tts_status.is_recording_button_pressed && is_rec) {
					btn_style.background = theme()->primary;
					btn_style.background_pressed = theme()->primary;
				}
				// NOTE: touching other thread controlled memory!!
				// ***********************************************
				if (ctx->audio->rec_audio_buffer.size_bytes >
				    0 /* NOTE: here */) {
					auto push_text_to_tts_field_btn =
						  mobile_icon_button(ctx, CLAY_ID("PushTextToTTSField"),
					                         Icons::CHEVRON_UP);
					if (push_text_to_tts_field_btn.activated()) {
						memcpy(ctx->tts_input.data, ctx->asr_result.data,
						       SDL_min(ctx->asr_result.size,
						               ctx->tts_input.max_size));
						ctx->tts_input.size = ctx->asr_result.size;
						ctx->tts_input.data[ctx->asr_result.size] = '\0';
					}
				}
				// NOTE: touching other thread controlled memory!!
				// ***********************************************
				auto asr_btn = mobile_button(ctx, CLAY_ID("ASRRecordButton"),
				                             "● Record"_v, btn_style);
				// static bool recording = false;
				// SDL_Log(" ======================+>>>> %s %s <<",
				//         asr_btn.held ? "HELD" : "", recording ? "REC" : "");
				if (asr_btn.held) {
					if (!ctx->audio_asr_tts_status.is_recording_button_pressed &&
					    !is_rec) {
						record_start(ctx);
						// NOTE: switching back handled in ui_event FINGER_UP
						ctx->audio_asr_tts_status.is_recording_button_pressed = true;
						// recording = !recording;
					}
				} else { // NOTE: handled in ui_event FINGER_UP
					     // if (recording) {
					     // 	record_stop(ctx);
					     // 	run_asr(ctx);
					     // 	record_deinit(ctx);
					     // 	// recording = !recording;
					     // }
				}
				// NOTE: touching other thread controlled memory!!
				// ***********************************************
				if (ctx->audio->rec_audio_buffer.size_bytes >
				    0 /* NOTE: here */) {
					auto play_btn = mobile_icon_button(
						  ctx, CLAY_ID("ASRPlayButton"), Icons::PLAY);
					if (play_btn.activated()) {
						record_play(ctx);
					}
				}
				// NOTE: touching other thread controlled memory!!
				// ***********************************************
			}

			// ASR result field (read-only)
			CLAY(CLAY_ID("ASRResultSlot"),
			     {.layout = {
						.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(1)},
						.padding = CLAY_PADDING_ALL(udpi(8.0f))}}) {
				auto rec_start_ticks = ctx->audio_asr_tts_status.recording_start_ticks_ms;
				if (ctx->audio_asr_tts_status.is_recording_button_pressed &&
				    rec_start_ticks > 0) {
					draw_text(StrView::from_number(
									ctx->arena_frame,
									UI_Audio_ASR_TTS::ticks_diff_to_seconds(
										  rec_start_ticks, ctx->ticks)),
					          theme()->onSurfaceContainer, udpi(16));
					ctx->anim();
				} else if (ctx->asr_result.size > 0) {
					draw_text(ctx->asr_result, theme()->onSurfaceContainer,
					          udpi(16));
				} else if (ctx->audio_asr_tts_status.is_asr_in_progress

				           // UIAudioContext::TRUE ==
				           //          SDL_GetAtomicInt(
				           // 		 &ctx->sound_ctx->is_asr_in_progress)
				) {
					draw_text("Performing transcription…"_v,
					          theme()->onSurfaceContainer, udpi(16));
				} else {
					draw_text("No transcription yet."_v,
					          theme()->onSurfaceContainer, udpi(16));
				}
			}

			// ASR clear button when there's a result
			// if (ctx->asr_result.size > 0) {
			// 	CLAY(CLAY_ID("ASRClearButtonSlot"),
			// 	     {.layout = {.sizing = {CLAY_SIZING_GROW(0),
			// 	                            CLAY_SIZING_FIXED(btn_height)}}})
			// { 		auto clear_btn = 			  mobile_button(ctx,
			// CLAY_ID("ASRClearButton"), "Clear"_v); 		if
			// (clear_btn.activated()) { ctx->asr_result = {};
			// ctx->push_one_frame();
			// 		}
			// 	}
			// }
		} else {
			CLAY(CLAY_ID("ASRNotInitialized"),
			     {.layout = {
						.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(1)},
						.padding = CLAY_PADDING_ALL(udpi(8.0f))}}) {
				draw_text("Initializing ASR Engine"_v,
				          theme()->onSurfaceContainer, udpi(48));
			}
		}
		// Section dividers
		CLAY(CLAY_ID("Divider2"),
		     {.layout = {
					.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(1.0f)}}});
	}
}
