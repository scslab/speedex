#pragma once

#include "hotstuff/block.h"

#include "hotstuff/vm/speculative_exec_gadget.h"

#include <cstdint>

#include <xdrpp/marshal.h>

namespace hotstuff {

class CountingVM {

	uint64_t state = 0;

	uint64_t last_committed_state = 0;

	SpeculativeExecGadget<std::optional<uint64_t>> height_map;

	void revert_to_last_commitment();

public:

	CountingVM()
		: height_map() 
		{}

	void make_empty_proposal(uint64_t proposal_height);

	xdr::opaque_vec<> get_and_apply_next_proposal(uint64_t proposal_height);

	void apply_block(block_ptr_t block);

	void notify_vm_of_commitment(block_ptr_t blk);

};


} /* hotstuff */
