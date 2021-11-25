#include "overlay/overlay_client_manager.h"

#include "mempool/mempool.h"

namespace speedex {

uint64_t
SelfOverlayClient::get_mempool_size() const {
	return mempool.total_size();
}

void 
SelfOverlayClient::send_txs(std::pair<uint32_t, std::shared_ptr<xdr::opaque_vec<>>> txs) {
	xdr::xvector<SignedTransaction> blk;
	try {
		xdr::xdr_from_opaque(*(txs.second), blk);
	} catch (...) {
		return;
	}

	OVERLAY_INFO("(self) got %lu new txs for mempool, cur size %lu", blk.size(), mempool.total_size());

	mempool.chunkify_and_add_to_mempool_buffer(std::move(blk));
}

OverlayClientManager::OverlayClientManager(ReplicaConfig const& config, ReplicaID self_id, Mempool& mempool)
	: self_client(mempool)
	, other_clients()
	{
		auto infos = config.list_info();
		for (auto const& info : infos)
		{
			if (info.id != self_id)
			{
				other_clients.emplace_back(std::make_unique<OverlayClient>(info));
			}
		}
	}

uint64_t 
OverlayClientManager::get_min_mempool_size() const {
	uint64_t minimum = self_client.get_mempool_size();

	for (auto const& other_client : other_clients) {
		auto res = other_client->get_cached_foreign_mempool_size();
		if (res) {
			minimum = std::min(minimum, *res);
		}
	}
	OVERLAY_INFO("poll result: min size is %lu", minimum);
	return minimum;
}

void 
OverlayClientManager::poll_foreign_mempool_size() {
	for (auto& other_client : other_clients) {
		other_client->poll_foreign_mempool_size();
	}
}

void 
OverlayClientManager::send_txs(std::pair<uint32_t, std::shared_ptr<xdr::opaque_vec<>>> txs)
{
	self_client.send_txs(txs);

	for (auto& other_client : other_clients) {
		other_client->send_txs(txs);
	}
}

} /* speedex */
