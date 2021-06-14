#pragma once

/*! \file mempool_cleaner.h

Background task for defragmenting mempool chunks and removing
committed transactions.
*/

#include "mempool/mempool.h"

#include "utils/async_worker.h"

namespace speedex {

/*! Background task that cleans the mempool.
Specifically, deletes confirmed/failed transactions
from the mempool, and defragments mempool chunks.
*/
class MempoolCleaner : public AsyncWorker {
	using AsyncWorker::mtx;
	using AsyncWorker::cv;

	Mempool& mempool;
	bool do_cleaning = false;
	float* output_measurement;

	bool exists_work_to_do() override final {
		return do_cleaning;
	}

	//! Run background cleaning loop
	void run();

public:
	MempoolCleaner(Mempool& mempool)
		:AsyncWorker()
		, mempool(mempool) {
			start_async_thread([this] () {run();});
		}

	~MempoolCleaner() {
		wait_for_async_task();
		end_async_thread();
	}

	//! Object will write the time it took to clean the mempool
	//! in the background to the measurement_out pointer.
	void do_mempool_cleaning(float* measurement_out);

	//! Wait for background task to complete.
	void wait_for_mempool_cleaning_done() {
		wait_for_async_task();
	}
};

} /* speedex */
