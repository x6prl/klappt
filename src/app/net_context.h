#pragma once

#include <cstdint>

#include <SDL3/SDL_log.h>

#include "base/atomic.h"
#include "base/dyn_arr.h"
#include "base/fixed_set.h"
#include "platform/net_worker.h"
#include "worker.h"

struct NetPlatformContext;

struct NetContext {
	static constexpr Size MAX_ACTIVE_REQUESTS = 4;
	static constexpr Size MAX_REQUESTS = 256;

	struct NetRequestSlot {
		struct NetThreadInternalData {
			uint64_t last_syncronization_ticks{0};
			Size bytes_offset{0};
			Size bytes_downloaded{0};
			Size bytes_total{0};
			FILE *file_handle{nullptr};
			bool is_cancelled{false};
			void *easy_handle{nullptr};
		};

		alignas(64) AtomicInt is_used{Atomic::init(false)};
		Size index_in_the_pool{-1};
		NetRequest req{};
		NetThreadInternalData int_data{};
	};

	void *multi_handle{nullptr};

	// NOTE: indexes to requests_pool
	FixedSet<Size, MAX_ACTIVE_REQUESTS> active_requests_indices{};

	DynArr<NetRequestSlot> requests_pool{};
	alignas(64) AtomicInt maybe_next_free{Atomic::init(0)};

	Size thread_safe_get_unused_slot() {
		auto get_idx = [this]() {
			auto first_free_index =
				  Atomic::inc(&maybe_next_free) % requests_pool.size;
			if (first_free_index >= requests_pool.size) {
				first_free_index = requests_pool.size;
				Atomic::compare_and_swap(&maybe_next_free, first_free_index, 0);
			}
			return first_free_index;
		};

		for (Size i{0}; i < requests_pool.size; ++i) {
			auto idx = get_idx();
			if (Atomic::compare_and_swap(&requests_pool[idx].is_used,
			                             Atomic::FALSE, Atomic::TRUE)) {
				requests_pool[idx].req.clear();
				return idx;
			}
		}
		SDL_LogError(SDL_LOG_CATEGORY_ERROR, "net requests_pool is full!");
		return -1;
	}

	void thread_safe_release_slot(Size idx) {
		Atomic::set(&requests_pool[idx].is_used, Atomic::FALSE);
	}
};
