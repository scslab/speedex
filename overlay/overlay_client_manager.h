#pragma once

#include "overlay/overlay_client.h"

#include <cstdint>
#include <vector>

namespace speedex {

class Mempool;

class SelfOverlayClient {

	Mempool& mempool;

public:

	SelfOverlayClient(Mempool& mempool)
		: mempool(mempool)
		{}

	uint64_t get_mempool_size() const;
	void send_txs(std::pair<uint32_t, std::shared_ptr<xdr::opaque_vec<>>> txs);
};

class OverlayClientManager {

	SelfOverlayClient self_client;
	std::vector<std::unique_ptr<OverlayClient>> other_clients;

public:
	
	OverlayClientManager(ReplicaConfig const& config, ReplicaID self_id, Mempool& mempool);

	uint64_t get_min_mempool_size() const;

	void poll_foreign_mempool_size();

	void send_txs(std::pair<uint32_t, std::shared_ptr<xdr::opaque_vec<>>> txs);

};






} /* speedex */
