#include "net_worker.h"

#include <filesystem>
#include <utility>

#include <cstddef>
#include <cstdint>
#include <cstdio>

#include <SDL3/SDL_log.h>
#include <SDL3/SDL_timer.h>

#include "app/app_context.h"
#include "app/net_context.h"
#include "app/worker.h"
#include "base/atomic.h"
#include "base/dyn_arr.h"
#ifndef __EMSCRIPTEN__
#include "base/fixed_set.h"
#include "base/str_builder.h"
#endif // !__EMSCRIPTEN__
#include "base/fixed_str.h"
#include "base/str_view.h"
#include "domain/settings.h"
#include "platform/zip.h"
#ifndef __EMSCRIPTEN__
#include "platform/files.h"
#include "platform/fs.h"
#endif // !__EMSCRIPTEN__

extern thread_local ThreadContext *_tctx; // in worker.cpp

#ifndef __EMSCRIPTEN__
#include <curl/curl.h>
#include <curl/multi.h>
#include <curl/system.h>
#include <openssl/ssl.h>
#else
// NOTE: __EMSCRIPTEN__

#include <emscripten/emscripten.h>

namespace {
constexpr size_t FETCH_SCRATCH_BUF_SIZE = 64 * 1024; // 64 KiB static buffer
alignas(
	  16) static unsigned char global_fetch_scratch_buf[FETCH_SCRATCH_BUF_SIZE];
} // namespace

extern "C" EMSCRIPTEN_KEEPALIVE uint8_t *
get_fetch_chunk_buffer(int slotIndex, size_t chunkSize) {
	auto &slot = tctx()->net->requests_pool[slotIndex];

	if (slot.req.is_write_to_memory()) {
		auto &out = slot.req.memory_buffer;
		if (static_cast<size_t>(out.size) + chunkSize <=
		    static_cast<size_t>(out.reserved)) {
			return out.data + out.size;
		}
		SDL_LogError(SDL_LOG_CATEGORY_ERROR,
		             "Callback memory buffer overflow! Buffer size: %" PRSize,
		             out.reserved);
		return nullptr;
	}

	if (chunkSize <= FETCH_SCRATCH_BUF_SIZE) {
		return global_fetch_scratch_buf;
	}

	return nullptr;
}

extern "C" EMSCRIPTEN_KEEPALIVE void
on_fetch_progress(int slotIndex, double downloaded, double total) {
	auto &slot = tctx()->net->requests_pool[slotIndex];
	Atomic::set(&slot.req.bytes_downloaded, static_cast<int64_t>(downloaded));
	if (total > 0)
		Atomic::set(&slot.req.bytes_total, static_cast<int64_t>(total));
	Atomic::set(&slot.req.status, NetRequest::STATUS_IN_PROGRESS);
}

extern "C" EMSCRIPTEN_KEEPALIVE void
on_fetch_chunk(int slotIndex, uint8_t *chunkData, size_t chunkSize) {
	auto &slot = tctx()->net->requests_pool[slotIndex];

	if (slot.int_data.file_handle) {
		fwrite(chunkData, 1, chunkSize, slot.int_data.file_handle);
	} else if (slot.req.is_write_to_memory()) {
		auto &out = slot.req.memory_buffer;
		if (chunkData == out.data + out.size) {
			out.size += chunkSize;
		} else {
			bool is_enough_space = static_cast<size_t>(out.size) + chunkSize <=
			                       static_cast<size_t>(out.reserved);
			if (is_enough_space) {
				memcpy(out.data + out.size, chunkData, chunkSize);
				out.size += chunkSize;
			}
		}
	}
}

extern "C" EMSCRIPTEN_KEEPALIVE void on_fetch_restart_file(int slotIndex) {
	auto &slot = tctx()->net->requests_pool[slotIndex];
	if (slot.int_data.file_handle) {
		fclose(slot.int_data.file_handle);
		slot.int_data.file_handle =
			  fopen(slot.req.file_name.mutable_to_cstr(), "wb");
	}
}

