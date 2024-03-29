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

#include "modlog/account_modification_log.h"
#include "modlog/log_entry_fns.h"

#include "config.h"

#include "utils/debug_macros.h"
#include "utils/save_load_xdr.h"
#include "utils/manage_data_dirs.h"

#include "speedex/speedex_static_configs.h"

#include <utils/time.h>

#include <mtt/trie/configs.h>

namespace speedex {

constexpr static bool DIFF_LOGS_ENABLED = false;


AccountModificationLog::AccountModificationLog() 
    : cache()
    , hash_log(std::nullopt)
    , modification_log()
    , persistable_block(std::make_unique<saved_block_t>())
    , mtx()
    , deleter() 
    {
        if constexpr (trie::TRIE_LOG_HASH_RECORDS)
        {
            hash_log.emplace();
        }
    };


void
AccountModificationLog::hash(Hash& hash, uint64_t block_number)
{
    std::lock_guard lock(mtx);

    auto timestamp = utils::init_time_measurement();

    modification_log.hash<LogNormalizeFn>(hash, hash_log);

    float res = utils::measure_time(timestamp);

    *persistable_block
        = modification_log
              .template accumulate_values_parallel<saved_block_t, AccumulateFn>();

    float res2 = utils::measure_time(timestamp);

    std::string log_filename = log_dir() + "account_hash_" + std::to_string(block_number);

    if (hash_log)
    {
        hash_log->write_logs(log_filename);
    }

    float res3 = utils::measure_time(timestamp);

    BLOCK_INFO("acct log hash: hash/normalize %lf acc vals %lf wait on hash logs write %lf", res, res2, res3);
}

void
AccountModificationLog::merge_in_log_batch()
{
    std::lock_guard lock(mtx);

    modification_log.template batch_merge_in<LogMergeFn>(cache);
}

void
AccountModificationLog::detached_clear()
{

    std::lock_guard lock(mtx);

    deleter.call_delete(persistable_block.release());

    persistable_block = std::make_unique<saved_block_t>();

    modification_log.clear();
    cache.clear();
}

std::unique_ptr<AccountModificationLog::saved_block_t>
AccountModificationLog::persist_block(uint64_t block_number,
                                      bool return_block,
                                      bool persist_block)
{

    std::lock_guard lock(mtx);

    BLOCK_INFO("saving account log for block %lu", block_number);
    if (write_buffer == nullptr) {
        std::printf("write buffer was null!!!\n");
        throw std::runtime_error("null write buffer");
    }

    if (persist_block || return_block) {
        if ((persistable_block->size() == 0) && (modification_log.size() > 0)) {
            std::printf("forming log in persist_block\n");
            *persistable_block
                = modification_log.template accumulate_values_parallel<
                    saved_block_t, AccumulateFn>();
        }
    }

    if (persist_block) {
      
        throw std::runtime_error("dead code path");
        /*
        auto& block_fd = file_preallocator.wait_for_prealloc();
        if (!block_fd) {
            throw std::runtime_error("block wasn't preallocated!!!");
        }

        BLOCK_INFO("persist_block size: %lu\n", persistable_block->size());
        if (persistable_block->size() != modification_log.size()) {
            throw std::runtime_error(
                "must be error in accumulate_values_parallel");
        }

        if constexpr (DIFF_LOGS_ENABLED) {
            save_xdr_to_file_fast(
                *persistable_block, block_fd, write_buffer, BUF_SIZE);
        } else {
            throw std::runtime_error("unimpl for serializedtxlist");
        //    save_account_block_fast(
          //      *persistable_block, block_fd, write_buffer, BUF_SIZE);
        }
        block_fd.clear();
        */
    }

    if (return_block) {
        std::unique_ptr<saved_block_t> out{
            persistable_block.release()
        };
        persistable_block = std::make_unique<saved_block_t>();
        return out;
    }
    return nullptr;
}

void
SerialAccountModificationLog::log_self_modification(AccountID owner,
                                                    uint64_t sequence_number)
{
    if constexpr (DETAILED_MOD_LOGGING)
    {
        modification_log.template insert<LogInsertFn, uint64_t>(owner,
                                                            std::move(sequence_number));
    } else
    {
        modification_log.template insert<LogKeyOnlyInsertFn, const void*>(owner, nullptr);
    }
}

void
SerialAccountModificationLog::log_other_modification(AccountID tx_owner,
                                                     uint64_t sequence_number,
                                                     AccountID modified_account)
{
    if constexpr (DETAILED_MOD_LOGGING)
    {
        TxIdentifier value{ tx_owner, sequence_number };
        modification_log.template insert<LogInsertFn, TxIdentifier>(
            modified_account, std::move(value));
    }
    else
    {
        modification_log.template insert<LogKeyOnlyInsertFn, const void*>(modified_account, nullptr);
    }
}

void
SerialAccountModificationLog::log_new_self_transaction(
    const SignedTransaction& tx)
{
    AccountID sender = tx.transaction.metadata.sourceAccount;

    if constexpr (DETAILED_MOD_LOGGING)
    {
        modification_log.template insert<LogInsertFn, SignedTransaction>(sender,
                                                                 SignedTransaction(tx));
    } else
    {
        modification_log.template insert<LogKeyOnlyInsertFn, const void*>(sender, nullptr);
    }
}

void
AccountModificationLog::diff_with_prev_log(uint64_t block_number)
{
    AccountModificationBlock prev;

    if constexpr (DIFF_LOGS_ENABLED) {
        // this error happens because we didn't save the entire log (since hotstuff just
        // saves the tx list, not the full modification log)
        // TODO this should likely not throw, but rather just return
        // It's purpose was diagnostical (prior to 9/29/2022), but is unusable 
        // unless we re-enable detailed logging within the speedex vm.
        throw std::runtime_error(
            "TODO can't compare account log if we didn't save entire log");
    }

    //auto filename = tx_block_name(block_number);
    //if (load_xdr_from_file(prev, filename.c_str())) {
      //  throw std::runtime_error("couldn't load previous comparison data");
    //}

    throw std::runtime_error("unimpl yet for AccountModificationEntry");
    /*

    AccountModificationBlock current
        = modification_log
              .template accumulate_values_parallel<saved_block_t>();

    std::printf("prev len %lu current len %lu\n", prev.size(), current.size());

    for (size_t i = 0; i < std::min(prev.size(), current.size()); i++) {
        AccountModificationTxListWrapper p = prev[i];
        AccountModificationTxListWrapper c = current[i];

        LogNormalizeFn::apply_to_value(p);
        LogNormalizeFn::apply_to_value(c);

        if (p.new_transactions_self.size() != c.new_transactions_self.size()) {
            std::printf(
                "%lu new_transactions_self.size() prev %lu current %lu\n",
                i,
                p.new_transactions_self.size(),
                c.new_transactions_self.size());
        } else {
            bool found_dif = false;

            for (unsigned int j = 0; j < p.new_transactions_self.size(); j++) {
                if (p.new_transactions_self[j]
                        .transaction.metadata.sequenceNumber
                    != c.new_transactions_self[j]
                           .transaction.metadata.sequenceNumber) {
                    found_dif = true;
                }
                if (p.new_transactions_self[j]
                        .transaction.metadata.sourceAccount
                    != c.new_transactions_self[j]
                           .transaction.metadata.sourceAccount) {
                    found_dif = true;
                }
            }
            if (found_dif) {
                for (unsigned int j = 0; j < p.new_transactions_self.size();
                     j++) {
                    std::printf("new_transactions_self prev_seqnum %" PRIu64
                                " current_seqnum %" PRIu64 "\n",
                                p.new_transactions_self[j]
                                    .transaction.metadata.sequenceNumber,
                                c.new_transactions_self[j]
                                    .transaction.metadata.sequenceNumber);
                    std::printf("new_transactions_self prev_owner %" PRIu64
                                " current_owner %" PRIu64 "\n",
                                p.new_transactions_self[j]
                                    .transaction.metadata.sourceAccount,
                                c.new_transactions_self[j]
                                    .transaction.metadata.sourceAccount);
                }
            }
        }
        if (p.identifiers_self.size() != c.identifiers_self.size()) {
            std::printf("%lu identifiers_self.size() prev %lu current %lu\n",
                        i,
                        p.identifiers_self.size(),
                        c.identifiers_self.size());
        } else {

            bool found_dif = false;
            for (size_t j = 0; j < p.identifiers_self.size(); j++) {
                if (std::find(c.identifiers_self.begin(),
                              c.identifiers_self.end(),
                              p.identifiers_self[j])
                    == c.identifiers_self.end()) {
                    found_dif = true;
                }
            }
            if (found_dif) {
                std::printf("%lu (account prev %" PRIu64 " / current %" PRIu64
                            ") different identifiers_self\n",
                            i,
                            p.owner,
                            c.owner);
                for (size_t j = 0; j < p.identifiers_self.size(); j++) {
                    // if (p.identifiers_self[j] != c.identifiers_self[j]) {
                    std::printf("%lu %lu identifiers_self_prev_seqnum %" PRIu64
                                " current_seqnum %" PRIu64 "\n",
                                i,
                                j,
                                p.identifiers_self[j],
                                c.identifiers_self[j]);
                    //}
                }
            }
        }
        if (p.identifiers_others.size() != c.identifiers_others.size()) {
            std::printf("%lu identifiers_others.size() prev %lu current %lu\n",
                        i,
                        p.identifiers_others.size(),
                        c.identifiers_others.size());
        }
    } */
}

} // namespace speedex
