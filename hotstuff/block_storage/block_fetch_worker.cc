#include "hotstuff/block_storage/block_fetch_worker.h"

#include "hotstuff/block.h"
#include "hotstuff/network_event.h"
#include "hotstuff/network_event_queue.h"

#include "utils/debug_macros.h"

#include <chrono>
#include <thread>

namespace hotstuff {

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

xdr::xvector<speedex::Hash>
BlockFetchWorker::extract_reqs() {
	xdr::xvector<speedex::Hash> out;
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
			cv.notify_one();
		}
		wait_for_try_open_connection();

		std::unique_ptr<BlockFetchResponse> res = nullptr;
		try {
			res = client->fetch(req);
		} catch(...) {
			readd_request(req);
			res = nullptr;
			clear_connection();
		}
		if (res == nullptr) continue;

		for (auto& response : res->responses)
		{
			auto blk = std::make_shared<HotstuffBlock>(std::move(response), info.id);

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
BlockFetchWorker::add_request(speedex::Hash const& request) {
	std::lock_guard lock(mtx);
	reqs.insert(request);
	cv.notify_all();
}

} /* hotstuff */