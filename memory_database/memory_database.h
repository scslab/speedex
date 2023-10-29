#pragma once

/**
 * SPEEDEX: A Scalable, Parallelizable, and Economically Efficient Decentralized Exchange
 * Copyright (C) 2023 Geoffrey Ramseyer

 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/*! \file memory_database.h
 Manages an in-memory database of all account states.

*/

#include "config.h"

#include "lmdb/lmdb_wrapper.h"

#include "memory_database/account_lmdb.h"
#include "memory_database/account_vector.h"
#include "memory_database/background_thunk_clearer.h"
#include "memory_database/thunk.h"
#include "memory_database/typedefs.h"
#include "memory_database/user_account.h"
#include "memory_database/transfer_logs.h"

#include "mtt/trie/merkle_trie.h"
#include "mtt/common/prefix.h"

#include "xdr/database_commitments.h"
#include "xdr/types.h"
#include "xdr/transaction.h"

#include <xdrpp/marshal.h>

#include <atomic>
#include <cstdio>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <shared_mutex>
#include <unordered_map>
#include <unordered_set>
#include <vector>

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

class AccountModificationLog;

struct AccountCreationThunk {
	uint64_t current_block_number;
	uint64_t num_accounts_created;
};

struct MemoryDatabaseGenesisData {
	xdr::xvector<AccountID> id_list;
	std::vector<PublicKey> pk_list;
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
	//! Designate one asset as the base asset of speedex.
	//! Newly created accounts are required to get a small
	//! amount of this asset.
	//! If we decided to charge a gas fee, it would be in this asset. 
	constexpr static AssetID NATIVE_ASSET = 0;

	using DBEntryT = UserAccount;
	using DBMetadataT = trie::CombinedMetadata<trie::SizeMixin>;

	static std::vector<uint8_t> 
	serialize(const AccountCommitment& v)
	{
		return xdr::xdr_to_opaque(v);
	}

	using DBStateCommitmentValueT = trie::XdrTypeWrapper<AccountCommitment, &serialize>;

	//using DBStateCommitmentValueT = XdrTypeWrapper<AccountCommitment>;

	using trie_prefix_t = trie::UInt64Prefix;//AccountIDPrefix;

	using DBStateCommitmentTrie 
		= trie::MerkleTrie<trie_prefix_t, DBStateCommitmentValueT, DBMetadataT>;


	static inline void write_trie_key(trie_prefix_t& buf, AccountID account) {
		buf = trie_prefix_t{account};
	}

	using index_map_t = std::map<AccountID, UserAccount*>;

private:

	index_map_t user_id_to_idx_map;
	//index_map_t uncommitted_idx_map;
	std::set<AccountID> reserved_account_ids;

	AccountVector database;
	//std::vector<DBEntryT> database;
	std::vector<DBEntryT> uncommitted_db;

	mutable std::shared_mutex committed_mtx;
	std::shared_mutex uncommitted_mtx;

	std::mutex db_thunks_mtx;

	DBStateCommitmentTrie commitment_trie;

	AccountLMDB account_lmdb_instance;
	BackgroundThunkClearer<DBPersistenceThunk> background_thunk_clearer;

	std::vector<DBPersistenceThunk> persistence_thunks;
	std::vector<AccountCreationThunk> account_creation_thunks;

	std::optional<TransferLogs> transfer_logs;
	std::optional<trie::HashLog<trie_prefix_t>> hash_log;

 	constexpr static char UNKNOWN_REASON[] = "unknown\0";

	//delete copy constructors, implicitly blocks move ctors
	MemoryDatabase(const MemoryDatabase&) = delete;
	MemoryDatabase& operator=(const MemoryDatabase&) = delete;

	//UserAccount& find_account(account_db_idx user_index);
	//const UserAccount& find_account(account_db_idx user_index) const;

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
	//friend class UnlimitedMoneyBufferedMemoryDatabaseView;

	void _produce_state_commitment(Hash& hash);

	void rollback_new_accounts_(uint64_t current_block_number);

	void set_trie_commitment_to_user_account_commits(const AccountModificationLog& log);


	friend class ValidityCheckLambda;
	//! Check whether one account (by db idx) is in a valid state.
	//bool _check_valid(account_db_idx account_idx) {
	//	if (account_idx >= database.size()) {
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
	//		return true;
	//	}
	//	return database[account_idx].in_valid_state();
	//}

	TransactionProcessingStatus reserve_sequence_number(
		UserAccount* user_index, uint64_t sequence_number);

	void release_sequence_number(
		UserAccount* user_index, uint64_t sequence_number);

	void commit_sequence_number(
		UserAccount* user_index, uint64_t sequence_number);
public:

	uint64_t get_persisted_round_number_by_account(AccountID account) const {
		return account_lmdb_instance.get_persisted_round_number_by_account(account);
	}
	
	std::pair<uint64_t, uint64_t> get_min_max_persisted_round_numbers() const
	{
		return account_lmdb_instance.get_min_max_persisted_round_numbers();
	}

