#pragma once

#include "xdr/types.h"

#include <unordered_map>

#include <libfyaml.h>

#include "hotstuff/config/replica_config.h"

namespace speedex {

struct ReplicaInfo : public hotstuff::ReplicaInfo
{
	std::string overlay_port;

	ReplicaInfo(ReplicaID id,
                PublicKey pk,
                std::string hostname,
                std::string block_fetch_port,
                std::string protocol_port,
                std::string root_data_folder,
                std::string overlay_port)
		: hotstuff::ReplicaInfo(
			id,
			pk,
			hostname,
			block_fetch_port,
			protocol_port,
			root_data_folder)
		, overlay_port(overlay_port)
		{}

	~ReplicaInfo() override
	{}
};

std::pair<hotstuff::ReplicaConfig, SecretKey>
parse_replica_config(fy_document* config_yaml, ReplicaID self_id);

} // namespace speedex
