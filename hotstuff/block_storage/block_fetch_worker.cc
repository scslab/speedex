#include "hotstuff/block_storage/block_fetch_worker.h"

#include "hotstuff/block.h"
#include "hotstuff/network_event.h"
#include "hotstuff/network_event_queue.h"

#include "utils/debug_macros.h"

#include <chrono>
#include <thread>

namespace hotstuff {

using speedex::ReplicaInfo;
using speedex::ReplicaID;
using speedex::Hash;

BlockFetchWorker::BlockFetchWorker(const ReplicaInfo& info, NetworkEventQueue& network_event_queue)
	: NonblockingRpcClient<client_t>(info)
	, reqs()
	, network_event_queue(network_event_queue)
	{
		start_async_thread([this] () {run();});
	}

void 
BlockFetchWorker::readd_request(BlockFetchRequest const& req)
{
	std::lock_guard lock(mtx);
	reqs.insert(
		req.reqs.begin(),
		req.reqs.end());
}

xdr::xvector<Hash>
BlockFetchWorker::extract_reqs() {
	xdr::xvector<Hash> out;
	for (auto const& hash : reqs) {
		out.push_back(hash);
	}
	reqs.clear();
	return out;
}

void
BlockFetchWorker::run() {
	while(true) {
		BlockFetchRequest req;
		{
			std::unique_lock lock(mtx);
			if ((!done_flag) && (!exists_work_to_do())) {
			cv.wait(
				lock, 
				[this] () {
					return done_flag || exists_work_to_do();
				});
			}
			if (done_flag) return;
			req.reqs = extract_reqs();
			reqs.clear();
			// used for shutdown wait
			cv.notify_all();
		}
		//wait_for_try_open_connection();

		std::unique_ptr<BlockFetchResponse> res = try_action<BlockFetchResponse>(
			[this, &req] {
				return client -> fetch(req);
			});
		if (res == nullptr) {
			readd_request(req);
			continue;
		}

		for (auto& response : res->responses)
		{
			auto blk = HotstuffBlock::receive_block(std::move(response), info.id);

			if (blk -> validate_hash()) {

			}

			network_event_queue.validate_and_add_event(
				NetEvent(
					BlockReceiveNetEvent(
						blk, info.id)));
		}
	}
}

bool 
BlockFetchWorker::exists_work_to_do() {
	return reqs.size() > 0;
}

void 
BlockFetchWorker::add_request(Hash const& request) {
	std::lock_guard lock(mtx);
	reqs.insert(request);
	cv.notify_all();
}

} /* hotstuff */