#pragma once

#include <ctime>
#include <string>
#ifdef __EMSCRIPTEN__
#include <unordered_map>
#include <utility>
#include <vector>
#endif

#ifndef __EMSCRIPTEN__
#include "lmdb.h"
#endif

#include "base/arena.h"
#include "base/arr.h"
#include "base/dyn_arr.h"
#include "base/pair.h"
#include "base/str_view.h"
#include "word_id.h"

// TODO: rewrite?
namespace Engine {

using Timestamp = time_t;

enum class Mode : int32_t { Entire = 0, Gaps = 1, Chunks = 2, Compose = 3, Count = 4 };

constexpr int modei(Mode m) { return static_cast<int>(m); }
constexpr Mode imode(int i) { return static_cast<Mode>(i); }
constexpr int MODE_COUNT = modei(Mode::Count);

struct Review {
	// Observed score for this review.
	Size right{};
	Size size{};

	// Review time.
	Timestamp at{};

	// Optional chance baseline for the current UI.
	// Examples:
	//   Entire with 5 options: 0.20
	//   Gaps with 4 options:   0.25
	//   Compose:               0.00
	// If negative, engine uses the mode default.
	double chance_success{-1.0};
};

void print(const Review &r);

struct ModeMemory {
	// Half-life in days in the current mode.
	double stability_days{0.0};

	// Smoothed quality in this mode, after chance-correction.
	double quality_ewma{0.0};

	// How many reviews the item has had in this mode.
	Size reviews{0};
};

struct Params {
	// Scheduling target.
	double target_retention{0.88};

	// Penalize total blackouts, but not too harshly.
	double penalty_lambda{0.11};

	// Numerical safety / product constraints.
	double stability_min_days{40.0 / 86400.0}; // 40 seconds
	double stability_max_days{365.0};         // 1 year

	// Reward and penalty strengths by mode.
	// Harder productive modes get more reward for successful retrieval,
	// but we keep failure growth milder than success growth.
	Arr<double, MODE_COUNT> eta_success{
		  0.78, // Entire
		  0.82, // Gaps
		  0.92, // Chunks
		  1.05  // Compose
	};

	Arr<double, MODE_COUNT> eta_failure{
		  0.09, // Entire
		  0.11, // Gaps
		  0.15, // Chunks
		  0.18  // Compose
	};

	// Success growth formula. The reward headroom is:
	// pow(max(1 - predicted_r, success_floor) + success_bonus, success_power)
	double success_power{0.85};
	double success_floor{0.55};
	double success_bonus{0.22};

	// Recovery boost after a streak of poor reviews.
	double recovery_boost{6.};
	Size recovery_window{3};

	// Bootstraping same-day learning, we do not have a separate "learning
	// steps" phase
	Arr<double, MODE_COUNT> initial_stability_days{
		  0.18, // ~4.3h base half-life
		  0.10, // ~2.4h
		  0.09, // ~2.2h
		  0.07  // ~1.7h
	};

	// Seed scale for a first review uses an exponential interpolation:
	// min * pow(max / min, quality^seed_scale_power)
	double seed_scale_min{0.008};
	double seed_scale_max{24.0};
	double seed_scale_power{1.0};

	// Promotion thresholds: current mode must be both stable and consistently
	// strong.
	Arr<double, MODE_COUNT> promotion_stability_days{
		  6.00, // Entire -> Gaps __________
		  10.00, // Gaps  -> Chunks
		  18.00, // Chunks -> Compose
		  0.0};

	Arr<double, MODE_COUNT> promotion_transfer{
		  0.18, // Entire -> Gaps __________
		  0.32, // Gaps   -> Chunks
		  0.42, // Chunks -> Compose
		  0.0};

	double promotion_quality{0.50};
	double demotion_quality{0.28};
	Size min_reviews_before_demotion{2};

	// Default chance baselines.
	// Entire is assumed to be 1-of-5 recognition unless overridden per review.
	Arr<double, MODE_COUNT> chance_success_default{
		  0.20, // Entire
		  0.20, // Gaps
		  0.05, // Chunks
		  0.00  // Compose
	};

	// Difficulty: 1 = easy, 10 = hard.
	double difficulty_init{5.0};
	double difficulty_min{1.0};
	double difficulty_max{10.0};

	// If observed performance is better than predicted retrievability, item
	// gets easier. If worse, item gets harder.
	double difficulty_lr{0.18};

	// Productive modes reveal intrinsic difficulty more clearly than
	// recognition.
	Arr<double, MODE_COUNT> difficulty_signal_by_mode{
		  0.60, // Entire
		  0.80, // Gaps
		  1.00, // Chunks
		  1.10  // Compose
	};

	// Stabilization decay: the higher the current stability, the smaller the
	// gain.
	double reward_decay{0.08};

	// Lapse protection: high-stability cards should not collapse too violently.
	double penalty_decay{0.08};

	// Difficulty scaling:
	// easier items grow stability more on success, harder items less;
	// harder items are penalized a bit more on poor recall.
	double easy_reward_scale{1.32};
	double hard_reward_scale{0.82};

	double easy_penalty_scale{0.92};
	double hard_penalty_scale{1.08};

	// EWMA smoothing for quality.
	double quality_ewma_alpha{0.34};

	// Due mapping:
	// due_days = due_pivot_days * pow(base_due_days / due_pivot_days, due_exponent)
	double due_exponent{0.92};
	double due_pivot_days{1.0};
};

struct State {
	static constexpr Params p{};
	Arr<ModeMemory, MODE_COUNT> memory{};
	double difficulty{p.difficulty_init};

	// Active mode currently used for scheduling this item.
	Mode mode{Mode::Entire};

	Timestamp last_review{};
	Timestamp due{};

	Size total_reviews{};
	Size lapses{};
	Size recent_failures{};

	double chance_success_for(Mode m, double override_value) const;
	double difficulty_norm() const;
	double reward_scale_from_difficulty() const;
	double penalty_scale_from_difficulty() const;
	double stabilization_decay(double stability_days,
	                           double decay_strength) const;
	double seed_scale_from_quality(double quality) const;
	double success_headroom(double predicted_retrievability) const;
	double recovery_multiplier(double effective_quality) const;
	double due_days_from_stability(double stability_days) const;
	double seed_initial_stability(Mode m, double quality) const;
	void update_difficulty(Mode review_mode, double effective_quality,
	                       double predicted_retrievability);
	void maybe_promote();
	void maybe_demote();
	void refresh_due(Timestamp now);
	bool update(const Review &r);
};

void print(const State &s);

struct States {
#ifdef __EMSCRIPTEN__
	std::string storage_key{};
	std::unordered_map<WordId, State, WordIdHash> states{};
	std::vector<std::pair<uint64_t, WordId>> due{}; // sorted by due, then word id
#else
	MDB_env *env{};
	MDB_dbi state_dbi{};
	MDB_dbi due_dbi{}; // secondary index
#endif

	bool open(StrView path = {});
	void close();

	// [is_success, is_found]
	Pair<bool,bool> get(WordId word_id, State &dst) const;
	// return false only on errors
	bool set(WordId word_id, const State &new_state);
	// return false only on errors
	bool erase(WordId word_id);
	// return false only on errors
	bool collect_due(Arena &a, Timestamp now, DynArr<WordId> &dst,
	                 Size limit = 0) const;
};

} // namespace Engine
