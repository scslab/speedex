#pragma once

#include "hotstuff/block.h"

#include "hotstuff/vm/vm_height_map_gadget.h"

#include <cstdint>

#include <xdrpp/marshal.h>

namespace hotstuff {

class CountingVM {

	uint64_t state = 0;

	uint64_t last_committed_state = 0;

	HeightMapGadget height_map;

public:

	CountingVM()
		: height_map() 
		{}

	xdr::opaque_vec<> get_next_proposal();

	void apply_block(block_ptr_t block);

	void notify_vm_of_commitment(block_ptr_t blk);

	void revert_to_last_commitment();
};


} /* hotstuff */
