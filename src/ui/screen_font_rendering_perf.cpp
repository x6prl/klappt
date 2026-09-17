#include "app/app_context.h"
#include "base/profiler.h"
#include "base/str_builder.h"
#include "base/str_view.h"

#include "screen_helpers.h"

#include "ui/components/button.h"

namespace {

enum class PerfPhase : uint8_t {
	ColdCache_UniqueStrings = 0,
	HotCache_DictionaryList = 1,
	DynamicWrapping_Paragraphs = 2,
	MixedFonts_Multilingual = 3,
	Complete = 4,
};

constexpr uint32_t FRAMES_PER_PHASE = 180;

struct PerfState {
	bool is_running{false};
	PerfPhase phase{PerfPhase::ColdCache_UniqueStrings};
	uint32_t frame_in_phase{0};
	uint32_t total_frames{0};
};

static PerfState g_perf_state{};

static StrView phase_name(PerfPhase phase) {
	switch (phase) {
	case PerfPhase::ColdCache_UniqueStrings:
		return "0: Cold Cache (Cache Miss / New Strings)"_v;
	case PerfPhase::HotCache_DictionaryList:
		return "1: Hot Cache (List Rows / Cache Hits)"_v;
	case PerfPhase::DynamicWrapping_Paragraphs:
		return "2: Text Measure & Multi-line Wrapping"_v;
	case PerfPhase::MixedFonts_Multilingual:
		return "3: Mixed Fonts & Unicode Scripts"_v;
	case PerfPhase::Complete:
		return "Completed (Idle)"_v;
	}
	return "Unknown"_v;
}

static void draw_phase_cold_cache(AppContext *ctx, uint32_t frame_idx) {
	KLAPPT_PROFILE_SCOPE_N("FontPerf::ColdCache");

	constexpr Arr<StrView, 8> BASE_WORDS = {
		  "lesen"_v,       "schreiben"_v,  "verstehen"_v,  "sprechen"_v,
		  "wiederholen"_v, "entwickeln"_v, "übersetzen"_v, "beobachten"_v,
	};

	CLAY(CLAY_ID("ColdCacheContainer"),
	     {
			   .layout =
					 {
						   .sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0)},
						   .childGap = sizes()->space.xs,
						   .layoutDirection = CLAY_TOP_TO_BOTTOM,
					 },
		 }) {
		for (Size i = 0; i < 10; ++i) {
			StrBuilder lemma_builder{};
			lemma_builder.push(ctx->arena_frame,
			                   BASE_WORDS[(frame_idx + i) % BASE_WORDS.size()]);
			lemma_builder.push(ctx->arena_frame, "_#f"_v);
			lemma_builder.push(ctx->arena_frame, frame_idx);
			lemma_builder.push(ctx->arena_frame, "_i"_v);
			lemma_builder.push(ctx->arena_frame, i);
			StrView dynamic_lemma = lemma_builder.join(ctx->arena_frame);

			StrBuilder trans_builder{};
			trans_builder.push(ctx->arena_frame, "dynamic trans entry #"_v);
			trans_builder.push(ctx->arena_frame, (frame_idx * 10) + i);
			trans_builder.push(ctx->arena_frame,
			                   " (forcing textcache alloc)"_v);
			StrView dynamic_trans = trans_builder.join(ctx->arena_frame);

			CLAY(CLAY_IDI("ColdRow", i),
			     {
					   .layout =
							 {
								   .sizing =
										 {CLAY_SIZING_GROW(0),
			                              CLAY_SIZING_FIXED(
												sizes()->dim.word_card_height)},
								   .padding = sizes()->pad.list_item,
								   .childGap = sizes()->space.sm,
								   .childAlignment = {CLAY_ALIGN_X_LEFT,
			                                          CLAY_ALIGN_Y_CENTER},
								   .layoutDirection = CLAY_LEFT_TO_RIGHT,
							 },
					   .backgroundColor = theme()->surfaceContainer,
					   .cornerRadius = sizes()->radius.sm,
				 }) {
				draw_text(dynamic_lemma, theme()->onSurface,
				          sizes()->font.body_md, FontID::MAIN,
				          CLAY_TEXT_WRAP_NONE, CLAY_TEXT_ALIGN_LEFT);

				CLAY(CLAY_IDI("ColSpacer", i),
				     {.layout = {.sizing = {CLAY_SIZING_GROW(0),
				                            CLAY_SIZING_GROW(0)}}}) {}

				draw_text(dynamic_trans, theme()->onSurfaceContainer,
				          sizes()->font.body_sm, FontID::MAIN,
				          CLAY_TEXT_WRAP_NONE, CLAY_TEXT_ALIGN_RIGHT);
			}
		}
	}
}

