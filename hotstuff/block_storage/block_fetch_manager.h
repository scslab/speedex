#pragma once

#include "hotstuff/block.h"
#include "hotstuff/network_event.h"
#include "hotstuff/replica_config.h"

#include "xdr/hotstuff.h"

#include <atomic>
#include <forward_list>
#include <map>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace hotstuff {

//using xdr::operator==;

class BlockStore;

// Not threadsafe in general.
// Worker threads can crawl this to read hash and see whether req was satisfied;
// two threads should not modify/read the network event list or requested_from concurrently.
class RequestContext {
	speedex::Hash request;
	std::atomic<bool> block_is_received;
	std::vector<NetEvent> dependent_network_events;

	ReplicaIDBitMap requested_from;

public:

	RequestContext(speedex::Hash request);

	void add_network_events(std::vector<NetEvent> events);

	bool is_received() const;

	void mark_received();

	std::vector<NetEvent> const& get_network_events() const {
		return dependent_network_events;
	}

	speedex::Hash const& get_requested_hash() const {
		return request;
	}

	bool was_requested_from(ReplicaID rid) const {
		return (requested_from >> rid) & 1;
	}
};

typedef std::shared_ptr<RequestContext> request_ctx_ptr;

class ReplicaFetchQueue {

	// CAS on an iterator (ptr to iterator) could eliminate this mtx.
	std::mutex mtx;

	const ReplicaInfo info;

	std::forward_list<request_ctx_ptr> outstanding_reqs;


public:

	ReplicaFetchQueue(const ReplicaInfo& info)
		: mtx()
		, info(info)
		, outstanding_reqs()
		{}

	std::vector<speedex::Hash> get_next_reqs();

	void add_request(request_ctx_ptr req) {
		std::lock_guard lock(mtx);
		outstanding_reqs.insert_after(outstanding_reqs.before_begin(), req);
	}
};

class BlockFetchManager {

	std::unordered_map<ReplicaID, std::unique_ptr<ReplicaFetchQueue>> queues;
	std::map<speedex::Hash, request_ctx_ptr> outstanding_reqs;

	BlockStore& block_store;

public:

	BlockFetchManager(BlockStore& block_store)
		: queues()
		, outstanding_reqs()
		, block_store(block_store)
		{}

	void
	add_fetch_request(
		speedex::Hash const& requested_block, 
		ReplicaID request_target, 
		std::vector<NetEvent> const& dependent_events);

	void add_replica(ReplicaInfo const& info);

	//returns network events to execute
	std::vector<NetEvent> deliver_block(block_ptr_t blk);
};

} /* hotstuff */