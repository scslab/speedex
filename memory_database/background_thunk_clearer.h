#pragma once

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