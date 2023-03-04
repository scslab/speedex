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
