#include "hotstuff/block_storage/garbage_collector.h"

namespace hotstuff {


BlockGarbageCollector::BlockGarbageCollector()
	: speedex::AsyncWorker()
	, hotstuff_height_to_gc(std::nullopt)
	, gc_list()
	, gc_list_buffer()
	{
		start_async_thread(
			[this] {run();}
		);
	}

BlockGarbageCollector::~BlockGarbageCollector() {
	wait_for_async_task();
	end_async_thread();
}


void 
BlockGarbageCollector::push_buffer_to_gc_list() {
	gc_list.insert(
		gc_list.end(), 
		std::make_move_iterator(gc_list_buffer.begin()), 
		std::make_move_iterator(gc_list_buffer.end()));
	gc_list_buffer.clear();
}

bool
BlockGarbageCollector::exists_work_to_do() {
	return ((bool) hotstuff_height_to_gc);
}

void
BlockGarbageCollector::do_gc(uint64_t hotstuff_height) {
	for (auto iter = gc_list.begin(); iter != gc_list.end();) {
		auto& blk = *iter;
		if (blk -> get_height() < hotstuff_height) {
			blk -> flush_from_memory();
			*iter = gc_list.back();
			gc_list.pop_back();
		} else {
			iter++;
		}
	}
}

void
BlockGarbageCollector::run() {

	while(true) {
		uint64_t hotstuff_height = 0;
		{
			std::unique_lock lock(mtx);
			if ((!done_flag) && (!exists_work_to_do())) {
				cv.wait(lock, 
					[this] () { return done_flag || exists_work_to_do();});
			}
			if (done_flag) return;

			push_buffer_to_gc_list();
			hotstuff_height = *hotstuff_height_to_gc;
			hotstuff_height_to_gc = std::nullopt;
		}
		do_gc(hotstuff_height);
		cv.notify_all();
	}
}

void 
BlockGarbageCollector::invoke_gc(uint64_t hotstuff_height) {
	std::lock_guard lock(mtx);
	hotstuff_height_to_gc = hotstuff_height;
	cv.notify_all();
}

void 
BlockGarbageCollector::add_block(block_ptr_t blk) {
	std::lock_guard lock(mtx);
	gc_list_buffer.push_back(blk);
}

} /* hotstuff */
