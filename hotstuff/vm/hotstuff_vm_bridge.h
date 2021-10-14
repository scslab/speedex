#pragma once

#include "hotstuff/vm/speculative_exec_gadget.h"
#include "hotstuff/vm/vm_control_interface.h"

namespace hotstuff {

template<typename VMType>
class HotstuffVMBridge {

	using vm_block_id = typename VMType::block_id;
	using vm_block_type = typename VMType::block_type;

	SpeculativeExecGadget<vm_block_id> speculation_map;

	VMControlInterface<VMType> vm_interface;

	void revert_to_last_commitment() {
		speculation_map.clear();
	}

	vm_block_id get_block_id(std::unique_ptr<vm_block_type> const& blk) {
		if (blk) {
			return VMType::nonempty_block_id(*blk);
		}
		return VMType::empty_block_id();
	}

public:

	HotstuffVMBridge(std::shared_ptr<VMType> vm)
		: speculation_map()
		, vm_interface(vm)
		{}

	xdr::opaque_vec<> make_empty_proposal(uint64_t proposal_height) {
		auto lock = speculation_map.lock();
		speculation_map.add_height_pair(proposal_height, VMType::empty_block_id());
		return xdr::opaque_vec<>();
	}
	
	xdr::opaque_vec<> 
	get_and_apply_next_proposal(uint64_t proposal_height) {
		auto lock = speculation_map.lock();
		auto proposal = vm_interface.get_proposal();
		if (proposal == nullptr) {
			speculation_map.add_height_pair(proposal_height, VMType::empty_block_id());
			return xdr::opaque_vec<>();
		}
		speculation_map.add_height_pair(proposal_height, VMType::nonempty_block_id(*proposal));
		return xdr::xdr_to_opaque(*proposal);
	}

	void apply_block(block_ptr_t blk) {

		auto lock = speculation_map.lock();

		auto const& [lowest_speculative_exec_hs_height, speculative_block_id] = speculation_map.get_lowest_speculative_hotstuff_height();

		auto blk_value = blk -> template try_vm_parse<vm_block_type>();
		auto blk_id = get_block_id(blk_value);

		if (blk_id == speculative_block_id) {
			return;
		}

		revert_to_last_commitment();

		speculation_map.add_height_pair(blk -> get_height(), blk_id);

		vm_interface.submit_block_for_exec(std::move(blk_value));
	}

	void notify_vm_of_commitment(block_ptr_t blk) {
		auto committed_block_id = speculation_map.on_commit_hotstuff(blk->get_height());

		vm_interface.log_commitment(committed_block_id);
	}

};

} /* hotstuff */
