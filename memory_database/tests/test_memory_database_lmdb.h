#include <cxxtest/TestSuite.h>

#include "crypto/crypto_utils.h"

#include "memory_database/memory_database.h"
#include "memory_database/user_account.h"

#include "xdr/types.h"

#include <tbb/parallel_for.h>

#include <vector>

using namespace speedex;

using xdr::operator==;

class MemoryDatabaseLMDBTestSuite : public CxxTest::TestSuite {
public:

	void setUp() {
		clear_memory_database_lmdb_dir();
		make_memory_database_lmdb_dir();
	}

	void tearDown() {
		clear_memory_database_lmdb_dir();
	}

	void init_memdb(MemoryDatabase& db, uint64_t num_accounts, uint32_t num_assets, uint64_t default_amount)
	{
		db.open_lmdb_env();
		db.create_lmdb();

		MemoryDatabaseGenesisData memdb_genesis;

		for (uint64_t i = 0; i < num_accounts; i++) {
			memdb_genesis.id_list.push_back(i);
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

		auto account_init_lambda = [&num_assets, &default_amount] (UserAccount& user_account) -> void {
			for (auto i = 0u; i < num_assets; i++) {
				user_account.transfer_available(i, default_amount);
			}
			user_account.commit();
		};

		db.install_initial_accounts_and_commit(memdb_genesis, account_init_lambda);

		db.persist_lmdb(0);
	}

	void test_set_genesis() {
		MemoryDatabase db;

		init_memdb(db, 10000, 10, 15);

		TS_ASSERT_EQUALS(db.size(), 10000);

		account_db_idx idx;
		TS_ASSERT(db.lookup_user_id(0, &idx));
		TS_ASSERT_EQUALS(db.lookup_available_balance(idx, 0), 15);
		TS_ASSERT_EQUALS(db.lookup_available_balance(idx, 1), 15);
		TS_ASSERT_EQUALS(db.lookup_available_balance(idx, 10), 0);

		TS_ASSERT(db.lookup_user_id(9999, &idx));
		TS_ASSERT_EQUALS(db.lookup_available_balance(idx, 5), 15);

		TS_ASSERT(!db.lookup_user_id(10000, &idx));
	}


};
