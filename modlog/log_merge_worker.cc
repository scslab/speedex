#include "modlog/log_merge_worker.h"

namespace speedex {

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

			account_modification_log.merge_in_log_batch();
			
			logs_ready_for_merge = false;
		}
		cv.notify_all();
	}
}
	
} /* speedex */