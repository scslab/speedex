#include <cstddef>

#include "automation/get_replica_id.h"

#include "config/replica_config.h"

#include "speedex/speedex_options.h"

#include "synthetic_data_generator/synthetic_data_gen.h"
#include "synthetic_data_generator/synthetic_data_gen_options.h"

#include "xdr/experiments.h"

#include <getopt.h>
#include <libfyaml.h>

using namespace speedex;

[[noreturn]]
static void usage() {
	std::printf(R"(
usage: synthetic_data_gen --speedex_options=<options_yaml, required> 
                          --exp_options=<experiment_options_yaml, required>
                          --exp_name=<experiment_name, required>
                          --just_params (optional)
                          --replica_id=<id, optional> 
                          --config_file=<filename, optional>
)");
	exit(1);
}

enum opttag {
	OPT_REPLICA_ID = 0x100,
	OPT_CONFIG_FILE,
	EXPERIMENT_OPTIONS,
	EXPERIMENT_NAME,
	JUST_PARAMS
};

static const struct option opts[] = {
	{"replica_id", required_argument, nullptr, OPT_REPLICA_ID},
	{"config_file", required_argument, nullptr, OPT_CONFIG_FILE},
	{"exp_options", required_argument, nullptr, EXPERIMENT_OPTIONS},
	{"exp_name", required_argument, nullptr, EXPERIMENT_NAME},
	{"just_params", no_argument, nullptr, JUST_PARAMS},
	{nullptr, 0, nullptr, 0}
};



int main(int argc, char* const* argv)
{
	//if (!(argc == 4 || argc == 5)) {
	//	std::printf("usage: ./synthetic_data_gen <speedex_options_yaml> <experiment_yaml> <experiment_name> <just_params>\n");
	//	return 1;
	//}

	std::optional<ReplicaID> self_id;
	std::optional<std::string> config_file;

	bool just_params = false;

	std::string experiment_options_file;
	std::string experiment_name;
	
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
			case EXPERIMENT_OPTIONS:
				experiment_options_file = optarg;
				break;
			case EXPERIMENT_NAME:
				experiment_name = optarg;
				break;
			case JUST_PARAMS:
				just_params = true;
				break;
			default:
				usage();
		}
	}

	std::optional<std::pair<ReplicaID, ReplicaConfig>> conf_pair;
	if (config_file.has_value()) {
		conf_pair = std::make_optional<std::pair<ReplicaID, ReplicaConfig>>();

		if (self_id) {
			conf_pair->first = *self_id;
		} else {
			conf_pair->first = get_replica_id();
		}

		struct fy_document* fyd = fy_document_build_from_file(NULL, config_file->c_str());
		if (fyd == NULL) {
			std::printf("Failed to build doc from file \"%s\"\n", config_file->c_str());
			usage();
		}

		conf_pair->second.parse(fyd, conf_pair->first);
	}

	if (experiment_options_file.size() == 0) {
		usage();
	}

	if (experiment_name.size() == 0) {
		usage();
	}


	std::minstd_rand gen(0);

	GenerationOptions options;
	auto parsed = options.parse(experiment_options_file.c_str());
	if (!parsed) {
		std::printf("failed to parse experiment options file\n");
		return 1;
	}

	std::string output_root = options.output_prefix + experiment_name + std::string("/");

	ExperimentParameters params;
	params.num_assets = options.num_assets;
	params.account_list_filename = output_root + "accounts";
	params.default_amount = 100'000'000;

	params.num_blocks = options.num_blocks;


	if (mkdir_safe(options.output_prefix.c_str())) {
		std::printf("directory %s already exists, continuing\n", options.output_prefix.c_str());
	}
	if (mkdir_safe(output_root.c_str())) {
		std::printf("directory %s already exists, continuing\n", output_root.c_str());
	}

	auto params_file = output_root + std::string("params");
	if (save_xdr_to_file(params, params_file.c_str())) {
		throw std::runtime_error("failed to save params file");
	}

	GeneratorState generator (gen, options, output_root, conf_pair);
	generator.dump_account_list(params.account_list_filename);

	if (!just_params) {
		generator.make_blocks();
	}

	std::printf("made experiment, output to %s\n", output_root.c_str());
}