extern "C" EMSCRIPTEN_KEEPALIVE void on_fetch_complete(int slotIndex,
                                                       int httpStatus) {
	auto &slot = tctx()->net->requests_pool[slotIndex];
	if (slot.int_data.file_handle) {
		fclose(slot.int_data.file_handle);
		slot.int_data.file_handle = nullptr;
	}

	bool is_success = (httpStatus == 200 || httpStatus == 206);
	int req_status =
		  is_success ? NetRequest::STATUS_FINISHED : NetRequest::STATUS_ERROR;
	Atomic::set(&slot.req.status, req_status);

	if (slot.req.on_finished_func) {
		slot.req.on_finished_func(slotIndex, slot.req.request_id, req_status,
		                          slot.req.file_name.view(),
		                          slot.req.memory_buffer);
	}
	tctx()->net->thread_safe_release_slot(slotIndex);
}

extern "C" EMSCRIPTEN_KEEPALIVE void on_fetch_error(int slotIndex,
                                                    int httpStatus) {
	auto &slot = tctx()->net->requests_pool[slotIndex];
	if (slot.int_data.file_handle) {
		fclose(slot.int_data.file_handle);
		slot.int_data.file_handle = nullptr;
	}
	Atomic::set(&slot.req.status, NetRequest::STATUS_ERROR);
	slot.req.error.copy_from("Network error"_v);
	if (slot.req.on_finished_func) {
		slot.req.on_finished_func(
			  slotIndex, slot.req.request_id, NetRequest::STATUS_ERROR,
			  slot.req.file_name.view(), slot.req.memory_buffer);
	}
	tctx()->net->thread_safe_release_slot(slotIndex);
}

extern "C" EMSCRIPTEN_KEEPALIVE void on_fetch_cancelled(int slotIndex) {
	auto &slot = tctx()->net->requests_pool[slotIndex];
	if (slot.int_data.file_handle) {
		fclose(slot.int_data.file_handle);
		slot.int_data.file_handle = nullptr;
	}
	Atomic::set(&slot.req.status, NetRequest::STATUS_CANCELLED);
	if (slot.req.on_finished_func) {
		slot.req.on_finished_func(
			  slotIndex, slot.req.request_id, NetRequest::STATUS_CANCELLED,
			  slot.req.file_name.view(), slot.req.memory_buffer);
	}
	tctx()->net->thread_safe_release_slot(slotIndex);
}

EM_JS(void, js_fetch_cancel, (int slotIndex, int requestId), {
	var request = globalThis._wasmFetchRequests[slotIndex];

	if (request && request.requestId == requestId) {
		request.controller.abort();
	}
});

