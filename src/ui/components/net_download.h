#pragma once

#include "base/arena.h"
#include "base/fixed_str.h"
#include <cstdint>

struct NetDownload {
	StrView title{};
	enum Status {
		EMPTY,
		TRACKING,
		FINISHED_OK,
		FINISHED_CANCELLED,
		FINISHED_ERROR
	} status = EMPTY;

	Size tracking_req_pool_index{0};
	int32_t tracking_req_id{-1};

	Size bytes_total = 0;

	// NOTE: these two used for speed calculation
	Size speed_last_dlnow = 0;
	float speed_kibs = -1.f;
	uint64_t speed_last_ticks{};

	FixedStr<128> error{};
};
