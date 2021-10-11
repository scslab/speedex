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

#include "xdr/hotstuff.h"
#include "xdr/types.h"

#include <xdrpp/socket.h>

#include <unordered_map>

namespace hotstuff {

struct ReplicaInfo {

    ReplicaID id;
    std::string hostname;
    //salticidae::PeerId peer_id;
    speedex::PublicKey pk;

    ReplicaInfo(ReplicaID id,
                std::string hostname,
                //const salticidae::PeerId &peer_id,
                const speedex::PublicKey& pk)
        : id(id)
        , hostname(hostname)
       // , peer_id(peer_id)
        , pk(pk) {}

    xdr::unique_sock tcp_connect(const char* service) const;
};

class ReplicaConfig {

    std::unordered_map<ReplicaID, ReplicaInfo> replica_map;

public:
    size_t nreplicas;
    size_t nmajority; 

    ReplicaConfig();

    void add_replica(ReplicaID rid, const ReplicaInfo &info);

    void finish_init();

    const ReplicaInfo& get_info(ReplicaID rid) const;

    const speedex::PublicKey& 
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

//    const salticidae::PeerId &get_peer_id(ReplicaID rid) const;
};

} /* hotstuff */