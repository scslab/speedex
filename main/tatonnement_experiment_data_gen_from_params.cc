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
usage: tatonnement_data_gen --exp_options=<experiment_options_yaml, required>
                            --exp_name=<experiment_name, required>
)");
	exit(1);
}

enum opttag {
	EXPERIMENT_OPTIONS = 0x100,
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
	std::printf("setting options.num_accounts to 1 and block_size to 500k for convenience\n");
	options.num_accounts = 1;
	options.block_size = 500'000;

	std::printf("Setting options.num_blocks to 5 for convenience\n");
	options.num_blocks = 5;

	options.reserve_currency = true;

	std::string output_root = options.output_prefix + experiment_name + std::string("/");

	if (utils::mkdir_safe(options.output_prefix.c_str())) {
		std::printf("directory %s already exists, continuing\n", options.output_prefix.c_str());
	}
	if (utils::mkdir_safe(output_root.c_str())) {
		std::printf("directory %s already exists, continuing\n", output_root.c_str());
	}

	GeneratorState generator (gen, options, output_root, std::nullopt);

	generator.make_offer_sets();

	std::printf("made tatonnement experiment, output to %s\n", output_root.c_str());
}
