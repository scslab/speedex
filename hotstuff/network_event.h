#pragma once

#include "config/replica_config.h"

#include "hotstuff/crypto.h"
#include "hotstuff/event.h"

#include "xdr/hotstuff.h"

namespace hotstuff {

class HotstuffBlock;

class VoteNetEvent {

	PartialCertificate cert;
	speedex::ReplicaID voter;

public:

	VoteNetEvent(std::unique_ptr<VoteMessage> v);

	bool validate(const speedex::ReplicaConfig& config) const;

	speedex::Hash const& get_block_hash() const {
		return cert.hash;
	}

	speedex::ReplicaID get_voter() const {
		return voter;
	}

	Event to_hotstuff_event(block_ptr_t voted_block);
};

class ProposalNetEvent {
	block_ptr_t proposed_block;
	speedex::ReplicaID proposer;

public:

	ProposalNetEvent(std::unique_ptr<ProposeMessage> p);

	bool validate(const speedex::ReplicaConfig& config) const;

	speedex::Hash const& get_parent_hash() const {
		return proposed_block->get_parent_hash();
	}

	speedex::ReplicaID get_proposer() const {
		return proposer;
	}

	block_ptr_t get_proposed_block() const {
		return proposed_block;
	}

	Event to_hotstuff_event();

};

class BlockReceiveNetEvent {
	block_ptr_t received_block;
	speedex::ReplicaID sender;

public:

	BlockReceiveNetEvent(block_ptr_t blk, speedex::ReplicaID sender)
		: received_block(blk)
		, sender(sender)
		{}

	block_ptr_t get_received_block() const {
		return received_block;
	}

	speedex::ReplicaID get_sender() const {
		return sender;
	}

	bool validate(const speedex::ReplicaConfig& config) const;
};

struct NetEvent {
	std::variant<VoteNetEvent, ProposalNetEvent, BlockReceiveNetEvent> net_event;

	template<typename NetEventSubtype>
	NetEvent(NetEventSubtype const& event)
		: net_event(event)
		{}

	bool validate(const speedex::ReplicaConfig& config) const;
};

} /* hotstuff */