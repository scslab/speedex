#include <catch2/catch_test_macros.hpp>

#include "crypto/crypto_utils.h"

#include "memory_database/memory_database.h"
#include "memory_database/user_account.h"

#include "modlog/account_modification_log.h"

#include "utils/manage_data_dirs.h"

#include "xdr/types.h"

#include <tbb/parallel_for.h>

#include <vector>

using xdr::operator==;

namespace speedex
{

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

	auto account_init_lambda = [&num_assets, &default_amount, &db] (UserAccount& user_account) -> void {
		for (auto i = 0u; i < num_assets; i++) {
			db.transfer_available(&user_account, i, default_amount);
		}
		user_account.commit();
	};

	db.install_initial_accounts_and_commit(memdb_genesis, account_init_lambda);

	db.persist_lmdb(0);
}

void modify_db_entry(SerialAccountModificationLog& log, MemoryDatabase& db, AccountID acct, uint32_t asset, int64_t delta) {
	UserAccount* idx = db.lookup_user(acct);
	REQUIRE(idx != nullptr);

	db.transfer_available(idx, asset, delta);
	log.log_self_modification(acct, 0);
}

void assert_balance(MemoryDatabase& db, AccountID acct, uint32_t asset, int64_t amount) {
	UserAccount* idx = db.lookup_user(acct);
	REQUIRE(idx != nullptr);

	REQUIRE(db.lookup_available_balance(idx, asset) == amount);
}


TEST_CASE("set genesis", "[memdb]")
{
	test::speedex_dirs s;

	MemoryDatabase db;

	init_memdb(db, 10000, 10, 15);

	REQUIRE(db.size() == 10000u);

	UserAccount* idx = db.lookup_user(0);
	REQUIRE(idx != nullptr);
	//TS_ASSERT(db.lookup_user_id(0, &idx));
	REQUIRE(db.lookup_available_balance(idx, 0) == 15);
	REQUIRE(db.lookup_available_balance(idx, 1) == 15);
	REQUIRE(db.lookup_available_balance(idx, 10) == 0);

	idx = db.lookup_user(9999);
	REQUIRE(idx != nullptr);
	REQUIRE(db.lookup_available_balance(idx, 5) == 15);


	idx = db.lookup_user(10000);
	REQUIRE(idx == nullptr);
}

TEST_CASE("set big genesis", "[memdb]")
{
	test::speedex_dirs s;

	MemoryDatabase db;

	init_memdb(db, 1000000, 10, 15);

	REQUIRE(db.size() == 1'000'000u);

	for (size_t i = 0; i < 1'000'000; i += 1000) {
		UserAccount* idx = db.lookup_user(i);
		REQUIRE(idx != nullptr);
	}

	UserAccount* idx = db.lookup_user(1'000'000);
	REQUIRE(idx == nullptr);
}

TEST_CASE("rollback account values", "[memdb]")
{	
	test::speedex_dirs s;

	MemoryDatabase db;

	init_memdb(db, 10000, 10, 15);

	REQUIRE(db.size() == 10000u);

	AccountModificationLog modlog;
	{
		SerialAccountModificationLog log(modlog);

		modify_db_entry(log, db, 0, 1, 30);

		modlog.merge_in_log_batch();
	}

	db.commit_values(modlog);

	// writes round 1 to lmdb
	db.add_persistence_thunk(1, modlog);
	db.commit_persistence_thunks(1);

	modlog.detached_clear();

	assert_balance(db, 0, 1, 45);

	{
		SerialAccountModificationLog log(modlog);

		modify_db_entry(log, db, 0, 1, 20);

		assert_balance(db, 0, 1, 65);

		modlog.merge_in_log_batch();
	}

	//make a round 2 persistence thunk, but do not commit
	db.commit_values(modlog);
	db.add_persistence_thunk(2, modlog);

	//reload data from lmdb at round 1
	db.clear_persistence_thunks_and_reload(1);

	// must have reloaded the original balance after rd 1.
	assert_balance(db, 0, 1, 45);
}

TEST_CASE("rollback with gaps", "[memdb]")
{
	test::speedex_dirs s;
	
	MemoryDatabase db;

	init_memdb(db, 10000, 10, 15);

	REQUIRE(db.size() == 10000u);

	AccountModificationLog modlog;
	{
		SerialAccountModificationLog log(modlog);
		modify_db_entry(log, db, 500, 1, 30);
		modlog.merge_in_log_batch();
	}

	db.commit_values(modlog);

	// writes round 1 to lmdb
	db.add_persistence_thunk(1, modlog);
	db.commit_persistence_thunks(1);
	modlog.detached_clear();

	assert_balance(db, 500, 1, 45);
	{
		SerialAccountModificationLog log(modlog);
		modify_db_entry(log, db, 501, 1, -10);
		modlog.merge_in_log_batch();
	}

	db.add_persistence_thunk(5, modlog);
	db.commit_persistence_thunks(3);

	REQUIRE_THROWS(db.clear_persistence_thunks_and_reload(4));

	db.commit_persistence_thunks(4);

	db.clear_persistence_thunks_and_reload(4);

	// 500 changed in round 1, so no rollback
	assert_balance(db, 500, 1, 45);
	// 501 changed in round 5, so rolled back
	assert_balance(db, 501, 1, 15);
}

}
