#pragma once

#include "xdr/types.h"

#include <unordered_map>

#include <libfyaml.h>

#include "hotstuff/config/replica_config.h"

namespace speedex {


std::pair<hotstuff::ReplicaConfig, SecretKey>
parse_replica_config(fy_document* config_yaml, ReplicaID self_id);

/*


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
}; */

} /* hotstuff */