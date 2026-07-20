#include "net_worker.h"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <curl/curl.h>

#include "SDL3/SDL_log.h"
#include "SDL3/SDL_timer.h"
#include "app/app_context.h"
#include "app/net_context.h"
#include "app/worker.h"
#include "base/atomic.h"
#include "base/dyn_arr.h"
#include "base/fixed_set.h"
#include "base/measure.h"
#include "base/str_builder.h"
#include "base/str_view.h"
#include "curl/multi.h"

namespace {
static size_t write_memory_callback(void *contents, size_t size, size_t nmemb,
                                    void *userp) {
	size_t realsize = size * nmemb;
	auto *slot = static_cast<NetContext::NetRequestSlot *>(userp);
	auto *data = static_cast<uint8_t *>(contents);

	auto &out = slot->req.memory_buffer;

	bool is_enough_space = static_cast<size_t>(out.size) + realsize <
	                       static_cast<size_t>(out.reserved);

	if (!is_enough_space) {
		SDL_LogError(SDL_LOG_CATEGORY_ERROR,
		             "request %d: Bufffer size (%d) is too small!",
		             slot->req.request_id, out.reserved);
		slot->req.error.copy_from("buffer size is too small"_v);
	}

	auto bytes_to_copy = is_enough_space ? realsize : out.reserved - out.size;
	memcpy(out.data + out.size, data, bytes_to_copy);
	out.size += bytes_to_copy;
	return bytes_to_copy;
}

} // namespace

extern thread_local ThreadContext *_tctx; // in worker.cpp

