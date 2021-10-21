#include "hotstuff/block_storage/block_fetch_server.h"

#include "hotstuff/block_storage/block_store.h"

#include "rpc/rpcconfig.h"

namespace hotstuff {

std::unique_ptr<BlockFetchResponse>
BlockFetchHandler::fetch(std::unique_ptr<BlockFetchRequest> req)
{
	std::unique_ptr<BlockFetchResponse> response = std::make_unique<BlockFetchResponse>();

	for (auto const& hash : req -> reqs)
	{
		auto resp_block = block_store.get_block(hash);
		if (resp_block) {
			if (!(resp_block -> is_flushed_from_memory())) {
				response->responses.push_back(resp_block->to_wire());
			}
		}
	}
	return response;
}

BlockFetchServer::BlockFetchServer(BlockStore& block_store)
	: handler(block_store)
	, ps()
	, fetch_listener(ps, xdr::tcp_listen(HOTSTUFF_BLOCK_FETCH_PORT, AF_INET), false, xdr::session_allocator<void>())
	{
		fetch_listener.register_service(handler);

		std::thread([this] {ps.run();}).detach();
	}

} /* hotstuff */
