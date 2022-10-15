#pragma once

/*! \file autorollback_validation_structures.h

Wrappers around Speedex data structures
that tracks validation-related changes
and rolls them back upon leaving scope,
unless validation succeeds and the block
commits.
*/

#include "xdr/block.h"

#include <cstdint>

namespace speedex
{

class AccountModificationLog;
class BlockHeaderHashMap;
class BlockStateUpdateStatsWrapper;
class MemoryDatabase;
class OrderbookManager;
class OrderbookStateCommitmentChecker;
class SpeedexManagementStructures;
class ThreadsafeValidationStatistics;

//! Rollback changes to database upon going out of scope,
//! unless whole block commits.
struct DatabaseAutoRollback
{
    MemoryDatabase& db;
    //! Current block number being modified
    uint64_t current_block_number = 0;

    //! Undo a set of balance modifications
    bool do_rollback_for_validation = false;
    //! Undo changes to commitment trie
    bool do_rollback_produce_state_commitment = false;

    const AccountModificationLog* rollback_log;

    DatabaseAutoRollback(MemoryDatabase& db, uint64_t current_block_number);

    //! Can't have this called after modlog is cleared
    //! which occurs when ~AccountModificationLogAutoRollback() is called.
    ~DatabaseAutoRollback();

    //! Commit new account creation,
    //! records that this should be undone later.
    void tentative_commit_for_validation();

    //! modifies commitment trie, records that
    //! this should be undone later.
    void tentative_produce_state_commitment(
        Hash& hash,
        const AccountModificationLog& dirty_accounts,
        uint64_t block_number);
    //! Finalize state changes.  Makes destucturo into a no-op.
    void finalize_commit();
};

//! Rollback changes to orderbooks when object leaves scope,
//! unless block validation succeeds and state changes commit.
struct OrderbookManagerAutoRollback
{
    OrderbookManager& manager;
    const OrderbookStateCommitmentChecker& clearing_log;

    bool do_rollback_for_validation = false;

    OrderbookManagerAutoRollback(
        OrderbookManager& manager,
        const OrderbookStateCommitmentChecker& clearing_log);

    //! rollback_validation() undoes both new offer creation and
    //! offer clearing, tracking which things need to be undone internally.
    ~OrderbookManagerAutoRollback();

    //! Clear offers in orderbooks, record that this action should
    //! be undone later.
    bool tentative_clear_offers_for_validation(
        SpeedexManagementStructures& management_structures,
        ThreadsafeValidationStatistics& validation_stats,
        BlockStateUpdateStatsWrapper& state_update_stats);

    //! Merge in newly created offers to orderbooks,
    //! log that this action should be undone later
    void tentative_commit_for_validation(uint64_t current_block_number);

    //! Finalize state changes to orderbooks, makes destructor a no-op.
    void finalize_commit();
};

//! Automatically clears mod log and cancels the request for a file descriptor
//! if the block does not commit.
//! If block commits, mod log must be cleared later.
struct AccountModificationLogAutoRollback
{
    AccountModificationLog& log;
    bool do_rollback = true;
    bool do_cancel_block_fd = true;

    AccountModificationLogAutoRollback(AccountModificationLog& log);

    ~AccountModificationLogAutoRollback();
    //! Makes destructor into a no-op.
    void finalize_commit();
};

//! Automatically undoes changes to block header-hash map upon leaving
//! scope, unless block commits.
struct BlockHeaderHashMapAutoRollback
{
    BlockHeaderHashMap& map;
    bool do_rollback = false;

    BlockHeaderHashMapAutoRollback(BlockHeaderHashMap& map)
        : map(map)
    {}

    ~BlockHeaderHashMapAutoRollback()
    {
        //	if (do_rollback) {
        //		map.rollback_validation();
        //	}
    }

    /*	//! Insert hash into map, logs that this action should be undone later.
            bool tentative_insert_for_validation(
                    uint64_t current_block_number, const Hash& hash) {
                    do_rollback = true;
                    return
       map.tentative_insert_for_validation(current_block_number, hash);
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
struct SpeedexManagementStructuresAutoRollback
{
    AccountModificationLogAutoRollback account_modification_log;
    DatabaseAutoRollback db;
    OrderbookManagerAutoRollback orderbook_manager;
    BlockHeaderHashMapAutoRollback block_header_hash_map;

    //! Initialize autorollback version from regular speedex structures.
    SpeedexManagementStructuresAutoRollback(
        SpeedexManagementStructures& management_structures,
        uint64_t current_block_number,
        OrderbookStateCommitmentChecker& clearing_log);

    //! Finalize block validation.  Makes destructor into a no-op
    void finalize_commit(uint64_t finalized_block_number,
                         BlockValidationMeasurements& stats);
};

} // namespace speedex
