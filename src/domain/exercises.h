#pragma once

#include "SDL3/SDL_log.h"
#include "base/dyn_arr.h"
#include "base/str_view.h"
#include "base/str_view_list.h"
#include "engine.h"
#include "words.h"

struct AppContext;

struct ExerciseResult {
	StrView expected{};
	StrView actual{};
	WordId word_id{};
	WordRef word_ref{};
	StrView source{};
	StrView source_sub0{};
	StrView source_sub1{};
	DynArr<StrView> expected_parts{};
	DynArr<StrView> actual_parts{};
	DynArr<int> is_right_actual_part{};
};

struct ExerciseState {
	struct SubStage {
		bool is_keypad{true};
		Size correct_option_index{-1};
		Size points_for_correct_answer{0};
		Size selected_option_index{-1};
		DynArr<StrView> opts{};
	};
	struct Stage {
		// prompt may differ for stages
		StrView source{};
		StrView before_answer = "_"_v;
		DynArr<SubStage> substages{};
		Size current_substage{0};
		struct {
			StrView left_part{};
			StrView right_part{};
			StrViewArray get_answer_for_str(Arena &a, StrView str) const {
				StrViewArray arr{};
				arr.push(a, left_part);
				arr.push(a, str);
				arr.push(a, right_part);
				return arr;
			}
		} gap{};
	};

	static constexpr auto EMPTY_ANSWER = "—"_v;

	WordId word_id{};
	WordRef word_ref{};
	Engine::Mode mode{};
	WordType word_type{};
	StrView response = EMPTY_ANSWER;
	// sub prompts are common for all the stages
	StrView source_sub0{};
	StrView source_sub1{};
	DynArr<Stage> stages{};
	Size current_stage{0};
	Size points_max{0};
	Size points_earned{0};

	Stage &s() { return stages[current_stage]; }
	const Stage &s() const { return stages[current_stage]; }
	SubStage &ss() { return s().substages[s().current_substage]; }
	const SubStage &ss() const { return s().substages[s().current_substage]; }

	// returns false if it was the last substage
	bool submit(Size res) {
		// give points
		ss().selected_option_index = res;
		if (res == ss().correct_option_index) {
			points_earned += ss().points_for_correct_answer;
		}
		if (s().current_substage + 1 >= s().substages.size) {
			// at the last substage
			if (current_stage + 1 >= stages.size) {
				return false;
			} else {
				s().current_substage = s().substages.size;
				++current_stage;
				s().current_substage = 0;
				return true;
			}
		} else {
			// more substages to go
			s().current_substage += 1;
			return true;
		}
	}
	// returns false if already at the start
	bool undo() {
		if (0 == s().current_substage) {
			if (0 == current_stage) {
				// already at the start
				return false;
			} else {
				// going one stage down
				--current_stage;
				// going one substage down
				s().current_substage = s().substages.size - 1;
				// taking points back, if the result was right
				if (ss().selected_option_index == ss().correct_option_index) {
					points_earned -= ss().points_for_correct_answer;
				}
				// resetting the substage result
				ss().selected_option_index = -1;
				return true;
			}
		} else {
			// going one substage down
			s().current_substage -= 1;
			// taking points back, if the result was right
			if (ss().selected_option_index == ss().correct_option_index) {
				points_earned -= ss().points_for_correct_answer;
			}
			// resetting the substage result
			ss().selected_option_index = -1;
			return true;
		}
	}
};

namespace Engine {
struct Exercises {
	Arena a{1 << 16}; // 64 KB

	DynArr<ExerciseState> exercises{};
	DynArr<ExerciseResult> results{};
	DynArr<Word *> words{};
	Size exercise_current_idx{0};
	Size exercise_total() const { return exercises.size; }
	Size pending_selection_index{-1};
	Size correct_exercise_count{};

	bool is_initialized() const { return exercises.size > 0; }
	bool handler_back_pressed(AppContext *ctx);
	// bool is_finished() const {
	// 	return is_initialized() && exercise_current_idx == exercise_total();
	// }

	const ExerciseState::SubStage &substage() const {
		return exercises[exercise_current_idx].ss();
	}
	const StrView source() const {
		return exercises[exercise_current_idx].s().source;
	}
	const StrView source_sub0() const {
		return exercises[exercise_current_idx].source_sub0;
	}
	const StrView source_sub1() const {
		return exercises[exercise_current_idx].source_sub1;
	}
	const StrView response() const {
		const auto &exercise = exercises[exercise_current_idx];
		if (exercise.mode == Mode::Gaps &&
		    exercise.response == ExerciseState::EMPTY_ANSWER) {
			return exercise.s().before_answer;
		}
		return exercise.response;
	}

	const ExerciseResult &current_result_review() const {
		return results[exercise_current_idx];
	}

	void submit_result(Size res) { pending_selection_index = res; }
	void next_result() { pending_selection_index = 0; }
	void build_result_reviews(Arena &tmpa, bool is_only_failed);

	Size generate_new_exercises(AppContext *ctx, Size n);

	enum class CommitResult {
		None,
		StartNextRound,
		ShowSummary,
	};

	CommitResult commit(AppContext *ctx);

	void reset() {
		SDL_Log("=======> %s <========= this=%p arena=%p offset=%td "
		        "used_words=%d exercises=%d results=%d",
		        __PRETTY_FUNCTION__, static_cast<void *>(this),
		        static_cast<void *>(a.data), a.offset, words.size,
		        exercises.size, results.size);
		pending_selection_index = -1;
		exercise_current_idx = 0;
		words.reset_size_reserved();
		exercises.reset_size_reserved();
		results.reset_size_reserved();
		a.clear();
	}
};
} // namespace Engine
