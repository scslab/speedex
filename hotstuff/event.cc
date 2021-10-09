#include "hotstuff/event.h"

#include "hotstuff/consensus.h"

namespace hotstuff {


VoteEvent::VoteEvent(PartialCertificate&& _cert, block_ptr_t blk, ReplicaID rid)
	: rid(rid)
	, cert(std::move(cert))
	, block(blk)
	{}

bool
VoteEvent::validate(const ReplicaConfig& config) const {
	auto const& info = config.get_info(rid);
	if (!cert.validate(info))
	{
		return false;
	}

	if (block->get_hash() != cert.hash) {
		throw std::runtime_error("built a VoteEvent where blk->get_hash() != cert.hash");
	}
	return true;
}

void
VoteEvent::operator() (HotstuffCore& core) const {
	core.on_receive_vote(cert, block, rid);
}

ProposalEvent::ProposalEvent(block_ptr_t block, ReplicaID rid)
	: rid(rid)
	, block(block)
	{}

bool
ProposalEvent::validate(const ReplicaConfig& config) const {
	if (block -> get_height() == 0) {
		// has not been entered into block storage yet
		return false;
	}

	// validates qc
	return block -> validate_hotstuff(config);
}

void
ProposalEvent::operator() (HotstuffCore& core) const {
	core.on_receive_proposal(block, rid);
}

bool 
Event::validate(ReplicaConfig const& config) const
{
	return std::visit([&config](auto const& event) -> bool {
		return event.validate(config);
	}, e);
}

void 
Event::operator() (HotstuffCore& core) const
{
	std::visit([&core](auto const& event) -> void {
		event(core);
	}, e);
}

} /* hotstuff */
