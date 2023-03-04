/**
 * SPEEDEX: A Scalable, Parallelizable, and Economically Efficient Decentralized Exchange
 * Copyright (C) 2023 Geoffrey Ramseyer

 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include "hotstuff/config/replica_config.h"

#include "xdr/overlay.h"

#include <atomic>
#include <condition_variable>
#include <mutex>

#include <xdrpp/pollset.h>
#include <xdrpp/srpc.h>

namespace speedex {

class Mempool;

class OverlayHandler {

	Mempool& mempool;

	std::unordered_map<hotstuff::ReplicaID, std::atomic<uint32_t>> max_seen_batch_nums;

public:

	OverlayHandler(Mempool& mempool, const hotstuff::ReplicaConfig& config)
		: mempool(mempool)
		, max_seen_batch_nums()
		{
			auto infos = config.list_info();
			for(auto& info : infos) {
				max_seen_batch_nums[info->id] = 0;
			}
		}

	using rpc_interface_type = OverlayV1;

	std::unique_ptr<uint64_t> mempool_size();
	void forward_txs(std::unique_ptr<ForwardingTxs> txs, std::unique_ptr<uint32_t> tx_batch_num, std::unique_ptr<hotstuff::ReplicaID> sender);

	//non-rpc methods
	uint32_t get_min_max_seen_batch_nums() const;
	void log_batch_receipt(ReplicaID source, uint32_t batch_num);

};

class OverlayServer {
	OverlayHandler handler;

	xdr::pollset ps;
	xdr::srpc_tcp_listener<> overlay_listener;

	bool ps_is_shutdown = false;
	std::atomic<bool> start_shutdown = false;
	std::mutex mtx;
	std::condition_variable cv;

	void await_pollset_shutdown();

public:

	OverlayServer(Mempool& mempool, const hotstuff::ReplicaConfig& config, hotstuff::ReplicaID self_id);

	uint32_t tx_batch_limit() const {
		return handler.get_min_max_seen_batch_nums() + 1;
	}

	OverlayHandler& get_handler() {
		return handler;
	}

	~OverlayServer()
	{
		start_shutdown = true;
		await_pollset_shutdown();
	}

};


} /* speedex */
