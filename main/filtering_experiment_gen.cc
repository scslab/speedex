#include <cstddef>

#include "speedex/speedex_options.h"

#include "synthetic_data_generator/synthetic_data_gen.h"
#include "synthetic_data_generator/synthetic_data_gen_options.h"

#include "xdr/experiments.h"

#include <getopt.h>
#include <libfyaml.h>

#include <utils/mkdir.h>

using namespace speedex;

[[noreturn]]
static void usage() {
	std::printf(R"(
usage: filtering_experiment_gen 
	--exp_options=<experiment_options_yaml, required>
    --exp_name=<experiment_name, required>
)");
	exit(1);
}

enum opttag {
	EXPERIMENT_OPTIONS,
	EXPERIMENT_NAME
};

static const struct option opts[] = {
	{"exp_options", required_argument, nullptr, EXPERIMENT_OPTIONS},
	{"exp_name", required_argument, nullptr, EXPERIMENT_NAME},
	{nullptr, 0, nullptr, 0}
};

int main(int argc, char* const* argv)
{
	std::string experiment_options_file;
	std::string experiment_name;
	
	experiment_options_file = "synthetic_data_config/filtering_experiment.yaml";
	experiment_name = "filtering";

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


	std::minstd_rand gen(0);

	GenerationOptions options;
	auto parsed = options.parse(experiment_options_file.c_str());
	if (!parsed) {
		std::printf("failed to parse experiment options file\n");
		return 1;
	}

	std::string output_root = options.output_prefix + experiment_name + std::string("/");

	if (utils::mkdir_safe(options.output_prefix.c_str())) {
		std::printf("directory %s already exists, continuing\n", options.output_prefix.c_str());
	}
	if (utils::mkdir_safe(output_root.c_str())) {
		std::printf("directory %s already exists, continuing\n", output_root.c_str());
	}

	GeneratorState generator (gen, options, output_root);
	generator.dump_account_list(output_root + "accounts");


	size_t num_txs = 500'000;

	size_t num_dups = 100'000;

	size_t num_bad_seq_accounts = 1'000;

	for (size_t b = 0; b < 5; b++)
	{

		ExperimentBlock block;

		for (size_t i = 0; i < num_txs - num_dups; i++)
		{
			block.push_back(generator.gen_payment_tx(1));
		}

		generator.fill_in_seqnos(block);

		for (size_t i = 0; i < num_dups; i++)
		{
			size_t dup_idx = generator.gen_random_index(num_txs-num_dups);
			block.push_back(block[dup_idx]);
		}
		for (size_t i = 0; i < num_bad_seq_accounts; i++)
		{
			size_t bad_idx = generator.gen_random_index(num_txs - num_dups);
			auto tx = block[bad_idx];
			tx.transaction.operations.at(0).body.paymentOp().amount+=10;
			block.push_back(tx);
		}

		generator.get_signer().sign_block(block);
		generator.write_block(block);
	}
}













