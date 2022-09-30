#include "memory_database/memory_database.h"

#include "mtt/utils/time.h"

#include "crypto/crypto_utils.h"

#include <getopt.h>

#include "utils/save_load_xdr.h"
#include "xdr/experiments.h"

#include "filtering/filter_log.h"

#include <tbb/global_control.h>


[[noreturn]]
static void usage() {
	std::printf(R"(
usage: filtering_experiment --exp_name=<experiment_name, required> --num_threads=<number, required>
)");
	exit(1);
}
enum opttag {
	EXPERIMENT_NAME,
	NUM_THREADS,
};

static const struct option opts[] = {
	{"exp_name", required_argument, nullptr, EXPERIMENT_NAME},
	{"num_threads", required_argument, nullptr, NUM_THREADS},
	{nullptr, 0, nullptr, 0}
};

using namespace speedex;

int main(int argc, char* const* argv)
{
	MemoryDatabaseGenesisData memdb_genesis;

	std::string experiment_name;
	std::string nthreads;
	
	int opt;

	while ((opt = getopt_long_only(argc, argv, "",
				 opts, nullptr)) != -1)
	{
		switch(opt) {
			case EXPERIMENT_NAME:
				experiment_name = optarg;
				break;
			case NUM_THREADS:
				nthreads = optarg;
				break;

			default:
				usage();
		}
	}

	if (experiment_name.size() == 0) {
		usage();
	}

	if (nthreads.size() == 0)
	{
		usage();
	}

	std::string experiment_root = std::string("experiment_data/") + experiment_name;

	std::string account_list_filename = experiment_root + "/accounts";

	if (load_xdr_from_file(memdb_genesis.id_list, account_list_filename.c_str())) {
		throw std::runtime_error("could not open zeroblock account list file");
	}

	DeterministicKeyGenerator key_gen;

	memdb_genesis.pk_list.resize(memdb_genesis.id_list.size());
	tbb::parallel_for(
		tbb::blocked_range<size_t>(0, memdb_genesis.id_list.size()),
		[&key_gen, &memdb_genesis](auto r) {
			for (size_t i = r.begin(); i < r.end(); i++) {
				memdb_genesis.pk_list[i] = key_gen.deterministic_key_gen(memdb_genesis.id_list[i]).second;
			}
		});

	size_t num_assets = 50;
	size_t default_amount = 1000'000;

	size_t num_threads = std::stoi(nthreads);

	for (size_t trial = 1; ; trial++)
	{
		MemoryDatabase db;

		auto account_init_lambda = [&] (UserAccount& user_account) -> void {
			for (auto i = 0u; i < num_assets; i++) {
				user_account.transfer_available(i, default_amount);
			}
			user_account.commit();
		};

		db.install_initial_accounts_and_commit(memdb_genesis, account_init_lambda);

		std::printf("num_accounts: %lu\n", memdb_genesis.pk_list.size());
		std::printf("db size: %lu\n", db.size());

		xdr::opaque_vec<> vec;
		ExperimentBlock block;
		std::string filename = experiment_root + "/" + std::to_string(trial) + ".txs";

		if (load_xdr_from_file(vec, filename.c_str())) {
			std::printf("no trial %lu, exiting\n", trial);
			break;
		}

		xdr::xdr_from_opaque(vec, block);

		tbb::global_control control(
			tbb::global_control::max_allowed_parallelism, num_threads);

		auto ts = utils::init_time_measurement();

		//ExperimentBlock truncated;
		//truncated.insert(truncated.end(), block.begin(), block.begin() + 10);

		FilterLog log;
		log.add_txs(block, db);

		auto res = utils::measure_time(ts);

		std::printf("duration: %lf\n", res);

		size_t num_valid_with_txs = 0, num_invalid = 0;

		for (auto const& acct : memdb_genesis.id_list)
		{

			switch(log.check_valid_account(acct))
			{
				case FilterResult::VALID_NO_TXS:
				{
					break;
				}
				case FilterResult::VALID_HAS_TXS:
				{
					num_valid_with_txs++;
					break;
				}
				case FilterResult::INVALID:
				{
					num_invalid++;
					break;
				}
				default:
					throw std::runtime_error("invalid filter result");
			}
		}
		std::printf("stats: num_valid_with_txs %lu num_invalid %lu total %lu\n", num_valid_with_txs, num_invalid, num_valid_with_txs + num_invalid);
	}

}


