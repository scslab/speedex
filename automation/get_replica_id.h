#pragma once

#include <cstdio>
#include <string>

#include "xdr/types.h"

namespace speedex {

static ReplicaID
get_replica_id() {
	FILE* f = std::fopen("automation/replica", "r");

	if (f == nullptr) {
		throw std::runtime_error("failed to open replica file");
	}

	char buf[10];

	// expected format: "node-X" 

	if (std::fread(buf, sizeof(char), 10, f) < 6) {
		throw std::runtime_error("failed to read replica file");
	}

	std::string replica_id = std::string(buf+5);

	std::fclose(f);

	return std::stol(replica_id.c_str());
}

} /* speedex */
