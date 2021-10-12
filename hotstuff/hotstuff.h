#pragma once

#include "hotstuff/block_storage/block_fetch_manager.h"
#include "hotstuff/block_storage/block_fetch_server.h"
#include "hotstuff/block_storage/block_store.h"
#include "hotstuff/consensus.h"
#include "hotstuff/event_queue.h"
#include "hotstuff/network_event_queue.h"
#include "hotstuff/protocol/hotstuff_protocol_manager.h"
#include "hotstuff/protocol/hotstuff_server.h"
#include "hotstuff/replica_config.h"

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

public:

	HotstuffAppBase(const ReplicaConfig& config_, ReplicaID self_id, speedex::SecretKey sk);

	void do_vote(block_ptr_t block, ReplicaID proposer) override final;
	speedex::Hash do_propose(xdr::opaque_vec<>&& body);

	void on_new_qc(speedex::Hash const& hash) override final;
	bool wait_for_new_qc(speedex::Hash const& expected_next_qc);
	void cancel_wait_for_new_qc();
};

} /* hotstuff */