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
	
	OverlayClientManager(hotstuff::ReplicaConfig const& config, hotstuff::ReplicaID self_id, Mempool& mempool, OverlayHandler& handler);

	uint64_t get_min_mempool_size() const;

	void poll_foreign_mempool_size();

	void send_txs(DataBuffer data);

};






} /* speedex */
