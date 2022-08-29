#include "speedex/autorollback_validation_structures.h"

#include "mtt/utils/time.h"

namespace speedex
{

DatabaseAutoRollback::DatabaseAutoRollback(MemoryDatabase& db,
                                           uint64_t current_block_number)
    : db(db)
    , current_block_number(current_block_number)
{}

//! Can't have this called after modlog is cleared
//! which occurs when ~AccountModificationLogAutoRollback() is called.
DatabaseAutoRollback::~DatabaseAutoRollback()
{
    if (do_rollback_for_validation)
    {
        db.rollback_new_accounts(current_block_number);
        db.rollback_values();
    }

    if (do_rollback_produce_state_commitment)
    {
        db.rollback_produce_state_commitment(*rollback_log);
    }
}

//! Commit new account creation,
//! records that this should be undone later.
void
DatabaseAutoRollback::tentative_commit_for_validation()
{
    do_rollback_for_validation = true;
    db.commit_new_accounts(current_block_number);
}

//! modifies commitment trie, records that
//! this should be undone later.
void
DatabaseAutoRollback::tentative_produce_state_commitment(
    Hash& hash,
    const AccountModificationLog& dirty_accounts)
{
    do_rollback_produce_state_commitment = true;
    db.tentative_produce_state_commitment(hash, dirty_accounts);
    rollback_log = &dirty_accounts;
}
//! Finalize state changes.  Makes destucturo into a no-op.
void
DatabaseAutoRollback::finalize_commit()
{
    if ((!do_rollback_for_validation)
        || (!do_rollback_produce_state_commitment))
    {
        throw std::runtime_error("committing from invalid state");
    }
    do_rollback_for_validation = false;
    do_rollback_produce_state_commitment = false;
    db.commit_values(*rollback_log);
    db.finalize_produce_state_commitment();
}

OrderbookManagerAutoRollback::OrderbookManagerAutoRollback(
    OrderbookManager& manager,
    const OrderbookStateCommitmentChecker& clearing_log)
    : manager(manager)
    , clearing_log(clearing_log)
{}

//! rollback_validation() undoes both new offer creation and
//! offer clearing, tracking which things need to be undone internally.
OrderbookManagerAutoRollback::~OrderbookManagerAutoRollback()
{
    if (do_rollback_for_validation)
    {
        manager.rollback_validation();
    }
}

//! Clear offers in orderbooks, record that this action should
//! be undone later.
bool
OrderbookManagerAutoRollback::tentative_clear_offers_for_validation(
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

void
OrderbookManagerAutoRollback::tentative_commit_for_validation(
    uint64_t current_block_number)
{
    do_rollback_for_validation = true;
    manager.commit_for_validation(current_block_number);
}

//! Finalize state changes to orderbooks, makes destructor a no-op.
void
OrderbookManagerAutoRollback::finalize_commit()
{
    do_rollback_for_validation = false;
    manager.finalize_validation();
}

AccountModificationLogAutoRollback::AccountModificationLogAutoRollback(
    AccountModificationLog& log)
    : log(log)
{}

AccountModificationLogAutoRollback::~AccountModificationLogAutoRollback()
{
    if (do_rollback)
    {
        log.detached_clear();
    }
    if (do_cancel_block_fd)
    {
        log.cancel_prepare_block_fd();
    }
}

//! Makes destructor into a no-op.
void
AccountModificationLogAutoRollback::finalize_commit()
{
    do_rollback = false;
    do_cancel_block_fd = false;
}

SpeedexManagementStructuresAutoRollback::
    SpeedexManagementStructuresAutoRollback(
        SpeedexManagementStructures& management_structures,
        uint64_t current_block_number,
        OrderbookStateCommitmentChecker& clearing_log)
    : account_modification_log(management_structures.account_modification_log)
    , db(management_structures.db, current_block_number)
    , orderbook_manager(management_structures.orderbook_manager, clearing_log)
    , block_header_hash_map(management_structures.block_header_hash_map)
{}

//! Finalize block validation.  Makes destructor into a no-op
void
SpeedexManagementStructuresAutoRollback::finalize_commit(
    uint64_t finalized_block_number,
    BlockValidationMeasurements& stats)
{
    auto timestamp = utils::init_time_measurement();

    account_modification_log.finalize_commit();
    stats.account_log_finalization_time = utils::measure_time(timestamp);

    db.finalize_commit();
    stats.db_finalization_time = utils::measure_time(timestamp);

    orderbook_manager.finalize_commit();
    stats.workunit_finalization_time = utils::measure_time(timestamp);

    // block_header_hash_map.finalize_commit(finalized_block_number);
    stats.header_map_finalization_time = utils::measure_time(timestamp);
}

} // namespace speedex
