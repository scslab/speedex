#include "speedex/vm/speedex_vm.h"

#include "crypto/crypto_utils.h"

#include "xdr/experiments.h"

namespace speedex {

void
SpeedexVM::init_clean() {

	management_structures.open_lmdb_env();
	management_structures.create_lmdb();

	auto& db = management_structures.db;

	MemoryDatabaseGenesisData memdb_genesis;

	if (load_xdr_from_file(memdb_genesis.id_list, accounts_filename.c_str())) {
		throw std::runtime_error("could not open zeroblock account list file");
	}

	DeterministicKeyGenerator key_gen;

	memdb_genesis.pk_list.resize(memdb_genesis.id_list.size());
	tbb::parallel_for(
		tbb::blocked_range<size_t>(0, memdb_genesis.id_list.size()),
		[&key_gen, &memdb_genesis](auto r) {
			for (size_t i = r.begin(); i < r.end(); i++) {
				memdb_genesis.pk_list[i] = key_gen.deterministic_key_gen(memdb_genesis.id_list[i]);
			}
		});

	auto account_init_lambda = [&num_assets, &default_amount] (UserAccount& user_account) -> void {
		for (auto i = 0; i  num_assets; i++) {
			user_account.transfer_available(i, default_amount);
		}
		user_account.commit();
	};

	db.install_initial_accounts_and_commit(memdb_genesis, account_init_lambda);

	db.persist_lmdb(0);
	management_structures.orderbook_manager.persist_lmdb(0);
	management_structures.block_header_hash_map.persist_lmdb(0);
}

void
SpeedexVM::init_from_disk(hotstuff::HotstuffLMDB const& decided_block_cache)
{
	management_structures.open_lmdb_env();
	management_structures.open_lmdb();

	auto top_block = speedex_load_persisted_data(management_structures, decided_block_cache);
	last_committed_block = top_block;
	proposal_base_block = top_block;
}

} /* speedex */
