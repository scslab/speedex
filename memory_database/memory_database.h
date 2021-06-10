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

#include "user_account.h"
#include "xdr/types.h"
#include "xdr/transaction.h"
#include "database_types.h"
#include "merkle_trie.h"
#include "merkle_trie_utils.h"
#include "xdr/database_commitments.h"

#include "lmdb_wrapper.h"
#include "utils.h"


#include "../config.h"
#include "account_modification_log.h"		

#include "memory_database/memory_database_auxiliary.h"

/*
Transaction processing should be stopped before running commit/rollback/check_valid_state/produce_commitment.
But this is a global property.  We do not put a lock on every db call (and indeed, we would regardless need some
global modification lock).

Commit/rollback/check_valid_state/produce commitment are locked so as to be safe with each other.
All db modifications are locked (or not) so as to be threadsafe with each other.

These two sets together are not threadsafe.
*/


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

class MemoryDatabase {
public:
	constexpr static int TRIE_KEYLEN = sizeof(AccountID);

	using DBEntryT = UserAccount;//std::unique_ptr<UserAccount>;
	using DBMetadataT = CombinedMetadata<SizeMixin>;
	using DBStateCommitmentValueT = XdrTypeWrapper<AccountCommitment>;

	using DBStateCommitmentTrie = MerkleTrie<TRIE_KEYLEN, DBStateCommitmentValueT, DBMetadataT>;

	static inline void write_trie_key(DBStateCommitmentTrie::prefix_t& buf, AccountID account) {
		PriceUtils::write_unsigned_big_endian(buf, account);
	}

	using index_map_t = std::map<AccountID, account_db_idx>;

private:

	friend class UserAccountWrapper;


	index_map_t user_id_to_idx_map;
	index_map_t uncommitted_idx_map;
	std::set<AccountID> reserved_account_ids;

	std::vector<DBEntryT> database;
	std::vector<DBEntryT> uncommitted_db;
	//std::vector<
	//	std::pair<AccountID, UserAccountWrapper>> uncommitted_trie_entries;

	mutable std::shared_mutex committed_mtx;
	std::shared_mutex uncommitted_mtx;

	std::mutex db_thunks_mtx;


	DBStateCommitmentTrie commitment_trie;

	AccountLMDB account_lmdb_instance;

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

	//void _rollback(); // rollback without the locks

	void _produce_state_commitment(Hash& hash);
//	void _produce_state_commitment(Hash& hash, const std::vector<AccountID>& dirty_accounts);

	void rollback_new_accounts_(uint64_t current_block_number);

	void set_trie_commitment_to_user_account_commits(const AccountModificationLog& log);
public:

	uint64_t get_persisted_round_number() {
		return account_lmdb_instance.get_persisted_round_number();
	}

	using FrozenDBStateCommitmentTrie = FrozenMerkleTrie<TRIE_KEYLEN, DBStateCommitmentValueT, DBMetadataT>;

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

	const static AssetID NATIVE_ASSET = 0;

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

	bool _check_valid(account_db_idx account_idx) {
		if (account_idx >= database.size()) {
			return true; //newly created accounts are always valid
		}
		return database[account_idx].in_valid_state();
	}

	void commit_values();
	void rollback_values();

	void commit_new_accounts(uint64_t current_block_number);
	void rollback_new_accounts(uint64_t current_block_number);

	//void produce_state_commitment(Hash& hash, const std::vector<AccountID>& dirty_accounts);
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

	//void tentative_produce_state_commitment(Hash& hash, const std::vector<AccountID>& dirty_accounts);
	void tentative_produce_state_commitment(Hash& hash, const AccountModificationLog& log);

	void rollback_produce_state_commitment(const AccountModificationLog& log);
	void finalize_produce_state_commitment();
	
	//std::optional<dbenv::wtxn> persist_lmdb(uint64_t current_block_number, AccountModificationLog& log, bool lazy_commit = false);
	//std::optional<dbenv::wtxn> persist_lmdb(uint64_t current_block_number, const std::vector<AccountID>& dirty_accounts, bool lazy_commit = false);
	void persist_lmdb(uint64_t current_block_number);
	//void finish_persist_lmdb(dbenv::wtxn write_txn, uint64_t current_block_number);

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
	//bool conditional_transfer_escrow(
	//	account_db_idx user_index, AssetID asset_type, int64_t change);
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

	//Obv not threadsafe with logging asset changes
	//not threadsafe with creating accounts or committing or rollbacking
	//not threadsafe with anything, probably. 
	bool check_valid_state();
	bool check_valid_state(const AccountModificationLog& dirty_accounts);

	AccountCommitment produce_commitment(account_db_idx idx) const {
		return database[idx].produce_commitment();
	}

	void add_persistence_thunk(uint64_t current_block_number, AccountModificationLog& log);
	void commit_persistence_thunks(uint64_t max_round_number);
	void force_sync() {
		account_lmdb_instance.sync();
	}
	void clear_persistence_thunks_and_reload(uint64_t expected_persisted_round_number);

};

}
