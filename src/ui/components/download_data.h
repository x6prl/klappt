#pragma once

#include "base/arena.h"
#include "base/fixed_str.h"

using NetFinishedCallback = void (*)(Size slot_index, int32_t request_id,
                                     int status, StrView file_name,
                                     DynArr<unsigned char> memory_buffer);
struct DownloadData {
	StrView title{};
	enum Status {
		EMPTY,
		TRACKING,
		UNPACKING,
		FINISHED_OK,
		FINISHED_CANCELLED,
		FINISHED_ERROR
	} status = EMPTY;

	Size tracking_req_pool_index{0};
	int32_t tracking_req_id{-1};
	int32_t retry_count{0};

	Size bytes_total = 0;

	// NOTE: these two used for speed calculation
	Size speed_last_dlnow = 0;
	float speed_kibs = -1.f;
	uint64_t speed_last_ticks{};
	FixedStr<128> error{};

	// used for retries
	FixedStr<128> copy_url{};
	FixedStr<128> copy_file_name{};
	DynArr<unsigned char> copy_memory_buffer{};
	NetFinishedCallback copy_on_finished_func{};
};