static void draw_phase_hot_cache(AppContext *ctx) {
	KLAPPT_PROFILE_SCOPE_N("FontPerf::HotCache");

	struct DictItem {
		StrView article;
		StrView lemma;
		StrView suffix;
		StrView trans;
	};

	constexpr Arr<DictItem, 10> STATIC_ROWS = {{
		  {"das"_v, "Buch"_v, "\"-er"_v, "book, volume"_v},
		  {"die"_v, "Zeit"_v, "-en"_v, "time, era, period"_v},
		  {"das"_v, "Jahr"_v, "-e"_v, "year, annual period"_v},
		  {"der"_v, "Mensch"_v, "-en"_v, "human, person, man"_v},
		  {""_v, "können"_v, "!*"_v, "to be able to, can"_v},
		  {""_v, "fahren"_v, "!*"_v, "to drive, ride, travel"_v},
		  {""_v, "machen"_v, "ᵛ"_v, "to make, to do, produce"_v},
		  {""_v, "groß"_v, "ᵃ"_v, "large, great, tall"_v},
		  {"die"_v, "Hand"_v, "\"-e"_v, "hand, manual control"_v},
		  {"das"_v, "Wasser"_v, "(sg.)"_v, "water, liquid, aqua"_v},
	}};

	CLAY(CLAY_ID("HotCacheContainer"),
	     {
			   .layout =
					 {
						   .sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0)},
						   .childGap = sizes()->space.xs,
						   .layoutDirection = CLAY_TOP_TO_BOTTOM,
					 },
		 }) {
		for (Size i = 0; i < STATIC_ROWS.size(); ++i) {
			const auto &item = STATIC_ROWS[i];

			CLAY(CLAY_IDI("HotRow", i),
			     {
					   .layout =
							 {
								   .sizing =
										 {CLAY_SIZING_GROW(0),
			                              CLAY_SIZING_FIXED(
												sizes()->dim.word_card_height)},
								   .padding = sizes()->pad.list_item,
								   .childGap = sizes()->space.xs,
								   .childAlignment = {CLAY_ALIGN_X_LEFT,
			                                          CLAY_ALIGN_Y_CENTER},
								   .layoutDirection = CLAY_LEFT_TO_RIGHT,
							 },
					   .backgroundColor = theme()->surfaceContainer,
					   .cornerRadius = sizes()->radius.sm,
				 }) {
				if (item.article) {
					auto pcolor = theme()->onSurfaceContainer;
					pcolor.a = static_cast<uint8_t>(pcolor.a * 0.5f);
					draw_text(item.article, pcolor, sizes()->font.body_md,
					          FontID::MAIN);
				}

				draw_text(item.lemma, theme()->onSurface, sizes()->font.body_md,
				          FontID::MAIN);

				if (item.suffix) {
					auto pcolor = theme()->onSurfaceContainer;
					pcolor.a = static_cast<uint8_t>(pcolor.a * 0.5f);
					draw_text(item.suffix, pcolor, sizes()->font.body_md,
					          FontID::MAIN);
				}

				CLAY(CLAY_IDI("HotSpacer", i),
				     {.layout = {.sizing = {CLAY_SIZING_GROW(0),
				                            CLAY_SIZING_GROW(0)}}}) {}

				draw_text(item.trans, theme()->onSurfaceContainer,
				          sizes()->font.body_sm, FontID::MAIN,
				          CLAY_TEXT_WRAP_NONE, CLAY_TEXT_ALIGN_RIGHT);
			}
		}
	}
}

