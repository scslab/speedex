#include "hotstuff/protocol/hotstuff_protocol_manager.h"

#include "hotstuff/event_queue.h"

namespace hotstuff {

MockClientForSelf::MockClientForSelf(EventQueue& heq, ReplicaID self_id)
	: hotstuff_event_queue(heq)
	, self_id(self_id)
	{}

void
MockClientForSelf::vote(block_ptr_t block, PartialCertificate const& pc)
{
	hotstuff_event_queue.validate_and_add_event(
		Event(
			VoteEvent(
				pc, block, self_id)));
}

void
MockClientForSelf::propose(block_ptr_t block)
{
	hotstuff_event_queue.validate_and_add_event(
		Event(
			ProposalEvent(
				block, self_id)));
}

HotstuffProtocolManager::HotstuffProtocolManager(EventQueue& heq, ReplicaConfig const& config, ReplicaID self_id)
	: config(config)
	, self_id(self_id)
	, self_client(heq, self_id)
	, other_clients()
	{
		auto infos = config.list_info();
		for (auto const& info : infos)
		{
			if (info.id != self_id)
			{
				other_clients.emplace(info.id, std::make_unique<HotstuffProtocolClient>(info));
			}
		}
	}

void 
HotstuffProtocolManager::send_vote_to(block_ptr_t block, PartialCertificate const& pc, ReplicaID target)
{
	if (target == self_id)
	{
		self_client.vote(block, pc);
	} else {
		auto v = std::make_shared<VoteMessage>();
		v->vote = pc;
		v->voter = self_id;
		other_clients.at(target)->vote(v);
	}
}
void 
HotstuffProtocolManager::broadcast_proposal(block_ptr_t block)
{
	// propose to self first.  Any vote messages that come in will
	// always have access to proposed block.
	self_client.propose(block);

	auto p = std::make_shared<ProposeMessage>();
	p->proposal = block->to_wire();
	p->proposer = self_id;

	for (auto const& [_, client] : other_clients) {
		client -> propose(p);
	}
}


} /* hotstuff */
