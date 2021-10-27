#pragma once

/**
 * Derived in part from libhotstuff/include/entity.h
 * Original copyright notice follows
 *
 * Copyright 2018 VMware
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "xdr/types.h"

#include <xdrpp/socket.h>

#include <unordered_map>

#include <libfyaml.h>

namespace speedex {

struct ReplicaInfo {

    ReplicaID id;
    std::string hostname;
    PublicKey pk;

    ReplicaInfo(ReplicaID id)
        : id(id)
        {}

    SecretKey parse(fy_node* info_yaml);

    xdr::unique_sock tcp_connect(const char* service) const;
};

class ReplicaConfig {

    std::unordered_map<ReplicaID, ReplicaInfo> replica_map;

    void add_replica(ReplicaID rid, const ReplicaInfo &info);

    void finish_init();

public:
    size_t nreplicas;
    size_t nmajority; 

    ReplicaConfig();

    SecretKey parse(fy_document* config_yaml, ReplicaID self_id);

    const ReplicaInfo& get_info(ReplicaID rid) const;

    const PublicKey& 
    get_publickey(ReplicaID rid) const;

    std::vector<ReplicaInfo> list_info() const {
        std::vector<ReplicaInfo> out;
        for (auto const& [_, info] : replica_map)
        {
            out.push_back(info);
        }
        return out;
    }

    bool is_valid_replica(ReplicaID replica) const {
        return (replica_map.find(replica) != replica_map.end());
    }
};

} /* hotstuff */