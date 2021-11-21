#include "config/replica_config.h"

#include "crypto/crypto_utils.h"

#include "utils/debug_macros.h"

#include <optional>

namespace speedex {

xdr::unique_sock
ReplicaInfo::tcp_connect(const char* service) const
{
	return xdr::tcp_connect(hostname.c_str(), service);
}

SecretKey
ReplicaInfo::parse(fy_node* info_yaml) {
	char hostname_buf[1024];
	memset(hostname_buf, 0, 1024);

	size_t sk_seed = 0;

	size_t found = fy_node_scanf(
		info_yaml,
		"sk_seed %lu "
		"hostname %1023s",
		&sk_seed,
		hostname_buf);

	if (found != 2) {
		throw std::runtime_error("failed to parse yaml");
	}

	DeterministicKeyGenerator key_gen;
	auto [sk_, pk_] = key_gen.deterministic_key_gen(sk_seed);

	hostname = std::string(hostname_buf);
	pk = pk_;
	return sk_;
}


ReplicaConfig::ReplicaConfig()
	: replica_map()
	, nreplicas(0)
	, nmajority(0) {}

SecretKey 
ReplicaConfig::parse(fy_document* config_yaml, ReplicaID self_id)
{
	if (config_yaml == NULL) {
		throw std::runtime_error("null config yaml");
	}

	ReplicaID num_replicas = 0;
	if (fy_document_scanf(config_yaml, "/num_replicas %u", &num_replicas) != 1)
	{
		throw std::runtime_error("failed to parse num_replicas");
	}
	std::optional<speedex::SecretKey> sk_out;

	fy_node* doc_root = fy_document_root(config_yaml);

	if (doc_root == NULL) {
		throw std::runtime_error("invalid document root");
	}

	for (ReplicaID i = 0; i < num_replicas; i++) {
		ReplicaInfo info(i);

		auto node_str = std::string("replica_") + std::to_string(i);

		fy_node* info_node = fy_node_by_path(doc_root, node_str.c_str(), FY_NT, FYNWF_PTR_DEFAULT);
		if (info_node == NULL) {
			throw std::runtime_error("failed to find info yaml");
		}
		auto sk = info.parse(info_node);

		add_replica(i, info);

		if (self_id == i) {
			sk_out = sk;
		}
	}
	if (sk_out) {
		finish_init();
		return *sk_out;
	}
	throw std::runtime_error("failed to parse self node");
}


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

} /* hotstuff */
