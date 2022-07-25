#include "memory_database/memory_database.h"

#include "utils/time.h"

#include "crypto/crypto_utils.h"

#include <getopt.h>

#include "utils/save_load_xdr.h"
#include "xdr/experiments.h"

#include "filtering/filter_log.h"

#include <tbb/global_control.h>


[[noreturn]]
static void usage() {
	std::printf(R"(
usage: filtering_experiment --exp_name=<experiment_name, required>
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

int main(int argc, char* const* argv)
{
	MemoryDatabaseGenesisData memdb_genesis;

	std::string experiment_name;
	
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
	size_t default_amount = 5;

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

		xdr::opaque_vec<> vec;
		ExperimentBlock block;

		std::string filename = experiment_root + "/" + std::to_string(trial) + ".txs";

		if (load_xdr_from_file(vec, filename.c_str())) {
			std::printf("no trial %lu, exiting\n", trial);
			break;
		}

		xdr::xdr_from_opaque(vec, block);


		tbb::global_control control(
			tbb::global_control::max_allowed_parallelism, 1);


		FilterLog log;
		log.add_txs(block, db);

		for (auto const& acct : memdb_genesis.id_list)
		{
			if (!log.check_valid_account(acct))
			{
				std::printf("invalid\n");
			}
		}
	}

}


