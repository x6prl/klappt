#pragma once

#include "SDL3/SDL_log.h"
#include <cstdint>
#include <ctime>

struct Measure {
	const char *name{nullptr};
	uint64_t tstart{};
	uint64_t tlast{};
	uint64_t tlap{};

	static inline uint64_t now_ns() noexcept {
		struct timespec ts;
		clock_gettime(CLOCK_MONOTONIC, &ts);
		return static_cast<uint64_t>(ts.tv_sec) * 1000000000ULL +
		       static_cast<uint64_t>(ts.tv_nsec);
	}

	Measure() { start(); }
	explicit Measure(const char *n) : name{n} { start(); }

	void start() noexcept {
		tstart = now_ns();
		tlast = tstart;
		tlap = 0;
	}

	Measure &lap() noexcept {
		uint64_t now = now_ns();
		tlap = now - tlast;
		tlast = now;
		return *this;
	}

	Measure &total() noexcept {
		uint64_t now = now_ns();
		tlap = now - tstart;
		return *this;
	}

	uint64_t elapsed_lap_ns() const noexcept { return now_ns() - tlast; }
	uint64_t elapsed_total_ns() const noexcept { return now_ns() - tstart; }

	Measure &print(const char *sub = nullptr) {
		SDL_Log("%s\t%s time: %lu ns", name ? name : "", sub ? sub : "", tlap);
		return *this;
	}
	Measure &printus(const char *sub = nullptr) {
		SDL_Log("%s\t%s time: %lu us", name ? name : "", sub ? sub : "",
		        tlap / 1000);
		return *this;
	}
	Measure &printms(const char *sub = nullptr) {
		SDL_Log("%s\t%s time: %lu ms", name ? name : "", sub ? sub : "",
		        tlap / 1000000);
		return *this;
	}
};
