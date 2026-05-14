#include <cstdint>

#include "SDL3/SDL_stdinc.h"
#include "app/event_codes.h"
#include "base/measure.h"
#include "base/profiler.h"
#include "base/str_view.h"
#include "domain/settings.h"
#include "platform/files.h"
#include "ui/components/word_edit_state.h"
#include "ui/components/word_view_state.h"
#include "ui/textcache.h"
#define SDL_MAIN_USE_CALLBACKS // This is necessary for the new callbacks API.
                               // To use the legacy API, don't define this.
#include "SDL3/SDL_init.h"
#include "SDL3/SDL_log.h"
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
// #include <SDL3_image/SDL_image.h>
// #include <SDL3_mixer/SDL_mixer.h>
#include <SDL3_ttf/SDL_ttf.h>

#include <filesystem>

#include "app/hotreload.h"
#include "app/words_init.h"

#include "app/app_context.h"

constexpr uint32_t windowStartWidth = 1200 / 3;
constexpr uint32_t windowStartHeight = 2670 / 3;

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
	case Screen::Start: return "Frame/Start";
	case Screen::Exercice: return "Frame/Exercise";
	case Screen::ExerciceResultSummary: return "Frame/ExerciseSummary";
	case Screen::ExerciseReview: return "Frame/ExerciseReview";
	case Screen::WordsList: return "Frame/WordsList";
	case Screen::LearningList: return "Frame/LearningList";
	case Screen::WordSuggestions: return "Frame/WordSuggestions";
	case Screen::Settings: return "Frame/Settings";
	case Screen::WordEdit: return "Frame/WordEdit";
	case Screen::Onboarding: return "Frame/Onboarding";
	}
	return "Frame/Unknown";
}
#endif

static void WaitForProfilerConnection() {
#if defined(TRACY_ENABLE)
	SDL_Log("Waiting for Tracy profiler connection on port 8086...");
	while (!KLAPPT_PROFILE_CONNECTED()) {
		SDL_Delay(100);
	}
	SDL_Log("Tracy profiler connected.");
#endif
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
	// init the library, here we make a window so we only need the Video
	// capabilities.
	{
		KLAPPT_PROFILE_SCOPE_N("SDL_Init");
		if (not SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO)) {
			return SDL_Fail();
		}
	}
	m.lap().printus("SDL_Init");
	{
		KLAPPT_PROFILE_SCOPE_N("WaitForProfilerConnection");
		WaitForProfilerConnection();
	}
	m.lap().printus("Tracy connect");

	// init TTf
	{
		KLAPPT_PROFILE_SCOPE_N("TTF_Init");
		if (not TTF_Init()) {
			return SDL_Fail();
		}
	}
	m.lap().printus("TTF_Init");

	// init Mixer
	// if (not MIX_Init()) {
	// 	return SDL_Fail();
	// }

	// create a window

	SDL_Window *window{};
	{
		KLAPPT_PROFILE_SCOPE_N("CreateWindow");
		window = SDL_CreateWindow("klappt", windowStartWidth, windowStartHeight,
		                          SDL_WINDOW_RESIZABLE |
		                                SDL_WINDOW_HIGH_PIXEL_DENSITY
		                          // Well, using fullscreen implies dancing
		                          // around safe area during text input //
		                          // #ifdef ANDROID | SDL_WINDOW_FULLSCREEN
		                          // #endif // ANDROID

		);
	}
	// 0, 0,
	// SDL_WINDOW_FULLSCREEN | SDL_WINDOW_BORDERLESS | SDL_WINDOW_RESIZABLE);
	if (not window) {
		return SDL_Fail();
	}
	m.lap().printus("create window");

#ifdef __EMSCRIPTEN__
	SDL_SetWindowFillDocument(window, true);
	#endif

	// create a renderer
	SDL_Renderer *renderer{};
	{
		KLAPPT_PROFILE_SCOPE_N("CreateRenderer");
		renderer = SDL_CreateRenderer(window, NULL);
	}
	if (not renderer) {
		return SDL_Fail();
	}
	m.lap().printus("create renderer");

	// load the font
#if __ANDROID__
	std::filesystem::path basePath =
		  ""; // on Android we do not want to use basepath. Instead, assets are
	          // available at the root directory.
#else
	auto basePathPtr = SDL_GetBasePath();
	if (not basePathPtr) {
		return SDL_Fail();
	}
	const std::filesystem::path basePath = basePathPtr;
