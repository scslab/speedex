#include "overlay/overlay_flooder.h"

#include <thread>

using namespace std::chrono_literals;

namespace speedex {

void 
OverlayFlooder::background_poll_thread() {
	while(!is_done()) {
		client_manager.poll_foreign_mempool_size();
		std::this_thread::sleep_for(500ms);
	}
}

void
OverlayFlooder::background_flood_thread() {
	while(!is_done()) {
		size_t sz = client_manager.get_min_mempool_size();
		if (sz < FLOOD_THRESHOLD) {
			auto data = data_stream.load_txs_unparsed();
			if (data.data) {
				client_manager.send_txs(data);
			} else {
				std::printf("done loading txs, terminating overlay flooder\n");
				return;
			}
		} else {
			std::this_thread::sleep_for(500ms);
		}
	}
}

OverlayFlooder::OverlayFlooder(DataStream& data_stream, OverlayClientManager& client_manager, size_t flood_threshold)
	: data_stream(data_stream)
	, client_manager(client_manager)
	, done_flag(false)
	, FLOOD_THRESHOLD(flood_threshold)
	{
		std::thread([this] {
			background_poll_thread();
		}).detach();
		std::thread([this] {
			background_flood_thread();
		}).detach();
	}

bool 
OverlayFlooder::is_done() const {
	return done_flag.load(std::memory_order_relaxed);
}

void 
OverlayFlooder::set_done() {
	done_flag = true;
}


} /* speedex */
