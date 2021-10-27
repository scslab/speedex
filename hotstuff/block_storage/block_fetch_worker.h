#pragma once

#include "config/replica_config.h"

#include "rpc/rpcconfig.h"

#include "utils/nonblocking_rpc_client.h"

#include "xdr/hotstuff.h"

#include <xdrpp/srpc.h>

#include <set>

namespace hotstuff {

class NetworkEventQueue;

/*!
	Manages one peer connection.

	Requests a set of blocks, then awaits the response, parses, and validates response (checks sigs).
*/ 
class BlockFetchWorker : public speedex::NonblockingRpcClient<xdr::srpc_client<FetchBlocksV1>> {

	using speedex::AsyncWorker::mtx;
	using speedex::AsyncWorker::cv;

	using client_t = xdr::srpc_client<FetchBlocksV1>;

	std::set<speedex::Hash> reqs;

	NetworkEventQueue& network_event_queue;

	bool exists_work_to_do() override final;

	void run();

	void readd_request(BlockFetchRequest const& req);

	const char* get_service() const override final {
		return HOTSTUFF_BLOCK_FETCH_PORT;
	}

	xdr::xvector<speedex::Hash> extract_reqs();


public:

	BlockFetchWorker(const speedex::ReplicaInfo& info, NetworkEventQueue& network_event_queue);

	void add_request(speedex::Hash const& request);

};

} /* hotstuff */