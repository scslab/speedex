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

#include "modlog/file_prealloc_worker.h"
#include "utils/save_load_xdr.h"

#include "speedex/speedex_static_configs.h"

#include <cinttypes>

namespace speedex {

void 
FilePreallocWorker::run() {

	while (true) {
		std::unique_lock lock(mtx);
		if ((!done_flag) && (!exists_work_to_do())) {
			cv.wait(lock, [this] () {return done_flag || exists_work_to_do();});
		}
		if (done_flag) return;
		prealloc(*next_alloc_block);
		next_alloc_block = std::nullopt;
		cv.notify_all();
	}
}

void 
FilePreallocWorker::prealloc(uint64_t block_number)
{
	if constexpr (PREALLOC_BLOCK_FILES)
	{
		auto filename = tx_block_name(block_number);
//		std::printf("preallocating file for block %" PRIu64 " filename %s\n", 
//			block_number, filename.c_str());
		block_fd = preallocate_file(filename.c_str());
	}
}

} /* speedex */
