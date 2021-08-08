
#include "experiments/producer_node.h"
#include "speedex/speedex_options.h"

#include "utils/save_load_xdr.h"

#include "xdr/experiments.h"

using namespace speedex;

int main(int argc, char const *argv[])
{
	if (argc != 4) {
		std::printf("usage: ./whatever <data_directory> <results_directory> <num_threads>\n");
		return -1;
	}

	SpeedexOptions options;

	ExperimentParameters params;

	std::string experiment_data_root = std::string(argv[1]) + "/";
	std::string results_output_root = std::string(argv[2]) + "/";

	std::string params_filename = experiment_data_root + "params";

	if (load_xdr_from_file(params, params_filename.c_str())) {
		throw std::runtime_error("failed to load experiment params file");
	}

	int num_threads = std::stoi(argv[3]);

	options.num_assets = params.num_assets;
	options.tax_rate = params.tax_rate;
	options.smooth_mult = params.smooth_mult;
	options.persistence_frequency = params.persistence_frequency;

	SimulatedProducerNode node {
		.params = params,
		.experiment_data_root = experiment_data_root,
		.results_output_root = results_output_root,
		.options = options,
		.num_threads = num_threads
	};

	node.run_experiment();

	return 0;
}
