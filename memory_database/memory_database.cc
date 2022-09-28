#include "memory_database/memory_database.h"

#include "modlog/account_modification_log.h"		

#include "utils/debug_macros.h"

#include <tbb/parallel_for.h>
#include <tbb/parallel_reduce.h>


#include <atomic>

#include <inttypes.h>

namespace speedex {

void MemoryDatabase::transfer_available(
	UserAccount* user_index, AssetID asset_type, int64_t change) {
	user_index -> transfer_available(asset_type, change);
//	find_account(user_index).transfer_available(asset_type, change);
}

void MemoryDatabase::escrow(
	UserAccount* user_index, AssetID asset_type, int64_t change) {
	user_index -> escrow(asset_type, change);
	//find_account(user_index).escrow(asset_type, change);
}

bool MemoryDatabase::conditional_transfer_available(
	UserAccount* user_index, AssetID asset_type, int64_t change) {
	//return find_account(user_index)
	return user_index ->
		conditional_transfer_available(asset_type, change);
}

bool MemoryDatabase::conditional_escrow(
	UserAccount* user_index, AssetID asset_type, int64_t change) {
	//return find_account(user_index).conditional_escrow(asset_type, change);
	return user_index -> conditional_escrow(asset_type, change);
}

TransactionProcessingStatus MemoryDatabase::reserve_sequence_number(
	UserAccount* user_index, uint64_t sequence_number) {
	return user_index -> reserve_sequence_number(sequence_number);
	//return find_account(user_index).reserve_sequence_number(sequence_number);
}

void MemoryDatabase::release_sequence_number(
	UserAccount* user_index, uint64_t sequence_number) {
	user_index -> release_sequence_number(sequence_number);
	//find_account(user_index).release_sequence_number(sequence_number);
}

void MemoryDatabase::commit_sequence_number(
	UserAccount* user_index, uint64_t sequence_number) {
	user_index -> commit_sequence_number(sequence_number);
//	find_account(user_index).commit_sequence_number(sequence_number);
}

uint64_t 
MemoryDatabase::get_last_committed_seq_number(UserAccount* idx) const
{
	return idx -> get_last_committed_seq_number();
	//return find_account(idx).get_last_committed_seq_number();
}


int64_t MemoryDatabase::lookup_available_balance(
	UserAccount* user_index, AssetID asset_type) {
	return user_index -> lookup_available_balance(asset_type);
	//return find_account(user_index).lookup_available_balance(asset_type);
}

void MemoryDatabase::clear_internal_data_structures() {
	uncommitted_db.clear();
	//uncommitted_idx_map.clear();
	reserved_account_ids.clear();
}

struct CommitValueLambda {
	MemoryDatabase& db;

