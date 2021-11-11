#pragma once

/*! \file autorollback_validation_structures.h

Wrappers around Speedex data structures
that tracks validation-related changes
and rolls them back upon leaving scope,
unless validation succeeds and the block
commits.
*/

#include "speedex/speedex_management_structures.h"

#include "utils/time.h"

#include "xdr/block.h"

namespace speedex {

//! Rollback changes to database upon going out of scope,
//! unless whole block commits.
struct DatabaseAutoRollback {
	MemoryDatabase& db;
	//! Current block number being modified
	uint64_t current_block_number = 0;

	//! Undo a set of balance modifications
	bool do_rollback_for_validation = false;
	//! Undo changes to commitment trie
	bool do_rollback_produce_state_commitment = false;

	const AccountModificationLog* rollback_log;

	DatabaseAutoRollback(MemoryDatabase& db, uint64_t current_block_number) 
		: db(db)
		, current_block_number(current_block_number) {}

	//! Can't have this called after modlog is cleared
	//! which occurs when ~AccountModificationLogAutoRollback() is called.
	~DatabaseAutoRollback() {
		if (do_rollback_for_validation) {
			db.rollback_new_accounts(current_block_number);
			db.rollback_values();
		}

		if (do_rollback_produce_state_commitment) {
			db.rollback_produce_state_commitment(*rollback_log);
		}
	}

	//! Commit new account creation,
	//! records that this should be undone later.
	void tentative_commit_for_validation() {
		do_rollback_for_validation = true;
		db.commit_new_accounts(current_block_number);
	}

	//! modifies commitment trie, records that
	//! this should be undone later.
	void tentative_produce_state_commitment(
		Hash& hash, const AccountModificationLog& dirty_accounts) 
	{
		do_rollback_produce_state_commitment = true;
		db.tentative_produce_state_commitment(hash, dirty_accounts);
		rollback_log = &dirty_accounts;
	}
	//! Finalize state changes.  Makes destucturo into a no-op.
	void finalize_commit() {
		if ((!do_rollback_for_validation) 
			|| (!do_rollback_produce_state_commitment)) {
			throw std::runtime_error("committing from invalid state");
		}
		do_rollback_for_validation = false;
		do_rollback_produce_state_commitment = false;
		db.commit_values(*rollback_log);
		db.finalize_produce_state_commitment();
	}
};

//! Rollback changes to orderbooks when object leaves scope,
//! unless block validation succeeds and state changes commit.
struct OrderbookManagerAutoRollback {
	OrderbookManager& manager;
	const OrderbookStateCommitmentChecker& clearing_log;

	bool do_rollback_for_validation = false;

	OrderbookManagerAutoRollback(
		OrderbookManager& manager, 
		const OrderbookStateCommitmentChecker& clearing_log)
		: manager(manager)
		, clearing_log(clearing_log) {}

	//! rollback_validation() undoes both new offer creation and
	//! offer clearing, tracking which things need to be undone internally.
	~OrderbookManagerAutoRollback() {
		if (do_rollback_for_validation) {
			manager.rollback_validation();
		}
	}

	//! Clear offers in orderbooks, record that this action should
	//! be undone later.
	bool tentative_clear_offers_for_validation(
		SpeedexManagementStructures& management_structures,
		ThreadsafeValidationStatistics& validation_stats,
		BlockStateUpdateStatsWrapper& state_update_stats) 
	{
		do_rollback_for_validation = true;
		return manager.tentative_clear_offers_for_validation(
			management_structures.db,
			management_structures.account_modification_log,
			validation_stats,
			clearing_log,
			state_update_stats);
	}

	//! Merge in newly created offers to orderbooks,
	//! log that this action should be undone later
	void tentative_commit_for_validation(uint64_t current_block_number) {
		do_rollback_for_validation = true;
		manager.commit_for_validation(current_block_number);
	}

	//! Finalize state changes to orderbooks, makes destructor a no-op.
	void finalize_commit() {
		do_rollback_for_validation = false;
		manager.finalize_validation();
	}
};

//! Automatically clears mod log and cancels the request for a file descriptor
//! if the block does not commit.
//! If block commits, mod log must be cleared later.
struct AccountModificationLogAutoRollback {
	AccountModificationLog& log;
	bool do_rollback = true;
	bool do_cancel_block_fd = true;

	AccountModificationLogAutoRollback(AccountModificationLog& log)
		: log(log) {}

	~AccountModificationLogAutoRollback() {
		if (do_rollback) {
			log.detached_clear();
		}
		if (do_cancel_block_fd) {
			log.cancel_prepare_block_fd();
		}
	}

	//! Makes destructor into a no-op.
	void finalize_commit() {
		do_rollback = false;
		do_cancel_block_fd = false;
	}
};

//! Automatically undoes changes to block header-hash map upon leaving
//! scope, unless block commits.
struct BlockHeaderHashMapAutoRollback {
	BlockHeaderHashMap& map;
	bool do_rollback = false;

	BlockHeaderHashMapAutoRollback(BlockHeaderHashMap& map) : map(map) {}

	~BlockHeaderHashMapAutoRollback() {
	//	if (do_rollback) {
	//		map.rollback_validation();
	//	}
	}

/*	//! Insert hash into map, logs that this action should be undone later.
	bool tentative_insert_for_validation(
		uint64_t current_block_number, const Hash& hash) {
		do_rollback = true;
		return map.tentative_insert_for_validation(current_block_number, hash);
	}

	//! Finalizes insertion to hash map, makes destructor into a no-op
	void finalize_commit(uint64_t finalized_block_number) {
		do_rollback = false;
		map.finalize_validation(finalized_block_number);
	} */
};

//! Automatically undoes all changes to speedex structures, unless
//! block validation commits.
//! Account modification log's constructor has to be called last,
//! since the database's rollback functionality depends on modlog contents.
struct SpeedexManagementStructuresAutoRollback {
	AccountModificationLogAutoRollback account_modification_log;
	DatabaseAutoRollback db;
	OrderbookManagerAutoRollback orderbook_manager;
	BlockHeaderHashMapAutoRollback block_header_hash_map;

	//! Initialize autorollback version from regular speedex structures.
	SpeedexManagementStructuresAutoRollback(
		SpeedexManagementStructures& management_structures,
		uint64_t current_block_number,
		OrderbookStateCommitmentChecker& clearing_log) 
		: account_modification_log(
			management_structures.account_modification_log) 
		, db(management_structures.db, current_block_number)
		, orderbook_manager(
			management_structures.orderbook_manager, clearing_log)
		, block_header_hash_map(management_structures.block_header_hash_map) 
		{}

	//! Finalize block validation.  Makes destructor into a no-op
	void finalize_commit(
		uint64_t finalized_block_number, 
		BlockValidationMeasurements& stats) 
	{
		auto timestamp = init_time_measurement();
		
		account_modification_log.finalize_commit();
		stats.account_log_finalization_time = measure_time(timestamp);

		db.finalize_commit();
		stats.db_finalization_time = measure_time(timestamp);

		orderbook_manager.finalize_commit();
		stats.workunit_finalization_time = measure_time(timestamp);

		//block_header_hash_map.finalize_commit(finalized_block_number);
		stats.header_map_finalization_time = measure_time(timestamp);

	}
};

} /* speedex */