#endif

	const auto ui_font_path = basePath / "Inter-VariableFont.ttf";
	const auto arabic_ui_font_path = basePath / "ReadexPro-Regular.ttf";
	const auto icons_font_path = basePath / "Font-Awesome-7-Free-Solid-900.otf";
	const auto monospace_regular_font_path =
		  basePath / "JetBrainsMono-Regular.ttf";
	const auto monospace_bold_font_path = basePath / "JetBrainsMono-Bold.ttf";
	TTF_Font *ui_font{};
	TTF_Font *arabic_ui_font{};
	TTF_Font *icons_font{};
	TTF_Font *monospace_regular_font{};
	TTF_Font *monospace_bold_font{};
	{
		KLAPPT_PROFILE_SCOPE_N("LoadFonts");
		ui_font = TTF_OpenFont(ui_font_path.string().c_str(), 48);
		arabic_ui_font = TTF_OpenFont(arabic_ui_font_path.string().c_str(), 48);
		icons_font = TTF_OpenFont(icons_font_path.string().c_str(), 128);
		monospace_regular_font =
			  TTF_OpenFont(monospace_regular_font_path.string().c_str(), 48);
		monospace_bold_font =
			  TTF_OpenFont(monospace_bold_font_path.string().c_str(), 48);
	}
	if (not ui_font or not arabic_ui_font or not icons_font or
	    not monospace_regular_font or not monospace_bold_font) {
		return SDL_Fail();
	}
	m.lap().printus("load fonts");
	TTF_TextEngine *text_engine{};
	{
		KLAPPT_PROFILE_SCOPE_N("CreateTextEngine");
		text_engine = TTF_CreateRendererTextEngine(renderer);
	}
	if (!text_engine) {
		return SDL_Fail();
	}
	m.lap().printus("create text engine");

	// init SDL Mixer
	// MIX_Mixer *mixer =
	// 	  MIX_CreateMixerDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, NULL);
	// if (mixer == nullptr) {
	// 	return SDL_Fail();
	// }

	// auto mixerTrack = MIX_CreateTrack(mixer);

	// load the music
	// auto musicPath = basePath / "the_entertainer.ogg";
	// auto music = MIX_LoadAudio(mixer, musicPath.string().c_str(), false);
	// if (not music) {
	// 	return SDL_Fail();
	// }

	// play the music (does not loop)
	// MIX_SetTrackAudio(mixerTrack, music);
	// MIX_PlayTrack(mixerTrack, 0);

	// print some information about the window
	int width, height, bbwidth, bbheight;
	SDL_ShowWindow(window);
	{
		SDL_GetWindowSize(window, &width, &height);
		SDL_GetWindowSizeInPixels(window, &bbwidth, &bbheight);
		SDL_Log("Window size: %ix%i", width, height);
		SDL_Log("Backbuffer size: %ix%i", bbwidth, bbheight);
		if (width != bbwidth) {
			SDL_Log("This is a highdpi environment.");
		}
	}

	auto text_cache = new TextCache{
		  .engine = text_engine,
		  .base_fonts = {ui_font, icons_font, monospace_regular_font,
	                     monospace_bold_font, arabic_ui_font}};
	m.lap().printus("create text cache");

	// set up the application data
	auto state = new AppContext{
		  .window = window,
		  .renderer = renderer,
		  .scale = SDL_GetWindowDisplayScale(window),
		  .display_width = static_cast<float>(width),
		  .ticks = SDL_GetTicks(),
		  // .clay_arena = clay_arena,
		  .text = text_cache,
		  .current = 0,
		  .stack = {Screen::Onboarding},
		  // .track = mixerTrack,
		  .word_view_state = new WordViewState{},
		  .word_edit_state = new WordEditState{},
	};
	*appstate = state;
	m.lap().printus("create app context");

// load app_hotreload
#if HOTRELOAD
#if __ANDROID__
	constexpr const char ANDROID_PACKAGED_MODULE_NAME[] = "libapp_hotreload.so";
	const char *initial_hotreload_path = ANDROID_PACKAGED_MODULE_NAME;
	#else
	const char *initial_hotreload_path = HOTRELOAD_MODULE_PATH;
#endif
	bool healthy{};
	{
		KLAPPT_PROFILE_SCOPE_N("LoadHotreloadModule");
		auto [hotreload_healthy, reloaded] = hotreload(initial_hotreload_path);
		healthy = hotreload_healthy;
		(void)reloaded;
	}
	if (!healthy) {
		SDL_LogError(SDL_LOG_CATEGORY_CUSTOM,
		             "Failed to load hotreload module from %s",
		             initial_hotreload_path);
		return SDL_APP_FAILURE;
	}
	m.lap().printus("hotreload");
