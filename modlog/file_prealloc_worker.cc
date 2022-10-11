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
