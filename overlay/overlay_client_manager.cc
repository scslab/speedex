#include "overlay/overlay_client_manager.h"

#include "mempool/mempool.h"

namespace speedex {

using hotstuff::ReplicaConfig;
using hotstuff::ReplicaID;

uint64_t
SelfOverlayClient::get_mempool_size() const {
	return mempool.total_size();
}

void 
SelfOverlayClient::send_txs(DataBuffer data) {
	xdr::xvector<SignedTransaction> blk;
	try {
		xdr::xdr_from_opaque(*(data.data), blk);
	} catch (...) {
		return;
	}

	OVERLAY_INFO("(self) got %lu new txs for mempool, cur size %lu", blk.size(), mempool.total_size());

	handler.log_batch_receipt(self_id, data.buffer_number);

	mempool.chunkify_and_add_to_mempool_buffer(std::move(blk));
}

OverlayClientManager::OverlayClientManager(ReplicaConfig const& config, ReplicaID self_id, Mempool& mempool, OverlayHandler& handler)
	: self_client(mempool, handler, self_id)
	, other_clients()
	{
		auto infos = config.list_info();
		for (auto const& info : infos)
		{
			if (info->id != self_id)
			{
				other_clients.emplace_back(std::make_unique<OverlayClient>(*info, self_id));
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
	OVERLAY_INFO("poll result: min size is %lu (self_size: %lu)", minimum, self_client.get_mempool_size());
	return minimum;
}

void 
OverlayClientManager::poll_foreign_mempool_size() {
	for (auto& other_client : other_clients) {
		other_client->poll_foreign_mempool_size();
	}
}

void 
OverlayClientManager::send_txs(DataBuffer data)
{
	self_client.send_txs(data);

	for (auto& other_client : other_clients) {
		other_client->send_txs(data);
	}
}

} /* speedex */
