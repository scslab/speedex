#include "modlog/log_merge_worker.h"

#include "modlog/account_modification_log.h"

namespace speedex {

LogMergeWorker::LogMergeWorker(AccountModificationLog& modification_log)
	: AsyncWorker()
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
