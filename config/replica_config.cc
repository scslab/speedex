#include "config/replica_config.h"

#include "crypto/crypto_utils.h"

#include "utils/debug_macros.h"

#include "config.h"
#include "rpc/rpcconfig.h"

#include <optional>

namespace speedex {

template<typename out_type>
bool try_parse_attr(fy_node* info_yaml, std::string query, out_type* out)
{
    if (fy_node_scanf(info_yaml,
            query.c_str(),
            out) != 1)
    {
        return false;
    }
    return true;
}

std::string get_attr_uint32(fy_node* info_yaml, std::string query, std::string default_string)
{
    std::string out = default_string;
    uint32_t out_read;
    if (try_parse_attr(info_yaml, query, &out_read))
    {
        out = std::to_string(out_read);
    }
    return out;
}

// query should be of the form "query %1023s"
std::string get_attr_string(fy_node* info_yaml, std::string query, std::string default_string)
{
    char buf[1024];
    memset(buf, 0, 1024);
    if (try_parse_attr(info_yaml, query, buf))
    {
        return std::string(buf);
    }
    return default_string;
}

std::pair<std::unique_ptr<hotstuff::ReplicaInfo>, SecretKey>
parse_replica_info(fy_node* info_yaml, ReplicaID id)
{
    char hostname_buf[1024];
    memset(hostname_buf, 0, 1024);

    size_t sk_seed = 0;

    size_t found = fy_node_scanf(info_yaml,
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

    std::string overlay_port = get_attr_uint32(info_yaml, "overlay_port %lu", OVERLAY_PORT);
    std::string hs_block_fetch_port = get_attr_uint32(info_yaml, "hotstuff_block_fetch_port %lu", HOTSTUFF_BLOCK_FETCH_PORT);
    std::string hs_protocol_port = get_attr_uint32(info_yaml, "hotstuff_protocol_port %lu", HOTSTUFF_PROTOCOL_PORT);

    std::string db_directory = get_attr_string(info_yaml, "root_database %1023s", ROOT_DB_DIRECTORY) + "/";

    auto out = std::make_unique<ReplicaInfo>(id,
                              pk,
                              hostname,
                              hs_block_fetch_port,
                              hs_protocol_port,
                              db_directory,
                              overlay_port);
    return { std::move(out), sk };
}

std::pair<std::shared_ptr<hotstuff::ReplicaConfig>, SecretKey>
parse_replica_config(fy_document* config_yaml, ReplicaID self_id)
{
    if (config_yaml == NULL) {
        throw std::runtime_error("null config yaml");
    }

    ReplicaID num_replicas = 0;
    if (fy_document_scanf(config_yaml, "/num_replicas %u", &num_replicas)
        != 1) {
        throw std::runtime_error("failed to parse num_replicas");
    }
    std::optional<speedex::SecretKey> sk_out;

    fy_node* doc_root = fy_document_root(config_yaml);

    if (doc_root == NULL) {
        throw std::runtime_error("invalid document root");
    }

    std::shared_ptr<hotstuff::ReplicaConfig> out = std::make_shared<hotstuff::ReplicaConfig>();

    for (ReplicaID i = 0; i < num_replicas; i++) {

        auto node_str = std::string("replica_") + std::to_string(i);

        fy_node* info_node = fy_node_by_path(
            doc_root, node_str.c_str(), FY_NT, FYNWF_PTR_DEFAULT);
        if (info_node == NULL) {
            throw std::runtime_error("failed to find info yaml");
        }
        auto [info, sk] = parse_replica_info(info_node, i);

        out->add_replica(std::move(info));

        if (self_id == i) {
            sk_out = sk;
        }
    }
    if (!sk_out)
    {
        throw std::runtime_error("failed to parse self node");
    }
    out->finish_init();
    return { out, *sk_out };
}

} // namespace speedex
