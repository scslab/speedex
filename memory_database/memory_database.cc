#include "memory_database/memory_database.h"

#include "utils/debug_macros.h"

#include <tbb/parallel_for.h>
#include <tbb/parallel_reduce.h>


#include <atomic>

namespace speedex {

UserAccount& MemoryDatabase::find_account(account_db_idx user_index) {
	uint64_t db_size = database.size();
	if (user_index >= db_size) {
		std::printf("invalid db access: %lu of %lu\n", user_index, db_size);
		throw std::runtime_error("invalid db idx access");
	}
	return database[user_index];
}

const UserAccount& MemoryDatabase::find_account(account_db_idx user_index) const {
	uint64_t db_size = database.size();
	if (user_index >= db_size) {
		std::printf("invalid db access: %lu of %lu\n", user_index, db_size);
		throw std::runtime_error("invalid db idx access");
	}
	return database[user_index];
}

void MemoryDatabase::transfer_available(
	account_db_idx user_index, AssetID asset_type, int64_t change) {
	find_account(user_index).transfer_available(asset_type, change);
}

void MemoryDatabase::escrow(
	account_db_idx user_index, AssetID asset_type, int64_t change) {
	find_account(user_index).escrow(asset_type, change);
}

bool MemoryDatabase::conditional_transfer_available(
	account_db_idx user_index, AssetID asset_type, int64_t change) {
	return find_account(user_index)
		.conditional_transfer_available(asset_type, change);
}

bool MemoryDatabase::conditional_escrow(
	account_db_idx user_index, AssetID asset_type, int64_t change) {
	return find_account(user_index).conditional_escrow(asset_type, change);
}

TransactionProcessingStatus MemoryDatabase::reserve_sequence_number(
	account_db_idx user_index, uint64_t sequence_number) {
	return find_account(user_index).reserve_sequence_number(sequence_number);
}

void MemoryDatabase::release_sequence_number(
	account_db_idx user_index, uint64_t sequence_number) {
	find_account(user_index).release_sequence_number(sequence_number);
}

void MemoryDatabase::commit_sequence_number(
	account_db_idx user_index, uint64_t sequence_number) {
	find_account(user_index).commit_sequence_number(sequence_number);
}

uint64_t 
MemoryDatabase::get_last_committed_seq_number(account_db_idx idx) const
{
	return find_account(idx).get_last_committed_seq_number();
}



//TODO this is not used in normal block processing, but seems generally useful
account_db_idx MemoryDatabase::add_account_to_db(AccountID user_id, const PublicKey pk) {
	auto idx_itr = user_id_to_idx_map.find(user_id);
	if (idx_itr != user_id_to_idx_map.end()) {
		return idx_itr -> second;
	}

	std::lock_guard lock(uncommitted_mtx);
	idx_itr = uncommitted_idx_map.find(user_id);
	if (idx_itr != uncommitted_idx_map.end()) {
		return idx_itr -> second;
	}
	auto idx = database.size() + uncommitted_db.size();
	uncommitted_idx_map[user_id] = idx;
	uncommitted_db.emplace_back(user_id, pk);
	return idx;
}

int64_t MemoryDatabase::lookup_available_balance(
	account_db_idx user_index, AssetID asset_type) {
	return find_account(user_index).lookup_available_balance(asset_type);
}

void MemoryDatabase::clear_internal_data_structures() {
	uncommitted_db.clear();
	uncommitted_idx_map.clear();
	reserved_account_ids.clear();
}

struct CommitValueLambda {
	MemoryDatabase& db;

