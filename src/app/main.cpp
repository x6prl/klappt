#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>

#define SDL_MAIN_USE_CALLBACKS // This is necessary for the new callbacks API.
                               // To use the legacy API, don't define this.
#include <SDL3/SDL.h>
#include <SDL3/SDL_events.h>
#include <SDL3/SDL_init.h>
#include <SDL3/SDL_log.h>
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_stdinc.h>
#include <SDL3/SDL_thread.h>
#include <SDL3_ttf/SDL_ttf.h>

#include "app/app_context.h"
#include "app/event_codes.h"
#include "app/net_context.h"
#include "app/sizes.h"
#include "app/textcache.h"
#include "app/words_init.h"
#include "app/worker.h"
#include "base/dyn_arr.h"
#include "base/measure.h"
#include "base/profiler.h"
#include "base/stats.h"
#include "base/str_view.h"
#include "domain/settings.h"
#include "platform/files.h"
#include "platform/fs.h"
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#else
// NOTE: !__EMSCRIPTEN__

#include "platform/net_worker.h"
#endif // !__EMSCRIPTEN__
#include "ui/entry.h"

#if HOTRELOAD
#include "app/hotreload.h"
#endif

#if NEURO
#include "platform/neuro.h"
#endif

constexpr uint32_t WINDOW_START_WIDTH = 1200 / 3;
constexpr uint32_t WINDOW_START_HEIGHT = 2670 / 3;

extern thread_local ThreadContext *_tctx;

#if defined(TRACY_ENABLE)
static const char *EventTypeName(Uint32 type) {
	switch (type) {
	case SDL_EVENT_QUIT:
		return "SDL_EVENT_QUIT";
	case SDL_EVENT_WINDOW_RESIZED:
		return "SDL_EVENT_WINDOW_RESIZED";
	case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
		return "SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED";
	case SDL_EVENT_WINDOW_FOCUS_GAINED:
		return "SDL_EVENT_WINDOW_FOCUS_GAINED";
	case SDL_EVENT_WINDOW_FOCUS_LOST:
		return "SDL_EVENT_WINDOW_FOCUS_LOST";
	case SDL_EVENT_KEY_DOWN:
		return "SDL_EVENT_KEY_DOWN";
	case SDL_EVENT_TEXT_EDITING:
		return "SDL_EVENT_TEXT_EDITING";
	case SDL_EVENT_TEXT_INPUT:
		return "SDL_EVENT_TEXT_INPUT";
	case SDL_EVENT_MOUSE_MOTION:
		return "SDL_EVENT_MOUSE_MOTION";
	case SDL_EVENT_MOUSE_BUTTON_DOWN:
		return "SDL_EVENT_MOUSE_BUTTON_DOWN";
	case SDL_EVENT_MOUSE_BUTTON_UP:
		return "SDL_EVENT_MOUSE_BUTTON_UP";
	case SDL_EVENT_MOUSE_WHEEL:
		return "SDL_EVENT_MOUSE_WHEEL";
	case SDL_EVENT_FINGER_DOWN:
		return "SDL_EVENT_FINGER_DOWN";
	case SDL_EVENT_FINGER_UP:
		return "SDL_EVENT_FINGER_UP";
	case SDL_EVENT_FINGER_MOTION:
		return "SDL_EVENT_FINGER_MOTION";
	case SDL_EVENT_FINGER_CANCELED:
		return "SDL_EVENT_FINGER_CANCELED";
	case SDL_EVENT_USER:
		return "SDL_EVENT_USER";
	default:
		return "SDL_EVENT_UNKNOWN";
	}
}
#endif

#if defined(TRACY_ENABLE)
static const char *FrameName(Screen screen) {
	switch (screen) {
	case Screen::Trainer:
		return "Frame/Start";
	case Screen::Exercice:
		return "Frame/Exercise";
	case Screen::ExerciceResultSummary:
		return "Frame/ExerciseSummary";
	case Screen::ExerciseReview:
		return "Frame/ExerciseReview";
	case Screen::Dictionary:
		return "Frame/WordsList";
	case Screen::LearningList:
		return "Frame/LearningList";
	case Screen::WordSuggestions:
		return "Frame/WordSuggestions";
	case Screen::Settings:
		return "Frame/Settings";
	case Screen::WordView:
		return "Frame/WordView";
	case Screen::WordEdit:
		return "Frame/WordEdit";
	case Screen::Onboarding:
		return "Frame/Onboarding";
	case Screen::TTS_ASR:
		return "Frame/TTS_ASR";
	}
	return "Frame/Unknown";
}
#endif

