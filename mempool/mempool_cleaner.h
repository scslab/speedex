#pragma once

/**
 * SPEEDEX: A Scalable, Parallelizable, and Economically Efficient Decentralized Exchange
 * Copyright (C) 2023 Geoffrey Ramseyer

 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/*! \file mempool_cleaner.h

Background task for defragmenting mempool chunks and removing
committed transactions.
*/

#include "mempool/mempool.h"

#include <utils/async_worker.h>

namespace speedex {

/*! Background task that cleans the mempool.
Specifically, deletes confirmed/failed transactions
from the mempool, and defragments mempool chunks.
*/
class MempoolCleaner : public utils::AsyncWorker {

	Mempool& mempool;
	bool do_cleaning = false;
	float output_measurement;
	//float* output_measurement;

	bool exists_work_to_do() override final {
		return do_cleaning;
	}

	//! Run background cleaning loop
	void run();

public:
	MempoolCleaner(Mempool& mempool)
		: utils::AsyncWorker()
		, mempool(mempool) {
			start_async_thread([this] () {run();});
		}

	~MempoolCleaner() {
		terminate_worker();
	}

	//! Object will write the time it took to clean the mempool
	//! in the background to the measurement_out pointer.
	void do_mempool_cleaning();//float* measurement_out);

	//! Wait for background task to complete.
	//! Returns time it took for this call to mempool_cleaning
	float wait_for_mempool_cleaning_done() {
		wait_for_async_task();
		std::lock_guard lock(mtx);
		float out = output_measurement;
		return out;
	}
};

} /* speedex */
