#include "hotstuff/network_event_queue.h"

#include "hotstuff/block_storage/block_store.h"
#include "hotstuff/block_storage/block_fetch_manager.h"
#include "hotstuff/event_queue.h"
#include "hotstuff/network_event.h"

namespace hotstuff {

using speedex::ReplicaConfig;
using speedex::ReplicaID;
using speedex::ReplicaInfo;

NetworkEventQueue::NetworkEventQueue(
	EventQueue& hotstuff_event_queue,
	BlockFetchManager& block_fetch_manager, 
	BlockStore& block_store, 
	ReplicaConfig const& config)
		: GenericEventQueue<NetEvent>()
		, hotstuff_event_queue(hotstuff_event_queue)
		, block_fetch_manager(block_fetch_manager)
		, block_store(block_store)
		, config(config)
		{}

template<typename... Ts> 
struct overloaded : Ts... {
	using Ts::operator()...; 
};

template<class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

void NetworkEventQueue::on_event(NetEvent& event)
{
	std::visit(
		overloaded {
			[this, &event] (VoteNetEvent& vote) -> void {
				//sig is already validated by this point

				auto const& hash = vote.get_block_hash();

				auto block_ptr = block_store.get_block(hash);

				if (block_ptr) {
					hotstuff_event_queue.validate_and_add_event(vote.to_hotstuff_event(block_ptr));
				} 
				else {
					block_fetch_manager.add_fetch_request(hash, vote.get_voter(), std::vector<NetEvent>{event});
				}
			},
			[this, &event] (ProposalNetEvent& proposal) -> void {
				auto blk = proposal.get_proposed_block();


				auto missing_deps = block_store.insert_block(blk);
				if (!missing_deps) {
					add_events_(block_fetch_manager.deliver_block(blk));
					hotstuff_event_queue.validate_and_add_event(proposal.to_hotstuff_event());
				}
				else {
					auto dep_events = block_fetch_manager.deliver_block(blk);
					dep_events.push_back(event);
					if (missing_deps.parent_hash)
					{
						block_fetch_manager.add_fetch_request(*(missing_deps.parent_hash), proposal.get_proposer(), dep_events);
						dep_events.clear();
					}
					if (missing_deps.justify_hash)
					{
						block_fetch_manager.add_fetch_request(*(missing_deps.justify_hash), proposal.get_proposer(), dep_events);
					}
				}
			},
			[this, &event] (BlockReceiveNetEvent& receive) -> void {
				auto blk = receive.get_received_block();

				auto missing_deps = block_store.insert_block(blk);

				if (!missing_deps) {
					add_events_(block_fetch_manager.deliver_block(blk));
				}
				else {
					auto dep_events = block_fetch_manager.deliver_block(blk);
					dep_events.push_back(event);
					if (missing_deps.parent_hash)
					{
						block_fetch_manager.add_fetch_request(*(missing_deps.parent_hash), receive.get_sender(), dep_events);
						dep_events.clear();
					}
					if (missing_deps.justify_hash)
					{
						block_fetch_manager.add_fetch_request(*(missing_deps.justify_hash), receive.get_sender(), dep_events);
					}
				}
			}
		},
		event.net_event);
}

void 
NetworkEventQueue::validate_and_add_event(NetEvent const& e)
{
	if (e.validate(config))
	{
		add_event_(e);
	}
}


} /* hotstuff */