#ifdef __EMSCRIPTEN__
/*
 * JavaScript bridge to trap browser navigation
 */
EM_JS(void, init_browser_back_handler, (), {
	history.pushState({page : 'sdl_app'}, '', '');

	window.addEventListener(
		  'popstate', function(event) {
			  history.pushState({page : 'sdl_app'}, '', '');
			  _on_browser_back_pressed();
		  });
});

extern "C" {
EMSCRIPTEN_KEEPALIVE
void on_browser_back_pressed() {
	SDL_Event event;
	SDL_zero(event);
	event.type = SDL_EVENT_USER;
	event.user.code = WEB_AC_BACK;
	SDL_PushEvent(&event);
}
}
#endif // __EMSCRIPTEN__

#if defined(TRACY_ENABLE)
static void WaitForProfilerConnection() {
	SDL_Log("Waiting for Tracy profiler connection on port 8086...");
	while (!KLAPPT_PROFILE_CONNECTED()) {
		SDL_Delay(100);
	}
	SDL_Log("Tracy profiler connected.");
}
#endif

static void load_fonts_job() {
	auto *ctx = tctx()->app_ctx;
	auto base_pathv = get_app_base_path();
	auto base_path =
		  std::filesystem::path({base_pathv.data, (size_t)base_pathv.size});

	const auto ui_path = (base_path / "Inter-Regular.ttf").string();
	const auto arabic_path = (base_path / "ReadexPro-Regular.ttf").string();
	const auto icons_path =
		  (base_path / "Font-Awesome-7-Free-Solid-900.otf").string();

	ctx->text->base_fonts[FontID::MAIN] =
		  TTF_OpenFont(ui_path.c_str(), sizes()->font.body_md);
	ctx->text->base_fonts[FontID::ARABIC_MAIN] =
		  TTF_OpenFont(arabic_path.c_str(), sizes()->font.body_sm);
	ctx->text->base_fonts[FontID::ICONS] =
		  TTF_OpenFont(icons_path.c_str(), sizes()->dim.icon_sm);

	// unblock UI init
	SDL_SignalSemaphore(ctx->fonts_ready_sem);
	SDL_Log("Thread Worker: base fonts loaded");

	// deferred fonts
	const auto mono_reg_path =
		  (base_path / "JetBrainsMono-Regular.ttf").string();
	const auto mono_bold_path = (base_path / "JetBrainsMono-Bold.ttf").string();

	ctx->text->base_fonts[FontID::MONOSPACE_REGULAR] =
		  TTF_OpenFont(mono_reg_path.c_str(), 48);
	ctx->text->base_fonts[FontID::MONOSPACE_BOLD] =
		  TTF_OpenFont(mono_bold_path.c_str(), 48);
	SDL_Log("Thread Worker: deffered fonts loaded");
}

SDL_Renderer *create_renderer(SDL_Window *window) {
	SDL_Renderer *renderer{};
#ifdef __ANDROID__
	renderer = SDL_CreateRenderer(window, "opengles2");
	if (renderer) {
		SDL_Log("opengles2 renderer created");
		return renderer;
	} else {
		SDL_LogError(SDL_LOG_CATEGORY_ERROR,
		             "Failed to create opengles2 renderer");
	}
#endif

	Measure m{"create_renderer"};
	const int num_drivers = SDL_GetNumRenderDrivers();
	SDL_Log("Found %d renderers", num_drivers);
	for (int i = 0; i < num_drivers; ++i) {
		const char *driver = SDL_GetRenderDriver(i);
		SDL_Log("\t%d: [%s]", i, driver);
	}
	m.lap().printus("enum_and_log_drivers");

	constexpr const char *preferred_drivers[] = {
		  "gpu", "vulkan", "opengl", "opengles2", "software",
	};

	constexpr int num_preferred =
		  sizeof(preferred_drivers) / sizeof(preferred_drivers[0]);

	for (int i = 0; i < num_preferred; ++i) {
		const char *driver = preferred_drivers[i];

		SDL_Log("Trying renderer: [%s]", driver);

		renderer = SDL_CreateRenderer(window, driver);
		m.lap().printus(driver);

		if (renderer) {
			SDL_Log("Successfully created renderer: [%s]",
			        SDL_GetRendererName(renderer));
			m.total().printus("total");
			return renderer;
		}

		SDL_Log("Failed to create renderer [%s]: %s", driver, SDL_GetError());
	}

	SDL_Log("All preferred renderers failed; trying SDL default");

	renderer = SDL_CreateRenderer(window, nullptr);
	m.lap().printus("fallback_default_renderer");

	if (renderer) {
		SDL_Log("Created default renderer: [%s]",
		        SDL_GetRendererName(renderer));
	}

	m.total().printus("total");
	return renderer;
}

