#include "utils/manage_data_dirs.h"

#include "automation/command_line_args.h"
#include "automation/get_experiment_vars.h"
#include "automation/get_replica_id.h"

#include "config/replica_config.h"

#include "utils/yaml.h"

#include <optional>
#include <chrono>

#include <getopt.h>
#include <libfyaml.h>


using namespace hotstuff;
using namespace speedex;

using namespace std::chrono_literals;
 /*
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
}; */

int main(int argc, char* const* argv)
{
	auto args = parse_cmd(argc, argv, "speedex_vm_hotstuff");

/*	std::optional<ReplicaID> self_id;
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
	*/

	if (!args.self_id) {
		args.self_id = get_replica_id();
	}
	if (!args.config_file) {
		args.config_file = get_config_file();
	} 

	auto fyd = yaml(*args.config_file);
	if (!fyd) {
		std::printf("Failed to build doc from file \"%s\"\n", args.config_file->c_str());
		exit(1);
	}

	auto [parsed_config, sk] = parse_replica_config(fyd.get(), *args.self_id);

	auto info = parsed_config->get_info(*args.self_id);

	clear_all_data_dirs(info);
	make_all_data_dirs(info);
	return 0;
}
