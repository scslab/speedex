#include "hotstuff/network_event.h"

namespace hotstuff {

bool
VoteNetEvent::validate(const ReplicaConfig& config) const
{
	auto const& info = config.get_info(voter);

	return cert.validate(info);
}

Event
VoteNetEvent::to_hotstuff_event(block_ptr_t voted_block)
{
	return Event(VoteEvent(std::move(cert), voted_block, voter));
}

bool
ProposalNetEvent::validate(const ReplicaConfig& config) const
{
	return proposed_block -> validate_hotstuff(config);
}

Event
ProposalNetEvent::to_hotstuff_event()
{
	return Event(ProposalEvent(proposed_block, proposer));
}

bool
BlockReceiveNetEvent::validate(const ReplicaConfig& config) const
{
	return received_block -> validate_hotstuff(config);
}

bool
NetEvent::validate(const ReplicaConfig& config) const
{
	return std::visit([&config] (auto const& event) -> bool {
		return event.validate(config);
	}, net_event);
}

} /* hotstuff */