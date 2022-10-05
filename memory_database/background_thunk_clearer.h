#pragma once

#include <vector>

#include <utils/async_worker.h>

namespace speedex {

//! Free database persistence thunks in the background.
template<typename Clearable>
class BackgroundThunkClearer : public utils::AsyncWorker {

	using utils::AsyncWorker::mtx;
	using utils::AsyncWorker::cv;

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
				cv.wait(
					lock, [this] () {return done_flag || exists_work_to_do();});
			}

			if (done_flag) return;
			do_work();
			cv.notify_all();
		}
	}

public:

	BackgroundThunkClearer()
		: utils::AsyncWorker() {
			start_async_thread([this] {run();});
		}

	~BackgroundThunkClearer() {
		terminate_worker();
	}
	
	void clear_batch(std::vector<Clearable>&& new_work) {
		wait_for_async_task();
		std::lock_guard lock(mtx);
		work = std::move(new_work);
		cv.notify_all();
	}
};


} /* speedex */