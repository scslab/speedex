#pragma once

#include <vector>

#include "utils/async_worker.h"

namespace speedex {

template<typename Clearable>
class BackgroundThunkClearer : public AsyncWorker {

	using AsyncWorker::mtx;
	using AsyncWorker::cv;

	std::vector<Clearable> work;

	bool exists_work_to_do() override final {
		return work.size() != 0;
	}

	void do_work() {
		for (auto& clearable : work) {
			clearable.clear();
		}
		work.clear();
	}

	void run() {
		while(true) {
			std::unique_lock lock(mtx);

			if ((!done_flag) && (!exists_work_to_do())) {
				cv.wait(lock, [this] () {return done_flag || exists_work_to_do();});
			}

			if (done_flag) return;
			do_work();
			cv.notify_all();
		}
	}

public:

	BackgroundThunkClearer()
		: AsyncWorker() {
			start_async_thread([this] {run();});
		}

	~BackgroundThunkClearer() {
		wait_for_async_task();
		end_async_thread();
	}
	
	void clear_batch(std::vector<Clearable>&& new_work) {
		wait_for_async_task();
		std::lock_guard lock(mtx);
		work = std::move(new_work);
		cv.notify_all();
	}
};


} /* speedex */