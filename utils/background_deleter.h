#pragma once

#include <vector>

#include "utils/async_worker.h"

namespace speedex {

/*! Background task that deletes batches of pointers.

Mainly used for deleting complex data structures, like tries.
*/
template<typename ToBeDeleted>
class BackgroundDeleter : public AsyncWorker {

	using AsyncWorker::mtx;
	using AsyncWorker::cv;

	std::vector<ToBeDeleted*> work;

	bool exists_work_to_do() override final {
		return work.size() != 0;
	}

	//! Delete a batch of pointers.
	void do_deletions() {
		for (ToBeDeleted* ptr : work) {
			delete ptr;
		}
		work.clear();
	}

	void run() {
		while(true) {
			std::unique_lock lock(mtx);

			if ((!done_flag) && (!exists_work_to_do())) {
				cv.wait(
					lock, 
					[this] () {
						return done_flag || exists_work_to_do();
					});
			}

			if (done_flag) return;
			do_deletions();
			cv.notify_all();
		}
	}

public:

	BackgroundDeleter()
		: AsyncWorker() {
			start_async_thread([this] {run();});
		}

	~BackgroundDeleter() {
		wait_for_async_task();
		end_async_thread();
	}
	
	//! Delete a single pointer
	void call_delete(ToBeDeleted* ptr) {
		wait_for_async_task();
		std::lock_guard lock(mtx);
		work.push_back(ptr);
		cv.notify_all();
	}

	//! Delete a batch of pointers
	void call_delete(std::vector<ToBeDeleted*> ptrs) {
		wait_for_async_task();
		std::lock_guard lock(mtx);
		work = ptrs;
		cv.notify_all();
	}
};


} /* speedex */