#pragma once

void web_persist_sync();
void web_persist_begin_batch();
void web_persist_end_batch();

struct WebPersistBatch {
	WebPersistBatch() { web_persist_begin_batch(); }
	~WebPersistBatch() { web_persist_end_batch(); }

	WebPersistBatch(const WebPersistBatch &) = delete;
	WebPersistBatch &operator=(const WebPersistBatch &) = delete;
	WebPersistBatch(WebPersistBatch &&) = delete;
	WebPersistBatch &operator=(WebPersistBatch &&) = delete;
};
