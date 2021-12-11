#include "hotstuff/block_storage/block_fetch_manager.h"

#include "hotstuff/block_storage/block_store.h"

#include "hotstuff/network_event_queue.h"

#include "utils/debug_macros.h"
#include "utils/debug_utils.h"


namespace hotstuff {

using speedex::ReplicaID;
using speedex::ReplicaConfig;
using speedex::ReplicaInfo;
using speedex::Hash;

RequestContext::RequestContext(speedex::Hash const& request)
	: request(request)
	, block_is_received(false)
	, dependent_network_events()
	, requested_from(0)
	{}

bool 
RequestContext::is_received() const {
	return block_is_received.load(std::memory_order_relaxed);
}

void 
RequestContext::mark_received() {
	return block_is_received.store(true, std::memory_order_relaxed);
}

void
RequestContext::add_network_events(std::vector<NetEvent> events)
{
	dependent_network_events.insert(
		dependent_network_events.end(),
		events.begin(),
		events.end());
}

void
ReplicaFetchQueue::do_gc() {

	for (auto i = 0u; i < outstanding_reqs.size();) {

		auto& req = outstanding_reqs[i];
		if (req -> is_received())
		{
			req = outstanding_reqs.back();
			outstanding_reqs.pop_back();
		} 
		else
		{
			i++;
		}
	}
}

void 
ReplicaFetchQueue::add_request(request_ctx_ptr req) {
	std::lock_guard lock(mtx);
	outstanding_reqs.push_back(req);
	worker.add_request(req -> get_requested_hash());
	if (outstanding_reqs.size() > GC_FREQ) {
		do_gc();
	}
}


void
BlockFetchManager::add_replica(ReplicaInfo const& info, NetworkEventQueue& net_queue) {
	queues.emplace(info.id, std::make_unique<ReplicaFetchQueue>(info, net_queue));
}


void
BlockFetchManager::add_fetch_request(speedex::Hash const& requested_block, ReplicaID request_target, std::vector<NetEvent> const& dependent_events)
{
	if (!config.is_valid_replica(request_target)) {
		return;
	}

	auto it = outstanding_reqs.find(requested_block);
	
	request_ctx_ptr ctx;

	if (it == outstanding_reqs.end()) {
		ctx = std::make_shared<RequestContext>(requested_block);
		outstanding_reqs.emplace(requested_block, ctx);
	} else {
		ctx = it -> second;
	}

	if (!ctx -> was_requested_from(request_target)) {
		queues.at(request_target)->add_request(ctx);
	}

	ctx -> add_network_events(dependent_events);
}

std::vector<NetEvent>
BlockFetchManager::deliver_block(block_ptr_t blk) {
	auto hash = blk -> get_hash();

	auto it = outstanding_reqs.find(hash);

	if (it == outstanding_reqs.end()) {
		HOTSTUFF_INFO("received block %s with no pending request", debug::array_to_str(hash.data(), hash.size()).c_str());
		return {};
	}

	auto req_ctx = it -> second;

	req_ctx -> mark_received();

	outstanding_reqs.erase(it);

	return req_ctx -> get_network_events();
}


} /* hotstuff */