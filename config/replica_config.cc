#include "config/replica_config.h"

#include "crypto/crypto_utils.h"

#include "utils/debug_macros.h"

#include "config.h"
#include "rpc/rpcconfig.h"

#include <optional>

namespace speedex
{

std::pair<hotstuff::ReplicaInfo, SecretKey>
parse_replica_info(fy_node* info_yaml, ReplicaID id)
{
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
	auto [sk, pk] = key_gen.deterministic_key_gen(sk_seed);

	std::string hostname = std::string(hostname_buf);

	hotstuff::ReplicaInfo out(id, pk, hostname, HOTSTUFF_BLOCK_FETCH_PORT, HOTSTUFF_PROTOCOL_PORT, ROOT_DB_DIRECTORY);
	return {out, sk};
}

std::pair<hotstuff::ReplicaConfig, SecretKey>
parse_replica_config(fy_document* config_yaml, ReplicaID self_id)
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

	hotstuff::ReplicaConfig out;

	for (ReplicaID i = 0; i < num_replicas; i++) {

		auto node_str = std::string("replica_") + std::to_string(i);

		fy_node* info_node = fy_node_by_path(doc_root, node_str.c_str(), FY_NT, FYNWF_PTR_DEFAULT);
		if (info_node == NULL) {
			throw std::runtime_error("failed to find info yaml");
		}
		auto [info, sk] = parse_replica_info(info_node, i);

		out.add_replica(info);

		if (self_id == i) {
			sk_out = sk;
		}
	}
	if (sk_out) {
		out.finish_init();
		return {out, *sk_out};
	}
	throw std::runtime_error("failed to parse self node");

}

} /* speedex */
