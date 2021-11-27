#include "overlay/overlay_flooder.h"

#include "utils/debug_macros.h"

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

	std::optional<DataBuffer> buffer;
	while(!is_done()) {
		size_t sz = client_manager.get_min_mempool_size();
		if (sz < FLOOD_THRESHOLD) {
			if (!buffer) {
				buffer = data_stream.load_txs_unparsed();
			}
			if (buffer->buffer_number <= server.tx_batch_limit()) {
				if (buffer ->data) {
					OVERLAY_INFO("forwarding tx input buffer number %lu", buffer -> buffer_number);
					client_manager.send_txs(*buffer);
					buffer = std::nullopt;
				}
			}
			if (buffer -> finished) {
				OVERLAY_INFO("done loading txs, terminating overlay flooder");
				return;
			}
		} else {
			std::this_thread::sleep_for(500ms);
		}
	}
}

OverlayFlooder::OverlayFlooder(DataStream& data_stream, OverlayClientManager& client_manager, OverlayServer& server, size_t flood_threshold)
	: data_stream(data_stream)
	, client_manager(client_manager)
	, server(server)
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
