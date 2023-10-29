#include "synthetic_data_generator/synthetic_data_gen.h"

#include "memory_database/memory_database.h"

#include "speedex/speedex_management_structures.h"

#include "block_processing/block_producer.h"
#include "automation/experiment_control.h"

#include "stats/block_update_stats.h"

#include "speedex/speedex_operation.h"

#include "speedex/vm/speedex_vm.h"

#include "mempool/mempool.h"

#include "utils/debug_macros.h"
#include "utils/manage_data_dirs.h"

#include <utils/mkdir.h>
#include <utils/time.h>

#include "automation/get_experiment_vars.h"

#include <getopt.h>

#include <tbb/global_control.h>

#include "utils/yaml.h"

#include <vector>
#include <cstdint>
#include <cinttypes>

using namespace speedex;

enum opttag {
	NUM_ACCOUNTS,
	BATCH_SIZE
};

static const struct option opts[] = {
	{"num_accounts", required_argument, nullptr, NUM_ACCOUNTS},
	{"batch_size", required_argument, nullptr, BATCH_SIZE},
	{nullptr, 0, nullptr, 0}
};

std::vector<double>
run_blockstm_experiment(const uint32_t num_accounts, const uint32_t batch_size, const uint32_t num_threads);

constexpr static size_t NUM_ROUNDS = 100;
constexpr static size_t WARMUP = 2;

int main(int argc, char* const* argv)
{

	if (!DETAILED_MOD_LOGGING) {
		throw std::runtime_error("you should probably turn on the full modification log generation for a better comparison");
	}

	if (USE_TATONNEMENT_TIMEOUT_THREAD || !DISABLE_PRICE_COMPUTATION)
	{
		throw std::runtime_error("why do you have price comp on here");
	}

	std::vector<uint32_t> thread_counts = {1, 2, 4, 8, 16, 24, 32, 48};

	std::vector<uint32_t> num_accounts = {2, 10, 100, 1000, 10000};

	std::vector<uint32_t> batch_sizes = {100, 1000, 10'000, 100'000};

	std::vector<std::tuple<uint32_t, uint32_t, uint32_t, double>> results;

	for (auto acc : num_accounts)
	{
		for (auto batch : batch_sizes)
		{
			std::printf("n_acc %" PRIu32 " batch %" PRIu32 "\n", acc, batch);
			for (auto n : thread_counts)
			{
				std::printf("threadcount %" PRIu32 "\n", n);

				auto res = run_blockstm_experiment(acc, batch, n);

				if (res.size() != NUM_ROUNDS + WARMUP)
				{
					throw std::runtime_error("invalid size return");
				}

				double avg = 0;
				for (auto i = WARMUP; i < NUM_ROUNDS + WARMUP; i++)
				{
					avg += res[i];
				}
				results.push_back({acc, batch, n, avg/NUM_ROUNDS});
			}
		}
	}

	std::printf("===== results =====\n\n");

	for (auto [acc, batch, n, time] : results)
	{
		std::printf("accounts = %" PRIu32 " batch_size = %" PRIu32 " nthread = %" PRIu32 " time = %lf tps = %lf\n", acc, batch, n, time, batch/time);
	}
}

std::vector<double>
run_blockstm_experiment(const uint32_t num_accounts, const uint32_t batch_size, const uint32_t num_threads)
{
	std::minstd_rand gen(0);
	GenerationOptions options;

	std::string experiment_options_file = "synthetic_data_config/blockstm_base.yaml";

	auto parsed = options.parse(experiment_options_file.c_str());
	if (!parsed) {
		throw std::runtime_error("failed to parse experiment options file");
	}

	options.num_accounts = num_accounts;
	options.block_size = batch_size;

	if (options.num_assets != 1 && options.num_assets > MemoryDatabase::NATIVE_ASSET)
	{
		throw std::runtime_error("invalid num assets");
	}

	std::string speedex_options_file = "experiment_config/blockstm_params.yaml";

	ReplicaID self_id = 0;
	std::string config_file = "config/config_local.yaml";

	yaml fyd(config_file);

	if (!fyd) {
		std::printf("Failed to build doc from file \"%s\"\n", config_file.c_str());
		exit(1);
	}

	auto [config, sk] = parse_replica_config(fyd.get(), self_id);

	ExperimentParameters params;

	params.num_assets = 1;
	params.default_amount = options.new_account_balance;
	params.account_list_filename = "blockstm_accounts";
	params.num_blocks = NUM_ROUNDS + WARMUP;
	params.n_replicas = config->nreplicas;

	if (options.num_assets != params.num_assets)
	{
		throw std::runtime_error("asset amount mismatch");
	}

	utils::mkdir_safe("experiment_data/blockstm_comparison_data/");

	GeneratorState generator (gen, options, "experiment_data/blockstm_comparison_data/");

	generator.dump_account_list(params.account_list_filename);

	SpeedexOptions speedex_options;
	speedex_options.parse_options(speedex_options_file.c_str());

	speedex_options.block_size = batch_size;

	if (speedex_options.num_assets != params.num_assets) {
		throw std::runtime_error("mismatch in num assets between speedex_options and experiment_options");
	}

	auto configs = get_runtime_configs(); 

	clear_all_data_dirs(config->get_info(self_id));
	make_all_data_dirs(config->get_info(self_id));

	std::string experiment_results_folder = "blockstm_comparison_direct_results/";

	auto vm = std::make_shared<SpeedexVM>(params, speedex_options, experiment_results_folder, configs);
	vm -> init_clean();
	
	std::vector<double> prices;
	std::vector<ExperimentBlock> blocks;

	for (size_t i = 0; i < params.num_blocks; i++)
	{
		blocks.push_back(generator.make_block(prices));
	}

	auto& mp = vm -> get_mempool();

	tbb::global_control control(
		tbb::global_control::max_allowed_parallelism, num_threads);

	std::vector<double> results;

	for (size_t i = 0; i < params.num_blocks; i++)
	{
		auto& block = blocks[i];
		mp.chunkify_and_add_to_mempool_buffer(std::move(block));
		mp.push_mempool_buffer_to_mempool();

		auto ts = utils::init_time_measurement();

		auto blk = vm -> propose();
		auto id = blk->get_id();

		vm -> log_commitment(id);

		double t = utils::measure_time(ts);
		results.push_back(t);
	}
	return results;
}	

