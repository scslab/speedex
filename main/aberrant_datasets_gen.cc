#include "synthetic_data_generator/synthetic_data_gen.h"


#include <cstddef>

#include "synthetic_data_generator/synthetic_data_gen_options.h"

#include "edce_options.h"

#include "xdr/experiments.h"


using namespace edce;

ExperimentConfigList config_list;

void save_params(ExperimentParameters params, std::string output_root) {
	auto params_file = output_root + std::string("params");
	if (save_xdr_to_file(params, params_file.c_str())) {
		throw std::runtime_error("failed to save params file");
	}
}

void add_config(std::string experiment_name, xdr::xvector<Price> prices = {}, std::string out_name = "") {
	ExperimentConfig config;
	config.name = experiment_name;
	config.starting_prices = prices;

	if (out_name.size() == 0) {
		config.out_name = experiment_name;
	} else {
		config.out_name = out_name;
	}
	config_list.push_back(config);
}

void gen_outlier_prices(GenerationOptions options, ExperimentParameters params, std::string name_prefix) {

	std::minstd_rand gen(0);

	std::string output_root = options.output_prefix + name_prefix + "_outlier_prices/";

	add_config(name_prefix + "_outlier_prices");
	xdr::xvector<Price> prices_config;
	prices_config.resize(options.num_assets);
	prices_config[0] = PriceUtils::from_double(10000);
	prices_config[1] = PriceUtils::from_double(0.1);
	for (size_t i = 2; i < options.num_assets; i++) {
		prices_config[i] = PriceUtils::from_double(1);
	}
	add_config(name_prefix + "_outlier_prices", prices_config, name_prefix + "_outlier_prices_start_near");

	if (mkdir_safe(output_root.c_str())) {
		std::printf("directory %s already exists, continuing\n", output_root.c_str());
	}

	save_params(params, output_root);

	GeneratorState generator(gen, options, output_root);

	auto prices = generator.gen_prices();

	prices[0] = 10000;
	prices[1] = 0.1;

	for (size_t i = 0; i < options.num_blocks; i++) {
		generator.make_block(prices);
	}
}

void gen_upper_outliers(GenerationOptions options, ExperimentParameters params, std::string name_prefix) {
	std::minstd_rand gen(0);

	std::string output_root = options.output_prefix + name_prefix + "_outlier_prices_high/";
	add_config(name_prefix + "_outlier_prices_high");

	xdr::xvector<Price> prices_config;
	prices_config.resize(options.num_assets);
	prices_config[0] = PriceUtils::from_double(1000);
	prices_config[1] = PriceUtils::from_double(10000);

	for (size_t i = 2; i < options.num_assets; i++) {
		prices_config[i] = PriceUtils::from_double(1);
	}

	add_config(name_prefix + "_outlier_prices_high", prices_config, name_prefix + "_outlier_prices_high_start_near");

	if (mkdir_safe(output_root.c_str())) {
		std::printf("directory %s already exists, continuing\n", output_root.c_str());
	}

	save_params(params, output_root);

	GeneratorState generator(gen, options, output_root);

	auto prices = generator.gen_prices();
	prices[0] = 1000;
	prices[1] = 10000;

	for (size_t i = 0; i < options.num_blocks; i++) {
		generator.make_block(prices);
	}
}

void gen_tight_cluster(GenerationOptions options, ExperimentParameters params, std::string name_prefix) {
	std::minstd_rand gen(0);

	std::string output_root = options.output_prefix + name_prefix + "_tight_cluster/";
	add_config(name_prefix + "_tight_cluster");

	if (mkdir_safe(output_root.c_str())) {
		std::printf("directory %s already exists, continuing\n", output_root.c_str());
	}

	save_params(params, output_root);

	options.price_options.min_tolerance = 0;
	options.price_options.max_tolerance = 0.001;

	GeneratorState generator(gen, options, output_root);

	generator.make_blocks();
}

void gen_no_cluster(GenerationOptions options, ExperimentParameters params, std::string name_prefix) {
	std::minstd_rand gen(0);

	std::string output_root = options.output_prefix + name_prefix + "_no_cluster/";
	add_config(name_prefix + "_no_cluster");

	if (mkdir_safe(output_root.c_str())) {
		std::printf("directory %s already exists, continuing\n", output_root.c_str());
	}

	save_params(params, output_root);

	options.price_options.min_tolerance = 0;
	options.price_options.max_tolerance = 0.5;

	GeneratorState generator(gen, options, output_root);

	generator.make_blocks();
}

