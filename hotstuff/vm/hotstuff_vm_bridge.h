#pragma once

#include "hotstuff/hotstuff_debug_macros.h"

#include "hotstuff/vm/speculative_exec_gadget.h"
#include "hotstuff/vm/vm_control_interface.h"

namespace hotstuff {

template<typename VMType>
class HotstuffVMBridge {

	using vm_block_id = typename VMType::block_id;
	using vm_block_type = typename VMType::block_type;

	SpeculativeExecGadget<vm_block_id> speculation_map;

	VMControlInterface<VMType> vm_interface;

	bool initialized;

	void revert_to_last_commitment() {
		VM_BRIDGE_INFO("revert to last commitment: clearing speculation map");
		speculation_map.clear();
	}

	vm_block_id get_block_id(std::unique_ptr<vm_block_type> const& blk) {
		if (blk) {
			return VMType::nonempty_block_id(*blk);
		}
		return VMType::empty_block_id();
	}

	void init_guard() const {
		if (!initialized) {
			throw std::runtime_error("uninitialized vm!");
		}
	}

public:

	HotstuffVMBridge(std::shared_ptr<VMType> vm)
		: speculation_map()
		, vm_interface(vm)
		, initialized(false)
		{}

	void init_clean() {
		vm_interface.init_clean();
		initialized = true;
	}

	void init_from_disk(HotstuffLMDB const& decided_block_index, uint64_t decided_hotstuff_height) {
		vm_interface.init_from_disk(decided_block_index);
		speculation_map.init_from_disk(decided_hotstuff_height);
		initialized = true;
	}

	xdr::opaque_vec<> make_empty_proposal(uint64_t proposal_height) {
		init_guard();
		auto lock = speculation_map.lock();
		VM_BRIDGE_INFO("made empty proposal at height %lu", proposal_height);
		speculation_map.add_height_pair(proposal_height, VMType::empty_block_id());
		return xdr::opaque_vec<>();
	}
	
	xdr::opaque_vec<> 
	get_and_apply_next_proposal(uint64_t proposal_height) {
		init_guard();
		VM_BRIDGE_INFO("start get_and_apply_next_proposal for height %lu", proposal_height);
		auto lock = speculation_map.lock();
		auto proposal = vm_interface.get_proposal();
		if (proposal == nullptr) {
			VM_BRIDGE_INFO("try make nonempty, got empty proposal at height %lu", proposal_height);
			speculation_map.add_height_pair(proposal_height, VMType::empty_block_id());
			return xdr::opaque_vec<>();
		}
		VM_BRIDGE_INFO("made nonempty proposal at height %lu", proposal_height);
		speculation_map.add_height_pair(proposal_height, VMType::nonempty_block_id(*proposal));
		return xdr::xdr_to_opaque(*proposal);
	}

	void apply_block(block_ptr_t blk, HotstuffLMDB::txn& txn) {

		init_guard();

		auto lock = speculation_map.lock();
		
		auto blk_value = blk -> template try_vm_parse<vm_block_type>();
		auto blk_id = get_block_id(blk_value);

		txn.add_decided_block(blk, blk_id);

		if (!speculation_map.empty()) {

			auto const& [lowest_speculative_exec_hs_height, speculative_block_id] = speculation_map.get_lowest_speculative_hotstuff_height();

			if (blk_id == speculative_block_id) {
				return;
			}

			VM_BRIDGE_INFO("rewinding vm");

			revert_to_last_commitment();
			vm_interface.finish_work_and_force_rewind();
		}

		VM_BRIDGE_INFO("adding height entry for %lu", blk -> get_height());
		speculation_map.add_height_pair(blk -> get_height(), blk_id);

		VM_BRIDGE_INFO("submitting height %lu for exec", blk -> get_height());
		vm_interface.submit_block_for_exec(std::move(blk_value));
		VM_BRIDGE_INFO("done submit for exec %lu", blk -> get_height());
	}

	void notify_vm_of_commitment(block_ptr_t blk) {
		init_guard();

		VM_BRIDGE_INFO("consuming height entry for %lu", blk -> get_height());
		auto committed_block_id = speculation_map.on_commit_hotstuff(blk->get_height());

		vm_interface.log_commitment(committed_block_id);
	}

	void put_vm_in_proposer_mode() {
		init_guard();
		
		vm_interface.set_proposer();
	}

	bool proposal_buffer_is_empty() const {
		return vm_interface.proposal_buffer_is_empty();
	}
	void stop_proposals() {
		vm_interface.stop_proposals();
	}

};

} /* hotstuff */
