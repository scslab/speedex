#include "memory_database/memory_database.h"

#include <utils/time.h>

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
};

static const struct option opts[] = {
	{"exp_name", required_argument, nullptr, EXPERIMENT_NAME},
	{nullptr, 0, nullptr, 0}
};

using namespace speedex;

double run_experiment(const size_t n_threads, std::string const& experiment_root, const std::vector<AccountID>& id_list, MemoryDatabase const& db);

int main(int argc, char* const* argv)
{
	MemoryDatabaseGenesisData memdb_genesis;

	std::string experiment_name = "filtering";	
	int opt;

	while ((opt = getopt_long_only(argc, argv, "",
				 opts, nullptr)) != -1)
	{
		switch(opt) {
			case EXPERIMENT_NAME:
				experiment_name = optarg;
				break;

			default:
				usage();
		}
	}

	if (experiment_name.size() == 0) {
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
	size_t default_amount = 1;

	MemoryDatabase db;

	auto account_init_lambda = [&] (UserAccount& user_account) -> void {
		db.transfer_available(&user_account, MemoryDatabase::NATIVE_ASSET, 10000);
		for (auto i = 0u; i < num_assets; i++) {
			db.transfer_available(&user_account, i, default_amount);
		}
		user_account.commit();
	};

	db.install_initial_accounts_and_commit(memdb_genesis, account_init_lambda);

	std::vector<uint32_t> thread_counts = {1, 6, 12, 24, 48};
	std::map<uint32_t, double> res;

	for (auto n : thread_counts)
	{
		double avg = run_experiment(n, experiment_root, memdb_genesis.id_list, db);
		std::printf("avg experiment %s with %lu threads: %lf\n", experiment_root.c_str(), avg, n);

		res[n] = avg;
	}

	std::printf("=======results:=======\n");
	for (auto n : thread_counts)
	{
		std::printf("n: %lu time %lf speedup relative to 1x %lf\n", n, res[n], res[n] / res[1]);
	}

}

double run_experiment(const size_t n_threads, std::string const& experiment_root, const std::vector<AccountID>& id_list, MemoryDatabase const& db)
{
	FilterLog log;

	std::printf("num accounts %lu num_threads %lu\n", id_list.size());
	tbb::global_control control(
		tbb::global_control::max_allowed_parallelism, n_threads);

	double acc = 0;

	size_t trial = 0;

	for (; ; trial++)
	{
		xdr::opaque_vec<> vec;
		ExperimentBlock block;
		std::string filename = experiment_root + "/" + std::to_string(trial + 1) + ".txs";

		if (load_xdr_from_file(vec, filename.c_str())) {
			std::printf("no trial %lu, exiting\n", trial);
			break;
		}

		xdr::xdr_from_opaque(vec, block);

		std::printf("trial size %lu txs\n", block.size());

		auto ts = utils::init_time_measurement();

		log.add_txs(block, db);

		auto res = utils::measure_time(ts);

		acc += res;

		std::printf("duration: %lf\n", res);

		size_t num_valid_with_txs = 0, num_bad_duplicates = 0, num_missing_requirements = 0;

		for (auto const& acct : id_list)
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
				case FilterResult::INVALID_DUPLICATE:
				{
					num_bad_duplicates++;
					break;
				}
				case FilterResult::MISSING_REQUIREMENT:
				{
					num_missing_requirements++;
					break;
				}
				default:
					throw std::runtime_error("invalid filter result");
			}
		}
		log.clear();
		std::printf("stats: num_valid_with_txs %lu num_bad_duplicates %lu num_missing_requirements %lu total %lu\n", 
			num_valid_with_txs, num_bad_duplicates,
			num_missing_requirements,
			 num_valid_with_txs + num_bad_duplicates + num_missing_requirements);
	}
	return acc / trial;
}



