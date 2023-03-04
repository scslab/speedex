#include <catch2/catch_test_macros.hpp>

#include "overlay/overlay_client_manager.h"
#include "overlay/overlay_server.h"
#include "overlay/overlay_flooder.h"

#include "synthetic_data_generator/data_stream.h"

#include "utils/yaml.h"

#include "mempool/mempool.h"

#include <iostream>
#include <cinttypes>
#include <optional>

#include <getopt.h>

using namespace speedex;
using namespace hotstuff;

class OverlaySim
{
	ReplicaConfig const& config;
	std::string overlay_port;

	Mempool mp;
	OverlayServer server;

public:

	OverlaySim(ReplicaConfig const& config,
		const char* overlay_port_name)
		: config(config)
		, overlay_port(overlay_port_name)
		, mp(10'000, 2'000'000)
		, server(mp, config, overlay_port)
		{}
}

int main(int argc, char **argv)
{

	yaml fyd("config/replica_local_4.yaml");
	if (!fyd) {
		exit(1);
	}

	ReplicaConfig config = parse_replica_config(fyd.get(), *self_id).first;

	Mempool mp(10'000, 2'000'000);
	OverlayServer server(mp, config);

	OverlayClientManager client_manager(config, *self_id, mp, server.get_handler());

	MockDataStream data_stream;

	OverlayFlooder flooder(data_stream, client_manager, server, 1'000'000);

	while (true) {
		std::cin.get();
		std::printf("mempool size: %" PRIu64 "\n", mp.total_size());
		mp.push_mempool_buffer_to_mempool();
		mp.drop_txs(550'000);
	}
}
