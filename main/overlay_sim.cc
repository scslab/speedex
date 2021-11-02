#include "overlay/overlay_client_manager.h"
#include "overlay/overlay_server.h"
#include "overlay/overlay_flooder.h"

#include "synthetic_data_generator/data_stream.h"

#include "config/replica_config.h"

#include "mempool/mempool.h"

#include <iostream>

#include <optional>

#include <getopt.h>
#include <libfyaml.h>


using namespace speedex;

[[noreturn]]
static void usage() {
	std::printf(R"(
usage: overlay_sim --replica_id=<id> --config_file=<filename>
)");
	exit(1);
}

enum opttag {
	OPT_REPLICA_ID = 0x100,
	OPT_CONFIG_FILE
};

static const struct option opts[] = {
	{"replica_id", required_argument, nullptr, OPT_REPLICA_ID},
	{"config_file", required_argument, nullptr, OPT_CONFIG_FILE},
	{nullptr, 0, nullptr, 0}
};

int main(int argc, char **argv)
{
	std::optional<ReplicaID> self_id;
	std::string config_file;
	
	int opt;

	while ((opt = getopt_long_only(argc, argv, "",
				 opts, nullptr)) != -1)
	{
		switch(opt) {
			case OPT_REPLICA_ID:
				self_id = std::stol(optarg);
				break;
			case OPT_CONFIG_FILE:
				config_file = optarg;
				break;
			default:
				usage();
		}
	}

	if (!self_id) {
		usage();
	}

	if (config_file.empty()) {
		usage();
	}

	ReplicaConfig config;

	struct fy_document* fyd = fy_document_build_from_file(NULL, config_file.c_str());
	if (fyd == NULL) {
		std::printf("Failed to build doc from file \"%s\"\n", config_file.c_str());
		usage();
	}

	config.parse(fyd, *self_id);

	Mempool mp(10'000, 2'000'000);
	OverlayServer server(mp);

	OverlayClientManager client_manager(config, *self_id, mp);

	MockDataStream data_stream;

	OverlayFlooder flooder(data_stream, client_manager, 1'000'000);

	while (true) {
		std::cin.get();
		std::printf("mempool size: %lu\n", mp.total_size());
		mp.push_mempool_buffer_to_mempool();
		mp.drop_txs(550'000);
	}
}
