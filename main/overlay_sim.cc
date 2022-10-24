#include "automation/get_experiment_vars.h"
#include "automation/get_replica_id.h"

#include "overlay/overlay_client_manager.h"
#include "overlay/overlay_server.h"
#include "overlay/overlay_flooder.h"

#include "synthetic_data_generator/data_stream.h"

#include "config/replica_config.h"

#include "mempool/mempool.h"

#include <iostream>
#include <cinttypes>
#include <optional>

#include <getopt.h>
#include <libfyaml.h>


using namespace speedex;
using namespace hotstuff;

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
		self_id = get_replica_id();
	}

	if (config_file.empty()) {
		config_file = get_config_file();
	}

	struct fy_document* fyd = fy_document_build_from_file(NULL, config_file.c_str());
	if (fyd == NULL) {
		std::printf("Failed to build doc from file \"%s\"\n", config_file.c_str());
		usage();
	}

	ReplicaConfig config = parse_replica_config(fyd, *self_id).first;

	//config.parse(fyd, *self_id);

	Mempool mp(10'000, 2'000'000);
	OverlayServer server(mp, config, *self_id);

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