	MemoryDatabase();

	uint64_t size() const {
		return database.size();
	}

	//Be careful with creating new accounts.  If we have to rearrange database/resize vector, we'll break any pointers we export, probably.
	//We use integer indexes so that we can enforce the invariant that these indices never change.  We could reuse indices if we delete accounts.

	//account_db_idx add_account_to_db(AccountID account, const PublicKey& pk);

	// Obviously not threadsafe with value modification

	//void commit(uint64_t current_block_number) {
	//	commit_new_accounts(current_block_number);
	//	commit_values();
	//}

	//! Commit changes to all of the values (account states)
	//! logged as modified in dirty_accounts.
	void commit_values(const AccountModificationLog& dirty_accounts);

	void _commit_value(UserAccount* account_idx) {
		account_idx -> commit();
		//database[account_idx].commit();
	}

	//! Commit changes to all accounts
	void commit_values();
	//! Rollback changes to all accounts.
	//! It would be natural to use an account modification log to only
	//! have to rollback modified accounts.  However, as implemented, 
	//! validation shortcircuits without necessarily logging exactly which
	//! accounts are modified and does not bother actually building the
	//! modification log trie.  TODO measure whether actually building the trie
	//! (and logging all modified accounts, even when txs fail) is a faster
	//! way of rolling back accounts.
	void rollback_values();

	//! Commit a set of newly created accounts.
	//! Creates a thunk logging which accounts were created in this block.
	void commit_new_accounts(uint64_t current_block_number);
	//! Rollback the creation of new accounts that occurred after the current
	//! block number (not including current_block_number).
	void rollback_new_accounts(uint64_t current_block_number);

	//! Generates a state commitment using committed account balance values.
	void produce_state_commitment(Hash& hash, const AccountModificationLog& log, uint64_t block_number);
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

	//! Generates a state commitment using account balance values
	//! that reflect uncommitted changes.
	void tentative_produce_state_commitment(
		Hash& hash, const AccountModificationLog& log, uint64_t current_block_number);

	//! Undo tentative_produce_state_commitment().  Note that in this case,
	//! we can use the modification log to rollback, given that when we get
	//! here, the log has actually been built properly (unlike when 
	//! we shortcircuit due to a bad transaction).
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

	UserAccount* lookup_user(AccountID account) const;

	void transfer_available(
		UserAccount* user_index, AssetID asset_type, int64_t change, const char* reason = UNKNOWN_REASON);

	void escrow(
		UserAccount* user_index, AssetID asset_type, int64_t change, const char* reason = UNKNOWN_REASON);

	bool conditional_transfer_available(
		UserAccount* user_index, AssetID asset_type, int64_t change, const char* reason = UNKNOWN_REASON);
	bool conditional_escrow(
		UserAccount* user_index, AssetID asset_type, int64_t change, const char* reason = UNKNOWN_REASON);


	int64_t lookup_available_balance(
		UserAccount* user_index, AssetID asset_type);

	//! should not be used concurrently with commit on a UserAccount
	uint64_t get_last_committed_seq_number(UserAccount* idx) const;

	//! Get the public key associated with an account.
	//! Returns nullopt if no such account exists.
	std::optional<PublicKey> get_pk(AccountID account) const;

	//not threadsafe with commit/rollback
	std::optional<PublicKey> get_pk_nolock(AccountID account) const;

	void log(FILE* out);
	void values_log();

	/*! Checks whether the database is in a valid state

	Specifically checks every modified account to validate that
	all account balances are positive.
	Obviously not threadsafe with logging asset changes
	not threadsafe with creating accounts or committing or rolling back
	*/
	bool check_valid_state(const AccountModificationLog& dirty_accounts);

	AccountCommitment produce_commitment(UserAccount* idx) const {
		return idx -> produce_commitment();
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

	void 
	install_initial_accounts_and_commit(
		MemoryDatabaseGenesisData const& genesis_data, 
		std::function<void(UserAccount&)> account_init_lambda);
};

class NullDB {

	UserAccount* mockAccount;
public:

	NullDB()
		: mockAccount(new UserAccount())
		{}
	~NullDB()
	{
		delete mockAccount;
	}

	UserAccount* lookup_user([[maybe_unused]] AccountID account) const {
		return mockAccount;
	}

	void transfer_available(
		UserAccount*, 
		AssetID, 
		int64_t,
		const char*) {}

	void escrow(
		[[maybe_unused]] UserAccount*, 
		[[maybe_unused]] AssetID asset_type, 
		[[maybe_unused]] int64_t change,
		const char*) {}

	bool conditional_transfer_available(
		[[maybe_unused]] UserAccount*, 
		[[maybe_unused]] AssetID asset_type, 
		int64_t,
		const char*) {
		return true;
	}

	bool conditional_escrow(
		[[maybe_unused]] UserAccount*, 
		[[maybe_unused]] AssetID asset_type, 
		[[maybe_unused]] int64_t change,
		const char*) {
		return true;
	}
};

} /* speedex */
