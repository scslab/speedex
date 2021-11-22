#pragma once

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

	const size_t buf_size = 10;

	char buf[buf_size + 1];

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
