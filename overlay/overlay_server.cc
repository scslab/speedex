#include "overlay/overlay_server.h"

#include "mempool/mempool.h"

#include "rpc/rpcconfig.h"

#include "xdr/transaction.h"

namespace speedex {

std::unique_ptr<uint64_t> 
OverlayHandler::mempool_size() {
	return std::make_unique<uint64_t>(mempool.total_size());
}

void
OverlayHandler::forward_txs(std::unique_ptr<ForwardingTxs> txs) {
	xdr::xvector<SignedTransaction> blk;
	try {
		xdr::xdr_from_opaque(*txs, blk);
	} catch (...) {
		return;
	}

	mempool.chunkify_and_add_to_mempool_buffer(std::move(blk));
}

OverlayServer::OverlayServer(Mempool& mempool)
	: handler(mempool)
	, ps()
	, overlay_listener(ps, xdr::tcp_listen(OVERLAY_PORT, AF_INET), false, xdr::session_allocator<void>())
	{
		overlay_listener.register_service(handler);

		std::thread([this] {ps.run();}).detach();
	}

} /* speedex */