	template<typename Applyable>
	void operator() (const Applyable& work_root) {

		auto lambda = [this] (const AccountID owner) {

			UserAccount* ptr = db.lookup_user(owner);
			if (ptr) {
				db._commit_value(ptr);
			}
			/*account_db_idx idx;
			if (db.lookup_user_id(owner, &idx)) {
				db._commit_value(idx);
			} */
			else {
				//Commit account creation should be done before
				// calling commit values.
				throw std::runtime_error(
					"couldn't lookup new acct" + std::to_string(owner));
			}
		};

		work_root . apply_to_keys(lambda);
	}
};

void MemoryDatabase::commit_values(const AccountModificationLog& dirty_accounts) {	
	std::lock_guard lock(committed_mtx);
	CommitValueLambda lambda{*this};
	dirty_accounts.parallel_iterate_over_log(lambda);
}

void MemoryDatabase::commit_values() {
	std::lock_guard lock(committed_mtx);

	size_t db_size = database.size();

	tbb::parallel_for(
		tbb::blocked_range<size_t>(0, db_size, 10000),
		[this] (auto r) {
			for (auto  i = r.begin(); i < r.end(); i++) {
				database.get(i) -> commit();
				//database[i].commit();
			}
		});
}
void MemoryDatabase::rollback_values() {
	std::lock_guard lock(committed_mtx);
	int db_size = database.size();
	
	tbb::parallel_for(
		tbb::blocked_range<std::size_t>(0, db_size, 10000),
		[this] (auto r) {
			for (auto  i = r.begin(); i < r.end(); i++) {
				database.get(i) -> rollback();
				//database[i].rollback();
			}
		});

}

void MemoryDatabase::commit_new_accounts(uint64_t current_block_number)
{
	std::lock_guard lock1(db_thunks_mtx);
	std::lock_guard lock2(committed_mtx);
	std::lock_guard lock3(uncommitted_mtx);

	if ((account_creation_thunks.size() == 0 && account_lmdb_instance.get_persisted_round_number() + 1 != current_block_number)
		|| (account_creation_thunks.size() > 0 && account_creation_thunks.back().current_block_number + 1 != current_block_number)) {

		if (!(current_block_number == 0 && account_lmdb_instance.get_persisted_round_number() ==0)) {
			BLOCK_INFO("mismatch: current_block_number = %lu account_lmdb_instance.get_persisted_round_number() = %lu",
					current_block_number, account_lmdb_instance.get_persisted_round_number());
			if (account_creation_thunks.size()) {
				BLOCK_INFO("account_creation_thunks.back().current_block_number:%lu",
						account_creation_thunks.back().current_block_number);
			}
			BLOCK_INFO("uncommitted db size: %lu", uncommitted_db.size());
			std::fflush(stdout);
			throw std::runtime_error("account creation thunks block number error");
		}
	}


	auto uncommitted_db_size = uncommitted_db.size();
	//database.reserve(database.size() + uncommitted_db_size);

	for (uint64_t i = 0; i < uncommitted_db_size; i++) {
		uncommitted_db[i].commit();
		UserAccount* committed_acct = database.emplace_back(std::move(uncommitted_db[i]));

		AccountID owner = committed_acct -> get_owner(); // database.back().get_owner();

		DBStateCommitmentTrie::prefix_t key_buf;
		MemoryDatabase::write_trie_key(key_buf, owner);
		//database.back().commit();
		commitment_trie.insert(key_buf, DBStateCommitmentValueT(committed_acct -> produce_commitment()));
		user_id_to_idx_map[owner] = committed_acct;
	}

	//user_id_to_idx_map.insert(uncommitted_idx_map.begin(), uncommitted_idx_map.end());


	account_creation_thunks.push_back(AccountCreationThunk{current_block_number, uncommitted_db_size});
	clear_internal_data_structures();
}

void MemoryDatabase::rollback_new_accounts(uint64_t current_block_number) {
	std::lock_guard lock1(db_thunks_mtx);
	std::lock_guard lock2(committed_mtx);
	std::lock_guard lock3(uncommitted_mtx);
	rollback_new_accounts_(current_block_number);
}


void MemoryDatabase::rollback_new_accounts_(uint64_t current_block_number) {
	for (size_t i = 0; i < account_creation_thunks.size();) {
		auto& thunk = account_creation_thunks[i];
		if (thunk.current_block_number > current_block_number) {
			size_t db_size = database.size();
			for (auto idx = db_size - thunk.num_accounts_created; idx < db_size; idx++) {

				auto owner = database.get(idx)->get_owner();
				user_id_to_idx_map.erase(owner);
				
				DBStateCommitmentTrie::prefix_t key_buf;
				MemoryDatabase::write_trie_key(key_buf, owner);
				commitment_trie.perform_deletion(key_buf);
			}

			database.erase(thunk.num_accounts_created);

		//	database.erase(database.begin() + (db_size - thunk.num_accounts_created), database.end());

			account_creation_thunks.erase(account_creation_thunks.begin() + i);
		} else {
			i++;
		}
	}
	clear_internal_data_structures();
}

struct ValidityCheckLambda {
	MemoryDatabase& db;
	std::atomic_flag& error_found;
	template<typename Applyable>
	void operator() (const Applyable& work_root) {

		auto lambda = [this] (const AccountID owner) {
			UserAccount* idx = db.lookup_user(owner);
			if (idx == nullptr) {
				// This occurs when owner is a newly created account this block.
				// Newly created account validity is enforced by tx semantics.
				return;
				//throw std::runtime_error("invalid db lookup");
			}
			if (! idx -> in_valid_state()) {
				error_found.test_and_set();
			}
			//account_db_idx idx;
			//db.lookup_user_id(owner, &idx);
			//if (!db._check_valid(idx)) {
			//	error_found.test_and_set();
			//}
		};

		work_root . apply_to_keys(lambda);
	}
};

bool MemoryDatabase::check_valid_state(const AccountModificationLog& dirty_accounts) {
	std::shared_lock lock(committed_mtx);
	std::shared_lock lock2(uncommitted_mtx);

	std::atomic_flag error_found = ATOMIC_FLAG_INIT;

	ValidityCheckLambda lambda{*this, error_found};

	dirty_accounts.parallel_iterate_over_log(lambda);

	if (error_found.test_and_set()) {
		return false;
	}

	size_t uncommitted_db_size = uncommitted_db.size();
	for (size_t i = 0; i < uncommitted_db_size; i++) {
		if (!uncommitted_db[i].in_valid_state()) {
			return false;
		}
	}
	return true;
}

bool MemoryDatabase::account_exists(AccountID account) {
	return user_id_to_idx_map.find(account) != user_id_to_idx_map.end();
}

UserAccount*
MemoryDatabase::lookup_user(AccountID account) const {
	auto idx_itr = user_id_to_idx_map.find(account);

	if (idx_itr != user_id_to_idx_map.end()) {
		return idx_itr -> second;
	}
	return nullptr;
}
/*
//returns index of user id.
bool MemoryDatabase::lookup_user_id(AccountID account, uint64_t* index_out) const {
	INFO("MemoryDatabase::lookup_user_id on account %ld", account);

	auto idx_itr = user_id_to_idx_map.find(account);

	if (idx_itr != user_id_to_idx_map.end()) {
		*index_out = idx_itr -> second;
		return true;
	}

	return false;
} */

TransactionProcessingStatus MemoryDatabase::reserve_account_creation(const AccountID account) {
	if (user_id_to_idx_map.find(account) != user_id_to_idx_map.end()) {
		return TransactionProcessingStatus::NEW_ACCOUNT_ALREADY_EXISTS;
	}
	std::lock_guard<std::shared_mutex> lock(uncommitted_mtx);
	if (reserved_account_ids.find(account) != reserved_account_ids.end()) {
		return TransactionProcessingStatus::NEW_ACCOUNT_TEMP_RESERVED;
	}
	reserved_account_ids.insert(account);
	return TransactionProcessingStatus::SUCCESS;
}

void MemoryDatabase::release_account_creation(const AccountID account) {
	std::lock_guard lock(uncommitted_mtx);
	reserved_account_ids.erase(account);
}

void MemoryDatabase::commit_account_creation(const AccountID account_id, DBEntryT&& account_data) {
	std::lock_guard lock(uncommitted_mtx);
	//account_db_idx new_idx = uncommitted_db.size() + database.size();
	//uncommitted_idx_map.emplace(account_id, new_idx);
	uncommitted_db.push_back(std::move(account_data));
}

std::optional<PublicKey> MemoryDatabase::get_pk(AccountID account) const {
	std::shared_lock lock(committed_mtx);
	return get_pk_nolock(account);
}

std::optional<PublicKey> MemoryDatabase::get_pk_nolock(AccountID account) const {
	auto iter = user_id_to_idx_map.find(account);
	if (iter == user_id_to_idx_map.end()) {
		return std::nullopt;
	}
	return iter->second -> get_pk();
	//return database[iter->second].get_pk();
}

//rollback_for_validation should be called in advance of this
void 
MemoryDatabase::rollback_produce_state_commitment(const AccountModificationLog& log) {
	std::lock_guard lock(committed_mtx);
	set_trie_commitment_to_user_account_commits(log);
}

void 
MemoryDatabase::finalize_produce_state_commitment() {
}

struct TentativeValueModifyLambda {
	AccountVector& database;
	//std::vector<MemoryDatabase::DBEntryT>& database;
	const MemoryDatabase::index_map_t& user_id_to_idx_map;

