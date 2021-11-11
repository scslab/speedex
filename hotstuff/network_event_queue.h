#pragma once

#include "config/replica_config.h"

#include "hotstuff/generic_event_queue.h"

namespace hotstuff {

class BlockFetchManager;
class BlockStore;
class EventQueue;
class NetEvent;

class NetworkEventQueue : public GenericEventQueue<NetEvent>
{
	EventQueue& hotstuff_event_queue;
	BlockFetchManager& block_fetch_manager;
	BlockStore& block_store;

	speedex::ReplicaConfig const& config;

	void on_event(NetEvent& e) override final;

public:

	NetworkEventQueue(EventQueue& hotstuff_event_queue, BlockFetchManager& block_fetch_manager, BlockStore& block_store, speedex::ReplicaConfig const& config);

	void validate_and_add_event(NetEvent const& e);
};

} /* hotstuff */
