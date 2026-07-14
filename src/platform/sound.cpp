#include "sound.h"

#include "app/app_context.h"
#include "app/worker.h"

void record_init(AppContext *ctx) {
	worker_job_push(ctx, {.type = Job::Type::ASR_RECORD_INIT});
}

void record_start(AppContext *ctx) {
	ctx->asr_result = {};
	worker_job_push(ctx, {.type = Job::Type::ASR_RECORD_START});
}

void record_stop(AppContext *ctx) {
	worker_job_push(ctx, {.type = Job::Type::ASR_RECORD_STOP});
}

void record_deinit(AppContext *ctx) {
	worker_job_push(ctx, {.type = Job::Type::ASR_RECORD_DEINIT});
}
void record_play(AppContext *ctx) {
	worker_job_push(ctx, {.type = Job::Type::ASR_RECORD_PLAY});
}

void run_asr(AppContext *ctx) {
	worker_job_push(ctx, {.type = Job::Type::ASR});
}
