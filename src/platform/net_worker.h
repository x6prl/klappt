#pragma once

#include "base/dyn_arr.h"
#include "base/fixed_str.h"
#include "base/str_view.h"
#include "base/atomic.h"
#include "app/worker.h"
#include "app/assets_dl.h"

struct NetRequest {
	using OnFinishedFunction = void (*)(Size slot_index, int32_t request_id,
	                                    int status, StrView file_name,
	                                    DynArr<uint8_t> memory_buffer);

	static constexpr int STATUS_INIT = 0;
	static constexpr int STATUS_IN_PROGRESS = 1;
	static constexpr int STATUS_FINISHED = 2;
	static constexpr int STATUS_CANCELLED = 3;
	static constexpr int STATUS_ERROR = 4;

	int32_t request_id{};
	OnFinishedFunction on_finished_func{};
	FixedStr<128> url{};
	FixedStr<128> file_name{};
	FixedStr<128> error{};
	DynArr<uint8_t> memory_buffer{}; // NOTE: should be preallocated
	alignas(64) AtomicInt bytes_downloaded{};
	alignas(64) AtomicInt bytes_total{};
	alignas(64) AtomicInt speed_kbit_sec{};
	alignas(64) AtomicInt status{};
	alignas(64) AtomicInt is_cancelled{};

	bool is_write_to_memory() const { return memory_buffer.reserved; }

	void clear() {
		request_id = -1;
		on_finished_func = nullptr;
		url.size = 0;
		file_name.size = 0;
		error.size = 0;
		memory_buffer = {};
		Atomic::set(&bytes_downloaded, 0);
		Atomic::set(&bytes_total, 0);
		Atomic::set(&speed_kbit_sec, 0);
		Atomic::set(&status, STATUS_INIT);
		Atomic::set(&is_cancelled, false);
	}
};

namespace Worker {
Size net_download_and_unpack_asset(AppContext *ctx, AssetsDL::Type type);

// void net_cancel_all(AppContext *ctx);
void net_cancel_request(AppContext *ctx, Size req_index_in_the_pool);
Size net_download_file(AppContext *ctx, StrView url, StrView path,
                       NetRequest::OnFinishedFunction cb);
Size net_download_memory(AppContext *ctx, StrView url,
                         DynArr<uint8_t> memory_buffer,
                         NetRequest::OnFinishedFunction cb);

void net_request_retry(AppContext *ctx, Size req_index_in_the_pool);
void net_request_push(AppContext *ctx, Size req_index_in_the_pool);
} // namespace Worker

int SDLCALL NetWorkerThread(void *userdata);
