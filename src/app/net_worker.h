#pragma once

#include "base/dyn_arr.h"
#include "base/str_view.h"
#include "worker.h"

struct NetJob {
	using ContinueCallback = void (*)(AppContext *, int);
	// NOTE: call cont(job_id) after using the resources
	using OnSuccessCallback = void (*)(NetJob job, DynArr<uint8_t> *opt_data,
	                                   ContinueCallback cont);
	// NOTE: call cont(job_id) after using the resources
	using OnFailureCallback = void (*)(NetJob job, ContinueCallback cont);

	enum class Type {
		// RECORD_AND_ASR,
		_FINISHED,
		INIT,
		GET,
		GET_TO_FILE,
	};
	int id{-1};
	Type type{NetJob::Type::INIT};
	StrView url{};
	StrView file_name{};
	// NOTE: call cont(job_id) after using the resources
	OnSuccessCallback on_success{nullptr};
	// NOTE: call cont(job_id) after using the resources
	OnFailureCallback on_failure{nullptr};
};

void net_worker_job_push(AppContext *ctx, NetJob job);

int SDLCALL NetWorkerThread(void *userdata);
