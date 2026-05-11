#pragma once

#include "SDL3/SDL_log.h"

#include "arr.h"

struct Stats {
	Arr<int, 10> top10_max{};
	int n {};
	int64_t sum {};
	void push(int value) {
		sum += value;
		++n;
		for (int i = 0; i < top10_max.size(); ++i) {
			if (value > top10_max[i]) {
				for (int j = top10_max.size() - 1; j > i; --j) {
					top10_max[j] = top10_max[j - 1];
				}
				top10_max[i] = value;
				break;
			}
		}
	}
	int avg() const { return n ? static_cast<int>(sum / n) : 0; }
	void print() const {
		for (int i = 0; i < top10_max.size(); ++i) {
			SDL_Log("stats #%d: %d", i, top10_max[i]);
		}
		SDL_Log("stats avg %d, total %d", avg(), n);
	}
};