	void operator() (AccountID owner, MemoryDatabase::DBStateCommitmentValueT& value) {
		UserAccount* idx = user_id_to_idx_map.at(owner);
		value = idx->tentative_commitment();
	}
};

struct ProduceValueModifyLambda {
	//relies on the fact that MemoryDatabase and AccountLog use the same key space
	AccountVector& database; //std::vector<MemoryDatabase::DBEntryT>& database;
	const MemoryDatabase::index_map_t& user_id_to_idx_map;

	void operator() (AccountID owner, MemoryDatabase::DBStateCommitmentValueT& value) {

		UserAccount* idx = user_id_to_idx_map.at(owner);
		value = idx -> produce_commitment();
	}
};

template<typename Lambda>
struct ParallelApplyLambda {
	MemoryDatabase::DBStateCommitmentTrie& commitment_trie;
	Lambda& modify_lambda;

	template<typename Applyable>
	void operator() (const Applyable& work_root) {
		//Must guarantee no concurrent modification of commitment_trie (other than values)

		auto* commitment_trie_subnode = commitment_trie.get_subnode_ref_nolocks(work_root.get_prefix(), work_root . get_prefix_len());

		if (commitment_trie_subnode == nullptr) {
			throw std::runtime_error("get_subnode_ref_nolocks should not return nullptr ever");
		}

		auto apply_lambda = [this, commitment_trie_subnode] (const AccountID owner) {

			auto modify_lambda_wrapper = [this, owner] (MemoryDatabase::DBStateCommitmentValueT& commitment_value_out) {
				modify_lambda(owner, commitment_value_out);
			};

			MemoryDatabase::trie_prefix_t prefix{owner};

			commitment_trie_subnode -> modify_value_nolocks(prefix, modify_lambda_wrapper);
		};

		work_root . apply_to_keys(apply_lambda);
		commitment_trie.invalidate_hash_to_node_nolocks(commitment_trie_subnode);
	}
};

void MemoryDatabase::tentative_produce_state_commitment(Hash& hash, const AccountModificationLog& log) {
	std::lock_guard lock(committed_mtx);

	TentativeValueModifyLambda func{database, user_id_to_idx_map};

	ParallelApplyLambda<TentativeValueModifyLambda> apply_lambda{commitment_trie, func};

	std::printf("starting tentative_produce_state_commitment, size = %" PRIu32 "\n", commitment_trie.size());

	log.parallel_iterate_over_log(apply_lambda);

	commitment_trie.hash(hash);
}

void MemoryDatabase::set_trie_commitment_to_user_account_commits(const AccountModificationLog& log) {

	ProduceValueModifyLambda func{database, user_id_to_idx_map};

	ParallelApplyLambda<ProduceValueModifyLambda> apply_lambda{commitment_trie, func};

	std::printf("starting produce_state_commitment, size = %" PRIu32 "\n", commitment_trie.size());

	log.parallel_iterate_over_log(apply_lambda);
}


void MemoryDatabase::produce_state_commitment(Hash& hash, const AccountModificationLog& log) {

	std::lock_guard lock(committed_mtx);

	set_trie_commitment_to_user_account_commits(log);

	commitment_trie.hash(hash);
}

void
MemoryDatabase::_produce_state_commitment(Hash& hash) {
	std::printf("_produce_state_commitment\n");

	std::atomic_int32_t state_modified_count = 0;

	const auto block_size = database.size() / 200;

	tbb::parallel_for(
		tbb::blocked_range<std::size_t>(0, database.size(), block_size),
		[this, &state_modified_count](auto r) {
			int tl_state_modified_count = 0;
			trie_prefix_t key_buf;
			DBStateCommitmentTrie local_trie;
			for (auto i = r.begin(); i < r.end(); i++) {
				tl_state_modified_count ++;
				UserAccount* cur_account = database.get(i);
				MemoryDatabase::write_trie_key(key_buf, cur_account->get_owner());
				local_trie.insert(key_buf, DBStateCommitmentValueT(cur_account -> produce_commitment()));
			}
			commitment_trie.merge_in(std::move(local_trie));
			state_modified_count.fetch_add(tl_state_modified_count, std::memory_order_relaxed);
		});

	BLOCK_INFO("state modified count = %" PRId32, state_modified_count.load());

	commitment_trie.hash(hash);

	BLOCK_INFO_F(commitment_trie._log("db commit"));
	MEMDB_INFO_F(log());
}

void MemoryDatabase::add_persistence_thunk(uint64_t current_block_number, AccountModificationLog& log) {
	std::lock_guard lock(db_thunks_mtx);
	BLOCK_INFO("persistence thunk sz = %lu", log.size());

	persistence_thunks.emplace_back(*this, current_block_number);
	log.template parallel_accumulate_keys<DBPersistenceThunk>(persistence_thunks.back());
}

void MemoryDatabase::clear_persistence_thunks_and_reload(uint64_t expected_persisted_round_number) {
	std::lock_guard lock1(db_thunks_mtx);
	std::lock_guard lock2(committed_mtx);
	std::lock_guard lock3(uncommitted_mtx);
	
	if (expected_persisted_round_number != account_lmdb_instance.get_persisted_round_number()) {
		throw std::runtime_error("mismatch between expected round in db and actual persisted round in db");
	}

	rollback_new_accounts_(expected_persisted_round_number);

	for (size_t i = persistence_thunks.size(); i != 0; i--) {
		auto& thunk = persistence_thunks.at(i-1);

		if (thunk.current_block_number > expected_persisted_round_number) {

			tbb::parallel_for(tbb::blocked_range<size_t>(0, thunk.kvs->size(), 10000),
				[this, &thunk] (auto r) {
					auto rtx = account_lmdb_instance.rbegin();
					for (auto idx = r.begin(); idx < r.end(); idx++) {
						dbval key = dbval{&thunk.kvs->at(idx).key, sizeof(AccountID)};

						auto res = rtx.get(account_lmdb_instance.get_data_dbi(), key);
						
						if (res) {
							AccountCommitment commitment;
							dbval_to_xdr(*res, commitment);

							auto iter = user_id_to_idx_map.find(thunk.kvs->at(idx).key);
						
							if (iter == user_id_to_idx_map.end()) {
								throw std::runtime_error("invalid lookup to user_id_to_idx_map!");
							}
							*(iter -> second) = UserAccount(commitment);
							//database[iter->second] = UserAccount(commitment);
						}
					}
				});
		}
	}
	persistence_thunks.clear();
}

void MemoryDatabase::commit_persistence_thunks(uint64_t max_round_number) {

	BLOCK_INFO("start memorydatabase::commit_persistence_thunks");
	// Gather all the thunks that can be persisted at once, 
	// to minimize time spent locking the persistence thunks
	// mutex (which block creation acquires occasionally).
	std::vector<DBPersistenceThunk> thunks_to_commit;
	{
		std::lock_guard lock(db_thunks_mtx);
		for (size_t i = 0; i < persistence_thunks.size();) {
			auto& thunk = persistence_thunks.at(i);
			if (thunk.current_block_number <= max_round_number) {
				thunks_to_commit.emplace_back(std::move(thunk));
				persistence_thunks.erase(persistence_thunks.begin() + i);
			} else {
				i++;
			}
		}
	}

	if (thunks_to_commit.size() == 0) {
		BLOCK_INFO("DB: No thunks to commit.  Modifying db metadata (round number) only");

		if (account_lmdb_instance) {
			auto wtxn = account_lmdb_instance.wbegin();
			account_lmdb_instance.commit_wtxn(wtxn, max_round_number);
		}
		return;
	}

	// counter used to enforce sequentiality of commitment
	auto current_block_number = get_persisted_round_number();

	BLOCK_INFO("gathered thunks to commit");

	dbenv::wtxn write_txn{nullptr};
	if (account_lmdb_instance)
	{
		write_txn = account_lmdb_instance.wbegin();
	}

	BLOCK_INFO("opened account db wtxn");

	for (uint32_t i = 0; i < static_cast<uint32_t>(thunks_to_commit.size()); i++) {

		auto& thunk = thunks_to_commit.at(i);

		if (thunk.current_block_number > max_round_number) {
			continue;
		}

		// Used to require strict sequentiality, now gaps allowed in case of validation failures
		// due to byzantine proposers.
		if (thunk.current_block_number < current_block_number + 1) {
			std::printf("i = %" PRIu32 " thunks[i].current_block_number= %" PRIu64 " current_block_number = %" PRIu64 "\n", 
				i, 
				thunk.current_block_number, 
				current_block_number);
			throw std::runtime_error("can't persist blocks in wrong order!!!");
		}

		size_t thunk_sz = thunk.kvs ->size();
		for (size_t i = 0; i < thunk_sz; i++) {

			ThunkKVPair const& kv = (*thunk.kvs)[i];
		
			dbval key = dbval{&kv.key, sizeof(AccountID)};//UserAccount::produce_lmdb_key(kv.key);

			if (!(kv.msg.size())) {
				std::printf("missing value for kv %" PRIu64 "\n", kv.key);
				std::printf("thunk.kvs.size() = %" PRIu32 "\n", static_cast<uint32_t>(thunk.kvs->size()));
				throw std::runtime_error("failed to accumulate value in persistence thunk");
			}

			dbval val = dbval{kv.msg.data(), kv.msg.size()};
			
			if (account_lmdb_instance) {
				write_txn.put(account_lmdb_instance.get_data_dbi(), &key, &val);
			}
		}
		current_block_number = thunk.current_block_number;
	}

	BLOCK_INFO("built wtxn");

	account_lmdb_instance.commit_wtxn(write_txn, current_block_number, false);

	BLOCK_INFO("committed wtxn");

	{
		std::lock_guard lock(db_thunks_mtx);
		for (size_t i = 0; i < account_creation_thunks.size();) {
			auto& thunk = account_creation_thunks.at(i);
			if (thunk.current_block_number <= max_round_number) {
				account_creation_thunks.erase(account_creation_thunks.begin() + i);
			} else {
				i++;
			}
		}
	}

	BLOCK_INFO("cleared account creation thunks");

	background_thunk_clearer.clear_batch(std::move(thunks_to_commit));

	BLOCK_INFO("cleared background data");

	// this check no longer valid if gaps in thunks are allowed
	//if (current_block_number != max_round_number) {
	//	std::printf("Missing a round commitment!\n");
	//	throw std::runtime_error("missing commitment");
	//}
	if (account_lmdb_instance) {
		auto stats = account_lmdb_instance.stat();
		BLOCK_INFO("db size: %" PRIu64 , stats.ms_entries);
	}
}

void MemoryDatabase::persist_lmdb(uint64_t current_block_number) {
	std::shared_lock lock(committed_mtx);

	auto write_txn = account_lmdb_instance.wbegin();

	std::printf("writing entire database\n");


	int32_t modified_count = 0;
	for (account_db_idx i = 0; i < database.size(); i++) {
		if (i % 100000 == 0) {
			std::printf("%" PRId64 "\n", i);
		}

		modified_count++;

		AccountCommitment commitment = database.get(i) -> produce_commitment();

		auto commitment_buf = xdr::xdr_to_opaque(commitment);
		dbval val = dbval{commitment_buf.data(), commitment_buf.size()};
		AccountID owner = database.get(i)->get_owner();
		dbval key = dbval(&owner, sizeof(AccountID));

		try {
			write_txn.put(account_lmdb_instance.get_data_dbi(), &key, &val);
		} catch(...) {
			std::printf("failed to insert to account lmdb\n");
			throw;
		}

	}

	BLOCK_INFO("modified count = %" PRId32, modified_count);

	account_lmdb_instance.commit_wtxn(write_txn, current_block_number);

	auto stats = account_lmdb_instance.stat();
	BLOCK_INFO("db size: %" PRIu32, static_cast<uint32_t>(stats.ms_entries));
	MEMDB_INFO_F(log());
}

void MemoryDatabase::load_lmdb_contents_to_memory() {
	std::lock_guard lock(committed_mtx);

	auto stats = account_lmdb_instance.stat();

	std::printf("db size: %" PRIu32 "\n", static_cast<uint32_t>(stats.ms_entries));

	auto rtx = account_lmdb_instance.rbegin();

	auto cursor = rtx.cursor_open(account_lmdb_instance.get_data_dbi());

	cursor.get(MDB_FIRST);
	while (cursor) {
		auto& kv = *cursor;

		auto account_owner = UserAccount::read_lmdb_key(kv.first);

		AccountCommitment commitment;

		auto bytes = kv.second.bytes();

		dbval_to_xdr(kv.second, commitment);

		auto owner = commitment.owner;
		if (account_owner != owner) {
			throw std::runtime_error("key read error");
		}
		UserAccount* acct = database.emplace_back(commitment);

		user_id_to_idx_map.emplace(owner, acct);
		++cursor;
	}

	rtx.commit();
	Hash hash;
	_produce_state_commitment(hash);
}

void MemoryDatabase::log() {
	commitment_trie._log("db: ");
}

void MemoryDatabase::values_log() {
	/*for (size_t i = 0; i < database.size(); i++) {
		std::printf("%lu account %lu  ", i, database[i].get_owner());
		database[i].stringify();
		std::printf("\n");
	}

	std::printf("uncommitted\n");
	for (size_t i = 0; i < uncommitted_db.size(); i++) {
		std::printf("%lu account %lu  ", i, uncommitted_db[i].get_owner());
		uncommitted_db[i].stringify();
		std::printf("\n");
	}*/
}

void KVAssignment::operator=(const AccountID account) {

	UserAccount* idx = db.lookup_user(account);
	//account_db_idx idx;
	//if (!db.lookup_user_id(account, &idx)) {
	if (idx == nullptr) {
		throw std::runtime_error("can't commit invalid account");
	}
	AccountCommitment commitment = db.produce_commitment(idx);
	kv.key = account;
	kv.msg = xdr::xdr_to_opaque(commitment);
} 

KVAssignment 
DBPersistenceThunk::operator[](size_t idx) {
	if (idx >= kvs->size()) {
		throw std::runtime_error(
			std::string("invalid kvs access: ")
			+ std::to_string(idx)
			+ std::string("(size: ")
			+ std::to_string(kvs->size())
			+ std::string(")"));
	}

	return KVAssignment{kvs->at(idx), *db};
}

void
MemoryDatabase::install_initial_accounts_and_commit(MemoryDatabaseGenesisData const& genesis_data, std::function<void(UserAccount&)> account_init_lambda)
{
	if (database.size()) {
		throw std::runtime_error("database reinitialization attempted");
	}

	database.resize(genesis_data.id_list.size());

	auto insert_lambda = [this, &account_init_lambda] (
		AccountID const& id, 
		PublicKey const& pk, 
		account_db_idx next_idx, 
		index_map_t& local_id_map, 
		DBStateCommitmentTrie& local_commitment_trie) -> void 
	{
		UserAccount* acct = database.get(next_idx);
		//std::printf("for next_idx %lu got ptr %p\n", next_idx, acct);
		local_id_map.emplace(id, acct);
		acct -> set_owner(id, pk, 0);
		//database[next_idx].set_owner(id, pk, 0);

		account_init_lambda(*acct);

		DBStateCommitmentTrie::prefix_t key_buf;
		MemoryDatabase::write_trie_key(key_buf, id);

		local_commitment_trie.insert(key_buf, DBStateCommitmentValueT(acct -> produce_commitment()));
	};

	tbb::parallel_for(
		tbb::blocked_range<size_t>(0, genesis_data.id_list.size(), 100'000),
		[this, &genesis_data, &insert_lambda] (auto r) {
			index_map_t local_id_map;
			DBStateCommitmentTrie local_commitment_trie;

			for (auto idx = r.begin(); idx < r.end(); idx++) {
				auto const& acct = genesis_data.id_list[idx];
				auto const& pk = genesis_data.pk_list[idx];
				insert_lambda(acct, pk, idx, local_id_map, local_commitment_trie);
			}

			std::lock_guard lock(committed_mtx);
			commitment_trie.merge_in(std::move(local_commitment_trie));
			user_id_to_idx_map.merge(std::move(local_id_map));
		});
}

} /* speedex */
