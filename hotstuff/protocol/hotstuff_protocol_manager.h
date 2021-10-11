#pragma once

#include "hotstuff/block.h"
#include "hotstuff/crypto.h"

#include "hotstuff/protocol/hotstuff_protocol_client.h"

#include "xdr/hotstuff.h"

namespace hotstuff {

class EventQueue;

class MockClientForSelf {
	EventQueue& hotstuff_event_queue;
	ReplicaID self_id;


public:
	MockClientForSelf(EventQueue& heq, ReplicaID self_id);

	void vote(block_ptr_t block, PartialCertificate const& pc);
	void propose(block_ptr_t block);
};

class HotstuffProtocolManager {

	const ReplicaConfig& config;

	ReplicaID self_id;

	MockClientForSelf self_client;

	using client_t = std::unique_ptr<HotstuffProtocolClient>;

	std::unordered_map<ReplicaID, client_t> other_clients;

public:

	HotstuffProtocolManager(EventQueue& heq, ReplicaConfig const& config, ReplicaID self_id);

	void send_vote_to(block_ptr_t block, PartialCertificate const& pc, ReplicaID target);
	void broadcast_proposal(block_ptr_t block);

};





} /* hotstuff */