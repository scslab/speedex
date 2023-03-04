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
