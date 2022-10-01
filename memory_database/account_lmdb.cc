#include "memory_database/account_lmdb.h"

#include "memory_database/thunk.h"

#include "speedex/speedex_static_configs.h"

#include <cinttypes>

#include "config.h"

#include <mtt/utils/time.h>

namespace speedex
{

namespace detail
{

void
AccountLMDBShard::load_hash_key()
{
	auto rtx = rbegin();

	auto hash_key = rtx.get(get_metadata_dbi(), dbval("hash key"));

	if (!hash_key)
	{
		throw std::runtime_error("failed to load hash key from lmdb");
	}

	auto bytes = hash_key->bytes();

	if (bytes.size() != crypto_shorthash_KEYBYTES)
	{
		throw std::runtime_error("saved key has wrong length");
	}

	std::memcpy(HASH_KEY, bytes.data(), bytes.size());
}

AccountLMDBShard::AccountLMDBShard(uint32_t idx) 
	: LMDBInstance()
	, idx(idx)
	, DB_NAME("account_db" + std::to_string(idx))
 {
 	if (idx >= NUM_ACCOUNT_DB_SHARDS)
 	{
 		throw std::runtime_error("invalid shard idx");
 	}
 }

void 
AccountLMDBShard::open_env() {
	LMDBInstance::open_env(
		std::string(ROOT_DB_DIRECTORY) + std::string(ACCOUNT_DB) + std::to_string(idx) + "/");
}

void 
AccountLMDBShard::create_db(const uint8_t* hash_key) {
	LMDBInstance::create_db(DB_NAME.c_str());

	std::memcpy(HASH_KEY, hash_key, crypto_shorthash_KEYBYTES);
}

void 
AccountLMDBShard::open_db() {
	LMDBInstance::open_db(DB_NAME.c_str());
	load_hash_key();
}

uint32_t get_shard(const AccountID& account, const uint8_t* HASH_KEY)
{
	static_assert(crypto_shorthash_BYTES == 8);

    uint64_t hash_out;

    if (crypto_shorthash(reinterpret_cast<uint8_t*>(&hash_out),
                         reinterpret_cast<const uint8_t*>(&account),
                         sizeof(AccountID),
                         HASH_KEY)
        != 0) {
        throw std::runtime_error("shorthash fail");
    }

    // https://lemire.me/blog/2016/06/27/a-fast-alternative-to-the-modulo-reduction
    return ((hash_out & 0xFFFF'FFFF) /* 32 bits */
            * static_cast<uint64_t>(NUM_ACCOUNT_DB_SHARDS))
           >> 32;
}

bool 
AccountLMDBShard::owns_account(const AccountID account) const
{
    uint32_t account_idx = get_shard(account, HASH_KEY);

    return account_idx == idx;
}

void 
AccountLMDBShard::export_hash_key(uint8_t* hash_key_out) const
{
	std::memcpy(hash_key_out, HASH_KEY, crypto_shorthash_KEYBYTES);
}


void 
AsyncAccountLMDBShardWorker::exec_one_thunk(
	const DBPersistenceThunk& thunk, 
	dbenv::wtxn& wtx, 
	uint64_t& current_block_number)
{
	// Used to require strict sequentiality, now gaps allowed in case of validation failures
	// due to byzantine proposers.
	if (thunk.current_block_number < current_block_number + 1) {
		throw std::runtime_error("can't persist blocks in wrong order!!!");
	}

	size_t thunk_sz = thunk.kvs ->size();
	for (size_t i = 0; i < thunk_sz; i++) {

		ThunkKVPair const& kv = (*thunk.kvs)[i];

		if (!shard.owns_account(kv.key))
		{
			continue;
		}
	
		dbval key = dbval{&kv.key, sizeof(AccountID)};//UserAccount::produce_lmdb_key(kv.key);

		if (!(kv.msg.size())) {
			std::printf("missing value for kv %" PRIu64 "\n", kv.key);
			std::printf("thunk.kvs.size() = %" PRIu32 "\n", static_cast<uint32_t>(thunk.kvs->size()));
			throw std::runtime_error("failed to accumulate value in persistence thunk");
		}

		dbval val = dbval{kv.msg.data(), kv.msg.size()};
		
		wtx.put(shard.get_data_dbi(), &key, &val);
	}
	current_block_number = thunk.current_block_number;
}

void
AsyncAccountLMDBShardWorker::exec_thunks()
{
	if (thunks_to_process == nullptr)
	{
		throw std::runtime_error("tried to exec_thunks when no thunks present");
	}

	auto ts = utils::init_time_measurement();

	uint64_t current_block_number = shard.get_persisted_round_number();

	const uint64_t starting_persisted_number = current_block_number;
	
	auto wtx = shard.wbegin();

	for (auto const& thunk : *thunks_to_process)
	{
		if (ignore_too_low && thunk.current_block_number <= starting_persisted_number)
		{
			//thunk at block i is included if persisted to round i
			continue;
		}
		exec_one_thunk(thunk, wtx, current_block_number);
	}

	if (current_block_number > max_round_number)
	{
		throw std::runtime_error("invalid persist max_round_number");
	}

	std::printf("thunk process time: %lf\n", utils::measure_time(ts));

	//TODO consider whether to sync here or later
	shard.commit_wtxn(wtx, max_round_number, ACCOUNT_DB_SYNC_IMMEDIATELY);

	std::printf("commit time: %lf\n", utils::measure_time(ts));

	thunks_to_process = nullptr;
}

void 
AsyncAccountLMDBShardWorker::add_thunks(const std::vector<DBPersistenceThunk>& thunks, uint64_t max_round_number_, bool ignore_too_low_)
{
	wait_for_async_task();
	std::lock_guard lock(mtx);
	thunks_to_process = &thunks;
	max_round_number = max_round_number_;
	ignore_too_low = ignore_too_low_;
	cv.notify_all();
}

void 
AsyncAccountLMDBShardWorker::run()
{
	while(true) {
		std::unique_lock lock(mtx);

		if ((!done_flag) && (!exists_work_to_do())) {
			cv.wait(
				lock, [this] () {return done_flag || exists_work_to_do();});
		}

		if (done_flag) return;
		exec_thunks();
		cv.notify_all();
	}
}

void 
AsyncFsyncWorker::run()
{
	while(true) {
		std::unique_lock lock(mtx);

		if ((!done_flag) && (!exists_work_to_do())) {
			cv.wait(
				lock, [this] () {return done_flag || exists_work_to_do();});
		}

		if (done_flag) return;
		shard.sync();
		cv.notify_all();
		do_fsync = false;
	}
}

void 
AsyncFsyncWorker::call_fsync()
{
	wait_for_async_task();
	std::lock_guard lock(mtx);
	do_fsync = true;
	cv.notify_all();
}

} /* detail */

AccountLMDB::AccountLMDB()
	: shards()
	, workers()
	{
		for (uint32_t i = 0; i < NUM_ACCOUNT_DB_SHARDS; i++)
		{
			shards.emplace_back(std::make_unique<detail::AccountLMDBShard>(i));
			workers.emplace_back(std::make_unique<detail::AsyncAccountLMDBShardWorker>(*shards.back()));
			syncers.emplace_back(std::make_unique<detail::AsyncFsyncWorker>(*shards.back()));
		}
	}


void
AccountLMDB::wait_for_all_workers()
{
	for (auto& worker : workers)
	{
		worker->wait_for_async_task();
	}
}

void
AccountLMDB::wait_for_all_syncers()
{
	for (auto& worker : syncers)
	{
		worker->wait_for_async_task();
	}
}

void 
AccountLMDB::create_db()
{
	crypto_shorthash_keygen(HASH_KEY);

	for (auto& shard : shards)
	{
		shard->create_db(HASH_KEY);
	}

	opened = true;
}

void
AccountLMDB::open_db()
{
	for (auto& shard : shards)
	{
		shard->open_db();
	}
	shards.at(0)->export_hash_key(HASH_KEY);
	opened = true;
}

void
AccountLMDB::open_env()
{
	for (auto& shard : shards)
	{
		shard -> open_env();
	}
}

void 
AccountLMDB::persist_thunks(const std::vector<DBPersistenceThunk>& thunks, uint64_t max_round_number, bool ignore_too_low)
{
	for (auto& worker : workers)
	{
		worker->add_thunks(thunks, max_round_number, ignore_too_low);
	}
	wait_for_all_workers();
}

void
AccountLMDB::sync()
{
	if constexpr (!ACCOUNT_DB_SYNC_IMMEDIATELY)
	{
		auto ts = utils::init_time_measurement();

		for (auto& sync : syncers)
		{
			sync->call_fsync();
		}

		wait_for_all_syncers();

		std::printf("out of band sync time: %lf\n", utils::measure_time(ts));
	}
	//else nothing to do, sync already was done
}

uint64_t 
AccountLMDB::get_persisted_round_number_by_account(const AccountID& account) const
{
	uint32_t shard = detail::get_shard(account, HASH_KEY);

	return shards.at(shard)->get_persisted_round_number();
}

std::pair<uint64_t, uint64_t> 
AccountLMDB::get_min_max_persisted_round_numbers() const
{
	uint64_t min = UINT64_MAX, max = 0;
	for (auto& shard : shards)
	{
		uint64_t round = shard -> get_persisted_round_number();

		min = std::min(min, round);
		max = std::max(max, round);
	}
	return {min, max};

}

uint64_t 
AccountLMDB::assert_snapshot_and_get_persisted_round_number() const
{
	auto [min, max] = get_min_max_persisted_round_numbers();

	if (min != max)
	{
		throw std::runtime_error("shorn read");
	}
	return min;
}


} /* speedex */