static void draw_phase_wrapping(AppContext *ctx, uint32_t frame_idx) {
	KLAPPT_PROFILE_SCOPE_N("FontPerf::WrappingStress");

	constexpr Arr<StrView, 4> EXAMPLES_DE = {
		  "Bevor ich schlafe, muss ich immer ein paar Seiten aus einem guten Buch lesen."_v,
		  "Sein oder Nichtsein, das ist hier die Frage: Ob es edler im Gemüt, die Pfeile und Schleudern des wütenden Geschicks zu erdulden."_v,
		  "Wir lesen den Wein mit vielen Helfern jedes Jahr im sonnigen Herbst an den steilen Hängen entlang des Rheins."_v,
		  "Aschenputtel musste die Linsen aus der Asche lesen, bevor sie zum königlichen Schlossball gehen durfte."_v,
	};

	constexpr Arr<StrView, 4> EXAMPLES_RU = {
		  "Прежде чем уснуть, я обязательно должен прочитать пару страниц из какой-нибудь хорошей книги."_v,
		  "Быть или не быть, вот в чем вопрос: благороднее ли молча терпеть удары жестокой судьбы."_v,
		  "Мы собираем виноград вместе со множеством помощников каждый год теплой солнечной осенью на крутых склонах Рейна."_v,
		  "Золушка должна была перебрать всю чечевицу из золы, прежде чем отправиться на королевский праздничный бал."_v,
	};

	const float width_mod =
		  std::sin(static_cast<float>(frame_idx) * 0.1f) * 20.0f;

	CLAY(CLAY_ID("WrapStressContainer"),
	     {
			   .layout =
					 {
						   .sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0)},
						   .padding =
								 {
									   static_cast<uint16_t>(sizes()->space.md +
	                                                         width_mod),
									   static_cast<uint16_t>(sizes()->space.md +
	                                                         width_mod),
									   sizes()->space.xs,
									   sizes()->space.xs,
								 },
						   .childGap = sizes()->space.sm,
						   .layoutDirection = CLAY_TOP_TO_BOTTOM,
					 },
		 }) {
		for (Size i = 0; i < EXAMPLES_DE.size(); ++i) {
			CLAY(CLAY_IDI("ExampleWrapCard", i),
			     {
					   .layout =
							 {
								   .sizing = {CLAY_SIZING_GROW(0),
			                                  CLAY_SIZING_FIT(0)},
								   .padding = sizes()->pad.example_quote,
								   .childGap = sizes()->space.xs,
								   .layoutDirection = CLAY_TOP_TO_BOTTOM,
							 },
					   .backgroundColor = theme()->surfaceContainer,
					   .cornerRadius = sizes()->radius.sm,
					   .border =
							 {
								   .color = theme()->secondary,
								   .width = {.left = static_cast<uint16_t>(
												   sizes()
														 ->dim
														 .accent_border_width)},
							 },
				 }) {
				draw_text(EXAMPLES_DE[i], theme()->onSurface,
				          sizes()->font.body_md, FontID::MAIN,
				          CLAY_TEXT_WRAP_WORDS, CLAY_TEXT_ALIGN_LEFT);

				draw_text(EXAMPLES_RU[i], theme()->onSurfaceContainer,
				          sizes()->font.body_sm, FontID::MAIN,
				          CLAY_TEXT_WRAP_WORDS, CLAY_TEXT_ALIGN_LEFT);
			}
		}
	}
}

