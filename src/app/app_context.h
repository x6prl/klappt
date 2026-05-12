#pragma once

#include "SDL3/SDL_timer.h"
#include "app/app_status.h"
#include "base/dyn_arr.h"
#include "domain/engine.h"
#include "domain/exercises.h"
#include "domain/settings.h"
#include "domain/word_store.h"
#include "domain/words.h"
#include <SDL3/SDL.h>
// #include <SDL3_mixer/SDL_mixer.h>
#include <SDL3_ttf/SDL_ttf.h>

#include "base/arena.h"
#include "base/profiler.h"
#include "ui/components/text_input_state.h"
#include "ui/textcache.h"
#include "ui/tslt.h"
#include <clay/clay.h>

enum class Screen {
	Start = 0,
	Exercice,
	ExerciceResultSummary,
	ExerciseReview,
	WordsList,
	LearningList,
	WordSuggestions,
	Settings,
	WordEdit,
	Onboarding,
};

inline const char *screen_name(Screen s) {
	switch (s) {
	case Screen::Start:
		return "Start";
	case Screen::Exercice:
		return "Exercise";
	case Screen::ExerciceResultSummary:
		return "ExerciseSummary";
	case Screen::ExerciseReview:
		return "ExerciseReview";
	case Screen::WordsList:
		return "WordsList";
	case Screen::LearningList:
		return "LearningList";
	case Screen::WordSuggestions:
		return "WordSuggestions";
	case Screen::Settings:
		return "Settings";
	case Screen::WordEdit:
		return "WordEdit";
	case Screen::Onboarding:
		return "Onboarding";
	}
	return "Unknown";
}

struct AppContext {
	using Idx = int;
	static constexpr Idx STACK_SIZE{16};
#ifdef __EMSCRIPTEN__
	static constexpr Size MAIN_ARENA_SIZE = 64 << 20;
	static constexpr Size TMP_ARENA_SIZE = 8 << 20;
#endif

	SDL_Window *window{};
	SDL_Renderer *renderer{};
	AppStatus app_status;
	Clay_Arena clay_arena{};
	float scale{1.f}, display_width{1000.f};
	uint64_t ticks{};
#ifdef __EMSCRIPTEN__
	Arena arena{MAIN_ARENA_SIZE};
	Arena tmparena{TMP_ARENA_SIZE};
#else
	Arena arena{};
	Arena tmparena{};
#endif
	TextCache *text{};
	Words *words{};
	WordStore word_store{};
	Engine::States states{};
	Engine::Exercises exercises{};
	DynArr<Word> suggestions_list{};

	// Word *word_edit{};

	Idx current{};
	Screen stack[STACK_SIZE]{};

	// used to burst high FPS for the next 1000ms
	bool animate{false};
	uint64_t animation_ticks_start{};
	SDL_TimerID animation_timer_id{0};

	TapSwipeLongTap tslt{};
	// SDL_AudioDeviceID audioDevice{};
	// MIX_Track *track{};
	MobileTextInputState mobile_text_input{};
	MobileTextInputBuffer words_search{};
	MobileTextInputBuffer learning_search{};
	Settings settings{};

	// uint64_t last_ticks[10]{};
	// uint64_t last_ticksef[10]{};

	void anim() {
		KLAPPT_PROFILE_SCOPE_N("AppContext::anim");
		animate = true;
	}
	Screen screen() const { return stack[current]; }
	/*
	 * NOTE:
	 * Do not use push method directly. Use transition functions instead.
	 */
	bool push(Screen s) {
		KLAPPT_PROFILE_SCOPE_N("AppContext::push");
		KLAPPT_PROFILE_NAME_F("AppContext::push -> %s", screen_name(s));
		if (current + 1 >= STACK_SIZE) {
			SDL_LogError(SDL_LOG_CATEGORY_ERROR, "Screen stack overflow");
			return false;
		}
		++current;
		stack[current] = s;
		anim(); // TODO: just push one frame
		return true;
	}
	/*
	 * NOTE:
	 * Do not use go method directly. Use transition functions instead.
	 */
	void go(Screen s) {
		KLAPPT_PROFILE_SCOPE_N("AppContext::go");
		KLAPPT_PROFILE_NAME_F("AppContext::go -> %s", screen_name(s));
		if (s != Screen::Start) {
			current = 1;
			stack[0] = Screen::Start;
			stack[1] = s;
		} else {
			current = 0;
			stack[0] = s;
		}
		anim(); // TODO: just push one frame
	}
	bool pop() {
		KLAPPT_PROFILE_SCOPE_N("AppContext::pop");
		if (current <= 0) {
			return false;
		}
		KLAPPT_PROFILE_NAME_F("AppContext::pop -> %s",
		                      screen_name(stack[current - 1]));
		--current;
		anim(); // TODO: just push one frame
		return true;
	}
	bool is_backable() const { return current > 0; }
};
