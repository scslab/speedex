#pragma once

#include "config/replica_config.h"

#include "hotstuff/block_storage/block_fetch_manager.h"
#include "hotstuff/block_storage/block_fetch_server.h"
#include "hotstuff/block_storage/block_store.h"
#include "hotstuff/consensus.h"
#include "hotstuff/event_queue.h"
#include "hotstuff/lmdb.h"
#include "hotstuff/network_event_queue.h"
#include "hotstuff/protocol/hotstuff_protocol_manager.h"
#include "hotstuff/protocol/hotstuff_server.h"
#include "hotstuff/vm/hotstuff_vm_bridge.h"

#include <xdrpp/types.h>

namespace hotstuff {

class HotstuffAppBase : public HotstuffCore {

	BlockStore block_store;
	BlockFetchManager block_fetch_manager; 			// outbound block requests
	BlockFetchServer block_fetch_server; 			// inbound block requests

	EventQueue event_queue;							// events for the protocol
	NetworkEventQueue network_event_queue;			// validated (sig checked) events in from net

	HotstuffProtocolManager protocol_manager; 		// outbound protocol messages
	HotstuffProtocolServer protocol_server;   		// inbound protocol messages

	speedex::SecretKey secret_key;					// sk for this node

	std::mutex qc_wait_mtx;
	std::condition_variable qc_wait_cv;
	std::optional<speedex::Hash> latest_new_qc;
	bool cancel_wait;

	void notify_ok_to_prune_blocks(uint64_t committed_hotstuff_height) override final;

	block_ptr_t find_block_by_hash(speedex::Hash const& hash) override final;

protected:
	
	virtual xdr::opaque_vec<> get_next_vm_block(bool nonempty_proposal, uint64_t hotstuff_height) = 0;

	void on_new_qc(speedex::Hash const& hash) override final;

	//returns highest decision height
	uint64_t reload_decided_blocks();

public:

	HotstuffAppBase(const speedex::ReplicaConfig& config_, speedex::ReplicaID self_id, speedex::SecretKey sk);

	void do_vote(block_ptr_t block, speedex::ReplicaID proposer) override final;
	speedex::Hash do_propose();
	speedex::Hash do_empty_propose();

	bool wait_for_new_qc(speedex::Hash const& expected_next_qc);
	void cancel_wait_for_new_qc();

};

template<typename VMType>
class HotstuffApp : public HotstuffAppBase {

	HotstuffVMBridge<VMType> vm_bridge;

	xdr::opaque_vec<> 
	get_next_vm_block(bool nonempty_proposal, uint64_t hotstuff_height) override final {
		if (!nonempty_proposal) {
			return vm_bridge.make_empty_proposal(hotstuff_height);
		}
		return vm_bridge.get_and_apply_next_proposal(hotstuff_height);
	}

	void apply_block(block_ptr_t blk, HotstuffLMDB::txn& tx) override final {
		vm_bridge.apply_block(blk, tx);
	}

	void notify_vm_of_commitment(block_ptr_t blk) override final {
		vm_bridge.notify_vm_of_commitment(blk);
	}

	void notify_vm_of_qc_on_nonself_block(block_ptr_t b_other) override final {
		//no op needed
	}

public:

	HotstuffApp(const speedex::ReplicaConfig& config, speedex::ReplicaID self_id, speedex::SecretKey sk, std::shared_ptr<VMType> vm)
		: HotstuffAppBase(config, self_id, sk)
		, vm_bridge(vm)
		{}

	void init_clean() {
		decided_hash_index.create_db();
		vm_bridge.init_clean();
	}

	void init_from_disk() {
		decided_hash_index.open_db();
		uint64_t highest_decision_height = reload_decided_blocks();
		vm_bridge.init_from_disk(decided_hash_index, highest_decision_height);
	}

	void put_vm_in_proposer_mode() {
		vm_bridge.put_vm_in_proposer_mode();
	}

	bool proposal_buffer_is_empty() const {
		return vm_bridge.proposal_buffer_is_empty();
	}
	void stop_proposals() {
		vm_bridge.stop_proposals();
	}	
};

} /* hotstuff */
