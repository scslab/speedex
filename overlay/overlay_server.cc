#include "overlay/overlay_server.h"

#include "mempool/mempool.h"

#include "rpc/rpcconfig.h"

#include "xdr/transaction.h"

#include "utils/debug_macros.h"

namespace speedex {

using hotstuff::ReplicaID;
using hotstuff::ReplicaConfig;

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
	}
	return min_max;
}


OverlayServer::OverlayServer(Mempool& mempool, const ReplicaConfig& config, std::string const& overlay_port)
	: handler(mempool, config)
	, ps()
	, overlay_listener(ps, xdr::tcp_listen(overlay_port.c_str(), AF_INET), false, xdr::session_allocator<void>())
	{
		overlay_listener.register_service(handler);

		std::thread th([this] {
			while(!start_shutdown)
			{
				ps.poll(1000);
			}
			std::lock_guard lock(mtx);
			ps_is_shutdown = true;
			cv.notify_all();
		});

		th.detach();
	}

void
OverlayServer::await_pollset_shutdown()
{
	auto done_lambda = [this] () -> bool {
		return ps_is_shutdown;
	};

	std::unique_lock lock(mtx);
	if (!done_lambda()) {
		cv.wait(lock, done_lambda);
	}
	std::printf("shutdown happened\n");
}

} /* speedex */
