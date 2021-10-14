#include "hotstuff/vm/counting_vm.h"

namespace hotstuff {

std::strong_ordering 
CountingVMBlockID::operator<=>(const CountingVMBlockID& other) const {
	if ((!value) && (!other.value)) {
		return std::strong_ordering::equal;
	}
	if ((!value) && (other.value)) {
		return std::strong_ordering::less;
	}
	if ((value) && (!other.value)) {
		return std::strong_ordering::greater;
	}
	return (*value) <=> (*other.value);
}

void 
CountingVM::exec_block(const block_type& blk) {
	state = last_committed_state;
	if (blk == state + 1) {
		state ++;
	} else {
		std::printf("got invalid input state, no op");
	}
}
/*
xdr::opaque_vec<> 
CountingVM::get_and_apply_next_proposal(uint64_t proposal_height)
{
	auto lock = height_map.lock();
	state += 1;
	height_map.add_height_pair(proposal_height, state);
	return xdr::xdr_to_opaque(state);
}

void
CountingVM::make_empty_proposal(uint64_t proposal_height)
{
	auto hm_lock = height_map.lock();
	height_map.add_height_pair(proposal_height, std::nullopt);
}


void
CountingVM::apply_block(block_ptr_t blk)
{
	auto hm_lock = height_map.lock();

	auto [lowest_speculative_exec_hs_height, speculative_block] = height_map.get_lowest_speculative_hotstuff_height();

	auto blk_value = blk -> template try_vm_parse<speedex::uint64>();

	//TODO actual equality check
	if (blk_value == speculative_block) {
		return;
	}

	revert_to_last_commitment();

	height_map.add_height_pair(blk -> get_height(), blk_value);


	if (blk_value) {
		if (state + 1 == *blk_value) {
			state ++;
		}
		else {
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
} */

} /* hotstuff */