	template<typename Applyable>
	void operator() (const Applyable& work_root) {

		auto lambda = [this] (const AccountID owner) {
			account_db_idx idx;
			if (db.lookup_user_id(owner, &idx)) {
				db._commit_value(idx);
			} else {
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

	int db_size = database.size();

	tbb::parallel_for(
		tbb::blocked_range<std::size_t>(0, db_size, 10000),
		[this] (auto r) {
			for (auto  i = r.begin(); i < r.end(); i++) {
				database[i].commit();
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
				database[i].rollback();
			}
		});

}

void MemoryDatabase::commit_new_accounts(uint64_t current_block_number) {
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
	database.reserve(database.size() + uncommitted_db_size);
	for (uint64_t i = 0; i < uncommitted_db_size; i++) {
		uncommitted_db[i].commit();
		database.emplace_back(std::move(uncommitted_db[i]));

		AccountID owner = database.back().get_owner();

		DBStateCommitmentTrie::prefix_t key_buf;
		MemoryDatabase::write_trie_key(key_buf, owner);
		//database.back().commit();
		commitment_trie.insert(key_buf, DBStateCommitmentValueT(database.back().produce_commitment()));
	}
	user_id_to_idx_map.insert(uncommitted_idx_map.begin(), uncommitted_idx_map.end());

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

				auto owner = database.at(idx).get_owner();
				user_id_to_idx_map.erase(owner);
				
				DBStateCommitmentTrie::prefix_t key_buf;
				MemoryDatabase::write_trie_key(key_buf, owner);
				commitment_trie.perform_deletion(key_buf);
			}

			database.erase(database.begin() + (db_size - thunk.num_accounts_created), database.end());

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
			account_db_idx idx;
			db.lookup_user_id(owner, &idx);
			if (!db._check_valid(idx)) {
				error_found.test_and_set();
			}
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

//returns index of user id.
bool MemoryDatabase::lookup_user_id(AccountID account, uint64_t* index_out) const {
	INFO("MemoryDatabase::lookup_user_id on account %ld", account);

	auto idx_itr = user_id_to_idx_map.find(account);

	if (idx_itr != user_id_to_idx_map.end()) {
		*index_out = idx_itr -> second;
		return true;
	}

	return false;
}

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
	account_db_idx new_idx = uncommitted_db.size() + database.size();
	uncommitted_idx_map.emplace(account_id, new_idx);
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
	return database[iter->second].get_pk();
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
	std::vector<MemoryDatabase::DBEntryT>& database;
	const MemoryDatabase::index_map_t& user_id_to_idx_map;

	void operator() (AccountID owner, MemoryDatabase::DBStateCommitmentValueT& value) {
		account_db_idx idx = user_id_to_idx_map.at(owner);
		value = database[idx].tentative_commitment();
	}
};

struct ProduceValueModifyLambda {
	//relies on the fact that MemoryDatabase and AccountLog use the same key space
	std::vector<MemoryDatabase::DBEntryT>& database;
	const MemoryDatabase::index_map_t& user_id_to_idx_map;

	void operator() (AccountID owner, MemoryDatabase::DBStateCommitmentValueT& value) {

		account_db_idx idx = user_id_to_idx_map.at(owner);
		value = database.at(idx).produce_commitment();
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

	std::printf("starting tentative_produce_state_commitment, size =%lu\n", commitment_trie.size());

	log.parallel_iterate_over_log(apply_lambda);

	commitment_trie.hash(hash);
}

void MemoryDatabase::set_trie_commitment_to_user_account_commits(const AccountModificationLog& log) {

	ProduceValueModifyLambda func{database, user_id_to_idx_map};

	ParallelApplyLambda<ProduceValueModifyLambda> apply_lambda{commitment_trie, func};

	std::printf("starting produce_state_commitment, size =%lu\n", commitment_trie.size());

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
				MemoryDatabase::write_trie_key(key_buf, database.at(i).get_owner());
				local_trie.insert(key_buf, DBStateCommitmentValueT(database.at(i).produce_commitment()));
			}
			commitment_trie.merge_in(std::move(local_trie));
			state_modified_count.fetch_add(tl_state_modified_count, std::memory_order_relaxed);
		});

	BLOCK_INFO("state modified count = %d", state_modified_count.load());

	commitment_trie.hash(hash);

	INFO_F(commitment_trie._log("db commit"));
	INFO_F(log());
}

void MemoryDatabase::add_persistence_thunk(uint64_t current_block_number, AccountModificationLog& log) {
	std::lock_guard lock(db_thunks_mtx);
	persistence_thunks.emplace_back(*this, current_block_number);
	log.template parallel_accumulate_keys<DBPersistenceThunk>(persistence_thunks.back());
}

void MemoryDatabase::clear_persistence_thunks_and_reload(uint64_t expected_persisted_round_number) {
	std::lock_guard lock1(db_thunks_mtx);
	std::lock_guard lock2(committed_mtx);
	std::lock_guard lock3(uncommitted_mtx);
	
	rollback_new_accounts_(expected_persisted_round_number);

	if (expected_persisted_round_number != account_lmdb_instance.get_persisted_round_number()) {
		throw std::runtime_error("invalid load!");
	}

	for (size_t i = persistence_thunks.size(); i != 0; i--) {
		auto& thunk = persistence_thunks.at(i-1);

		if (thunk.current_block_number > expected_persisted_round_number) {

			tbb::parallel_for(tbb::blocked_range<size_t>(0, thunk.kvs->size(), 10000),
				[this, &thunk] (auto r) {
					auto rtx = account_lmdb_instance.rbegin();
					for (auto idx = r.begin(); idx < r.end(); idx++) {
						dbval key = UserAccount::produce_lmdb_key(thunk.kvs->at(idx).key);

						auto res = rtx.get(account_lmdb_instance.get_data_dbi(), key);
						
						if (res) {
							AccountCommitment commitment;
							dbval_to_xdr(*res, commitment);

							auto iter = user_id_to_idx_map.find(thunk.kvs->at(idx).key);
						
							if (iter == user_id_to_idx_map.end()) {
								throw std::runtime_error("invalid lookup to user_id_to_idx_map!");
							}
							database[iter->second] = UserAccount(commitment);
						}
					}
				});
		}
	}
	persistence_thunks.clear();
}

void MemoryDatabase::commit_persistence_thunks(uint64_t max_round_number) {

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
		BLOCK_INFO("NO THUNKS TO COMMIT");
		return;
	}

	auto current_block_number = get_persisted_round_number();

