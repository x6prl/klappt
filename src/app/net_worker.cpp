#include "net_worker.h"

#include "app/api.h"
#include "app_context.h"
#include "base/dyn_arr.h"
#include "base/str_view.h"
#include "platform/net.h"
#include <list>

namespace {
std::string to_string(StrView str) {
	return {str.data, static_cast<size_t>(str.size)};
}

} // namespace

int SDLCALL NetWorkerThread(void *userdata) {
	Arena a{};

	auto _ctx = static_cast<AppContext *>(userdata);
	auto &job_queue = _ctx->net_worker_job_queue;

	std::list<int> not_freed_jobs_list{};
	DynArr<DynArr<uint8_t>> opt_result_list{nullptr};

	while (true) {
		SDL_LockMutex(job_queue.mutex);

		while (job_queue.queue.empty() && !job_queue.quit) {
			SDL_WaitCondition(job_queue.cond, job_queue.mutex);
		}

		if (job_queue.quit && job_queue.queue.empty()) {
			SDL_UnlockMutex(job_queue.mutex);
			break;
		}

		NetJob current_job = job_queue.queue.front();
		job_queue.queue.pop();

		SDL_UnlockMutex(job_queue.mutex);

		SDL_Log("Worker Thread: Processing Job %d (Payload: " StrView_Fmt ")",
		        current_job.id, StrView_Arg(current_job.url));

		auto copy_if_exist = [&a](StrView &str) {
			if (str) {
				str = str.copy(a);
			}
		};
		copy_if_exist(current_job.url);
		copy_if_exist(current_job.file_name);
		if (current_job.type != NetJob::Type::_FINISHED) {
			not_freed_jobs_list.push_back(current_job.id);
		}

		bool ok{false};
		DynArr<uint8_t> *opt_result_ptr{nullptr};

		switch (current_job.type) {
		case NetJob::Type::INIT: {
			// ot_test(a);
		} break;
		case NetJob::Type::_FINISHED: {
			not_freed_jobs_list.remove(current_job.id);
		} break;
		case NetJob::Type::GET_TO_FILE: {
			ok = net::get_and_write(to_string(current_job.url),
			                        to_string(current_job.file_name));
		} break;
		case NetJob::Type::GET: {
			auto res = net::get(a, current_job.url);
			opt_result_list.push(a, res);
			opt_result_ptr = &opt_result_list.last();
		} break;
		}

		if (not_freed_jobs_list.empty()) {
			a.clear();
		} else {
			auto push_free_netjob_cb = [](AppContext *ctx, int job_id) {
				net_worker_job_push(
					  ctx, {.id = job_id, .type = NetJob::Type::_FINISHED});
			};
			if (ok) {
				if (current_job.on_success) {
					current_job.on_success(current_job, opt_result_ptr,
					                       push_free_netjob_cb);
				}
			} else {
				if (current_job.on_failure) {
					current_job.on_failure(current_job, push_free_netjob_cb);
				}
			}
		}
	}

	SDL_Log("Worker Thread: Exiting cleanly...");
	return 0;
}

void net_worker_job_push(AppContext *ctx, NetJob job) {
	static int job_id_counter = 0;
	if (job.id < 0) {
		job.id = job_id_counter;
		++job_id_counter;
	}
	SDL_Log("Main Thread: Pushing Job %d to the net worker queue.", job.id);

	SDL_LockMutex(ctx->net_worker_job_queue.mutex);
	ctx->net_worker_job_queue.queue.push(job);
	SDL_SignalCondition(ctx->net_worker_job_queue.cond);
	SDL_UnlockMutex(ctx->net_worker_job_queue.mutex);
}
