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

#include <memory>
#include <vector>

#include <utils/async_worker.h>

namespace speedex {

/*! Background task that deletes batches of pointers.

Mainly used for deleting complex data structures, like tries.
*/
template<typename ToBeDeleted>
class BackgroundDeleter : public utils::AsyncWorker {

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

			if (done_flag) break;
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
		terminate_worker();
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

template<typename garbage_t>
class ThunkGarbage {
	std::vector<garbage_t*> to_delete;

public:
	ThunkGarbage() : to_delete() {}

	ThunkGarbage(const ThunkGarbage&) = delete;
	ThunkGarbage(ThunkGarbage&&) = delete;

	~ThunkGarbage() {
		for (auto* ptr : to_delete) {
			delete ptr;
		}
	}

	void add(garbage_t* garbage) {
		to_delete.push_back(garbage);
	}

	//! Add a vector of garbage pointers
	//! (i.e. the result of release() on another garbage object).
	void add(std::vector<garbage_t*> ptrs) {
		to_delete.insert(to_delete.end(), ptrs.begin(), ptrs.end());
	}

	//! Release the list of pointers (Caller becomes responsible for deleting
	//! these pointers).
	std::vector<garbage_t*>
	__attribute__((warn_unused_result))
	release() {
		auto out = std::move(to_delete);
		to_delete.clear();
		return out;
	}
};


} /* speedex */