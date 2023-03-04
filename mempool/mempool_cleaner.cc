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

#include "mempool/mempool_cleaner.h"

#include <utils/time.h>

namespace speedex {


void 
MempoolCleaner::run() {
	std::unique_lock lock(mtx);
	while(true) {
		if ((!done_flag) && (!exists_work_to_do())) {
		cv.wait(lock, 
			[this] () { return done_flag || exists_work_to_do();});
		}
		if (done_flag) return;
		if (do_cleaning) {
			auto timestamp = utils::init_time_measurement();
			mempool.remove_confirmed_txs();
			mempool.join_small_chunks();
			output_measurement = utils::measure_time(timestamp);

			do_cleaning = false;
		}
		cv.notify_all();
	}
}

void 
MempoolCleaner::do_mempool_cleaning() {
	wait_for_async_task();
	std::lock_guard lock(mtx);
	do_cleaning=true;
	cv.notify_all();
}

} /* speedex */