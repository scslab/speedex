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

#include "filtering/filter_log.h"

#include <stdexcept>

namespace speedex
{

void
FilterLog::add_txs(std::vector<SignedTransaction> const& txs,
                   MemoryDatabase const& db)
{
    serial_cache_t cache;

    auto serial_add_lambda = [&cache, &txs, &db, this](
                                 const tbb::blocked_range<size_t> r) {
        auto& local_log = cache.get(entries);

        for (size_t i = r.begin(); i < r.end(); i++)
        {
            auto const& tx = txs[i];
            local_log.template insert<AccountFilterInsertFn>(
                tx.transaction.metadata.sourceAccount, std::make_pair(tx, &db));
        }
    };

    tbb::blocked_range<size_t> r(0, txs.size());
    tbb::parallel_for(r, serial_add_lambda);

    entries.template batch_merge_in<AccountFilterMergeFn>(cache);

    auto validate_lambda = [&db, this](AccountFilterEntry& entry) {
        entry.compute_validity(db, accounts);
    };
    entries.parallel_apply(validate_lambda);
}

FilterResult
FilterLog::check_valid_account(AccountID const& account) const
{
    const auto* res = entries.get_value(account);
    if (!res)
    {
        return FilterResult::VALID_NO_TXS;
    }
	return res -> check_valid();
}

bool 
AccountCreationFilter::is_valid_account_creation(AccountID account) const
{
    auto it = created_counts.find(account);
    if (it == created_counts.end())
    {
        return true;
    }
    return it -> second == 1;
}

void
AccountCreationFilter::log_account_creation(AccountID src)
{
    std::lock_guard lock(mtx);
    created_counts[src] ++;
}

bool 
AccountCreationFilter::check_valid_tx(const SignedTransaction& tx) const
{
    for (auto const& op : tx.transaction.operations)
    {
        if (op.body.type() == OperationType::CREATE_ACCOUNT)
        {
            if (!is_valid_account_creation(op.body.createAccountOp().newAccountId))
            {
                return false;
            }
        }
    }
    return true;
}

} // namespace speedex