static void draw_phase_mixed_fonts(AppContext *ctx) {
	KLAPPT_PROFILE_SCOPE_N("FontPerf::MixedFonts");

	CLAY(CLAY_ID("MixedFontsContainer"),
	     {
			   .layout =
					 {
						   .sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0)},
						   .padding = sizes()->pad.card,
						   .childGap = sizes()->space.sm,
						   .childAlignment = {CLAY_ALIGN_X_LEFT,
	                                          CLAY_ALIGN_Y_TOP},
						   .layoutDirection = CLAY_TOP_TO_BOTTOM,
					 },
			   .backgroundColor = theme()->surfaceContainerLow,
			   .cornerRadius = sizes()->radius.lg,
		 }) {

		CLAY(CLAY_ID("HeaderRow"),
		     {
				   .layout =
						 {
							   .sizing = {CLAY_SIZING_GROW(0),
		                                  CLAY_SIZING_FIT(0)},
							   .childGap = sizes()->space.sm,
							   .childAlignment = {CLAY_ALIGN_X_LEFT,
		                                          CLAY_ALIGN_Y_CENTER},
							   .layoutDirection = CLAY_LEFT_TO_RIGHT,
						 },
			 }) {
			draw_text(Icons::PLAY, theme()->primary, sizes()->font.title_lg,
			          FontID::ICONS);
			draw_text("ausgezeichnet"_v, theme()->onSurface,
			          sizes()->font.title_lg, FontID::MAIN);
			draw_text("✓"_v, theme()->onRightContainer, sizes()->font.title_md,
			          FontID::MAIN);
		}

		draw_text("[ˈaʊ̯sɡəˌt͡saɪ̯çnət]"_v, theme()->onSurfaceContainer,
		          sizes()->font.label_md, FontID::MAIN);

		CLAY(CLAY_ID("MonoCard"),
		     {
				   .layout =
						 {
							   .sizing = {CLAY_SIZING_GROW(0),
		                                  CLAY_SIZING_FIT(0)},
							   .padding = sizes()->pad.card_compact,
							   .childGap = sizes()->space.xs,
							   .layoutDirection = CLAY_TOP_TO_BOTTOM,
						 },
				   .backgroundColor = theme()->surfaceContainer,
				   .cornerRadius = sizes()->radius.sm,
			 }) {
			draw_text("er/sie/es: zeichnet aus"_v, theme()->onSurface,
			          sizes()->font.body_sm, FontID::MONOSPACE_REGULAR,
			          CLAY_TEXT_WRAP_NONE, CLAY_TEXT_ALIGN_LEFT);
			draw_text("Präteritum: zeichnete aus"_v, theme()->onSurface,
			          sizes()->font.body_sm, FontID::MONOSPACE_REGULAR,
			          CLAY_TEXT_WRAP_NONE, CLAY_TEXT_ALIGN_LEFT);
			draw_text("Perfekt: hat ausgezeichnet"_v, theme()->onSurface,
			          sizes()->font.body_sm, FontID::MONOSPACE_REGULAR,
			          CLAY_TEXT_WRAP_NONE, CLAY_TEXT_ALIGN_LEFT);
		}

		CLAY(CLAY_ID("ArabicRow"),
		     {
				   .layout =
						 {
							   .sizing = {CLAY_SIZING_GROW(0),
		                                  CLAY_SIZING_FIT(0)},
							   .childAlignment = {CLAY_ALIGN_X_RIGHT,
		                                          CLAY_ALIGN_Y_CENTER},
						 },
			 }) {
			draw_text("ممتاز، رائع، متفوق (عربي)"_v, theme()->secondary,
			          sizes()->font.body_md, FontID::ARABIC_MAIN,
			          CLAY_TEXT_WRAP_NONE, CLAY_TEXT_ALIGN_RIGHT);
		}

		CLAY(CLAY_ID("HeroCounterRow"),
		     {
				   .layout =
						 {
							   .sizing = {CLAY_SIZING_GROW(0),
		                                  CLAY_SIZING_FIT(0)},
							   .childAlignment = {CLAY_ALIGN_X_CENTER,
		                                          CLAY_ALIGN_Y_CENTER},
						 },
			 }) {
			draw_text("100%"_v, theme()->primary, sizes()->font.display,
			          FontID::MAIN);
		}
	}
}

} // namespace

void screen_font_rendering_perf_go(AppContext *ctx) {
	g_perf_state = {
		  .is_running = true,
		  .phase = PerfPhase::ColdCache_UniqueStrings,
		  .frame_in_phase = 0,
		  .total_frames = 0,
	};
	ctx->go(Screen::FontPerf);
	ctx->push_one_frame();
}