static SDL_AppResult SDL_Fail() {
	SDL_LogError(SDL_LOG_CATEGORY_CUSTOM, "Error %s", SDL_GetError());
	return SDL_APP_FAILURE;
}

// Simple timer callback to keep the UI "alive"
static Uint32 SDLCALL WakeUpTimer(void *userdata, SDL_TimerID timerID,
                                  Uint32 interval) {
	(void)userdata;
	(void)timerID;
	SDL_Event event{};
	event.type = SDL_EVENT_USER;
	event.user.code = HOTRELOAD_EVENT_CODE;
	SDL_PushEvent(&event);
	return interval; // Keep running
}

extern "C" SDL_AppResult SDLCALL SDL_AppInit(void **appstate, int argc,
                                             char *argv[]) {
	(void)argc;
	(void)argv;
	KLAPPT_PROFILE_SCOPE_N("SDL_AppInit");
	KLAPPT_PROFILE_THREAD("main");
	Measure m{__FUNCTION__};

	SDL_SetHint(SDL_HINT_ORIENTATIONS, "Portrait");
	// TODO: think about putting it to 0
	// SDL_SetHint(SDL_HINT_ANDROID_BLOCK_ON_PAUSE, "1");
	if (!SDL_Init(SDL_INIT_VIDEO))
		return SDL_Fail();
	m.lap().printus("SDL_Init");
	if (!TTF_Init())
		return SDL_Fail();
	m.lap().printus("TTF_Init");

	auto text_cache = new TextCache{};
	auto ctx = new AppContext{
		  .ticks = SDL_GetTicks(),
		  .text = text_cache,
		  .current = 0,
		  .stack = {Screen::Onboarding},
		  .word_view_state = new WordViewState{},
		  .word_edit_state = new WordEditState{},
		  .fonts_ready_sem = SDL_CreateSemaphore(0),
	};
	ctx->downloads = DynArr<DownloadData>{
		  .data = ctx->arena.pushN<DownloadData>(NetContext::MAX_REQUESTS),
		  .size = 0,
		  .reserved = NetContext::MAX_REQUESTS,
	};
	*appstate = ctx;
	m.lap().printus("AppContext created");

	thread_local ThreadContext tctx_var = {.app_ctx = ctx};
	_tctx = &tctx_var;

	// starting the worker thread
	SDL_Thread *worker = SDL_CreateThread(WorkerThread, "WorkerThread", ctx);
	if (!worker) {
		SDL_LogError(SDL_LOG_CATEGORY_CUSTOM, "SDL_CreateThread failed: %s",
		             SDL_GetError());
	}
	m.lap().printus("worker started");

#ifdef __ANDROID__
	SDL_Window *window = SDL_CreateWindow(
		  "klappt", 0, 0, SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY);
#else
	SDL_Window *window = SDL_CreateWindow(
		  "klappt", WINDOW_START_WIDTH, WINDOW_START_HEIGHT,
		  SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY);
#endif
	if (!window)
		return SDL_Fail();
	m.lap().printus("window created");

	int width, height;
	SDL_ShowWindow(window);
	SDL_GetWindowSize(window, &width, &height);

	ctx->window = window;
	ctx->scale = SDL_GetWindowDisplayScale(window);
	ctx->display_width = static_cast<float>(width);
	m.lap().printus("window values received");

	FileLoader settings_file{};
	auto g = ctx->arena_frame.guard();
	if (settings_file.load_from_writable(ctx->arena_frame, "settings.dat"_v)) {
		if (!Settings::decode(settings_file.data, settings_file.size,
		                      &ctx->settings)) {
			SDL_LogError(SDL_LOG_CATEGORY_ERROR, "Cannot decode settings.dat");
			ctx->app_status.set_exit_with_error(
				  "cannot decode settings file"_v);
		}
	}
	m.lap().printus("settings loaded");

	// set up theme and sizes
	{
		sizes_set_scale(ctx->scale, ctx->settings.density,
		                ctx->settings.font_scale);
		theme_set(ctx->settings.theme_type);
	}
	m.lap().printus("themes and sizes are set");

	Worker::job_push(ctx, Job{
								.id = -10,
								.func = load_fonts_job,
						  });
	m.lap().printus("font job queued");

#ifdef __EMSCRIPTEN__
	SDL_SetWindowFillDocument(window, true);
#endif

	{
		SDL_Renderer *renderer = create_renderer(window);
		if (!renderer) {
			ctx->app_status.set_exit_with_error("cannot create renderer"_v);
			return SDL_Fail();
		}
		// turn off vsync
		SDL_SetRenderVSync(renderer, -1);
		ctx->renderer = renderer;
	}
	m.lap().printus("renderer created");

	text_cache->atlas_init(ctx->renderer);
	m.lap().printus("text atlas init");

#ifdef HOTRELOAD
	auto [healthy, reloaded] = hotreload(HOTRELOAD_MODULE_PATH);
	if (!healthy) {
		SDL_LogError(SDL_LOG_CATEGORY_ERROR, "Failed loading %s",
		             HOTRELOAD_MODULE_PATH);
		ctx->app_status.set_exit_with_error("cannot load hotreaload module"_v);
		return SDL_Fail();
	}
	m.lap().printus("hotreload module loaded");
#endif

	ui_clay_init(ctx);
	ui_settings_init(ctx);

	m.lap().printus("ui clay init");

	// set event based rendering
	SDL_SetHint(SDL_HINT_MAIN_CALLBACK_RATE, "waitevent");
	// set a timer to render 1 fps
	constexpr auto UI_UPDATE_EVENT_TIME_MS = 1000;
	SDL_AddTimer(UI_UPDATE_EVENT_TIME_MS, WakeUpTimer, nullptr);

	if (ctx->settings.onboarding_stage < 0) {
		if (!init_runtime_data(*ctx))
			return SDL_APP_FAILURE;
		ctx->go(static_cast<Screen>(ctx->settings.default_screen));
	}
	m.lap().printus("runtime data initialized");

	// Launch remaining background workers
	auto init_other_workers_job = []() {
		auto ctx = tctx()->app_ctx;
#if NEURO
		SDL_CreateThread(NeuroWorkerThread, "NeuroWorkerThread", ctx);
#endif
		SDL_CreateThread(AudioWorkerThread, "AudioWorkerThread", ctx);
#ifndef __EMSCRIPTEN__
		SDL_CreateThread(NetWorkerThread, "NetWorkerThread", ctx);
#endif
	};
	Worker::job_push(ctx, Job{.id = -2, .func = init_other_workers_job});
	m.lap().printus("backgroun workers run");

	// sync with base font loading
	{
		SDL_Log("Syncing with font loader...");
		Measure mw{"WaitFonts"};
		SDL_WaitSemaphore(ctx->fonts_ready_sem);
		SDL_DestroySemaphore(ctx->fonts_ready_sem);
		ctx->fonts_ready_sem = nullptr;
		mw.lap().printus("Font sync complete");
	}

	// NOTE: too slow :c
	if (false) {
		Measure mw{"TextCache prewarm"};
		auto &a = ctx->arena_frame;
		auto g = a.guard();
		auto s = DynArr<uint16_t>::with(
			  a,
			  //
			  sizes()->font.body_md // takes 834 ms

			  // ,sizes()->font.body_sm
		      // ,sizes()->font.label_md, sizes()->font.label_sm
		      // ,sizes()->font.title_md, sizes()->font.title_lg
		      //
		);
		ctx->text->prewarm(s);
		mw.lap().printms("completed");
	}

	SDL_Log("Application started successfully!");
	m.total().printus("total");

	return SDL_APP_CONTINUE;
}

