#pragma once

#include "xdr/hotstuff.h"

#include <memory>

#include <xdrpp/pollset.h>
#include <xdrpp/srpc.h>

namespace hotstuff {

class BlockStore;

class BlockFetchHandler {

	BlockStore& block_store;

public:
	using rpc_interface_type = FetchBlocksV1;

	BlockFetchHandler(BlockStore& block_store)
		: block_store(block_store)
		{}

	//rpc methods
	std::unique_ptr<BlockFetchResponse> fetch(std::unique_ptr<BlockFetchRequest> req);
};

class BlockFetchServer {
	BlockFetchHandler handler;

	xdr::pollset ps;
	xdr::srpc_tcp_listener<> fetch_listener;

public:

	BlockFetchServer(BlockStore& block_store);
};


} /* hotstuff */