int SDLCALL NetWorkerThread(void *userdata) {
	auto app_ctx = static_cast<AppContext *>(userdata);
	auto &job_queue = app_ctx->net_worker_job_queue;

	ThreadContext tctx_var = {
		  .a = {1 << 20},
		  .app_ctx = app_ctx,
	};
	_tctx = &tctx_var;

	NetContext netctx{
		  .requests_pool =
				DynArr<NetContext::NetRequestSlot>::filled_zero_or_default(
					  tctx()->a, NetContext::MAX_REQUESTS),
	};

	_tctx->net = &netctx;
	app_ctx->net = &netctx;

	for (Size i{0}; i < netctx.requests_pool.size; ++i) {
		netctx.requests_pool[i].index_in_the_pool = i;
	}

	// DynArr<DynArr<uint8_t>> opt_result_list{nullptr};

	{ // CURL global init
		curl_global_init(CURL_GLOBAL_DEFAULT);
		netctx.multi_handle = curl_multi_init();
	}

	FixedSet<Size, NetContext::MAX_REQUESTS> pending_requests{};
	bool is_queue_empty = false;
	uint64_t queue_touched_last_time_ticks_ms = 0;
	constexpr auto TOUCH_QUEUE_NOT_FASTER_THAN_MS = 16;
	for (;;) {
		bool is_still_have_enough_work =
			  (netctx.active_requests_indices.size >=
		       netctx.active_requests_indices.capacity()) //
			  ||                                          //

			  (pending_requests.size + netctx.active_requests_indices.size >=
		       NetContext::MAX_REQUESTS);

		auto queue_touch_throtler =
			  [&queue_touched_last_time_ticks_ms]() -> bool {
			auto now = SDL_GetTicks();
			if (now - queue_touched_last_time_ticks_ms >
			    TOUCH_QUEUE_NOT_FASTER_THAN_MS) {
				queue_touched_last_time_ticks_ms = now;
				return true;
			}
			return false;
		};

		if (!is_still_have_enough_work && queue_touch_throtler() &&
		    SDL_TryLockMutex(job_queue.mutex)) {
			size_t job_queue_size = job_queue.queue.size();
			is_queue_empty = 0 == job_queue_size;

			size_t awailable_space =
				  std::min(pending_requests.capacity() - pending_requests.size,
			               netctx.active_requests_indices.capacity() -
			                     netctx.active_requests_indices.size);
			if (job_queue_size > awailable_space) {
				SDL_Log("net requests queue is too big: %zu, but we can "
				        "add only %zu",
				        job_queue_size, awailable_space);
				job_queue_size = awailable_space;
			}

			Size i = pending_requests.size + job_queue_size - 1;
			for (; i >= pending_requests.size; --i) {
				Size index = job_queue.queue.front();
				job_queue.queue.pop();
				pending_requests[i] = index;
			}
			pending_requests.size += job_queue_size;

			SDL_UnlockMutex(job_queue.mutex);
		}

		if (is_queue_empty && netctx.active_requests_indices.size == 0) {
			SDL_Log("NET: going to sleep: no pending or active requests");
			SDL_LockMutex(job_queue.mutex);
			SDL_WaitCondition(job_queue.cond, job_queue.mutex);

			// **************************
			// NOTE: replace for goto?..
			SDL_UnlockMutex(job_queue.mutex);
			continue;
			// **************************
		}
		for (auto req_index = pending_requests.top_value(); //
		     !pending_requests.is_empty();                  //
		     pending_requests.pop_top(),
		          req_index = pending_requests.top_value() //
		) {
			auto &pool_slot = netctx.requests_pool[req_index];
			auto &req = pool_slot.req;
			pool_slot.int_data = {};

			SDL_Log("Net Thread: Processing Net Request %d "
			        "(URL: " StrView_Fmt ", FNAME \"%s\", BUF %dB)",
			        req.request_id, StrView_Arg(req.url.view()),
			        req.file_name.mutable_to_cstr(),
			        req.memory_buffer.reserved);
			{ // adding the request
				CURL *easy_handle = curl_easy_init();
				bool is_error_occured{false};

				curl_easy_setopt(easy_handle, CURLOPT_URL,
				                 req.url.mutable_to_cstr());
				curl_easy_setopt(
					  easy_handle, CURLOPT_PRIVATE,
					  &pool_slot); // to get back via CURLINFO_PRIVATE
				if (req.is_write_to_memory()) {
					curl_easy_setopt(easy_handle, CURLOPT_WRITEFUNCTION,
					                 write_memory_callback);
					curl_easy_setopt(easy_handle, CURLOPT_WRITEDATA,
					                 &pool_slot);
				} else {
					FILE *file = fopen(req.file_name.mutable_to_cstr(), "wb");
					if (!file) {
						is_error_occured = true;
						curl_easy_cleanup(easy_handle);
						SDL_LogError(SDL_LOG_CATEGORY_ERROR,
						             "NET: Failed to open file: %s",
						             req.file_name.mutable_to_cstr());

						auto g = tctx()->a.guard();
						StrBuilder strs{};
						strs.push(tctx()->a, "Failed to open file: "_v);
						strs.push(tctx()->a, req.file_name.view());
						req.error.copy_from(strs.join(tctx()->a));
						continue;
					}
					pool_slot.int_data.file_handle = file;
					curl_easy_setopt(easy_handle, CURLOPT_WRITEDATA, file);
				}

				auto mcode =
					  curl_multi_add_handle(netctx.multi_handle, easy_handle);
				if (CURLM_OK != mcode) {
					is_error_occured = true;
					req.error.copy_from(
						  StrView::lit(curl_multi_strerror(mcode)));
				}

				if (!is_error_occured) {
					netctx.active_requests_indices.push_one(req_index);
					pool_slot.int_data.easy_handle = easy_handle;
				} else {
					if (pool_slot.int_data.file_handle) {
						fclose(pool_slot.int_data.file_handle);
					}
					curl_easy_cleanup(easy_handle);

					Atomic::set(&pool_slot.req.status,
					            NetRequest::STATUS_ERROR);
					pool_slot.req.error.copy_from("can't run curl request"_v);
					if (pool_slot.req.on_finished_func) {
						pool_slot.req.on_finished_func(
							  pool_slot.index_in_the_pool, req.request_id,
							  NetRequest::STATUS_ERROR, req.memory_buffer);
					}
					netctx.thread_safe_release_slot(req_index);
				}
			}
		}

		{ // handling active rerequests
			constexpr auto PROGRESS_UPDATE_FREQUENCY_MS = 50;

			int running_handles{};
			// TODO: switch from dedicated thread to using curl_multi_perform in
			// the main thread, increasing fps for the time of downloads?
			curl_multi_perform(netctx.multi_handle, &running_handles);
			auto g = tctx()->a.guard();
			DynArr<Size> to_cancel{};

			// getting progress for active downloads
			// TODO: replace with a callback with timeout
			for (Size i = 0; i < netctx.active_requests_indices.size; ++i) {
				auto &slot =
					  netctx.requests_pool[netctx.active_requests_indices[i]];
				auto *easy_handle = slot.int_data.easy_handle;

				{ // checking cancellation
					if (Atomic::is_true(&slot.req.is_cancelled)) {
						SDL_Log("%d GOING TO CANCEL!!!", slot.req.request_id);
						slot.int_data.is_cancelled = true;
						to_cancel.push(tctx()->a, slot.index_in_the_pool);
					}
				}

				curl_off_t bytes_downloaded = 0;
				curl_off_t bytes_total = 0;

				curl_easy_getinfo(easy_handle, CURLINFO_SIZE_DOWNLOAD_T,
				                  &bytes_downloaded);
				curl_easy_getinfo(easy_handle,
				                  CURLINFO_CONTENT_LENGTH_DOWNLOAD_T,
				                  &bytes_total);

				if (bytes_downloaded != slot.int_data.bytes_downloaded) {
					slot.int_data.bytes_downloaded = bytes_downloaded;
					slot.int_data.bytes_total = bytes_total;

					auto ticks_now = SDL_GetTicks();
					auto time_diff_ms =
						  ticks_now - slot.int_data.last_syncronization_ticks;

					if (time_diff_ms > PROGRESS_UPDATE_FREQUENCY_MS) {
						slot.int_data.last_syncronization_ticks = ticks_now;
						Atomic::set(&slot.req.bytes_downloaded,
						            bytes_downloaded);
						if (bytes_total > 0) {
							Atomic::set(&slot.req.bytes_total, bytes_total);
						}
						Atomic::set(&slot.req.status,
						            NetRequest::STATUS_IN_PROGRESS);
					}
				}
			}

			for (auto idx : to_cancel) {
				auto &slot = netctx.requests_pool[idx];
				// "Removing an easy handle while being in use is
				// perfectly legal and effectively halts the transfer in
				// progress involving that easy handle. All other easy
				// handles and transfers remain unaffected."
				curl_multi_remove_handle(netctx.multi_handle,
				                         slot.int_data.easy_handle);
				netctx.active_requests_indices.remove_by_val(idx);
				// NOTE to be cleaned up by CURLMSG_DONE signal

				// curl_easy_cleanup(slot.int_data.easy_handle);

				// if (slot.int_data.file_handle) {
				// 	fclose(slot.int_data.file_handle);
				// 	slot.int_data.file_handle = nullptr;
				// }
				//
				// netctx.active_requests_indices.remove_by_val(
				// 	  slot.index_in_the_pool);
				// netctx.thread_safe_release_slot(slot.index_in_the_pool);
			}

			// handling finished
			CURLMsg *msg;
			while ((msg = curl_multi_info_read(netctx.multi_handle,
			                                   &running_handles))) {
				if (msg->msg == CURLMSG_DONE) {
					SDL_Log("NET: done...");
					CURL *easy_handle = msg->easy_handle;
					NetContext::NetRequestSlot *slot;
					curl_easy_getinfo(easy_handle, CURLINFO_PRIVATE, &slot);

					if (slot->req.is_write_to_memory()) {
						Atomic::set(&slot->req.bytes_total,
						            slot->req.memory_buffer.size);
					}

					bool is_success = (msg->data.result == CURLE_OK);
					bool is_cancelled = slot->int_data.is_cancelled;

					int req_status =
						  is_success
								? (is_cancelled ? NetRequest::STATUS_CANCELLED
					                            : NetRequest::STATUS_FINISHED)
								: NetRequest::STATUS_ERROR;
					Atomic::set(&slot->req.status, req_status);

					if (!is_success) {
						SDL_LogError(SDL_LOG_CATEGORY_ERROR,
						             "NET: Request %d error: %s",
						             slot->req.request_id,
						             curl_easy_strerror(msg->data.result));

						// auto g = tctx()->a.guard();
						// StrBuilder strs{};
						slot->req.error.copy_from(StrView::lit(
							  curl_easy_strerror(msg->data.result)));
					} else if (is_cancelled) {
						SDL_Log("NET: Request %d was cancelled",
						        slot->req.request_id);
					} else {
						SDL_Log("NET: Request %d completed successfully",
						        slot->req.request_id);
					}

					// calling on_finished before cleaning
					if (slot->req.on_finished_func) {
						slot->req.on_finished_func(
							  slot->index_in_the_pool, slot->req.request_id,
							  req_status, slot->req.memory_buffer);
					}

					// cleanup
					{
						curl_multi_remove_handle(netctx.multi_handle,
						                         easy_handle);
						curl_easy_cleanup(easy_handle);

						if (slot->int_data.file_handle) {
							fclose(slot->int_data.file_handle);
							slot->int_data.file_handle = nullptr;
						}

						netctx.active_requests_indices.remove_by_val(
							  slot->index_in_the_pool);
						netctx.thread_safe_release_slot(
							  slot->index_in_the_pool);
					}
				}
			}

			constexpr auto TIMEOUT_MS = 50;
			if (netctx.active_requests_indices.size > 0) {
				CURLMcode mresult = curl_multi_poll(
					  netctx.multi_handle, nullptr, 0, TIMEOUT_MS, nullptr);

				if (mresult != CURLM_OK) {
					SDL_LogError(SDL_LOG_CATEGORY_ERROR,
					             "curl_multi_poll failed: %s",
					             curl_multi_strerror(mresult));
				}
			}
		}
	}

	SDL_Log("Worker Thread: Exiting cleanly...");
	return 0;
}

