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

#include "overlay/overlay_client.h"

namespace speedex {

std::optional<uint64_t> 
OverlayClient::get_cached_foreign_mempool_size() const {
	uint64_t sz = foreign_mempool_size.load(std::memory_order_relaxed);
	uint64_t local_sz = local_buffer_size.load(std::memory_order_relaxed);
	bool valid = connected_to_foreign_mempool.load(std::memory_order_relaxed);
	if (valid) {
		return {sz + local_sz};
	}
	return std::nullopt;
}

void
OverlayClient::poll_foreign_mempool_size() {
	force_repoll = true;
	cv.notify_all();
}

void 
OverlayClient::send_txs(forward_t txs) {
	std::lock_guard lock(mtx);
	txs_to_forward.push_back(txs);
	local_buffer_size.fetch_add(txs.num_txs, std::memory_order_relaxed);
	cv.notify_all();
}

bool
OverlayClient::exists_work_to_do() {
	return force_repoll || (txs_to_forward.size() > 0);
}

void
OverlayClient::run()
{
	while(true) {
		std::vector<forward_t> to_forward;
		{
			std::unique_lock lock(mtx);
			if ((!done_flag) && (!exists_work_to_do())) {
			cv.wait(
				lock, 
				[this] () {
					return done_flag || exists_work_to_do();
				});
			}
			if (done_flag) return;


			to_forward = std::move(txs_to_forward);
		}

		while (to_forward.size() > 0) {
			bool success = try_action_void(
				[this, &to_forward] {
					auto front = to_forward.front();
					client -> forward_txs(*(front.data), front.buffer_number, self_id);
					local_buffer_size.fetch_sub(front.num_txs);
					foreign_mempool_size.fetch_add(front.num_txs);
				});

			if (success) {
				to_forward.erase(to_forward.begin());
			} else {
				std::lock_guard lock(mtx);
				txs_to_forward.insert(txs_to_forward.end(), to_forward.begin(), to_forward.end());
				to_forward.clear();
			}
		}
		
		try_action_void(
			[this] {
				auto res = client -> mempool_size();
				if (res) {
					foreign_mempool_size = *res;
				}
			});
	}
}


} /* speedex */
