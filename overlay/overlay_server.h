#pragma once

#include "config/replica_config.h"

#include "xdr/overlay.h"

#include <atomic>

#include <xdrpp/pollset.h>
#include <xdrpp/srpc.h>

namespace speedex {

class Mempool;

class OverlayHandler {

	Mempool& mempool;

	std::unordered_map<ReplicaID, std::atomic<uint32_t>> max_seen_batch_nums;

public:

	OverlayHandler(Mempool& mempool, ReplicaConfig& config)
		: mempool(mempool)
		, max_seen_batch_nums()
		{
			auto infos = config.list_info();
			for(auto& info : infos) {
				max_seen_batch_nums[info.id] = 0;
			}
		}

	using rpc_interface_type = OverlayV1;

	std::unique_ptr<uint64_t> mempool_size();
	void forward_txs(std::unique_ptr<ForwardingTxs> txs, std::unique_ptr<uint32_t> tx_batch_num, std::unique_ptr<ReplicaID> sender);

	//non-rpc methods
	uint32_t get_min_max_seen_batch_nums() const;
	void log_batch_receipt(ReplicaID source, uint32_t batch_num);

};

class OverlayServer {
	OverlayHandler handler;

	xdr::pollset ps;
	xdr::srpc_tcp_listener<> overlay_listener;

public:

	OverlayServer(Mempool& mempool, ReplicaConfig& config);

	uint32_t tx_batch_limit() const {
		return handler.get_min_max_seen_batch_nums() + 1;
	}

	OverlayHandler& get_handler() {
		return handler;
	}

};


} /* speedex */