void Worker::net_cancel_all(AppContext *ctx) {
	// for (auto req_index : ctx->net->active_requests_indices) {
	Measure m{__FUNCTION__};
	for (Size i{0}; i < ctx->net->requests_pool.size; ++i) { // like really all
		Worker::net_cancel_request(ctx, i);
	}
	m.lap().print("atomics");
	SDL_LockMutex(ctx->net_worker_job_queue.mutex);
	while (!ctx->net_worker_job_queue.queue.empty()) {
		ctx->net_worker_job_queue.queue.pop();
	}
	SDL_UnlockMutex(ctx->net_worker_job_queue.mutex);
	m.lap().print("mutex");
}

void Worker::net_cancel_request(AppContext *ctx, Size req_index_in_the_pool) {
	Atomic::set_true(
		  &ctx->net->requests_pool[req_index_in_the_pool].req.is_cancelled);
}

Size Worker::net_download_file(AppContext *ctx, StrView url, StrView path,
                               NetRequest::OnFinishedFunction cb) {
	auto req_idx = ctx->net->thread_safe_get_unused_slot();
	auto &slot = ctx->net->requests_pool[req_idx];
	slot.req.clear();
	{
		slot.req.on_finished_func = cb;
		slot.req.url.copy_from(url);
		slot.req.file_name.copy_from(path);
	}
	Worker::net_request_push(ctx, req_idx);
	return req_idx;
}

