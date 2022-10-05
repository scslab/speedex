#pragma once

/*! \file log_merge_worker.h

Merges in a batch of serial account modification logs
in a background thread.
*/

#include <utils/async_worker.h>

namespace speedex {

class AccountModificationLog;

/*! Runs a background thread that, when requested, merges all of the 
serial account modification logs (that are cached in the main account log's
threadlocal cache) into the actual main log.
*/
class LogMergeWorker : public utils::AsyncWorker {

	AccountModificationLog& modification_log;
	bool logs_ready_for_merge = false;

	bool exists_work_to_do() override final {
		return logs_ready_for_merge;
	}

	void run();

public:
	//! Start the mod log background thread
	LogMergeWorker(AccountModificationLog& modification_log);

	//! Background thread is signaled to terminate when object leaves scope.
	~LogMergeWorker() {
		terminate_worker();
	}

	//! Initiate a call to merge in modification logs in the background.
	void do_merge();
	
	//! Wait for background merge to finish.
	void wait_for_merge_finish();
};

}
