#include "mempool/mempool_cleaner.h"

#include "utils/time.h"

namespace speedex {


void 
MempoolCleaner::run() {
	std::unique_lock lock(mtx);
	while(true) {
		if ((!done_flag) && (!exists_work_to_do())) {
		cv.wait(lock, 
			[this] () { return done_flag || exists_work_to_do();});
		}
		if (done_flag) return;
		if (do_cleaning) {
			auto timestamp = init_time_measurement();
			mempool.remove_confirmed_txs();
			mempool.join_small_chunks();
			output_measurement = measure_time(timestamp);

			do_cleaning = false;
		}
		cv.notify_all();
	}
}

void 
MempoolCleaner::do_mempool_cleaning() {
	wait_for_async_task();
	std::lock_guard lock(mtx);
	do_cleaning=true;
	cv.notify_all();
}

} /* speedex */