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

#include "modlog/log_merge_worker.h"

#include "modlog/account_modification_log.h"

namespace speedex {

LogMergeWorker::LogMergeWorker(AccountModificationLog& modification_log)
	: utils::AsyncWorker()
	, modification_log(modification_log) {
		start_async_thread([this] {run();});
	}

void
LogMergeWorker::run() {
	std::unique_lock lock(mtx);
	while(true) {
		if ((!done_flag) && (!exists_work_to_do())) {
			cv.wait(lock, 
				[this] () { return done_flag || exists_work_to_do();});
		}
		if (done_flag) return;
		if (logs_ready_for_merge) {

			modification_log.merge_in_log_batch();
			
			logs_ready_for_merge = false;
		}
		cv.notify_all();
	}
}

void 
LogMergeWorker::do_merge() {
	wait_for_async_task();
	std::lock_guard lock(mtx);
	logs_ready_for_merge = true;
	cv.notify_all();
}

void 
LogMergeWorker::wait_for_merge_finish() {
	wait_for_async_task();
}
	
} /* speedex */