void screen_font_rendering_perf_draw(AppContext *ctx) {
	KLAPPT_PROFILE_SCOPE_N("screen_font_rendering_perf_draw");

	KLAPPT_PROFILE_FRAME_N("FontBenchmarkFrame");

	auto &state = g_perf_state;

	if (state.is_running && state.phase != PerfPhase::Complete) {
		KLAPPT_PROFILE_NAME_F("FontPerf_P%u_F%u",
		                      static_cast<unsigned>(state.phase),
		                      static_cast<unsigned>(state.frame_in_phase));

		++state.frame_in_phase;
		++state.total_frames;

		if (state.frame_in_phase >= FRAMES_PER_PHASE) {
			state.frame_in_phase = 0;
			const auto next_phase_idx = static_cast<uint8_t>(state.phase) + 1;
			state.phase = static_cast<PerfPhase>(next_phase_idx);

			if (state.phase == PerfPhase::Complete) {
				state.is_running = false;
				SDL_Log(
					  "Font benchmark completed successfully. Total frames: %u",
					  state.total_frames);
			}
		}

		if (state.is_running) {
			ctx->push_one_frame();
		}
	}

	CLAY(CLAY_ID("PerfScreenRoot"),
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

		CLAY(CLAY_ID("ControlPanel"),
		     {
				   .layout =
						 {
							   .sizing = {CLAY_SIZING_GROW(0),
		                                  CLAY_SIZING_FIT(0)},
							   .padding = sizes()->pad.card_compact,
							   .childGap = sizes()->space.xs,
							   .childAlignment = {CLAY_ALIGN_X_LEFT,
		                                          CLAY_ALIGN_Y_CENTER},
							   .layoutDirection = CLAY_TOP_TO_BOTTOM,
						 },
				   .backgroundColor = theme()->surfaceContainerHigh,
				   .cornerRadius = sizes()->radius.md,
			 }) {

			draw_text(phase_name(state.phase), theme()->primary,
			          sizes()->font.body_md, FontID::MAIN, CLAY_TEXT_WRAP_NONE,
			          CLAY_TEXT_ALIGN_LEFT);

			const float progress =
				  (state.phase == PerfPhase::Complete)
						? 1.0f
						: (static_cast<float>(state.frame_in_phase) /
			               static_cast<float>(FRAMES_PER_PHASE));

			CLAY(CLAY_ID("PhaseProgressTrack"),
			     {
					   .layout =
							 {
								   .sizing = {CLAY_SIZING_GROW(0),
			                                  CLAY_SIZING_FIXED(
													static_cast<float>(
														  sizes()->space.xs))},
								   .layoutDirection = CLAY_LEFT_TO_RIGHT,
							 },
					   .backgroundColor = theme()->surfaceContainer,
					   .cornerRadius = sizes()->radius.xs,
				 }) {
				CLAY(CLAY_ID("PhaseProgressFill"),
				     {
						   .layout =
								 {
									   .sizing = {CLAY_SIZING_PERCENT(progress),
				                                  CLAY_SIZING_GROW(0)},
								 },
						   .backgroundColor = theme()->secondary,
						   .cornerRadius = sizes()->radius.xs,
					 }) {}
			}

			CLAY(CLAY_ID("ButtonsRow"),
			     {
					   .layout =
							 {
								   .sizing = {CLAY_SIZING_GROW(0),
			                                  CLAY_SIZING_FIT(0)},
								   .childGap = sizes()->space.sm,
								   .childAlignment = {CLAY_ALIGN_X_LEFT,
			                                          CLAY_ALIGN_Y_CENTER},
								   .layoutDirection = CLAY_LEFT_TO_RIGHT,
							 },
				 }) {
				auto btn_style = mobile_button_style_primary();
				btn_style.height = sizes()->dim.action_btn_size;
				btn_style.font_size = sizes()->font.body_sm;

				if (state.is_running) {
					auto pause_btn = mobile_button(ctx, CLAY_ID("PauseBtn"),
					                               "Pause"_v, btn_style);
					if (pause_btn.activated()) {
						state.is_running = false;
						ctx->push_one_frame();
					}
				} else {
					auto start_btn = mobile_button(
						  ctx, CLAY_ID("StartBtn"),
						  (state.phase == PerfPhase::Complete) ? "Restart"_v
															   : "Resume"_v,
						  btn_style);
					if (start_btn.activated()) {
						if (state.phase == PerfPhase::Complete) {
							state.phase = PerfPhase::ColdCache_UniqueStrings;
							state.frame_in_phase = 0;
							state.total_frames = 0;
						}
						state.is_running = true;
						ctx->push_one_frame();
					}
				}
			}
		}

		CLAY(CLAY_ID("BenchViewport"),
		     {
				   .layout =
						 {
							   .sizing = {CLAY_SIZING_GROW(0),
		                                  CLAY_SIZING_GROW(0)},
							   .childAlignment = {CLAY_ALIGN_X_CENTER,
		                                          CLAY_ALIGN_Y_TOP},
							   .layoutDirection = CLAY_TOP_TO_BOTTOM,
						 },
			 }) {
			switch (state.phase) {
			case PerfPhase::ColdCache_UniqueStrings:
				draw_phase_cold_cache(ctx, state.total_frames);
				break;
			case PerfPhase::HotCache_DictionaryList:
				draw_phase_hot_cache(ctx);
				break;
			case PerfPhase::DynamicWrapping_Paragraphs:
				draw_phase_wrapping(ctx, state.total_frames);
				break;
			case PerfPhase::MixedFonts_Multilingual:
				draw_phase_mixed_fonts(ctx);
				break;
			case PerfPhase::Complete:
				draw_phase_hot_cache(ctx);
				break;
			}
		}
	}
}