EM_JS(void, js_fetch_start,
      (const char *url, int slotIndex, int requestId, double resumeOffset), {
		  resumeOffset = Number(resumeOffset);
		  var urlStr = UTF8ToString(url);
		  var controller = new AbortController();

		  if (!globalThis._wasmFetchRequests) {
			  globalThis._wasmFetchRequests = {};
		  }

		  globalThis._wasmFetchRequests[slotIndex] = {
			  requestId : requestId,
			  controller : controller
		  };

		  var headers = new Headers();

		  if (resumeOffset > 0) {
			  headers.append('Range', 'bytes=' + resumeOffset + '-');
		  }

		  var doFetch = async function() {
			  try {
				  var response = await fetch(
						urlStr,
						{headers : headers, signal : controller.signal});

				  if (response.status != 200 && response.status != 206) {
					  Module._on_fetch_error(slotIndex, response.status);
					  return;
				  }

				  if (resumeOffset > 0 && response.status == 200) {
					  Module._on_fetch_restart_file(slotIndex);
					  resumeOffset = 0;
				  }

				  if (!response.body) {
					  Module._on_fetch_error(slotIndex, -1);
					  return;
				  }

				  var contentLength = response.headers.get('Content-Length');
				  var total = 0;

				  if (contentLength) {
					  total = Number(contentLength) + resumeOffset;
				  }

				  var reader = response.body.getReader();
				  var receivedLength = resumeOffset;
				  var SCRATCH_SIZE = 65536; // 64 KiB

				  while (true) {
					  var result = await reader.read();

					  if (result.done) {
						  break;
					  }

					  var value = result.value;
					  receivedLength += value.length;

					  Module._on_fetch_progress(slotIndex, receivedLength,
				                                total);

					  var offset = 0;
					  while (offset < value.length) {
						  var chunkSize =
								Math.min(value.length - offset, SCRATCH_SIZE);
						  var ptr = Module._c_get_fetch_chunk_buffer(slotIndex,
					                                                 chunkSize);

						  if (!ptr) {
							  console.error("Fetch buffer overflow!");
							  Module._on_fetch_error(slotIndex, -1);
							  return;
						  }

						  var chunkView =
								value.subarray(offset, offset + chunkSize);
						  Module.HEAPU8.set(chunkView, ptr);

						  Module._on_fetch_chunk(slotIndex, ptr, chunkSize);

						  offset += chunkSize;
					  }
				  }

				  Module._on_fetch_complete(slotIndex, response.status);

			  } catch (e) {
				  if (e.name == 'AbortError') {
					  Module._on_fetch_cancelled(slotIndex);
				  } else {
					  console.error('Fetch error: ' + e.message);
					  Module._on_fetch_error(slotIndex, -1);
				  }
			  } finally {
				  var request = globalThis._wasmFetchRequests[slotIndex];
				  if (request && request.requestId == requestId) {
					  delete globalThis._wasmFetchRequests[slotIndex];
				  }
			  }
		  };

		  doFetch();
	  });

#endif // __EMSCRIPTEN__

#ifndef __EMSCRIPTEN__
namespace {

constexpr auto CACERT_PEM_FILENAME = "cacert.pem"_v;
constexpr auto CACERT_PEM_URL =
	  "https://curl.se/ca/cacert.pem"_v; // TODO: auto-update cacert.pem

size_t write_memory_callback(void *contents, size_t size, size_t nmemb,
                             void *userp) {
	size_t realsize = size * nmemb;
	auto *slot = static_cast<NetContext::NetRequestSlot *>(userp);
	auto *data = static_cast<unsigned char *>(contents);

	auto &out = slot->req.memory_buffer;

	bool is_enough_space = static_cast<size_t>(out.size) + realsize <=
	                       static_cast<size_t>(out.reserved);

	if (!is_enough_space) {
		SDL_LogError(SDL_LOG_CATEGORY_ERROR,
		             "request %d: Buffer size (%" PRSize ") is too small!",
		             slot->req.request_id, out.reserved);
		slot->req.error.copy_from("buffer size is too small"_v);
	}

	auto bytes_to_copy =
		  is_enough_space
				? realsize
				: (out.reserved > out.size ? out.reserved - out.size : 0);
	if (bytes_to_copy > 0) {
		memcpy(out.data + out.size, data, bytes_to_copy);
		out.size += bytes_to_copy;
	}
	return bytes_to_copy;
}
} // namespace

