#pragma once

#include <SDL3/SDL_timer.h>
#include <SDL3/SDL.h>
// #include <SDL3_mixer/SDL_mixer.h>
#include <SDL3_ttf/SDL_ttf.h>

#include <clay/clay.h>

#include "base/dyn_arr.h"
#include "base/str_view.h"
#include "base/arena.h"
#include "base/profiler.h"
#include "app/app_status.h"
#include "app/audio_context.h"
#include "app/worker.h"
#include "domain/engine.h"
#include "domain/exercises.h"
#include "domain/settings.h"
#include "domain/word_store.h"
#include "domain/words.h"
#include "platform/audio.h"
#include "ui/components/download_data.h"
#include "ui/components/text_input_state.h"
#include "ui/components/word_edit_state.h"
#include "ui/components/word_view_state.h"
#include "ui/textcache.h"
#include "ui/tslt.h"

enum class Screen {
	Trainer = 0,
	Exercice,
	ExerciceResultSummary,
	ExerciseReview,
	Dictionary,
	LearningList,
	WordSuggestions,
	Settings,
	WordView,
	WordEdit,
	Onboarding,
	TTS_ASR,
};

inline const char *screen_name(Screen s) {
	switch (s) {
	case Screen::Trainer:
		return "Start";
	case Screen::Exercice:
		return "Exercise";
	case Screen::ExerciceResultSummary:
		return "ExerciseSummary";
	case Screen::ExerciseReview:
		return "ExerciseReview";
	case Screen::Dictionary:
		return "WordsList";
	case Screen::LearningList:
		return "LearningList";
	case Screen::WordSuggestions:
		return "WordSuggestions";
	case Screen::Settings:
		return "Settings";
	case Screen::WordView:
		return "WordView";
	case Screen::WordEdit:
		return "WordEdit";
	case Screen::Onboarding:
		return "Onboarding";
	case Screen::TTS_ASR:
		return "TTS/ASR";
	}
	return "Unknown";
}

struct AppContext {

	using Idx = Size;
	static constexpr Idx STACK_SIZE{16};
	static constexpr Size MAIN_ARENA_SIZE = 32 << 20;
	static constexpr Size TMP_ARENA_SIZE = 8 << 20;

	SDL_Window *window{};
	SDL_Renderer *renderer{};
	AppStatus app_status;
	Clay_Arena clay_arena{};
	float scale{1.f}, display_width{1000.f};
	uint64_t ticks{};
	Arena arena{MAIN_ARENA_SIZE};
	Arena arena_frame{TMP_ARENA_SIZE};
	TextCache *text{};
	Words *words{};
	WordStore word_store{};
	Engine::States states{};
	Engine::Exercises exercises{};
	DynArr<Word> suggestions_list{};

	// Word *word_edit{};

	Idx current{};
	Screen stack[STACK_SIZE]{};
	Arena arena_screen_list[STACK_SIZE]{};
	Arena &arena_screen() { return arena_screen_list[current]; }

	// used to burst high FPS for the next 1000ms
	bool animate{false};
	uint64_t animation_ticks_start{};
	SDL_TimerID animation_timer_id{0};

	TapSwipeLongTap tslt{};
	UI_Audio_ASR_TTS audio_asr_tts_status;
	// SDL_AudioDeviceID audioDevice{};
	// MIX_Track *track{};
	MobileTextInputState mobile_text_input{};
	MobileTextInputBuffer words_search{};
	MobileTextInputBuffer learning_search{};
	MobileTextInputBuffer tts_input{};
	StrView asr_result{};
	WordViewState *word_view_state{};
	WordEditState *word_edit_state{};
	Settings settings{};

	JobQueue<Job> worker_job_queue{};

	DynArr<DownloadData> downloads{};
	NetContext *net{nullptr};     // NOTE: created by NetThread
	JobQueue<Size> net_worker_job_queue{};

	AudioContext *audio{nullptr}; //       created by AudioThread
	JobQueue<AudioJob> audio_worker_job_queue{};

#if NEURO
	NeuroContext *neuro{nullptr}; //       created by NeuroThread
	JobQueue<NeuroJob> neuro_worker_job_queue{};
#endif

	// uint64_t last_ticks[10]{};
	// uint64_t last_ticksef[10]{};

	void anim() {
		KLAPPT_PROFILE_SCOPE_N("AppContext::anim");
		animate = true;
	}
	void push_one_frame() {
		anim(); // TODO: just push one frame
	}
	Screen screen() const { return stack[current]; }
	Screen screen_prev() const {
		if (current >0) {
			return stack[current-1];
		} 
		return Screen::Trainer;
	}
	void on_screen_change(Screen from, Screen to) {
		(void)from;
		(void)to;
		switch (from) {
		case Screen::TTS_ASR: {
			// stop recording and turn off micro
			// TODO: FINISH
			// if (UIAudioContext::TRUE ==
			//     SDL_GetAtomicInt(&sound_ctx->is_recording)) {
			// 	record_stop(this);
			// }
			// if (UIAudioContext::TRUE ==
			//     SDL_GetAtomicInt(&sound_ctx->is_initialized)) {
			// 	record_deinit(this);
			// }
			record_stop_then_do_nothing(this);
			record_deinit(this);
		}
		default:
			break;
		}
	}
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
		push_one_frame();
		return true;
	}
	/*
	 * NOTE:
	 * Do not use go method directly. Use transition functions instead.
	 */
	void go(Screen s) {
		KLAPPT_PROFILE_SCOPE_N("AppContext::go");
		KLAPPT_PROFILE_NAME_F("AppContext::go -> %s", screen_name(s));
		const auto was = screen();
		if (s != Screen::Trainer) {
			current = 1;
			stack[0] = Screen::Trainer;
			stack[1] = s;
		} else {
			current = 0;
			stack[0] = s;
		}
		on_screen_change(was, screen());
		push_one_frame();
	}
	bool pop() {
		KLAPPT_PROFILE_SCOPE_N("AppContext::pop");
		if (current <= 0) {
			return false;
		}
		KLAPPT_PROFILE_NAME_F("AppContext::pop -> %s",
		                      screen_name(stack[current - 1]));
		arena_screen_list[current].clear();
		--current;
		push_one_frame();
		return true;
	}
	bool is_backable() const { return current > 0; }
};
