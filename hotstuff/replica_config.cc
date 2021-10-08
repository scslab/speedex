#include "hotstuff/replica_config.h"

#include "utils/debug_macros.h"

namespace hotstuff {

using PublicKey = speedex::PublicKey;

ReplicaConfig::ReplicaConfig()
	: nreplicas(0), nmajority(0) {}

void
ReplicaConfig::add_replica(ReplicaID rid, const ReplicaInfo &info) {
	auto it = replica_map.find(rid);
	if (it != replica_map.end()) {
		throw std::runtime_error("can't add replicaid twice!");
	}
    replica_map.insert(std::make_pair(rid, info));
    nreplicas++;
}

void
ReplicaConfig::finish_init() {
	size_t nfaulty = nreplicas / 3;
	HOTSTUFF_INFO("nfaulty = %lu", nfaulty);
	if (nfaulty == 0) {
		HOTSTUFF_INFO("num faulty tolerated is 0!  Is this ok?");
	}
	nmajority = nreplicas - nfaulty;
}

const ReplicaInfo&
ReplicaConfig::get_info(ReplicaID rid) const {
    auto it = replica_map.find(rid);
    if (it == replica_map.end())
        throw std::runtime_error(std::string("rid") + std::to_string(rid) + "not found");
    return it->second;
}

const PublicKey&
ReplicaConfig::get_publickey(ReplicaID rid) const {
    return get_info(rid).pk;
}

    //const salticidae::PeerId &get_peer_id(ReplicaID rid) const {
    //   return get_info(rid).peer_id;
    //}
} /* hotstuff */