	dbenv::wtxn write_txn{nullptr};
	if (account_lmdb_instance)
	{
		write_txn = account_lmdb_instance.wbegin();
	}

	for (size_t i = 0; i < thunks_to_commit.size(); i++) {

		auto& thunk = thunks_to_commit.at(i);

		if (thunk.current_block_number > max_round_number) {
			continue;
		}

		if (thunk.current_block_number != current_block_number + 1) {
			std::printf("i = %lu thunks[i].current_block_number= %lu current_block_number = %lu\n", i, thunk.current_block_number, current_block_number);
			throw std::runtime_error("can't persist blocks in wrong order!!!");
		}

		size_t thunk_sz = thunk.kvs ->size();
		for (size_t i = 0; i < thunk_sz; i++) {

			auto& kv = (*thunk.kvs)[i];
		
			dbval key = UserAccount::produce_lmdb_key(kv.key);

			if (!(kv.msg.size())) {
				std::printf("missing value for kv %lu\n", kv.key);
				std::printf("thunk.kvs.size() = %lu\n", thunk.kvs->size());
				throw std::runtime_error("failed to accumulate value in persistence thunk");
			}

			dbval val = dbval{kv.msg.data(), kv.msg.size()};
			
			if (account_lmdb_instance) {
				write_txn.put(account_lmdb_instance.get_data_dbi(), key, val);
			}
		}
		current_block_number = thunk.current_block_number;
	}

	account_lmdb_instance.commit_wtxn(write_txn, current_block_number, false);

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

	background_thunk_clearer.clear_batch(std::move(thunks_to_commit));

	if (current_block_number != max_round_number) {
		std::printf("Missing a round commitment!\n");
		throw std::runtime_error("missing commitment");
	}
	if (account_lmdb_instance) {
		auto stats = account_lmdb_instance.stat();
		BLOCK_INFO("db size: %lu", stats.ms_entries);
	}
}

void MemoryDatabase::persist_lmdb(uint64_t current_block_number) {
	std::shared_lock lock(committed_mtx);

	auto write_txn = account_lmdb_instance.wbegin();

	std::printf("writing entire database\n");


	int modified_count = 0;
	for (account_db_idx i = 0; i < database.size(); i++) {
		if (i % 100000 == 0) {
			std::printf("%lu\n", i);
		}

		modified_count++;

		AccountCommitment commitment = database[i].produce_commitment();

		auto commitment_buf = xdr::xdr_to_opaque(commitment);
		dbval val = dbval{commitment_buf.data(), commitment_buf.size()};
		dbval key = UserAccount::produce_lmdb_key(database[i].get_owner());


		write_txn.put(account_lmdb_instance.get_data_dbi(), key, val);

	}

	BLOCK_INFO("modified count = %d", modified_count);

	account_lmdb_instance.commit_wtxn(write_txn, current_block_number);

	auto stats = account_lmdb_instance.stat();
	BLOCK_INFO("db size: %lu", stats.ms_entries);
	INFO_F(log());
}

void MemoryDatabase::load_lmdb_contents_to_memory() {
	std::lock_guard lock(committed_mtx);

	auto stats = account_lmdb_instance.stat();

	std::printf("db size: %lu\n", stats.ms_entries);

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
		user_id_to_idx_map.emplace(owner, database.size());
		database.emplace_back(commitment);
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

	account_db_idx idx;
	if (!db.lookup_user_id(account, &idx)) {
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
MemoryDatabase::install_initial_accounts_and_commit(MemoryDatabaseGenesisData const& genesis_data, auto account_init_lambda)
{
	if (database.size()) {
		throw std::runtime_error("database reinitialization attempted");
	}

	database.resize(genesis_data.id_list.size());

	auto insert_lambda = [&database, &account_init_lambda] (
		AccountID const& id, 
		PublicKey const& pk, 
		account_db_idx next_idx, 
		index_map_t& local_id_map, 
		DBStateCommitmentTrie& local_commitment_trie) -> void 
	{
		local_id_map.insert(id, next_idx);
		database[next_idx].set_owner(id, pk, 0);

		account_init_lambda(database[next_idx]);

		DBStateCommitmentTrie::prefix_t key_buf;
		MemoryDatabase::write_trie_key(key_buf, owner);

		local_commitment_trie.insert(key_buf, DBStateCommitmentValueT(database[next_idx].produce_commitment()));
	};

	tbb::parallel_for(
		tbb::blocked_range<size_t>(0, genesis_data.id_list.size(), 100'000),
		[this, &genesis_data] (auto r) {
			index_map_t local_id_map;
			DBStateCommitmentTrie local_commitment_trie;

			for (auto idx = r.begin(); idx < r.end(); idx++) {
				auto const& acct = genesis_data.id_list[idx];
				auto const& pk = genesis_data.pk_list[idx];
				insert_lambda(acct, pk, idx, local_id_map, local_commitment_trie);
			}

			std::lock_guard lock(commitment_mtx);
			commitment_trie.merge_in(std::move(local_commitment_trie));
			user_id_to_idx_map.merge(std::move(local_id_map));
		});
}

} /* speedex */
