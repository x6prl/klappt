#pragma once

#include "SDL3/SDL_log.h"
#include <cstdint>
#include <ctime>

namespace {
__attribute__((always_inline)) static inline uint64_t ns_now() {
	timespec ts{};
#if defined(__EMSCRIPTEN__)
	constexpr clockid_t primary_clock = CLOCK_MONOTONIC;
#else
	constexpr clockid_t primary_clock = CLOCK_MONOTONIC_RAW;
#endif
	if (clock_gettime(primary_clock, &ts) != 0 &&
	    clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
		return 0;
	}
	return (uint64_t)ts.tv_sec * 1000000000ull + (uint64_t)ts.tv_nsec;
}
} // namespace

struct Measure {
	const char *name{nullptr};
	uint64_t t0{};
	uint64_t tlap{};
	Measure() { start(); }
	Measure(const char *n) : name{n} { start(); }
	void start() { t0 = ns_now(); }
	Measure &lap() {
		auto now = ns_now();
		tlap = ns_now() - t0;
		t0 = now;
		return *this;
	}
	Measure point() const {
		auto m = *this;
		m.name = "\t[time point] -> ";
		m.lap();
		return m;
	}
	Measure &print(const char *sub = nullptr) {
		SDL_Log("%s\t%s time: %llu ns", name ? name : "", sub ? sub : "",
		        (unsigned long long)tlap);
		return *this;
	}
	Measure &printms(const char *sub = nullptr) {
		SDL_Log("%s\t%s time: %llu ms", name ? name : "", sub ? sub : "",
		        (unsigned long long)tlap / 1000000);
		return *this;
	}
	Measure &printus(const char *sub = nullptr) {
		SDL_Log("%s\t%s time: %llu us", name ? name : "", sub ? sub : "",
		        (unsigned long long)tlap / 1000);
		return *this;
	}
};
