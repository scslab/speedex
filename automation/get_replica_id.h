#pragma once

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

#include <cstdio>
#include <string>

#include "utils/debug_macros.h"

#include "xdr/types.h"

namespace speedex {

static ReplicaID
get_replica_id() {
	FILE* f = std::fopen("automation/replica", "r");

	if (f == nullptr) {
		throw std::runtime_error("failed to open replica file");
	}

	const size_t buf_size = 20;

	char buf[buf_size + 1];
	std::memset(buf, 0, buf_size + 1);

	// expected format: "node-X" 

	if (std::fread(buf, sizeof(char), buf_size, f) < 7) {
		throw std::runtime_error("failed to read replica file");
	}
	size_t dash_offset = 0;

	for (size_t offset = 0; offset < buf_size; offset++) {
		if (buf[offset] == '-') {
			dash_offset = offset;
			break;
		}
	}

	std::string replica_id = std::string(buf+dash_offset + 1);

	std::fclose(f);

	ReplicaID out = std::stol(replica_id.c_str());

	LOG("loaded replica id = %lu", out);
	return out;
}

} /* speedex */
