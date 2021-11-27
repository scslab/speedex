#pragma once

#include "overlay/overlay_client_manager.h"
#include "overlay/overlay_server.h"

#include "synthetic_data_generator/data_stream.h"

#include <atomic>
#include <cstdint>

namespace speedex {

class OverlayFlooder {

	DataStream& data_stream;
	OverlayClientManager& client_manager;
	OverlayServer& server;

	std::atomic<bool> done_flag;

	void background_poll_thread();
	void background_flood_thread();

	bool is_done() const;
	void set_done();

	const size_t FLOOD_THRESHOLD;

public:

	OverlayFlooder(DataStream& data_stream, OverlayClientManager& client_manager, OverlayServer& server, size_t flood_threshold);

	~OverlayFlooder()
	{
		set_done();
	}
};



} /* speedex */
