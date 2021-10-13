#include "hotstuff/vm/counting_vm.h"

namespace hotstuff {

xdr::opaque_vec<> 
CountingVM::get_next_proposal()
{
	state += 1;
	return xdr::xdr_to_opaque(state);
}

void
CountingVM::apply_block(block_ptr_t blk)
{
	auto value = blk -> template try_vm_parse<speedex::uint64>();

	if (value) {
		if (state + 1 == *value) {
			state ++;
			height_map.add_height_pair(blk -> get_height(), state);
		} else {
			std::printf("invalid vm proposal");
		}
	} else {
		std::printf("got empty vm proposal");
	}
}

void 
CountingVM::notify_vm_of_commitment(block_ptr_t blk)
{
	auto committed_vm_height = height_map.on_commit_hotstuff(blk -> get_height());
	if (committed_vm_height)
	{
		if (last_committed_state + 1 != *committed_vm_height)
		{
			throw std::runtime_error("skipped a commitment notification");
		}
		last_committed_state = *committed_vm_height;
	}
}

void 
CountingVM::revert_to_last_commitment()
{
	height_map.clear();
	state = last_committed_state;	
}

} /* hotstuff */