int SDLCALL NetWorkerThread(void *userdata) {
	KLAPPT_PROFILE_THREAD("net");
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

	for (Size i{0}; i < netctx.requests_pool.size; ++i) {
		netctx.requests_pool[i].index_in_the_pool = i;
	}
#ifdef OPENSSL_IS_BORINGSSL
	SDL_Log("using BoringSSL");
#elif OPENSSL_VERSION_NUMBER
	SDL_Log("using OpenSSL");
#else
#error SLL is not availiable
#endif
	{ // CURL init
		curl_global_init(CURL_GLOBAL_DEFAULT);
		netctx.multi_handle = curl_multi_init();
	}

	FixedSet<Size, NetContext::MAX_REQUESTS> pending_requests{};

	MT::run_with_payload(&netctx, [](AppContext *ctx, void *ptr) {
		ctx->net = static_cast<NetContext *>(ptr);
	});

	FixedStr<256> cacert_pem_path{};

	for (;;) {
		{ // reading incoming requests from job queue
			SDL_LockMutex(job_queue.mutex);
			while (
				  !job_queue.queue.empty() &&
				  (pending_requests.size + netctx.active_requests_indices.size <
			       NetContext::MAX_REQUESTS)) {
				Size index = job_queue.queue.front();
				job_queue.queue.pop();
				pending_requests.push_one(index);
			}

			if (pending_requests.is_empty() &&
			    netctx.active_requests_indices.size == 0 &&
			    job_queue.queue.empty()) {
				SDL_Log("NET: going to sleep: no pending or active requests");
				SDL_WaitCondition(job_queue.cond, job_queue.mutex);

				while (!job_queue.queue.empty() &&
				       (pending_requests.size +
				              netctx.active_requests_indices.size <
				        NetContext::MAX_REQUESTS)) {
					Size index = job_queue.queue.front();
					job_queue.queue.pop();
					pending_requests.push_one(index);
				}
			}
			SDL_UnlockMutex(job_queue.mutex);
		}

		for (auto req_index = pending_requests.top_value(); //
		     !pending_requests.is_empty();                  //
		     pending_requests.pop_top(),
		          req_index = pending_requests.top_value() //
		) {
			auto &pool_slot = netctx.requests_pool[req_index];
			auto &req = pool_slot.req;
			pool_slot.int_data = {};

			SDL_Log("Net Thread: Processing Net Request %d (URL: " StrView_Fmt
			        ", FNAME \"%s\", BUF %" PRSize "B)",
			        req.request_id, StrView_Arg(req.url.view()),
			        req.file_name.mutable_to_cstr(),
			        req.memory_buffer.reserved);
			{ // adding the request
				CURL *easy_handle = curl_easy_init();
				bool is_error_occured{false};

				{ // setting TLS
					if (!cacert_pem_path.size) {
						auto &a = tctx()->a;
						auto g = a.guard();
						auto external_path = get_writable_file_path_for(
							  a, CACERT_PEM_FILENAME);
						cacert_pem_path.copy_from(external_path);
						constexpr auto CA_CERT_MINFILESIZE = 100 * 1024;
						if (std::filesystem::exists(
								  cacert_pem_path.mutable_to_cstr()) &&
						    std::filesystem::is_regular_file(
								  cacert_pem_path.mutable_to_cstr()) &&
						    std::filesystem::file_size(
								  cacert_pem_path.mutable_to_cstr()) >=
						          CA_CERT_MINFILESIZE) {
							SDL_Log("cacert.pem found");
							// TODO: add updating mechanism
							// auto on_f_mem = [](Size slot_index,
							//                    int32_t request_id, int
							//                    status, StrView file_name,
							//                    DynArr<unsigned char>
							//                    memory_buffer)
							//                    {
							// 	SDL_Log("mem loaded %d, status %d", request_id,
							// 	        status);
							// 	StrView v{(char *)memory_buffer.data,
							// 	          memory_buffer.size};
							// 	SDL_Log(StrView_Fmt, StrView_Arg(v));
							// };
							// Worker::net_download_memory(
							// 	  tctx()->app_ctx,
							// 	  "https://curl.se/ca/cacert.pem"_v,
							// 	  {
							// 			.data = tctx()->app_ctx->arena_screen()
							//                           .pushN<unsigned
							//                           char>(190000),
							// 			.size = 0,
							// 			.reserved = 190000,
							// 	  },
							// 	  on_f_mem);
						} else {
							auto internal_path = StrView::concat(
								  a, get_app_base_path(), CACERT_PEM_FILENAME);
							FileLoader cacert_in{};
							if (!cacert_in.load_from_path(a, internal_path)) {
								SDL_LogError(SDL_LOG_CATEGORY_ERROR,
								             "Asset %s not found",
								             internal_path.to_cstr(a));
							} else {
								if (cacert_in.size <= 0 ||
								    !file_save_relative(a, CACERT_PEM_FILENAME,
								                        cacert_in.data,
								                        cacert_in.size)) {

									SDL_LogError(SDL_LOG_CATEGORY_ERROR,
									             "Cannot save %s",
									             external_path.to_cstr(a));
								} else {
									SDL_Log(
										  "cacert.pem copied to external path");
								}
							}
						}
					}
					curl_easy_setopt(easy_handle, CURLOPT_CAINFO,
					                 cacert_pem_path.mutable_to_cstr());
				}

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
					FILE *file{nullptr};

					using namespace std::filesystem;
					path fpath = req.file_name.mutable_to_cstr();
					curl_off_t foffset{0};
					if (exists(fpath) && is_regular_file(fpath) &&
					    (foffset = file_size(fpath))) {
						file = fopen(req.file_name.mutable_to_cstr(), "ab");
					} else {
						if (exists(fpath) && foffset) {
							SDL_LogError(SDL_LOG_CATEGORY_ERROR,
							             "something strange instead of a file "
							             "%s... removing all",
							             req.file_name.mutable_to_cstr());
							remove_all(fpath);
							foffset = 0;
						}
						file = fopen(req.file_name.mutable_to_cstr(), "wb");
					}

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
					pool_slot.int_data.bytes_offset = foffset;

					curl_easy_setopt(easy_handle, CURLOPT_WRITEDATA, file);
					curl_easy_setopt(easy_handle, CURLOPT_RESUME_FROM_LARGE,
					                 foffset);
					// NOTE: redirection support
					{
						curl_easy_setopt(easy_handle, CURLOPT_FOLLOWLOCATION,
						                 1);
						curl_easy_setopt(easy_handle, CURLOPT_MAXREDIRS, 2);
					}
					// curl_easy_setopt(easy_handle,
					// CURLOPT_REDIR_PROTOCOLS_STR,
					//                  "http,https");
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
							  NetRequest::STATUS_ERROR,
							  pool_slot.req.file_name.view(),
							  req.memory_buffer);
					}
					netctx.thread_safe_release_slot(req_index);
				}
			}
		}

		{ // handling active rerequests
			constexpr auto PROGRESS_UPDATE_FREQUENCY_MS = 50;

			int running_handles{};
			curl_multi_perform(netctx.multi_handle, &running_handles);
			auto g = tctx()->a.guard();
			DynArr<Size> to_cancel{};

			// getting progress for active downloads
			// TODO: replace with a callback with timeout?...........
			for (Size i = 0; i < netctx.active_requests_indices.size; ++i) {
				auto &slot =
					  netctx.requests_pool[netctx.active_requests_indices[i]];
				auto *easy_handle = slot.int_data.easy_handle;

				{ // checking cancellation
					if (Atomic::is_true(&slot.req.is_cancelled)) {
						SDL_Log("%d GOING TO BE CANCELLED!!!",
						        slot.req.request_id);
						slot.int_data.is_cancelled = true;
						to_cancel.push(tctx()->a, slot.index_in_the_pool);
					}
				}

				// TODO: check storage space
				curl_off_t bytes_total = 0;
				curl_easy_getinfo(easy_handle,
				                  CURLINFO_CONTENT_LENGTH_DOWNLOAD_T,
				                  &bytes_total);
				bytes_total += slot.int_data.bytes_offset;

				curl_off_t bytes_downloaded = 0;
				curl_easy_getinfo(easy_handle, CURLINFO_SIZE_DOWNLOAD_T,
				                  &bytes_downloaded);
				bytes_downloaded += slot.int_data.bytes_offset;

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

					// NOTE: the network request itself
					bool is_result_ok = (msg->data.result == CURLE_OK);
					int response_code{0};
					if (is_result_ok) {
						curl_easy_getinfo(easy_handle, CURLINFO_RESPONSE_CODE,
						                  &response_code);
					}

					// NOTE: the answer for the request
					bool is_success = is_result_ok && (206 == response_code ||
					                                   200 == response_code);

					bool is_cancelled = slot->int_data.is_cancelled;

					int req_status =
						  is_success
								? (is_cancelled ? NetRequest::STATUS_CANCELLED
					                            : NetRequest::STATUS_FINISHED)
								: NetRequest::STATUS_ERROR;
					SDL_Log("REQ %d STATUS %s", slot->req.request_id,
					        req_status == NetRequest::STATUS_FINISHED
					              ? "FINISHED"
					              : "not finished");
					Atomic::set(&slot->req.status, req_status);

					if (!is_success) {
						if (!is_result_ok) {
							SDL_LogError(SDL_LOG_CATEGORY_ERROR,
							             "NET: Request %d error: %s",
							             slot->req.request_id,
							             curl_easy_strerror(msg->data.result));
							slot->req.error.copy_from(StrView::lit(
								  curl_easy_strerror(msg->data.result)));
						} else { // HTTP code is not 200 or 206
							StrView estr{};
							switch (response_code) {
							case 404:
								estr = "File not found"_v;
								break;
							default: {
								auto g = tctx()->a.guard();
								estr = StrView::concat(
									  tctx()->a, "Code: "_v,
									  StrView::from_number(tctx()->a,
								                           response_code));
							} break;
							}
							slot->req.error.copy_from(estr);
						}

					} else if (is_cancelled) {
						SDL_Log("NET: Request %d was cancelled",
						        slot->req.request_id);
					} else {
						SDL_Log("NET: Request %d completed successfully",
						        slot->req.request_id);
					}

					// closing file before calling on_finished
					if (slot->int_data.file_handle) {
						fclose(slot->int_data.file_handle);
						slot->int_data.file_handle = nullptr;
					}

					// calling on_finished before cleaning
					if (slot->req.on_finished_func) {
						slot->req.on_finished_func(
							  slot->index_in_the_pool, slot->req.request_id,
							  req_status, slot->req.file_name.view(),
							  slot->req.memory_buffer);
					}

					// cleanup
					{
						curl_multi_remove_handle(netctx.multi_handle,
						                         easy_handle);
						curl_easy_cleanup(easy_handle);

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

#else
// NOTE: __EMSCRIPTEN__

// TODO: unify nectx initialization
void web_netctx_init(AppContext *ctx) {
	auto tctx_var = new ThreadContext{
		  .a = {1 << 20},
		  .app_ctx = ctx,
	};
	_tctx = tctx_var;
	auto netctx = new NetContext{
		  .requests_pool =
				DynArr<NetContext::NetRequestSlot>::filled_zero_or_default(
					  tctx()->a, NetContext::MAX_REQUESTS),
	};

	for (Size i{0}; i < netctx->requests_pool.size; ++i) {
		netctx->requests_pool[i].index_in_the_pool = i;
	}
	ctx->net = netctx;
	_tctx->net = netctx;
}

#endif // __EMSCRIPTEN__

// void Worker::net_cancel_all(AppContext *ctx) {
// 	Measure m{__FUNCTION__};
// 	for (Size i{0}; i < ctx->net->requests_pool.size; ++i) { // like really all
// 		Worker::net_cancel_request(ctx, i);
// 	}
// 	m.lap().print("atomics");
// 	SDL_LockMutex(ctx->net_worker_job_queue.mutex);
// 	while (!ctx->net_worker_job_queue.queue.empty()) {
// 		ctx->net_worker_job_queue.queue.pop();
// 	}
// 	SDL_UnlockMutex(ctx->net_worker_job_queue.mutex);
// 	m.lap().print("mutex");
// }

void Worker::net_cancel_request(AppContext *ctx, Size req_index_in_the_pool) {
	Atomic::set_true(
		  &ctx->net->requests_pool[req_index_in_the_pool].req.is_cancelled);
#ifdef __EMSCRIPTEN__
	auto &req = ctx->net->requests_pool[req_index_in_the_pool].req;
	js_fetch_cancel(req_index_in_the_pool, req.request_id);
#endif // __EMSCRIPTEN__
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
                                 DynArr<unsigned char> memory_buffer,
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

namespace {
using AType = AssetsDL::Type;
template <AType ASSET_TYPE> //
struct AssetsCallbacks {
	static void set_asset_zip_ready_to_unpack(AppContext *ctx, void *payload) {
		Size bytes_total = reinterpret_cast<int64_t>(payload);
		ctx->settings.asset(ASSET_TYPE).expected_size = bytes_total;
		ctx->settings.asset(ASSET_TYPE).is_zip_ready_to_unpack = true;
		ctx->settings.save(ctx->arena_frame);
		SDL_Log("downloaded SET %d", (int)(ASSET_TYPE));
	};
	static void set_asset_unpacked_and_remove_zip(AppContext *ctx) {
		auto g = ctx->arena_frame.guard();
		ctx->settings.asset(ASSET_TYPE).is_unpacked = true;
		SDL_Log("unpacked SET %d", (int)ASSET_TYPE);
		ctx->settings.asset(ASSET_TYPE).is_zip_ready_to_unpack = false;
		SDL_Log("zip ready UNSET %d", (int)ASSET_TYPE);
		auto zip_path = ctx->settings.assets.zip_path(
			  ctx->arena_frame, ASSET_TYPE, ctx->settings.tr_language);
		ctx->settings.save(ctx->arena_frame);
		std::filesystem::remove(
			  std::string_view{zip_path.data, (size_t)zip_path.size});
		ctx->settings.asset(ASSET_TYPE).is_zip_removed = true;
		ctx->settings.save(ctx->arena_frame);
		SDL_Log("zip REMOVED %d", (int)ASSET_TYPE);
	};
	static void job_asset_unpack() {
		bool res = false;
		auto &a = tctx()->a;
		auto g = a.guard();
		auto &s = tctx()->app_ctx->settings;
		res = unpack_asset(s.assets.zip_path(a, ASSET_TYPE, s.tr_language));
		if (res) {
			MT::run(set_asset_unpacked_and_remove_zip);
		} else {
			SDL_LogError(SDL_LOG_CATEGORY_ERROR, "%s, %d: failed to unpack",
			             __FILE__, __LINE__);
		}
	};
	static void on_zip_downloaded(Size slot_index, int32_t request_id,
	                              int status, StrView file_name,
	                              DynArr<unsigned char> memory_buffer) {
		(void)request_id;
		(void)memory_buffer;
		auto &slot = tctx()->net->requests_pool[slot_index];
		if (status == NetRequest::STATUS_FINISHED) {
			MT::run_with_payload(
				  (void *)(int64_t)Atomic::get(&slot.req.bytes_total),
				  set_asset_zip_ready_to_unpack);
			Worker::job_push(tctx()->app_ctx, {.func = job_asset_unpack});
		} else {
			SDL_LogError(SDL_LOG_CATEGORY_ERROR,
			             "%s, %d: Downloading of " StrView_Fmt
			             " wasn't successfull",
			             __FILE__, __LINE__, StrView_Arg(file_name));
		}
	};
};

template <AType... Types> struct AssetsCallbacksTables {
	static constexpr auto on_zip_downloaded_table =
		  std::array{&AssetsCallbacks<Types>::on_zip_downloaded...};

	static constexpr auto job_asset_unpack_table =
		  std::array{&AssetsCallbacks<Types>::job_asset_unpack...};
};

// NOTE: thorougly check the order)
// enum class Type : int32_t {
// 	XAPIAN_TR = 0,
// 	OPTIONAL_XAPIAN_DE = 1,
// 	OPTIONAL_TTS,
// 	OPTIONAL_ASR,
// 	_COUNT
// };
using AssetsCbs =
	  AssetsCallbacksTables<AType::XAPIAN_TR, AType::OPTIONAL_XAPIAN_DE,
                            AType::OPTIONAL_XAPIAN_EN, AType::OPTIONAL_TTS,
                            AType::OPTIONAL_ASR>;

} // namespace

Size Worker::net_download_and_unpack_asset(AppContext *ctx,
                                           AssetsDL::Type asset_type) {
	auto &a = ctx->arena_frame;
	StrView out_fname = ctx->settings.assets.zip_path(
		  ctx->arena_frame, asset_type, ctx->settings.tr_language);

	bool is_zip_dowloaded_and_present =
		  ctx->settings.asset(asset_type).is_zip_ready_to_unpack;

	if (is_zip_dowloaded_and_present) {
		std::filesystem::path p = {out_fname.to_cstr(a)};
		if (!std::filesystem::exists(p)             // does not exist
		    || !std::filesystem::is_regular_file(p) // not a regular file
		    || (std::filesystem::file_size(p) !=
		        (uintmax_t)ctx->settings.asset(asset_type)
		              .expected_size) // file size mismatch
		) {
			SDL_LogError(SDL_LOG_CATEGORY_ERROR,
			             "got wrong is_zip_ready_to_unpack flag for %d asset",
			             std::to_underlying(asset_type));
			std::filesystem::remove_all(p);
			is_zip_dowloaded_and_present = false;
		}
	}

	StrView url = ctx->settings.assets.zip_url(ctx->arena_frame, asset_type,
	                                           ctx->settings.tr_language);
	if (!is_zip_dowloaded_and_present) {
		return Worker::net_download_file(
			  ctx, url, out_fname,
			  AssetsCbs::on_zip_downloaded_table[std::to_underlying(
					asset_type)]);
	} else {
		SDL_Log(StrView_Fmt " already downloaded", StrView_Arg(out_fname));
		auto is_unpacked = ctx->settings.asset(asset_type).is_unpacked;
		if (!is_unpacked) {
			Worker::job_push(
				  tctx()->app_ctx,
				  {.func = AssetsCbs::job_asset_unpack_table[std::to_underlying(
						 asset_type)]});
		} else {
			SDL_Log(StrView_Fmt " already unpacked", StrView_Arg(out_fname));
			return -2;
		}
		return -1;
	}
}

void Worker::net_request_push(AppContext *ctx, Size req_index_in_the_pool) {
	static AtomicInt request_id_counter{1};
	auto &req = ctx->net->requests_pool[req_index_in_the_pool].req;
	if (req.request_id < 0) {
		req.request_id = Atomic::inc(&request_id_counter);
	}
#ifdef __EMSCRIPTEN__
	long resume_offset = 0;
	if (!req.is_write_to_memory()) {
		std::error_code ec;
		if (std::filesystem::exists(req.file_name.mutable_to_cstr(), ec)) {
			resume_offset = std::filesystem::file_size(
				  req.file_name.mutable_to_cstr(), ec);
		}
		auto &req_slot = ctx->net->requests_pool[req_index_in_the_pool];
		req_slot.int_data.file_handle = fopen(req.file_name.mutable_to_cstr(),
		                                      resume_offset > 0 ? "ab" : "wb");
	}
	js_fetch_start(req.url.mutable_to_cstr(), req_index_in_the_pool,
	               req.request_id, static_cast<double>(resume_offset));
#else
	// NOTE: !__EMSCRIPTEN__

	SDL_Log("Main Thread: Pushing Net Request %d to the net worker queue.",
	        req.request_id);

	SDL_LockMutex(ctx->net_worker_job_queue.mutex);
	ctx->net_worker_job_queue.queue.push(req_index_in_the_pool);
	SDL_SignalCondition(ctx->net_worker_job_queue.cond);
	SDL_UnlockMutex(ctx->net_worker_job_queue.mutex);
#endif // !__EMSCRIPTEN__
}
// void Worker::net_request_retry(AppContext *ctx, Size req_index_in_the_pool) {
// 	auto &req = ctx->net->requests_pool[req_index_in_the_pool].req;
// 	req.request_id = -1;
// 	Atomic::set(&req.bytes_downloaded, 0);
// 	Atomic::set(&req.bytes_total, 0);
// 	Atomic::set(&req.speed_kbit_sec, 0);
// 	Atomic::set(&req.status, NetRequest::STATUS_INIT);
// 	Atomic::set(&req.is_cancelled, false);
// 	net_request_push(ctx, req_index_in_the_pool);
// }
