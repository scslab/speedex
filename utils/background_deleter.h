#pragma once

#include "utils/async_worker.h"

namespace speedex {

template<typename ToBeDeleted>
class BackgroundDeleter : public AsyncWorker {

	using AsyncWorker::mtx;
	using AsyncWorker::cv;

	ToBeDeleted* to_delete = nullptr;

	bool exists_work_to_do() override final {
		return to_delete != nullptr;
	}

	void do_deletions() {
		if (to_delete != nullptr) {
			delete to_delete;
			to_delete = nullptr;
		}
	}

	void run() {
		while(true) {
			std::unique_lock lock(mtx);

			if ((!done_flag) && (!exists_work_to_do())) {
				cv.wait(lock, [this] () {return done_flag || exists_work_to_do();});
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
	
	void call_delete(ToBeDeleted* ptr) {
		wait_for_async_task();
		std::lock_guard lock(mtx);
		to_delete = ptr;
		cv.notify_all();
	}
};


} /* speedex */