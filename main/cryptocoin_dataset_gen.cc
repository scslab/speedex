#include "automation/get_experiment_vars.h"
#include "automation/get_replica_id.h"

#include "synthetic_data_generator/synthetic_data_gen.h"
#include "synthetic_data_generator/synthetic_data_gen_options.h"

#include "speedex/speedex_options.h"
#include "utils/save_load_xdr.h"

#include "hotstuff/config/replica_config.h"

#include "xdr/cryptocoin_experiment.h"

#include <cstddef>
#include <vector>

#include <utils/mkdir.h>

#include <getopt.h>
#include <libfyaml.h>

using namespace speedex;

std::vector<double> 
get_cumulative_volumes(const CryptocoinExperiment& experiment, size_t idx) {
	std::vector<double> out;
	double acc = 0;
	for (size_t i = 0; i < experiment.coins.size(); i++) {
		std::printf("%s %lf\n", experiment.coins[i].name.c_str(), experiment.coins[i].snapshots[idx].volume);
		acc += experiment.coins[i].snapshots[idx].volume;
		
		out.push_back(acc);
	}
	return out;
}

std::vector<double> 
get_prices(const CryptocoinExperiment& experiment, size_t idx) {
	std::vector<double> out;
	for (size_t i = 0; i < experiment.coins.size(); i++) {
		double price = experiment.coins[i].snapshots[idx].price;
		out.push_back(price);
	}
	return out;
}

[[noreturn]]
static void usage() {
	std::printf(R"(
usage: cryptocoin_dataset_gen --exp_options=<experiment_options_yaml, required>
                              --exp_name=<experiment_name, required>
)");
	exit(1);
}

enum opttag {
	OPT_REPLICA_ID = 0x100,
	OPT_CONFIG_FILE,
	EXPERIMENT_OPTIONS,
	EXPERIMENT_NAME
};

static const struct option opts[] = {
	{"replica_id", required_argument, nullptr, OPT_REPLICA_ID},
	{"config_file", required_argument, nullptr, OPT_CONFIG_FILE},
	{"exp_options", required_argument, nullptr, EXPERIMENT_OPTIONS},
	{"exp_name", required_argument, nullptr, EXPERIMENT_NAME},
	{nullptr, 0, nullptr, 0}
};



int main(int argc, char* const* argv)
{

	std::string experiment_options_file;
	std::string experiment_name;

	std::optional<hotstuff::ReplicaID> self_id;
	std::optional<std::string> config_file;
	
	int opt;

	while ((opt = getopt_long_only(argc, argv, "",
				 opts, nullptr)) != -1)
	{
		switch(opt) {
			case EXPERIMENT_OPTIONS:
				experiment_options_file = optarg;
				break;
			case EXPERIMENT_NAME:
				experiment_name = optarg;
				break;
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

	if (experiment_options_file.size() == 0) {
		usage();
	}

	if (experiment_name.size() == 0) {
		usage();
	}
	
	if (!config_file) {
		config_file = get_config_file();
	}
	
	ReplicaID rid;
	if (self_id) {
		rid = *self_id;
	} else {
		rid = get_replica_id();
	}

	struct fy_document* fyd = fy_document_build_from_file(NULL, config_file->c_str());
	if (fyd == NULL) {
		std::printf("Failed to build doc from file \"%s\"\n", config_file->c_str());
		usage();
	}

	auto [conf, _] = speedex::parse_replica_config(fyd, rid);


	CryptocoinExperiment experiment;

	std::string coin_file = std::string("coingecko_snapshot/unified_data");

	if (load_xdr_from_file(experiment, coin_file.c_str())) {
		throw std::runtime_error("failed to load " + coin_file);
	}

	size_t num_assets = experiment.coins.size();

	if (num_assets == 0) {
		throw std::runtime_error("no coins!");
	}

	size_t num_coin_datapts = experiment.coins[0].snapshots.size();

	for (size_t i = 1; i < num_assets; i++) {
		if (experiment.coins[i].snapshots.size() != num_coin_datapts) {
			throw std::runtime_error("invalid number of snapshots");
		}
	}

	GenerationOptions options;
	auto parsed = options.parse(experiment_options_file.c_str());
	if (!parsed) {
		std::printf("yaml parse error\n");
		return -1;
	}

	if (num_assets != options.num_assets) {
		throw std::runtime_error("mismatch between num coins and num assets in yaml");
	}

	std::string output_root = options.output_prefix + experiment_name + std::string("/");

	ExperimentParameters params;
	params.num_assets = options.num_assets;
	params.account_list_filename = output_root + "accounts";
	params.default_amount = options.new_account_balance;
	params.num_blocks = options.num_blocks;
	params.n_replicas = conf.nreplicas;

	if (utils::mkdir_safe(options.output_prefix.c_str())) {
		std::printf("directory %s already exists, continuing\n", options.output_prefix.c_str());
	}

	if (utils::mkdir_safe(output_root.c_str())) {
		std::printf("directory %s already exists, continuing\n", output_root.c_str());
	}


	auto params_file = output_root + std::string("params");
	if (save_xdr_to_file(params, params_file.c_str())) {
		throw std::runtime_error("failed to save params file");
	}

	std::minstd_rand gen(0);

	//TODO do we care about splitting between replicas here?  we don't pass conf_pair into generator
	GeneratorState generator (gen, options, output_root);

	generator.dump_account_list(params.account_list_filename);

	for (size_t i = 0; i < num_coin_datapts; i++) {
		generator.asset_probabilities = get_cumulative_volumes(experiment, i);
		auto prices = get_prices(experiment, i);
		generator.make_block(prices);
	}

	return 0;
}
