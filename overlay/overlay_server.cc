#include "overlay/overlay_server.h"

#include "mempool/mempool.h"

#include "rpc/rpcconfig.h"

#include "xdr/transaction.h"

#include "utils/debug_macros.h"

namespace speedex {

std::unique_ptr<uint64_t> 
OverlayHandler::mempool_size() {
	return std::make_unique<uint64_t>(mempool.total_size());
}

void
OverlayHandler::log_batch_receipt(ReplicaID source, uint32_t batch_num) {
	auto it = max_seen_batch_nums.find(source);
	if (it == max_seen_batch_nums.end())
	{
		return;
	}
	it -> second = std::max(it -> second.load(std::memory_order_relaxed), batch_num);
	//max_seen_batch_num.at(*sender) = std::max(max_seen_batch_num.at(source), *tx_batch_num);
}

void
OverlayHandler::forward_txs(std::unique_ptr<ForwardingTxs> txs, std::unique_ptr<uint32_t> tx_batch_num, std::unique_ptr<ReplicaID> sender) {

	try {
		log_batch_receipt(*sender, *tx_batch_num);
		xdr::xvector<SignedTransaction> blk;
			xdr::xdr_from_opaque(*txs, blk);

		OVERLAY_INFO("got %lu new txs for mempool, cur size %lu", blk.size(), mempool.total_size());

		mempool.chunkify_and_add_to_mempool_buffer(std::move(blk));
	} catch (...) {
		return;
	}

}

uint32_t
OverlayHandler::get_min_max_seen_batch_nums() const
{
	uint32_t min_max = UINT32_MAX;
	for (auto const& kv : max_seen_batch_nums) {
		min_max = std::min(kv.second.load(std::memory_order_relaxed), min_max);
		std::printf("got %lu from %lu\n", kv.second.load(std::memory_order_relaxed), kv.first);
	}
	return min_max;
}


OverlayServer::OverlayServer(Mempool& mempool, ReplicaConfig& config)
	: handler(mempool, config)
	, ps()
	, overlay_listener(ps, xdr::tcp_listen(OVERLAY_PORT, AF_INET), false, xdr::session_allocator<void>())
	{
		overlay_listener.register_service(handler);

		std::thread([this] {ps.run();}).detach();
	}

} /* speedex */