#endif

	{
		KLAPPT_PROFILE_SCOPE_N("ui_clay_init");
		ui_clay_init(state);
	}
	m.lap().printus("ui clay init");

	SDL_SetRenderVSync(renderer, -1); // enable vysnc

	// redraw only on events
	SDL_SetHint(SDL_HINT_MAIN_CALLBACK_RATE, "waitevent");
	constexpr auto UI_UPDATE_EVENT_TIME = 1000;
	// add timer event to allow animations
	SDL_AddTimer(UI_UPDATE_EVENT_TIME, WakeUpTimer,
	             nullptr); // Wake up 10 times a second
	m.lap().printus("render loop setup");

	FileLoader settingsfl{};
	if (settingsfl.load("settings.dat"_v)) {
		if (!Settings::decode(settingsfl.data, settingsfl.size, &state->settings)) {
			state->app_status.set_exit_with_error("cannot decode settings file"_v);
		}
	}
	m.lap().printus("load settings");
	{
		KLAPPT_PROFILE_SCOPE_N("ui_settings_init");
		ui_settings_init(state);
	}
	m.lap().printus("ui settings init");
	if (state->settings.onboarding_stage < 0) {
		{
			KLAPPT_PROFILE_SCOPE_N("init_words");
			if (!init_words(*state, basePath)) {
				return SDL_APP_FAILURE;
			}
		}
		m.lap().printus("init words");
		state->go(Screen::Start);
	}
	SDL_Log("Application started successfully!");
	m.lap().printus("total");

	return SDL_APP_CONTINUE;
}

extern "C" SDL_AppResult SDLCALL SDL_AppEvent(void *appstate,
                                              SDL_Event *event) {
	KLAPPT_PROFILE_SCOPE_N("SDL_AppEvent");
	KLAPPT_PROFILE_NAME_F("SDL_AppEvent:%s", EventTypeName(event->type));
	auto *app = (AppContext *)appstate;
	app->ticks = SDL_GetTicks();
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
		if (app->animation_timer_id) {
			SDL_RemoveTimer(app->animation_timer_id);
			app->animation_timer_id = 0;
		}
		app->animate = false;
		auto [healthy, reloaded] = hotreload(HOTRELOAD_MODULE_PATH);
		if (!healthy) {
			SDL_Log("Failed to reload app_hotreload from %s",
			        HOTRELOAD_MODULE_PATH);
			// return SDL_APP_CONTINUE;
		}
		if (reloaded) {
			ui_clay_init(app);
			ui_settings_init(app);
		}
	}
#endif
	{
		KLAPPT_PROFILE_SCOPE_N("ui_event");
		return ui_event(app, event);
	}
}

// void update_ticks_array(uint64_t (*ts)[10], uint64_t t) {
// 	for (int i{}; i < 9; ++i) {
// 		(*ts)[i] = (*ts)[i + 1];
// 	}
// 	(*ts)[9] = t;
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
		auto *app = (AppContext *)appstate;
		app->ticks = tick;
		KLAPPT_PROFILE_FRAME_N(FrameName(app->screen()));
		// update_ticks_array(&(app->last_ticks), tick);
		SDL_AppResult ret;
		{
			KLAPPT_PROFILE_SCOPE_N("ui_iterate");
			ret = ui_iterate(app);
		}
		// update_ticks_array(&(app->last_ticksef), SDL_GetTicks());
		m.lap();
		if (m.tlap > uint64_t(st.avg()*2)) {
			m.printms();
			SDL_Log("and average %d us", st.avg()/1000);
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
	auto *app = (AppContext *)appstate;
	Measure m{__PRETTY_FUNCTION__};
	(void)result;
	app->word_store.close();
	app->states.close();
	m.lap().printus("save states");
	// OS will destroy everything itself on exit

	// if (app) {
	//   SDL_DestroyRenderer(app->renderer);
	//   SDL_DestroyWindow(app->window);
	//
	//   // prevent the music from abruptly ending.
	// MIX_StopTrack(app->track, MIX_TrackMSToFrames(app->track, 1000));
	// std::this_thread::sleep_for(std::chrono::milliseconds(1000));
	//   // Mix_FreeMusic(app->music); // this call blocks until the music has
	//   // finished fading
	//   SDL_CloseAudioDevice(app->audioDevice);
	//
	//   delete app;
	// }
	// TTF_Quit();
	// MIX_Quit();
	//
	SDL_Log("Application quit successfully!\nStatus code: %d\nUnhandled errors: %d", app->app_status.app_quit, app->app_status.error_msgs.size);
	for (auto &emsg: app->app_status.error_msgs) {
		SDL_LogError(SDL_LOG_CATEGORY_ERROR, StrView_Fmt, StrView_Arg(emsg));
	}
}
