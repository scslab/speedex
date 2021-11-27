#pragma once

#include "overlay/overlay_client.h"
#include "overlay/overlay_server.h"

#include "synthetic_data_generator/data_stream.h"

#include <cstdint>
#include <vector>

namespace speedex {

class Mempool;

class SelfOverlayClient {

	Mempool& mempool;
	OverlayHandler& handler;
	ReplicaID self_id;

public:

	SelfOverlayClient(Mempool& mempool, OverlayHandler& handler, ReplicaID self_id)
		: mempool(mempool)
		, handler(handler)
		, self_id(self_id)
		{}

	uint64_t get_mempool_size() const;
	void send_txs(DataBuffer data);
};

class OverlayClientManager {

	SelfOverlayClient self_client;
	std::vector<std::unique_ptr<OverlayClient>> other_clients;

public:
	
	OverlayClientManager(ReplicaConfig const& config, ReplicaID self_id, Mempool& mempool, OverlayHandler& handler);

	uint64_t get_min_mempool_size() const;

	void poll_foreign_mempool_size();

	void send_txs(DataBuffer data);

};






} /* speedex */
