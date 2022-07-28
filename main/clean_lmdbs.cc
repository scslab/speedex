#include "utils/manage_data_dirs.h"

#include "automation/get_experiment_vars.h"
#include "automation/get_replica_id.h"

#include "config/replica_config.h"

#include <optional>
#include <chrono>

#include <getopt.h>
#include <libfyaml.h>


using namespace hotstuff;
using namespace speedex;

using namespace std::chrono_literals;

[[noreturn]]
static void usage() {
	std::printf(R"(
usage: clean_lmdbs
        --replica_id=<id> 
        --config_file=<filename>
)");
	exit(1);
}

enum opttag {
	OPT_REPLICA_ID = 0x100,
	OPT_CONFIG_FILE,
};



static const struct option opts[] = {
	{"replica_id", required_argument, nullptr, OPT_REPLICA_ID},
	{"config_file", required_argument, nullptr, OPT_CONFIG_FILE},
	{nullptr, 0, nullptr, 0}
};

int main(int argc, char* const* argv)
{
	std::optional<ReplicaID> self_id;
	std::optional<std::string> config_file;
	
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
	if (!config_file) {
		config_file = get_config_file();
	}

	struct fy_document* fyd = fy_document_build_from_file(NULL, config_file->c_str());
	if (fyd == NULL) {
		std::printf("Failed to build doc from file \"%s\"\n", config_file->c_str());
		usage();
	}

	auto [config, _] = parse_replica_config(fyd, *self_id);

	fy_document_destroy(fyd);

	auto info = config.get_info(*self_id);


	clear_all_data_dirs(info);
	make_all_data_dirs(info);
	return 0;
}
