#pragma once

#include "hotstuff/block_storage/block_fetch_manager.h"
#include "hotstuff/block_storage/block_store.h"
#include "hotstuff/consensus.h"
#include "hotstuff/event_queue.h"
#include "hotstuff/network_event_queue.h"
#include "hotstuff/protocol/hotstuff_protocol_manager.h"
#include "hotstuff/replica_config.h"

#include <xdrpp/types.h>

namespace hotstuff {

class HotstuffAppBase : public HotstuffCore {

	BlockStore block_store;
	BlockFetchManager block_fetch_manager;

	EventQueue event_queue;
	NetworkEventQueue network_event_queue;

	HotstuffProtocolManager protocol_manager;

	speedex::SecretKey secret_key;

public:

	HotstuffAppBase(const ReplicaConfig& config_, ReplicaID self_id, speedex::SecretKey sk)
		: HotstuffCore(config_, self_id)
		, block_store(get_genesis())
		, block_fetch_manager(block_store)
		, event_queue(*this)
		, network_event_queue(event_queue, block_fetch_manager, block_store, config)
		, protocol_manager(event_queue, config, self_id)
		, secret_key(sk)
		{}

	void do_vote(block_ptr_t block, ReplicaID proposer) override final;

	void do_propose(xdr::opaque_vec<>&& body);
};

} /* hotstuff */