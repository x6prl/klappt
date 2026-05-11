#include "engine.h"

#include "SDL3/SDL_log.h"
#include "base/profiler.h"

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

#ifndef __EMSCRIPTEN__
#include "lmdb.h"
#include <filesystem>
#include <string>
#include <system_error>
#include "platform/files.h"
#endif

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>

namespace Engine {
namespace {

constexpr double A_DAY_SECONDS = 86400.0;

double clamp01(double x) { return std::clamp(x, 0.0, 1.0); }

double lerp(double a, double b, double t) { return a + (b - a) * t; }

double safe_exp2_neg(double x) {
	// x is expected >= 0
	return std::exp2(-std::max(0.0, x));
}

double interval_days_from_half_life(double stability_days,
                                    double target_retention) {
	// target_retention = 2^(-dt / S)  ->  dt = S * log2(1 / target_retention)
	return stability_days * std::log2(1.0 / target_retention);
}

double retrievability(double elapsed_days, double stability_days) {
	if (stability_days <= 0.0) {
		return 0.0;
	}
	return safe_exp2_neg(elapsed_days / stability_days);
}

double quality_from_partial(double raw_score, double chance_success) {
	raw_score = clamp01(raw_score);
	chance_success = clamp01(chance_success);

	// Chance-correct recognition-like tasks.
	// If chance = 0.20 and raw = 0.20, effective quality is 0.
	// If chance = 0.20 and raw = 1.00, effective quality is 1.
	if (chance_success >= 0.999999) {
		return raw_score >= 0.999999 ? 1.0 : 0.0;
	}
	return clamp01((raw_score - chance_success) / (1.0 - chance_success));
}

const char *mode_name(Mode mode) {
	switch (mode) {
	case Mode::Entire:
		return "Entire";
	case Mode::Gaps:
		return "Gaps";
	case Mode::Chunks:
		return "Chunks";
	case Mode::Compose:
		return "Compose";
	case Mode::Count:
		return "Count";
	}
	return "Unknown";
}

} // namespace

void print(const Review &r) {
	SDL_Log("Engine::Review { right=%d size=%d at=%lld chance_success=%.3f }",
	        static_cast<int>(r.right), static_cast<int>(r.size),
	        static_cast<long long>(r.at), r.chance_success);
}

double State::chance_success_for(Mode m, double override_value) const {
	if (override_value >= 0.0) {
		return clamp01(override_value);
	}
	return clamp01(p.chance_success_default[modei(m)]);
}

double State::difficulty_norm() const {
	const double span = p.difficulty_max - p.difficulty_min;
	if (span <= 0.0) {
		return 0.5;
	}
	return clamp01((difficulty - p.difficulty_min) / span);
}

double State::reward_scale_from_difficulty() const {
	return lerp(p.easy_reward_scale, p.hard_reward_scale, difficulty_norm());
}

double State::penalty_scale_from_difficulty() const {
	return lerp(p.easy_penalty_scale, p.hard_penalty_scale, difficulty_norm());
}

double State::stabilization_decay(double stability_days,
                                  double decay_strength) const {
	// Higher stability => lower incremental gain/loss in log-space.
	return 1.0 /
	       (1.0 + decay_strength * std::log1p(std::max(0.0, stability_days)));
}

double State::seed_scale_from_quality(double quality) const {
	const double min_scale = std::max(p.seed_scale_min, 1e-9);
	const double max_scale = std::max(p.seed_scale_max, min_scale);
	const double q = std::pow(clamp01(quality), p.seed_scale_power);
	return min_scale * std::pow(max_scale / min_scale, q);
}

double State::success_headroom(double predicted_retrievability) const {
	const double base = std::max(0.0, 1.0 - predicted_retrievability);
	return std::max(base, p.success_floor) + p.success_bonus;
}

double State::recovery_multiplier(double effective_quality) const {
	if (effective_quality < 0.5) {
		return 1.0;
	}
	const double window = static_cast<double>(std::max<Size>(1, p.recovery_window));
	return 1.0 + p.recovery_boost *
	                    std::min(1.0, static_cast<double>(recent_failures) / window);
}

double State::due_days_from_stability(double stability_days) const {
	const double base_days =
	      interval_days_from_half_life(stability_days, p.target_retention);
	const double pivot = std::max(p.due_pivot_days, 1e-9);
	return pivot * std::pow(std::max(base_days / pivot, 1e-9), p.due_exponent);
}

double State::seed_initial_stability(Mode m, double quality) const {
	const int i = modei(m);
	const double base = p.initial_stability_days[i];

	// Easier items start slightly stronger.
	const double d_norm = difficulty_norm();
	const double difficulty_scale = lerp(1.10, 0.85, d_norm);

	// Partial success should seed something better than a blackout.
	const double quality_scale = seed_scale_from_quality(quality);

	return std::clamp(base * difficulty_scale * quality_scale,
	                  p.stability_min_days, p.stability_max_days);
}

void State::update_difficulty(Mode review_mode, double effective_quality,
                              double predicted_retrievability) {
	const double surprise = effective_quality - predicted_retrievability;
	const double signal = p.difficulty_signal_by_mode[modei(review_mode)];

	// Better-than-expected => easier.
	// Worse-than-expected  => harder.
	difficulty -= p.difficulty_lr * signal * surprise;
	difficulty = std::clamp(difficulty, p.difficulty_min, p.difficulty_max);
}

void State::maybe_promote() {
	const int i = modei(mode);
	if (mode >= Mode::Compose) {
		return;
	}

	const ModeMemory &cur = memory[i];
	if (cur.quality_ewma < p.promotion_quality) {
		return;
	}
	if (cur.stability_days < p.promotion_stability_days[i]) {
		return;
	}

	const Mode next = imode(i + 1);
	ModeMemory &dst = memory[modei(next)];

	// Partial transfer upward: recognition/productive skills overlap, but are
	// not identical.
	dst.stability_days = std::max(dst.stability_days,
	                              cur.stability_days * p.promotion_transfer[i]);
	dst.quality_ewma = std::max(dst.quality_ewma, cur.quality_ewma * 0.80);

	mode = next;
}

void State::maybe_demote() {
	if (mode <= Mode::Entire) {
		return;
	}

	const ModeMemory &cur = memory[modei(mode)];
	if (cur.reviews < p.min_reviews_before_demotion) {
		return;
	}
	if (cur.quality_ewma > p.demotion_quality) {
		return;
	}

	mode = imode(modei(mode) - 1);
}

void State::refresh_due(Timestamp now) {
	const ModeMemory &active = memory[modei(mode)];

	const double stability = std::clamp(
		  active.stability_days, p.stability_min_days, p.stability_max_days);

	const double days_until_due = due_days_from_stability(stability);
	const double seconds_until_due =
		  std::max(0.0, days_until_due * A_DAY_SECONDS);

	due = now + static_cast<Timestamp>(seconds_until_due);
}

bool State::update(const Review &r) {
	KLAPPT_PROFILE_SCOPE_N("Engine::State::update");
	if (r.size == 0) {
		return false;
	}

	const double raw =
		  clamp01(static_cast<double>(r.right) / static_cast<double>(r.size));
	const double chance = chance_success_for(mode, r.chance_success);
	const double q = quality_from_partial(raw, chance);

	ModeMemory &m = memory[modei(mode)];

	double predicted_r = 0.0;
	double elapsed_days = 0.0;

	if (last_review != 0 && r.at > last_review && m.stability_days > 0.0) {
		elapsed_days = static_cast<double>(r.at - last_review) / A_DAY_SECONDS;
		predicted_r = retrievability(elapsed_days, m.stability_days);
	}

	// Update difficulty from surprise signal before stability update.
	update_difficulty(mode, q, predicted_r);

	if (m.stability_days <= 0.0) {
		m.stability_days = seed_initial_stability(mode, q);
	} else {
		const int i = modei(mode);

		const double reward =
			  p.eta_success[i] * q *
			  std::pow(std::max(success_headroom(predicted_r), 1e-9),
			           p.success_power) *
			  recovery_multiplier(q) *
			  reward_scale_from_difficulty() *
			  stabilization_decay(m.stability_days, p.reward_decay);

		const double penalty =
			  p.eta_failure[i] * (1.0 - q) * (predicted_r + p.penalty_lambda) *
			  penalty_scale_from_difficulty() *
			  stabilization_decay(m.stability_days, p.penalty_decay);

		const double log_s_new =
			  std::log(m.stability_days) + (reward - penalty);
		m.stability_days = std::clamp(std::exp(log_s_new), p.stability_min_days,
		                              p.stability_max_days);
	}

	m.quality_ewma = (1.0 - p.quality_ewma_alpha) * m.quality_ewma +
	                 p.quality_ewma_alpha * q;
	m.reviews += 1;

	total_reviews += 1;
	if (q < 0.5) {
		lapses += 1;
		recent_failures += 1;
	} else {
		recent_failures = std::max<Size>(0, recent_failures - 1);
	}

	last_review = r.at;

	// Mode transitions after updating the reviewed mode.
	if (q >= 0.5) {
		maybe_promote();
	} else {
		maybe_demote();
	}

	refresh_due(r.at);
	return true;
}

void print(const State &s) {
	SDL_Log(
		  "Engine::State { mode=%s difficulty=%.3f total_reviews=%d lapses=%d "
		  "last_review=%lld due=%lld }",
		  mode_name(s.mode), s.difficulty, static_cast<int>(s.total_reviews),
		  static_cast<int>(s.lapses), static_cast<long long>(s.last_review),
		  static_cast<long long>(s.due));

	for (int i = 0; i < MODE_COUNT; ++i) {
		const Mode mode = imode(i);
		const ModeMemory &m = s.memory[i];
		SDL_Log("  memory[%s] { stability_days=%.3f quality_ewma=%.3f "
		        "reviews=%d }",
		        mode_name(mode), m.stability_days, m.quality_ewma,
		        static_cast<int>(m.reviews));
	}
}

namespace {

#ifndef __EMSCRIPTEN__
void log_lmdb_error(const char *op, int rc) {
	SDL_LogError(SDL_LOG_CATEGORY_ERROR, "%s failed: %s", op, mdb_strerror(rc));
}

constexpr size_t LMDB_MAP_SIZE = 64ull << 20;
constexpr char STATE_DB_NAME[] = "state";
constexpr char DUE_DB_NAME[] = "due";

std::string default_lmdb_path(StrView leaf = "states.lmdb"_v) {
	return FileLoader::path_for(leaf);
}

void encode_be64(uint64_t value, unsigned char *out) {
	for (int i = 7; i >= 0; --i) {
		out[7 - i] = static_cast<unsigned char>((value >> (i * 8)) & 0xffu);
	}
}

uint64_t decode_be64(const unsigned char *data) {
	uint64_t value = 0;
	for (int i = 0; i < 8; ++i) {
		value = (value << 8) | static_cast<uint64_t>(data[i]);
	}
	return value;
}

MDB_val mdb_val_from_bytes(void *data, size_t size) { return {size, data}; }

MDB_val state_key_from_word_id(WordId word_id, unsigned char (&buffer)[8]) {
	encode_be64(word_id.value, buffer);
	return mdb_val_from_bytes(buffer, sizeof(buffer));
}

MDB_val due_key_from(Timestamp due, WordId word_id,
                     unsigned char (&buffer)[16]) {
	encode_be64(static_cast<uint64_t>(std::max<Timestamp>(0, due)), buffer);
	encode_be64(word_id.value, buffer + 8);
	return mdb_val_from_bytes(buffer, sizeof(buffer));
}

bool parse_due_key(const MDB_val &key, Timestamp &due, WordId &word_id) {
	if (key.mv_size != 16 || !key.mv_data) {
		return false;
	}
	const auto *bytes = static_cast<const unsigned char *>(key.mv_data);
	due = static_cast<Timestamp>(decode_be64(bytes));
	word_id = WordId{decode_be64(bytes + 8)};
	return true;
}

bool read_state_value(StrView key, const MDB_val &value, State &out) {
	if (value.mv_size != sizeof(State)) {
		SDL_LogError(SDL_LOG_CATEGORY_ERROR,
		             "LMDB state payload has wrong size for key %.*s", key.size,
		             key.data ? key.data : "");
		return false;
	}
	memcpy(&out, value.mv_data, sizeof(State));
	return true;
}
#endif

} // namespace

#ifndef __EMSCRIPTEN__
bool States::open(StrView requested_path) {
	KLAPPT_PROFILE_SCOPE_N("Engine::States::open");
	close();
	const auto dir =
		  requested_path ? std::string(requested_path.data,
	                                   static_cast<size_t>(requested_path.size))
						 : default_lmdb_path();
	if (dir.empty()) {
		return false;
	}

	std::error_code ec;
	std::filesystem::create_directories(dir, ec);
	if (ec) {
		SDL_LogError(SDL_LOG_CATEGORY_ERROR,
		             "create_directories(%s) failed: %s", dir.c_str(),
		             ec.message().c_str());
		return false;
	}

	int rc = mdb_env_create(&env);
	if (rc != 0) {
		log_lmdb_error("mdb_env_create", rc);
		return false;
	}

	rc = mdb_env_set_maxdbs(env, 4);
	if (rc != 0) {
		log_lmdb_error("mdb_env_set_maxdbs", rc);
		close();
		return false;
	}

	rc = mdb_env_set_mapsize(env, LMDB_MAP_SIZE);
	if (rc != 0) {
		log_lmdb_error("mdb_env_set_mapsize", rc);
		close();
		return false;
	}

	rc = mdb_env_open(env, dir.c_str(), 0, 0664);
	if (rc != 0) {
		log_lmdb_error("mdb_env_open", rc);
		close();
		return false;
	}

	MDB_txn *txn{};
	rc = mdb_txn_begin(env, nullptr, 0, &txn);
	if (rc != 0) {
		log_lmdb_error("mdb_txn_begin(write)", rc);
		close();
		return false;
	}

	rc = mdb_dbi_open(txn, STATE_DB_NAME, MDB_CREATE, &state_dbi);
	if (rc != 0) {
		log_lmdb_error("mdb_dbi_open(state)", rc);
		mdb_txn_abort(txn);
		close();
		return false;
	}

	rc = mdb_dbi_open(txn, DUE_DB_NAME, MDB_CREATE, &due_dbi);
	if (rc != 0) {
		log_lmdb_error("mdb_dbi_open(due)", rc);
		mdb_txn_abort(txn);
		close();
		return false;
	}

	rc = mdb_txn_commit(txn);
	if (rc != 0) {
		log_lmdb_error("mdb_txn_commit", rc);
		close();
		return false;
	}

	return true;
}

void States::close() {
	if (env) {
		if (state_dbi) {
			mdb_dbi_close(env, state_dbi);
		}
		if (due_dbi) {
			mdb_dbi_close(env, due_dbi);
		}
		mdb_env_close(env);
	}
	env = nullptr;
	state_dbi = 0;
	due_dbi = 0;
}

// returns [success, was_found]
Pair<bool, bool> States::get(WordId word_id, State &dst) const {
	KLAPPT_PROFILE_SCOPE_N("Engine::States::get");
	MDB_txn *txn;
	auto rc = mdb_txn_begin(env, nullptr, MDB_RDONLY, &txn);
	if (rc != 0) {
		log_lmdb_error("mdb_txn_begin(read)", rc);
		return {false, false};
	}

	unsigned char key_bytes[8];
	auto k = state_key_from_word_id(word_id, key_bytes);
	MDB_val value{};
	rc = mdb_get(txn, state_dbi, &k, &value);
	if (rc == MDB_NOTFOUND) {
		dst.due = 0;
		mdb_txn_abort(txn);
		// success, but not found
		return {true, false};
	}
	if (rc != 0) {
		log_lmdb_error("mdb_get", rc);
		mdb_txn_abort(txn);
		return {false, false};
	}

	const auto ok = read_state_value("state"_v, value, dst);
	mdb_txn_abort(txn);
	return {ok, ok};
}

bool States::set(WordId word_id, const State &new_state) {
	KLAPPT_PROFILE_SCOPE_N("Engine::States::set");
	MDB_txn *txn;
	// open rw-transaction
	constexpr auto MDB_READ_WRITE = 0;
	auto rc = mdb_txn_begin(env, nullptr, MDB_READ_WRITE, &txn);
	if (rc != 0) {
		log_lmdb_error("mdb_txn_begin(write)", rc);
		return false;
	}

	unsigned char state_key_bytes[8]{};
	auto state_key = state_key_from_word_id(word_id, state_key_bytes);

	// we should get an old state to obtain state.due in order to build due_key
	// for due_dbi
	// TODO: optimize this. add another index? store old due in state for
	// convinience?
	MDB_val existing;
	rc = mdb_get(txn, state_dbi, &state_key, &existing);
	if (rc != 0 && rc != MDB_NOTFOUND) {
		log_lmdb_error("mdb_get", rc);
		mdb_txn_abort(txn);
		return false;
	}

	if (rc == 0) {
		// old state exists
		// decode it
		State old_state;
		if (!read_state_value("state"_v, existing, old_state)) {
			mdb_txn_abort(txn);
			return false;
		}
		unsigned char old_due_key_bytes[16]{};
		auto old_due_key =
			  due_key_from(old_state.due, word_id, old_due_key_bytes);
		// remove old entry in due_dbi, because it is linked not only to
		// word_id, but to old due timestamp
		rc = mdb_del(txn, due_dbi, &old_due_key, nullptr);
		if (rc != 0 && rc != MDB_NOTFOUND) {
			log_lmdb_error("mdb_del(old_due)", rc);
			mdb_txn_abort(txn);
			return false;
		}
	}

	// SDL_Log("Saving new state to state_dbi");
	// save new state to state_dbi
	MDB_val value{sizeof(State),
	              const_cast<State *>(std::addressof(new_state))};
	rc = mdb_put(txn, state_dbi, &state_key, &value, 0);
	if (rc != 0) {
		log_lmdb_error("mdb_put", rc);
		mdb_txn_abort(txn);
		return false;
	}

	// save new due timestamp to due_dbi
	// if (new_state.due > 0) {
	unsigned char due_key_bytes[16]{};
	auto due_key = due_key_from(new_state.due, word_id, due_key_bytes);
	MDB_val due_value{};
	rc = mdb_put(txn, due_dbi, &due_key, &due_value, 0);
	if (rc != 0) {
		log_lmdb_error("mdb_put(due)", rc);
		mdb_txn_abort(txn);
		return false;
	}
	// }

	rc = mdb_txn_commit(txn);
	if (rc != 0) {
		log_lmdb_error("mdb_txn_commit", rc);
		return false;
	}

	return true;
}

bool States::erase(WordId word_id) {
	KLAPPT_PROFILE_SCOPE_N("Engine::States::erase");
	MDB_txn *txn;
	auto rc = mdb_txn_begin(env, nullptr, 0, &txn);
	if (rc != 0) {
		log_lmdb_error("mdb_txn_begin(write)", rc);
		return false;
	}

	unsigned char state_key_bytes[8];
	auto state_key = state_key_from_word_id(word_id, state_key_bytes);

	// still we have to get the old state to create old due_key
	MDB_val existing{};
	rc = mdb_get(txn, state_dbi, &state_key, &existing);
	if (rc == MDB_NOTFOUND) {
		// if a state does not exist — we do nothing and it is a success,
		// obviously
		mdb_txn_abort(txn);
		return true;
	}
	if (rc != 0) {
		log_lmdb_error("mdb_get", rc);
		mdb_txn_abort(txn);
		return false;
	}

	State old_state{};
	if (!read_state_value("state"_v, existing, old_state)) {
		mdb_txn_abort(txn);
		return false;
	}

	// if a state exists — its due is not 0, so due_key should exist
	unsigned char due_key_bytes[16];
	auto due_key = due_key_from(old_state.due, word_id, due_key_bytes);
	// remove old due
	rc = mdb_del(txn, due_dbi, &due_key, nullptr);
	if (rc != 0 && rc != MDB_NOTFOUND) {
		log_lmdb_error("mdb_del(due)", rc);
		mdb_txn_abort(txn);
		return false;
	}

	// remove old state
	rc = mdb_del(txn, state_dbi, &state_key, nullptr);
	if (rc != 0) {
		log_lmdb_error("mdb_del(state)", rc);
		mdb_txn_abort(txn);
		return false;
	}

	rc = mdb_txn_commit(txn);
	if (rc != 0) {
		log_lmdb_error("mdb_txn_commit", rc);
		return false;
	}

	return true;
}

bool States::collect_due(Arena &a, Timestamp now, DynArr<WordId> &dst,
                         Size limit) const {
	KLAPPT_PROFILE_SCOPE_N("Engine::States::collect_due");
	MDB_txn *txn;
	auto rc = mdb_txn_begin(env, nullptr, MDB_RDONLY, &txn);
	if (rc != 0) {
		log_lmdb_error("mdb_txn_begin(read)", rc);
		return false;
	}

	MDB_cursor *cursor;
	// TODO: reuse the cursor with mdb_cursor_renew to skip malloc
	rc = mdb_cursor_open(txn, due_dbi, &cursor);
	if (rc != 0) {
		log_lmdb_error("mdb_cursor_open", rc);
		mdb_txn_abort(txn);
		return false;
	}

	MDB_val key;
	MDB_val value;
	rc = mdb_cursor_get(cursor, &key, &value, MDB_FIRST);
	if (limit > 0) {
		// with limit
		for (; rc == 0; rc = mdb_cursor_get(cursor, &key, &value, MDB_NEXT)) {
			if (dst.size >= limit) {
				break;
			}
			Timestamp due;
			WordId word_id;
			if (!parse_due_key(key, due, word_id)) {
				SDL_LogError(SDL_LOG_CATEGORY_ERROR,
				             "Malformed due key in LMDB");

				mdb_cursor_close(cursor);
				mdb_txn_abort(txn);
				return false;
			}
			if (due > now) {
				break;
			}
			dst.push(a, word_id);
		}
	} else {
		// without limit
		// do we need it? add simple count function?
		for (; rc == 0; rc = mdb_cursor_get(cursor, &key, &value, MDB_NEXT)) {
			Timestamp due;
			WordId word_id;
			if (!parse_due_key(key, due, word_id)) {
				SDL_LogError(SDL_LOG_CATEGORY_ERROR,
				             "Malformed due key in LMDB");
				mdb_cursor_close(cursor);
				mdb_txn_abort(txn);
				return false;
			}
			if (due > now) {
				break;
			}
			dst.push(a, word_id);
		}
	}

	if (rc != 0 && rc != MDB_NOTFOUND) {
		log_lmdb_error("mdb_cursor_get", rc);
		mdb_cursor_close(cursor);
		mdb_txn_abort(txn);
		return false;
	}

	mdb_cursor_close(cursor);
	mdb_txn_abort(txn);
	return true;
}
#else
namespace {

constexpr char WEB_STATES_STORAGE_KEY[] = "lexi-sdl.states.v1";

struct WebStatesHeader {
	uint32_t magic{};
	uint32_t version{};
	uint32_t count{};
};

constexpr uint32_t WEB_STATES_MAGIC = 0x4c585331u; // "LXS1"
constexpr uint32_t WEB_STATES_VERSION = 1;

static_assert(std::is_trivially_copyable_v<State>);
static_assert(std::is_trivially_copyable_v<WordId>);

EM_JS(int, web_storage_size, (const char *key_ptr), {
	const key = UTF8ToString(key_ptr);
	try {
		const value = globalThis.localStorage.getItem(key);
		if (value === null) {
			return -1;
		}
		return value.length;
	} catch (e) {
		console.error("localStorage getItem failed", e);
		return -2;
	}
});

EM_JS(int, web_storage_load, (const char *key_ptr, uint8_t *dst, int size), {
	const key = UTF8ToString(key_ptr);
	try {
		const value = globalThis.localStorage.getItem(key);
		if (value === null) {
			return 0;
		}
		if (value.length !== size) {
			return -1;
		}
		for (let i = 0; i < size; ++i) {
			HEAPU8[dst + i] = value.charCodeAt(i) & 0xff;
		}
		return 1;
	} catch (e) {
		console.error("localStorage load failed", e);
		return -2;
	}
});

EM_JS(int, web_storage_save,
      (const char *key_ptr, const uint8_t *src, int size), {
	const key = UTF8ToString(key_ptr);
	try {
		const chunk_size = 0x8000;
		let value = "";
		for (let i = 0; i < size; i += chunk_size) {
			const end = Math.min(i + chunk_size, size);
			value += String.fromCharCode.apply(
			      null, HEAPU8.subarray(src + i, src + end));
		}
		globalThis.localStorage.setItem(key, value);
		return 1;
	} catch (e) {
		console.error("localStorage setItem failed", e);
		return 0;
	}
});

std::pair<uint64_t, WordId> due_entry_from(Timestamp due, WordId word_id) {
	return {static_cast<uint64_t>(std::max<Timestamp>(0, due)), word_id};
}

auto due_lower_bound(std::vector<std::pair<uint64_t, WordId>> &due,
                     const std::pair<uint64_t, WordId> &entry) {
	return std::lower_bound(due.begin(), due.end(), entry);
}

auto due_lower_bound(const std::vector<std::pair<uint64_t, WordId>> &due,
                     const std::pair<uint64_t, WordId> &entry) {
	return std::lower_bound(due.begin(), due.end(), entry);
}

void erase_due_entry(std::vector<std::pair<uint64_t, WordId>> &due,
                     Timestamp old_due, WordId word_id) {
	const auto entry = due_entry_from(old_due, word_id);
	const auto it = due_lower_bound(due, entry);
	if (it != due.end() && *it == entry) {
		due.erase(it);
	}
}

bool persist_web_states(const States &states) {
	const char *storage_key = states.storage_key.empty()
	                                ? WEB_STATES_STORAGE_KEY
	                                : states.storage_key.c_str();
	const uint32_t count = static_cast<uint32_t>(states.states.size());
	const size_t record_size = sizeof(WordId) + sizeof(State);
	const size_t total_size =
	      sizeof(WebStatesHeader) + static_cast<size_t>(count) * record_size;

	std::vector<uint8_t> bytes(total_size);
	auto *cursor = bytes.data();

	const WebStatesHeader header{
	      .magic = WEB_STATES_MAGIC,
	      .version = WEB_STATES_VERSION,
	      .count = count,
	};
	memcpy(cursor, &header, sizeof(header));
	cursor += sizeof(header);

	for (const auto &[word_id, state] : states.states) {
		memcpy(cursor, &word_id, sizeof(word_id));
		cursor += sizeof(word_id);
		memcpy(cursor, &state, sizeof(state));
		cursor += sizeof(state);
	}

	return web_storage_save(storage_key, bytes.data(),
	                        static_cast<int>(bytes.size())) == 1;
}

bool restore_web_states(States &states) {
	const char *storage_key = states.storage_key.empty()
	                                ? WEB_STATES_STORAGE_KEY
	                                : states.storage_key.c_str();
	const int stored_size = web_storage_size(storage_key);
	if (stored_size == -1) {
		return true;
	}
	if (stored_size < 0 ||
	    stored_size < static_cast<int>(sizeof(WebStatesHeader))) {
		SDL_LogError(SDL_LOG_CATEGORY_ERROR,
		             "Loading web states storage failed");
		return false;
	}

	std::vector<uint8_t> bytes(static_cast<size_t>(stored_size));
	const int load_rc = web_storage_load(storage_key, bytes.data(),
	                                     stored_size);
	if (load_rc != 1) {
		SDL_LogError(SDL_LOG_CATEGORY_ERROR,
		             "Loading web states storage failed");
		return false;
	}

	WebStatesHeader header{};
	memcpy(&header, bytes.data(), sizeof(header));
	if (header.magic != WEB_STATES_MAGIC ||
	    header.version != WEB_STATES_VERSION) {
		SDL_LogError(SDL_LOG_CATEGORY_ERROR,
		             "Web states storage header mismatch");
		return false;
	}

	const size_t record_size = sizeof(WordId) + sizeof(State);
	const size_t expected_size = sizeof(WebStatesHeader) +
	                             static_cast<size_t>(header.count) * record_size;
	if (bytes.size() != expected_size) {
		SDL_LogError(SDL_LOG_CATEGORY_ERROR,
		             "Web states storage size mismatch");
		return false;
	}

	states.states.clear();
	states.due.clear();
	states.states.reserve(header.count);
	states.due.reserve(header.count);

	const uint8_t *cursor = bytes.data() + sizeof(WebStatesHeader);
	for (uint32_t i = 0; i < header.count; ++i) {
		WordId word_id{};
		State state{};
		memcpy(&word_id, cursor, sizeof(word_id));
		cursor += sizeof(word_id);
		memcpy(&state, cursor, sizeof(state));
		cursor += sizeof(state);

		states.states.emplace(word_id, state);
		states.due.push_back(due_entry_from(state.due, word_id));
	}
	std::sort(states.due.begin(), states.due.end());
	return true;
}

} // namespace

bool States::open(StrView requested_path) {
	KLAPPT_PROFILE_SCOPE_N("Engine::States::open");
	states.clear();
	due.clear();
	storage_key = requested_path
	                    ? std::string(requested_path.data,
	                                  static_cast<size_t>(requested_path.size))
	                    : WEB_STATES_STORAGE_KEY;
	return restore_web_states(*this);
}

void States::close() {
	storage_key.clear();
}

Pair<bool, bool> States::get(WordId word_id, State &dst) const {
	KLAPPT_PROFILE_SCOPE_N("Engine::States::get");
	const auto it = states.find(word_id);
	if (it == states.end()) {
		dst.due = 0;
		return {true, false};
	}

	dst = it->second;
	return {true, true};
}

bool States::set(WordId word_id, const State &new_state) {
	KLAPPT_PROFILE_SCOPE_N("Engine::States::set");
	const auto existing = states.find(word_id);
	if (existing != states.end()) {
		erase_due_entry(due, existing->second.due, word_id);
	}

	states[word_id] = new_state;

	const auto new_entry = due_entry_from(new_state.due, word_id);
	const auto insert_it = due_lower_bound(due, new_entry);
	due.insert(insert_it, new_entry);
	return persist_web_states(*this);
}

bool States::erase(WordId word_id) {
	KLAPPT_PROFILE_SCOPE_N("Engine::States::erase");
	const auto existing = states.find(word_id);
	if (existing == states.end()) {
		return true;
	}

	erase_due_entry(due, existing->second.due, word_id);
	states.erase(existing);
	return persist_web_states(*this);
}

bool States::collect_due(Arena &a, Timestamp now, DynArr<WordId> &dst,
                         Size limit) const {
	KLAPPT_PROFILE_SCOPE_N("Engine::States::collect_due");
	const uint64_t now_due = static_cast<uint64_t>(std::max<Timestamp>(0, now));
	for (const auto &[due_at, word_id] : due) {
		if (limit > 0 && dst.size >= limit) {
			break;
		}
		if (due_at > now_due) {
			break;
		}
		dst.push(a, word_id);
	}
	return true;
}
#endif

} // namespace Engine