void gen_gap_at_market_prices(GenerationOptions options, ExperimentParameters params, std::string name_prefix) {
	std::minstd_rand gen(0);

	std::string output_root = options.output_prefix + name_prefix + "_price_gap/";
	add_config(name_prefix + "_price_gap");

	if (mkdir_safe(output_root.c_str())) {
		std::printf("directory %s already exists, continuing\n", output_root.c_str());
	}

	save_params(params, output_root);

	double gap = options.price_options.max_tolerance - options.price_options.min_tolerance;

	options.price_options.min_tolerance = 0.1;
	options.price_options.max_tolerance = options.price_options.min_tolerance + gap;

	GeneratorState generator(gen, options, output_root);

	generator.make_blocks();
}

void gen_50percent_good(GenerationOptions options, ExperimentParameters params, std::string name_prefix) {
	std::minstd_rand gen(0);

	std::string output_root = options.output_prefix + name_prefix + "_50percent_good/";
	add_config(name_prefix + "_50percent_good");

	if (mkdir_safe(output_root.c_str())) {
		std::printf("directory %s already exists, continuing\n", output_root.c_str());
	}

	save_params(params, output_root);

	options.bad_tx_fraction = 0.5;

	GeneratorState generator(gen, options, output_root);

	generator.make_blocks();
}

void gen_10percent_good(GenerationOptions options, ExperimentParameters params, std::string name_prefix) {
	std::minstd_rand gen(0);

	std::string output_root = options.output_prefix + name_prefix + "_10percent_good/";
	add_config(name_prefix + "_10percent_good");

	if (mkdir_safe(output_root.c_str())) {
		std::printf("directory %s already exists, continuing\n", output_root.c_str());
	}

	save_params(params, output_root);

	options.bad_tx_fraction = 0.9;

	GeneratorState generator(gen, options, output_root);

	generator.make_blocks();
}

void gen_whales(GenerationOptions options, ExperimentParameters params, std::string name_prefix) {
	std::minstd_rand gen(0);

	std::string output_root = options.output_prefix + name_prefix + "_whales/";
	add_config(name_prefix + "_whales");

	if (mkdir_safe(output_root.c_str())) {
		std::printf("directory %s already exists, continuing\n", output_root.c_str());
	}

	save_params(params, output_root);

	options.whale_percentage = 0.1;
	options.whale_multiplier = 10;

	GeneratorState generator(gen, options, output_root);
	generator.make_blocks();
}

void gen_biased_assets(GenerationOptions options, ExperimentParameters params, std::string name_prefix) {
	std::minstd_rand gen(0);

	std::string output_root = options.output_prefix + name_prefix + "_biased_assets/";
	add_config(name_prefix + "_biased_assets");

	if (mkdir_safe(output_root.c_str())) {
		std::printf("directory %s already exists, continuing\n", output_root.c_str());
	}

	save_params(params, output_root);

	options.asset_bias = 0.3;

	GeneratorState generator(gen, options, output_root);
	generator.make_blocks();
}


int main(int argc, char const *argv[])
{
	if (argc != 4) {
		std::printf("usage: ./aberrant_data_gen <edce_options> <base_template_yaml> <name_prefix\n");
		return -1;
	}


	GenerationOptions options;
	auto parsed = options.parse(argv[2]);
	if (!parsed) {
		std::printf("yaml parse error\n");
		return -1;
	}
	std::printf("done parse\n");

	//auto txs = make_blocks(gen, options);

	ExperimentParameters params;
	params.num_assets = options.num_assets;
	params.num_accounts = options.num_accounts;

	EdceOptions edce_options;
	edce_options.parse_options(argv[1]);
	params.tax_rate = edce_options.tax_rate;
	params.smooth_mult = edce_options.smooth_mult;
	params.num_threads = 0; // SET LATER
	params.persistence_frequency = edce_options.persistence_frequency;
	params.num_blocks = options.num_blocks;

	std::string name_prefix = std::string(argv[3]);

	if (mkdir_safe(options.output_prefix.c_str())) {
		std::printf("directory %s already exists, continuing\n", options.output_prefix.c_str());
	}

	if (params.num_assets != edce_options.num_assets) {
		throw std::runtime_error("mismatch in number of assets.  Are you sure?");
	}
	gen_upper_outliers(options, params, name_prefix);
	gen_outlier_prices(options, params, name_prefix);
	gen_tight_cluster(options, params, name_prefix);
	gen_no_cluster(options, params, name_prefix);
	gen_gap_at_market_prices(options, params, name_prefix);
	gen_50percent_good(options, params, name_prefix);
	gen_10percent_good(options, params, name_prefix);
	gen_whales(options, params, name_prefix);
	gen_biased_assets(options, params, name_prefix);		

	std::string name_list_file = options.output_prefix + "experiments_list";

	if (save_xdr_to_file(config_list, name_list_file.c_str())) {
		throw std::runtime_error("failed to save name list file!");
	}
	return 0;
}
