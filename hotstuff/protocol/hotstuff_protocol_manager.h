#pragma once

#include "hotstuff/block.h"
#include "hotstuff/crypto.h"

#include "hotstuff/protocol/hotstuff_protocol_client.h"

#include "xdr/hotstuff.h"

namespace hotstuff {

class EventQueue;

class MockClientForSelf {
	EventQueue& hotstuff_event_queue;
	speedex::ReplicaID self_id;

public:
	MockClientForSelf(EventQueue& heq, speedex::ReplicaID self_id);

	void vote(block_ptr_t block, PartialCertificate const& pc);
	void propose(block_ptr_t block);
};

class HotstuffProtocolManager {

	const speedex::ReplicaConfig& config;

	speedex::ReplicaID self_id;

	MockClientForSelf self_client;

	using client_t = std::unique_ptr<HotstuffProtocolClient>;

	std::unordered_map<speedex::ReplicaID, client_t> other_clients;

public:

	HotstuffProtocolManager(EventQueue& heq, speedex::ReplicaConfig const& config, speedex::ReplicaID self_id);

	void send_vote_to(block_ptr_t block, PartialCertificate const& pc, speedex::ReplicaID target);
	void broadcast_proposal(block_ptr_t block);

};





} /* hotstuff */