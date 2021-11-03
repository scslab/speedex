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
	local_buffer_size.fetch_add(txs.first, std::memory_order_relaxed);
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
		std::vector<std::pair<uint32_t, std::shared_ptr<ForwardingTxs>>> to_forward;
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
					client -> forward_txs(*(front.second));
					local_buffer_size.fetch_sub(front.first);
					foreign_mempool_size.fetch_add(front.first);
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
