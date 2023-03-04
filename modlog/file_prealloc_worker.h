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

#include <optional>
#include <cstdint>

#include "config.h"

#include <utils/async_worker.h>
#include <utils/cleanup.h>

namespace speedex {

namespace {
static inline 
std::string tx_block_name(uint64_t block_number) {
	return std::string(ROOT_DB_DIRECTORY) 
		+ std::string(TX_BLOCK_DB) 
		+ std::to_string(block_number) 
		+ std::string(".block");
}
} /* anonymous namespace */

class FilePreallocWorker : public utils::AsyncWorker {
	using utils::AsyncWorker::mtx;
	using utils::AsyncWorker::cv;

	utils::unique_fd block_fd;
	
	std::optional<uint64_t> next_alloc_block;

	bool exists_work_to_do() override final {
		return (bool)next_alloc_block;
	}

	void prealloc(uint64_t block_number);

public:

	FilePreallocWorker()
		: utils::AsyncWorker()
		, block_fd()
		, next_alloc_block(std::nullopt) {
			start_async_thread([this] {run();});
		}

	~FilePreallocWorker() {
		terminate_worker();
	}

	void run();

	void call_prealloc(uint64_t next_block) {
		wait_for_async_task();
		std::lock_guard lock(mtx);
		next_alloc_block = next_block;
		cv.notify_all();
	}

	utils::unique_fd& wait_for_prealloc() {
		wait_for_async_task();
		return block_fd;
	}

	void cancel_prealloc() {
		wait_for_async_task();
		std::lock_guard lock(mtx);
		block_fd.clear();
	}
};

} /* speedex */