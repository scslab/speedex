#pragma once

#include <atomic>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <shared_mutex>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <xdrpp/marshal.h>

#include "memory_database/background_thunk_clearer.h"
#include "memory_database/typedefs.h"
#include "memory_database/user_account.h"

#include "trie/merkle_trie.h"
#include "trie/prefix.h"

#include "utils/big_endian.h"

#include "xdr/database_commitments.h"
#include "xdr/types.h"
#include "xdr/transaction.h"

#include "lmdb/lmdb_wrapper.h"
#include "utils/time.h"

#include "config.h"
#include "modlog/account_modification_log.h"		


/*
//TODO the commit model might be unnecessary for block production?  specifically the loading atomic vals into regular vals.  Maybe gives faster access though.

Commit loads atomic asset numbers into stable locations, and more importantly, merges in new accounts to main db.

Block production workflow:
- tx processing
- commit 
- do offer clearing
- commit again
- hashing (uses committed db values).

Block validation
- tx processing
- tentative_commit_for_validation
- other work, etc.
- tentative_produce_state_commitment

Then if success:
- commit()
- finalize_produce_state_commitment() in this order

If fail
- rollback_for_validation()
- rollback_produce_state_commitment(); in this order
*/

namespace speedex {

class MemoryDatabase;

/*! Wrapper class around an LMDB instance for the account database,
with some extra parameters filled in (i.e. database file location).
*/
struct AccountLMDB : public LMDBInstance {

	constexpr static auto DB_NAME = "account_lmdb";

	AccountLMDB() : LMDBInstance() {}

	void open_env() {
		LMDBInstance::open_env(
			std::string(ROOT_DB_DIRECTORY) + std::string(ACCOUNT_DB));
	}

	void create_db() {
		LMDBInstance::create_db(DB_NAME);
	}

	void open_db() {
		LMDBInstance::open_db(DB_NAME);
	}

	using LMDBInstance::sync;
};


/*! Database thunk entry.
Key of modified account, and bytes vector to store in database.
*/
struct ThunkKVPair {
	AccountID key;
	xdr::opaque_vec<> msg;

	ThunkKVPair() = default;
};

/*! Transient value for accumulating the contents of a DBPersistenceThunk.

The nonstandard operator= takes in an accountID and computes the ThunkKVPair
for the account.

Used when iterating over an account mod log.
*/
struct KVAssignment {
	ThunkKVPair& kv;
	const MemoryDatabase& db;
	void operator=(const AccountID owner);
};


/*! Stores all of the changes to the account database, to be persisted to disk
later.

Acts as a vector for the purposes of account mod log's 
parallel_accumulate_values().  Returns KVAssignment objects in response to 
operator[].  Operator=, applied to these output values, inserts a ThunkKVPair
into this thunk (at the location that was given to operator[]).
*/
struct DBPersistenceThunk {
	using thunk_list_t = std::vector<ThunkKVPair>;

	std::unique_ptr<thunk_list_t> kvs;
	MemoryDatabase* db;
	uint64_t current_block_number;

	DBPersistenceThunk(MemoryDatabase& db, uint64_t current_block_number)
		: kvs(std::make_unique<thunk_list_t>())
		, db(&db)
		, current_block_number(current_block_number) {}

	KVAssignment operator[](size_t idx);

	void clear() {
		kvs.reset();
	}

	void resize(size_t sz) {
		kvs->resize(sz);
	}

	void reserve(size_t sz) {
		kvs->reserve(sz);
	}
	
	size_t size() const {
		return kvs->size();
	}
};

struct AccountCreationThunk {
	uint64_t current_block_number;
	uint64_t num_accounts_created;
};

/*!
An in memory datastore mapping AccountIDs to account balances.

Transaction processing should be stopped before running 
commit/rollback/check_valid_state/produce_commitment.
We do not put a lock on every db call.  However,
commit/rollback/check_valid_state are locked against each other,
and database persistence can be done safely in the background.

*/

class MemoryDatabase {
public:
	constexpr static int TRIE_KEYLEN = sizeof(AccountID);

	using DBEntryT = UserAccount;
	using DBMetadataT = CombinedMetadata<SizeMixin>;
	using DBStateCommitmentValueT = XdrTypeWrapper<AccountCommitment>;

	using trie_prefix_t = AccountIDPrefix;

	using DBStateCommitmentTrie 
		= MerkleTrie<trie_prefix_t, DBStateCommitmentValueT, DBMetadataT>;


	static inline void write_trie_key(trie_prefix_t& buf, AccountID account) {
		buf = trie_prefix_t{account};
	}

	using index_map_t = std::map<AccountID, account_db_idx>;

private:

	index_map_t user_id_to_idx_map;
	index_map_t uncommitted_idx_map;
	std::set<AccountID> reserved_account_ids;

	std::vector<DBEntryT> database;
	std::vector<DBEntryT> uncommitted_db;

	mutable std::shared_mutex committed_mtx;
	std::shared_mutex uncommitted_mtx;

	std::mutex db_thunks_mtx;


	DBStateCommitmentTrie commitment_trie;

	AccountLMDB account_lmdb_instance;
	BackgroundThunkClearer<DBPersistenceThunk> background_thunk_clearer;

	std::vector<DBPersistenceThunk> persistence_thunks;
	std::vector<AccountCreationThunk> account_creation_thunks;

	//delete copy constructors, implicitly blocks move ctors
	MemoryDatabase(const MemoryDatabase&) = delete;
	MemoryDatabase& operator=(const MemoryDatabase&) = delete;

	UserAccount& find_account(account_db_idx user_index);

	friend class MemoryDatabaseView;

	bool account_exists(AccountID account);

