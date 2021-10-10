#pragma once

#include "hotstuff/nonblocking_rpc_client.h"
#include "hotstuff/replica_config.h"

#include "rpc/rpcconfig.h"

#include "utils/async_worker.h"

#include "xdr/hotstuff.h"

#include <xdrpp/srpc.h>

namespace hotstuff {

class NetworkEventQueue;

/*!

	Manages one peer connection.

	Requests a set of blocks, then awaits the response, parses, and validates response (checks sigs).
*/ 
class BlockFetchWorker : public NonblockingRpcClient<xdr::srpc_client<FetchBlocksV1>> {

	using speedex::AsyncWorker::mtx;
	using speedex::AsyncWorker::cv;

	using client_t = xdr::srpc_client<FetchBlocksV1>;


	//xdr::unique_sock socket;
	//std::unique_ptr<client_t> fetch_client;

	xdr::xvector<speedex::Hash> reqs;

	//ReplicaInfo info;

	NetworkEventQueue& network_event_queue;

	bool exists_work_to_do() override final;

	void run();

	void readd_request(BlockFetchRequest const& req);

	/*void wait_for_try_open_connection();

	void open_connection();
	void try_open_connection();
	bool connection_is_open();
	void clear_connection();*/

	const char* get_service() const override final {
		return HOTSTUFF_BLOCK_FETCH_PORT;
	}


public:

	BlockFetchWorker(const ReplicaInfo& info, NetworkEventQueue& network_event_queue);


	~BlockFetchWorker();

	void add_request(speedex::Hash const& request);

	//! wakes requester thread, if sleeping.  Sends any pending request.
	void send_requests();

};

} /* hotstuff */