extern "C" SDL_AppResult SDLCALL SDL_AppEvent(void *appstate,
                                              SDL_Event *event) {
	KLAPPT_PROFILE_SCOPE_N("SDL_AppEvent");
	KLAPPT_PROFILE_NAME_F("SDL_AppEvent:%s", EventTypeName(event->type));
	auto *ctx = (AppContext *)appstate;
	ctx->ticks = SDL_GetTicks();
	{
		char event_type_text[32];
		auto len = SDL_snprintf(event_type_text, sizeof(event_type_text),
		                        "type=%u", event->type);
		if (len > 0) {
			KLAPPT_PROFILE_ZONE_TEXT(event_type_text, static_cast<size_t>(len));
		}
	}
#if HOTRELOAD
	if (SDL_EVENT_USER == event->type &&
	    event->user.code == HOTRELOAD_EVENT_CODE

	) {
		if (ctx->animation_timer_id) {
			SDL_RemoveTimer(ctx->animation_timer_id);
			ctx->animation_timer_id = 0;
		}
		ctx->animate = false;
		auto [healthy, reloaded] = hotreload(HOTRELOAD_MODULE_PATH);
		if (!healthy) {
			SDL_Log("Failed to reload app_hotreload from %s",
			        HOTRELOAD_MODULE_PATH);
			// return SDL_APP_CONTINUE;
		}
		if (reloaded) {
			ui_clay_init(ctx);
			ui_settings_init(ctx);
		}
	}
#endif
	{
		KLAPPT_PROFILE_SCOPE_N("ui_event");
		return ui_event(ctx, event);
	}
}