Size Worker::net_download_memory(AppContext *ctx, StrView url,
                                 DynArr<uint8_t> memory_buffer,
                                 NetRequest::OnFinishedFunction cb) {
	auto req_idx = ctx->net->thread_safe_get_unused_slot();
	auto &slot = ctx->net->requests_pool[req_idx];
	slot.req.clear();
	{
		slot.req.on_finished_func = cb;
		slot.req.url.copy_from(url);
		slot.req.memory_buffer = memory_buffer;
	}
	Worker::net_request_push(ctx, req_idx);
	return req_idx;
}

void Worker::net_request_push(AppContext *ctx, Size req_index_in_the_pool) {
	static AtomicInt request_id_counter{0};
	auto &req = ctx->net->requests_pool[req_index_in_the_pool].req;
	if (req.request_id < 0) {
		req.request_id = Atomic::inc(&request_id_counter);
	}
	SDL_Log("Main Thread: Pushing Net Request %d to the net worker queue.",
	        req.request_id);

	SDL_LockMutex(ctx->net_worker_job_queue.mutex);
	ctx->net_worker_job_queue.queue.push(req_index_in_the_pool);
	SDL_SignalCondition(ctx->net_worker_job_queue.cond);
	SDL_UnlockMutex(ctx->net_worker_job_queue.mutex);
}
