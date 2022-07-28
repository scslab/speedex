#pragma once

#include "xdr/types.h"

#include <unordered_map>

#include <libfyaml.h>

#include "hotstuff/config/replica_config.h"

namespace speedex
{

std::pair<hotstuff::ReplicaConfig, SecretKey>
parse_replica_config(fy_document* config_yaml, ReplicaID self_id);

} /* speedex */
