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

#include <sodium.h>

#include <cstdint>
#include <memory>
#include <mutex>

#include "lmdb/lmdb_wrapper.h"

#include "xdr/types.h"

#include <utils/async_worker.h>

#include <utils/non_movable.h>

namespace speedex
{

class DBPersistenceThunk;

namespace detail
{

uint32_t get_shard(AccountID const& account, const uint8_t* HASH_KEY);

class AccountLMDBShard : public lmdb::LMDBInstance, utils::NonMovableOrCopyable
{
	public:
	const uint32_t idx;
	
	
	private:
	const std::string DB_NAME;

	uint8_t HASH_KEY[crypto_shorthash_KEYBYTES];

	void load_hash_key();

public:

	AccountLMDBShard(uint32_t idx);

	void open_env();

	void create_db(const uint8_t* hash_key);

	void open_db();

	using LMDBInstance::sync;

	bool owns_account(const AccountID account) const;

	void export_hash_key(uint8_t* hash_key_out) const;
};

class AsyncAccountLMDBShardWorker : public utils::AsyncWorker, utils::NonMovableOrCopyable
{

	const std::vector<DBPersistenceThunk>* thunks_to_process;
	uint64_t max_round_number;
	bool ignore_too_low;

	AccountLMDBShard& shard;

	bool exists_work_to_do() override final {
		return (thunks_to_process != nullptr);
	}

	void run();

	void exec_thunks();
	void exec_one_thunk(const DBPersistenceThunk& thunk, lmdb::dbenv::wtxn& wtx, uint64_t& current_block_number);

public:
	AsyncAccountLMDBShardWorker(AccountLMDBShard& shard)
		: utils::AsyncWorker()
		, thunks_to_process(nullptr)
		, shard(shard)
	{
		start_async_thread([this] {run();});
	}

	void add_thunks(const std::vector<DBPersistenceThunk>& thunks, uint64_t max_round_number_, bool ignore_too_low_);

	//! Background thread is signaled to terminate when object leaves scope.
	~AsyncAccountLMDBShardWorker() {
		terminate_worker();
	}
};

class AsyncFsyncWorker : public utils::AsyncWorker, utils::NonMovableOrCopyable
{
	bool do_fsync;

	AccountLMDBShard& shard;

	bool exists_work_to_do() override final {
		return (do_fsync);
	}

	void run();

public:
	AsyncFsyncWorker(AccountLMDBShard& shard)
		: utils::AsyncWorker()
		, do_fsync(false)
		, shard(shard)
	{
		start_async_thread([this] {run();});
	}

	void call_fsync();

	~AsyncFsyncWorker() {
		terminate_worker();
	}
};

} /* detail */

class AccountLMDB
{
	std::vector<std::unique_ptr<detail::AccountLMDBShard>> shards;
	std::vector<std::unique_ptr<detail::AsyncAccountLMDBShardWorker>> workers;
	std::vector<std::unique_ptr<detail::AsyncFsyncWorker>> syncers;

	bool opened = false;

	void wait_for_all_workers();
	void wait_for_all_syncers();

	// many copies of this key, but that's not a huge problem
	uint8_t HASH_KEY[crypto_shorthash_KEYBYTES];

	mutable std::mutex mtx;
	
	uint64_t min_persisted_round_number, max_persisted_round_number;

	std::pair<uint64_t, uint64_t> get_min_max_persisted_round_numbers_direct() const;

public:

	AccountLMDB();

	void create_db();

	void open_db();

	void open_env();

	void persist_thunks(const std::vector<DBPersistenceThunk>& thunks, uint64_t max_round_number, bool ignore_too_low = false);

	void sync();

	operator bool() const {
		return opened;
	}

	// don't call concurrently with persistence, or else results might become out of date
	// by the time they're referenced.
	uint64_t get_persisted_round_number_by_account(const AccountID& account) const;

	std::pair<uint64_t, uint64_t> get_min_max_persisted_round_numbers() const;
	uint64_t assert_snapshot_and_get_persisted_round_number() const;

	struct rtxn {
		using txn_t = std::pair<lmdb::dbenv::txn, MDB_dbi>;
		std::vector<txn_t> rtxns;

	private:
		uint8_t HASH_KEY[crypto_shorthash_KEYBYTES];
	public:

		rtxn(AccountLMDB& main_lmdb)
			: rtxns()
		{
			for (auto& shard : main_lmdb.shards)
			{
				rtxns.emplace_back(std::make_pair(shard->rbegin(), shard -> get_data_dbi()));
			}
			std::memcpy(HASH_KEY, main_lmdb.HASH_KEY, crypto_shorthash_KEYBYTES);
		}

		std::optional<lmdb::dbval> get(AccountID const& account)
		{
			uint32_t idx = detail::get_shard(account, HASH_KEY);
			lmdb::dbval key{&account, sizeof(AccountID)};

			auto& [rtx, dbi] = rtxns.at(idx);

			return rtx.get(dbi, key);
		}
	};

	rtxn rbegin() {
		return rtxn(*this);
	}
};

}
