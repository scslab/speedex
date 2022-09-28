#include "cda/serial_ob_experiment.h"

#include "synthetic_data_generator/synthetic_data_gen.h"

#include "memory_database/memory_database.h"

#include "speedex/speedex_management_structures.h"

#include "block_processing/block_producer.h"

#include <mtt/utils/time.h>

#include "stats/block_update_stats.h"

#include "speedex/speedex_operation.h"

#include "mempool/mempool.h"

#include "utils/debug_macros.h"

#include <getopt.h>

#include <tbb/global_control.h>

using namespace speedex;

[[noreturn]]
static void usage() {
	std::printf(R"(
usage: blockstm_comparison --num_accounts=<n> --batch_size=<n>
)");
	exit(1);
}
enum opttag {
	NUM_ACCOUNTS,
	BATCH_SIZE
};

static const struct option opts[] = {
	{"num_accounts", required_argument, nullptr, NUM_ACCOUNTS},
	{"batch_size", required_argument, nullptr, BATCH_SIZE},
	{nullptr, 0, nullptr, 0}
};

void run_blockstm_experiment(const uint32_t num_accounts, const uint32_t batch_size, const uint32_t num_threads);

int main(int argc, char* const* argv)
{
	int opt;

	uint64_t num_accounts = 0;
	uint64_t batch_size = 0;

	while ((opt = getopt_long_only(argc, argv, "",
				 opts, nullptr)) != -1)
	{
		switch(opt) {
			case NUM_ACCOUNTS:
				num_accounts = std::stol(optarg);
				break;
			case BATCH_SIZE:
				batch_size = std::stol(optarg);
				break;
			default:
				usage();
		}
	}

	if (num_accounts == 0 || batch_size == 0) {
		usage();
	}

	std::vector<uint32_t> thread_counts = {1, 2, 4, 8, 16, 32, 48, 96};

	for (auto n : thread_counts)
	{
		run_blockstm_experiment(num_accounts, batch_size, n);
	}
}


void speedex_block_creation_logic_notatonnement(
	SpeedexManagementStructures& management_structures,
	const HashedBlock& prev_block,
	OverallBlockProductionMeasurements& overall_measurements,
	BlockStateUpdateStatsWrapper& state_update_stats)
{
	auto& stats = overall_measurements.block_creation_measurements;

	HashedBlock new_block;

	auto prev_block_number = prev_block.block.blockNumber;

	uint64_t current_block_number = prev_block_number + 1;
	
	// Allocate a file descriptor/file for persisting account modification log.
	management_structures.account_modification_log.prepare_block_fd(
		current_block_number);

	auto timestamp = utils::init_time_measurement();

	auto& db = management_structures.db;
	auto& orderbook_manager = management_structures.orderbook_manager;

	//Push new accounts into main db (also updates database merkle trie).
	db.commit_new_accounts(current_block_number);
	stats.initial_account_db_commit_time = utils::measure_time(timestamp);


	//! Commits newly created offers, preps orderbooks for Tatonnement
	orderbook_manager.commit_for_production(current_block_number);

	stats.initial_offer_db_commit_time =utils::measure_time(timestamp);


	stats.offer_clearing_time =utils::measure_time(timestamp);

	if (!db.check_valid_state(management_structures.account_modification_log)) {
		throw std::runtime_error("DB left in invalid state!!!");
	}

	stats.db_validity_check_time =utils::measure_time(timestamp);

	db.commit_values(management_structures.account_modification_log);

	stats.final_commit_time =utils::measure_time(timestamp);

	auto& hashing_measurements 
		= overall_measurements.production_hashing_measurements;

	detail::speedex_make_state_commitment(
		new_block.block.internalHashes,
		management_structures,
		hashing_measurements);

	overall_measurements.state_commitment_time =utils::measure_time(timestamp);

	std::vector<Price> price_workspace;
	price_workspace.resize(management_structures.orderbook_manager.get_num_assets());

	detail::speedex_format_hashed_block(
		new_block,
		prev_block,
		price_workspace,
		0);

	overall_measurements.format_time = utils::measure_time(timestamp);

	management_structures.block_header_hash_map.insert(new_block.block, true);//new_block.block.blockNumber, new_block.hash);
}

void run_blockstm_experiment(const uint32_t num_accounts, const uint32_t batch_size, const uint32_t num_threads)
{
 	tbb::global_control control(tbb::global_control::max_allowed_parallelism, num_threads);

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

	SpeedexManagementStructures management_structures(options.num_assets, ApproximationParameters());
	
	BlockStateUpdateStatsWrapper state_update_stats;
	BlockCreationMeasurements measurements;
	OverallBlockProductionMeasurements overall_measurements;

	auto& db = management_structures.db;

	GeneratorState generator (gen, options, "foo");

	MemoryDatabaseGenesisData data;
	data.id_list = generator.get_accounts();
	data.pk_list.resize(data.id_list.size());

	for (size_t i = 0; i < data.pk_list.size(); i++)
	{
		data.pk_list[i] = generator.signer.get_public_key(data.id_list[i]);
	}

	auto init_lambda = [&options](UserAccount& user)
	{
		user.transfer_available(MemoryDatabase::NATIVE_ASSET, options.new_account_balance);
		user.commit();
	};

	db.install_initial_accounts_and_commit(data, init_lambda);

	std::vector<double> prices;

	ExperimentBlock block = generator.make_block(prices);

	Mempool mempool(1, 1'000'000);

	mempool.chunkify_and_add_to_mempool_buffer(std::move(block));
	mempool.push_mempool_buffer_to_mempool();

	LogMergeWorker log_merge_worker(management_structures.account_modification_log);
	BlockProducer producer(management_structures, log_merge_worker);

	//block processing:
	// step 1: assemble block
	// step 2: block creation logic
	// step 3: persist/commit (we skip here)

	auto ts = utils::init_time_measurement();

	//step 1
	producer.build_block(
		mempool,
		batch_size,
		measurements,
		state_update_stats);

	if (state_update_stats.payment_count != batch_size)
	{
		throw std::runtime_error("block sz mismatch");
	}

	//step 2

	HashedBlock prev_block;

	speedex_block_creation_logic_notatonnement(
		management_structures,
		prev_block,
		overall_measurements,
		state_update_stats);

	//step 3
	//nothing

	double res = utils::measure_time(ts);

	std::printf("nthreads = %lu batch_size = %lu num_accounts = %lu time = %lf tps = %lf\n", num_threads, batch_size, num_accounts, res, batch_size / res);
}