	TransactionProcessingStatus reserve_account_creation(const AccountID account);
	void release_account_creation(const AccountID account);
	void commit_account_creation(
		const AccountID account, DBEntryT&& user_account);

	void clear_internal_data_structures();

	friend class UnbufferedMemoryDatabaseView;
	friend class BufferedMemoryDatabaseView;
	friend class AccountCreationView;
	friend class UnlimitedMoneyBufferedMemoryDatabaseView;

	void _produce_state_commitment(Hash& hash);

	void rollback_new_accounts_(uint64_t current_block_number);

	void set_trie_commitment_to_user_account_commits(const AccountModificationLog& log);


	friend class ValidityCheckLambda;
	//! Check whether one account (by db idx) is in a valid state.
	bool _check_valid(account_db_idx account_idx) {
		if (account_idx >= database.size()) {
			/*
				Newly created accounts are always valid.
				Transaction validation checks that ID is well formed,
				pk exists, etc.

				Creation transfers a small amount of money to the account,
				and that account cannot send its own transactions 
				until the next block.

				It can receive payments within the dbview of the tx that 
				created the account, but receiving payments can't make an
				account invalid.
			*/
			return true;
		}
		return database[account_idx].in_valid_state();
	}
public:

	uint64_t get_persisted_round_number() {
		return account_lmdb_instance.get_persisted_round_number();
	}

	MemoryDatabase()
		: user_id_to_idx_map(),
		uncommitted_idx_map(),
		reserved_account_ids(),
		database(),
		uncommitted_db(),
		committed_mtx(),
		uncommitted_mtx(),
		commitment_trie(),
		account_lmdb_instance() {};

	uint64_t size() const {
		return database.size();
	}

	//Be careful with creating new accounts.  If we have to rearrange database/resize vector, we'll break any pointers we export, probably.
	//We use integer indexes so that we can enforce the invariant that these indices never change.  We could reuse indices if we delete accounts.

	account_db_idx add_account_to_db(AccountID account, const PublicKey pk = PublicKey{});

	// Obviously not threadsafe with value modification

	void commit(uint64_t current_block_number) {
		commit_new_accounts(current_block_number);
		commit_values();
	}

	void commit_values(const AccountModificationLog& dirty_accounts);

	void _commit_value(account_db_idx account_idx) {
		database[account_idx].commit();
	}

	void commit_values();
	void rollback_values();

	void commit_new_accounts(uint64_t current_block_number);
	void rollback_new_accounts(uint64_t current_block_number);

	void produce_state_commitment(Hash& hash, const AccountModificationLog& log);
	void produce_state_commitment() {
		//for init only
		std::lock_guard lock(committed_mtx);
		Hash hash;
		_produce_state_commitment(hash);
	}

	void produce_state_commitment(Hash& hash) {
		std::lock_guard lock(committed_mtx);
		_produce_state_commitment(hash);
	}

	void tentative_produce_state_commitment(
		Hash& hash, const AccountModificationLog& log);

	void rollback_produce_state_commitment(const AccountModificationLog& log);
	void finalize_produce_state_commitment();
	
	void persist_lmdb(uint64_t current_block_number);

	void open_lmdb_env() {
		account_lmdb_instance.open_env();
	}
	void create_lmdb() {
		account_lmdb_instance.create_db();
	}

	void open_lmdb() {
		account_lmdb_instance.open_db();
	}

	void load_lmdb_contents_to_memory();

	bool lookup_user_id(AccountID account, account_db_idx* index_out) const;

	//input to index is what is returned from lookup

	void transfer_available(
		account_db_idx user_index, AssetID asset_type, int64_t change);
	void transfer_escrow(
		account_db_idx user_index, AssetID asset_type, int64_t change);
	void escrow(
		account_db_idx user_index, AssetID asset_type, int64_t change);

	bool conditional_transfer_available(
		account_db_idx user_index, AssetID asset_type, int64_t change);
	bool conditional_escrow(
		account_db_idx user_index, AssetID asset_type, int64_t change);


	int64_t lookup_available_balance(
		account_db_idx user_index, AssetID asset_type);

	TransactionProcessingStatus reserve_sequence_number(
		account_db_idx user_index, uint64_t sequence_number);

	void release_sequence_number(
		account_db_idx user_index, uint64_t sequence_number);

	void commit_sequence_number(
		account_db_idx user_index, uint64_t sequence_number);

	std::optional<PublicKey> get_pk(AccountID account) const;

	//not threadsafe with commit/rollback
	std::optional<PublicKey> get_pk_nolock(AccountID account) const;

	void log();
	void values_log();

	/*! Checks whether the database is in a valid state

	Specifically checks every modified account to validate that
	all account balances are positive.
	Obviously not threadsafe with logging asset changes
	not threadsafe with creating accounts or committing or rolling back
	*/
	bool check_valid_state(const AccountModificationLog& dirty_accounts);

	AccountCommitment produce_commitment(account_db_idx idx) const {
		return database[idx].produce_commitment();
	}

	/*! Generate a peristence thunk given a log of which accounts were modified.
	*/
	void 
	add_persistence_thunk(
		uint64_t current_block_number, AccountModificationLog& log);
	
	/*! Commit stored persistence thunks, up to and including the input
	round number.
	*/
	void 
	commit_persistence_thunks(uint64_t max_round_number);
	
	void force_sync() {
		account_lmdb_instance.sync();
	}

	/*! Clear persistence thunks and reload database state from LDMB.

	Expects the input round number to be the round number in the database.
	If not, then we must have persisted some amount of data that we were not
	supposed to (which means some kind of bug).
	*/
	void clear_persistence_thunks_and_reload(
		uint64_t expected_persisted_round_number);

};

} /* speedex */
