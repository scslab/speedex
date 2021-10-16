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
	//, socket()
	//, fetch_client()
	, reqs()
//	, info(info)
	, network_event_queue(network_event_queue)
	{
		start_async_thread([this] () {run();});
	}


/*
void
BlockFetchWorker::try_open_connection()
{
	try {
		open_connection();
	} catch (...)
	{
		HOTSTUFF_INFO("failed to open connection on rid=%d", info.id);
		clear_connection();
	}
}

void wait() {
	using namespace std::chrono_literals;
	std::this_thread::sleep_for(1000ms);
}

void
BlockFetchWorker::wait_for_try_open_connection()
{
	if (connection_is_open() || done_flag)
	{
		return;
	}
	while(true) {
		try_open_connection();
		if (connection_is_open() || done_flag)
		{
			return;
		}
		wait();
	}
}

bool
BlockFetchWorker::connection_is_open() {
	return fetch_client != nullptr;
}

void
BlockFetchWorker::clear_connection() {
	fetch_client = nullptr;
	socket.clear();
}

void
BlockFetchWorker::open_connection() 
{
	socket = info.tcp_connect(HOTSTUFF_BLOCK_FETCH_PORT);
	fetch_client = std::make_unique<client_t>(socket.get());
}
 */

void 
BlockFetchWorker::readd_request(BlockFetchRequest const& req)
{
	std::lock_guard lock(mtx);
	reqs.insert(
		reqs.end(),
		req.reqs.begin(),
		req.reqs.end());
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
			req.reqs = std::move(reqs);
			reqs.clear();
			// used for shutdown wait
			cv.notify_one();
		}
		wait_for_try_open_connection();

		std::unique_ptr<BlockFetchResponse> res = nullptr;
		try {
			std::unique_ptr<BlockFetchResponse> res = client->fetch(req);
		} catch(...) {
			readd_request(req);
			res = nullptr;
		}
		if (res == nullptr) continue;


		for (auto& response : res->responses)
		{
			network_event_queue.validate_and_add_event(
				NetEvent(
					BlockReceiveNetEvent(std::make_shared<HotstuffBlock>(std::move(response), info.id), info.id)));
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
	reqs.push_back(request);
	cv.notify_all();
}

/*
void 
BlockFetchWorker::send_requests() {
	std::lock_guard lock(mtx);
	//No waiting for async task - if we notify worker thread while it's not waiting,
	// it'll recheck for work upon being done anyways
	cv.notify_all();
}*/

} /* hotstuff */