#pragma once

/*! \file log_merge_worker.h

Merges in a batch of serial account modification logs
in a background thread.
*/

#include "utils/async_worker.h"
#include "modlog/account_modification_log.h"

namespace speedex {

/*! Runs a background thread that, when requested, merges all of the 
serial account modification logs (that are cached in the main account log's
threadlocal cache) into the actual main log.
*/
class LogMergeWorker : public AsyncWorker {
	using AsyncWorker::mtx;
	using AsyncWorker::cv;

	AccountModificationLog& modification_log;
	bool logs_ready_for_merge = false;

	bool exists_work_to_do() override final {
		return logs_ready_for_merge;
	}

	void run();

public:
	//! Start the mod log background thread
	LogMergeWorker(AccountModificationLog& modification_log)
		: AsyncWorker()
		, modification_log(modification_log) {
			start_async_thread([this] {run();});
		}

	//! Background thread is signaled to terminate when object leaves scope.
	~LogMergeWorker() {
		wait_for_async_task();
		end_async_thread();
	}

	//! Initiate a call to merge in modification logs in the background.
	void do_merge() {
		wait_for_async_task();
		std::lock_guard lock(mtx);
		logs_ready_for_merge = true;
		cv.notify_all();
	}

	//! Wait for background merge to finish.
	void wait_for_merge_finish() {
		wait_for_async_task();
	}
};

}
