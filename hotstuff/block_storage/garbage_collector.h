#pragma once

#include "hotstuff/block.h"

#include "utils/async_worker.h"

#include <optional>

namespace hotstuff {

class BlockGarbageCollector : public speedex::AsyncWorker {
	using speedex::AsyncWorker::cv;
	using speedex::AsyncWorker::mtx;

	std::optional<uint64_t> hotstuff_height_to_gc;

	std::vector<block_ptr_t> gc_list;

	std::vector<block_ptr_t> gc_list_buffer;

	bool exists_work_to_do() override final;

	void push_buffer_to_gc_list();
	void do_gc(uint64_t hotstuff_height);

	void run();

public:

	BlockGarbageCollector();
	~BlockGarbageCollector();

	void invoke_gc(uint64_t hotstuff_height);
	void add_block(block_ptr_t blk);


};


} /* hotstuff */