// void update_ticks_array(uint64_t (*ts)[10], uint64_t t) {
//	for (int i{}; i < 9; ++i) {
//		(*ts)[i] = (*ts)[i + 1];
//	}
//	(*ts)[9] = t;
// }
//
// NOTE: When "waitevent" is set, this callback is only called _after_
// SDL_AppEvent https://wiki.libsdl.org/SDL3/SDL_HINT_MAIN_CALLBACK_RATE
extern "C" SDL_AppResult SDLCALL SDL_AppIterate(void *appstate) {
	KLAPPT_PROFILE_SCOPE_N("SDL_AppIterate");
	static uint64_t last_tick;
	auto tick = SDL_GetTicks();
	auto delta = tick - last_tick;
	Measure m{__FUNCTION__};
	static Stats st{};
	// TODO: research: 4 gives us about 120fps
	if (delta > 4) {
		KLAPPT_PROFILE_NAME_F("SDL_AppIterate:frame delta=%llu ms",
		                      static_cast<unsigned long long>(delta));
		last_tick = tick;
		auto *ctx = (AppContext *)appstate;
		ctx->ticks = tick;
		KLAPPT_PROFILE_FRAME_N(FrameName(ctx->screen()));
		// update_ticks_array(&(ctx->last_ticks), tick);
		SDL_AppResult ret;
		{
			KLAPPT_PROFILE_SCOPE_N("ui_iterate");
			ret = ui_iterate(ctx);
		}
		// update_ticks_array(&(ctx->last_ticksef), SDL_GetTicks());
		m.lap();
		if (m.tlap > uint64_t(st.avg() * 2)) {
			// m.printms();
			// SDL_Log("and average %d us", st.avg() / 1000);
		}
		st.push(static_cast<int>(m.tlap));
		return ret;
	} else {
		KLAPPT_PROFILE_NAME_F("SDL_AppIterate:skip delta=%llu ms",
		                      static_cast<unsigned long long>(delta));
		return SDL_APP_CONTINUE;
	}
}

extern "C" void SDLCALL SDL_AppQuit(void *appstate, SDL_AppResult result) {
	KLAPPT_PROFILE_SCOPE();
	auto *ctx = (AppContext *)appstate;
	Measure m{__PRETTY_FUNCTION__};
	(void)result;
	ctx->word_store.close();
	ctx->states.close();
	m.lap().printus("save states");
	// OS will destroy everything itself on exit

	// if (ctx) {
	//   SDL_DestroyRenderer(ctx->renderer);
	//   SDL_DestroyWindow(ctx->window);
	//
	//   // prevent the music from abruptly ending.
	// MIX_StopTrack(ctx->track, MIX_TrackMSToFrames(ctx->track, 1000));
	// std::this_thread::sleep_for(std::chrono::milliseconds(1000));
	//   // Mix_FreeMusic(app->music); // this call blocks until the music
	//   has
	//   // finished fading
	//   SDL_CloseAudioDevice(ctx->audioDevice);
	//
	//   delete ctx;
	// }
	// TTF_Quit();
	// MIX_Quit();
	//
	SDL_Log("Application quit successfully!\nStatus code: %d\nUnhandled "
	        "errors: %ld",
	        ctx->app_status.app_quit, ctx->app_status.error_msgs.size);
	for (auto &emsg : ctx->app_status.error_msgs) {
		SDL_LogError(SDL_LOG_CATEGORY_ERROR, StrView_Fmt, StrView_Arg(emsg));
	}
}
