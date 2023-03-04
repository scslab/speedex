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

#include "filtering/account_filter_entry.h"

#include "mtt/trie/recycling_impl/trie.h"

#include <utils/threadlocal_cache.h>

#include "filtering/error_code.h"

namespace speedex
{

class AccountCreationFilter
{
    std::mutex mtx;
    std::map<AccountID, uint32_t> created_counts;

    bool is_valid_account_creation(AccountID account) const;
public:

    void log_account_creation(AccountID src);

    bool check_valid_tx(const SignedTransaction& tx) const;

    void clear()
    {
        created_counts.clear();
    }
};

class FilterLog
{
    using trie_t = trie::RecyclingTrie<AccountFilterEntry>;
    using serial_trie_t = trie_t::serial_trie_t;

    using serial_cache_t = utils::ThreadlocalCache<serial_trie_t>;

    trie_t entries;

    AccountCreationFilter accounts;

public:
    void add_txs(std::vector<SignedTransaction> const& txs,
                 MemoryDatabase const& db);

    FilterResult check_valid_account(AccountID const& account) const;
    bool check_valid_tx(const SignedTransaction& tx) const
    {
        FilterResult filter_res = check_valid_account(tx.transaction.metadata.sourceAccount);
        if (filter_res < 0) {
            return false;
        }
        return accounts.check_valid_tx(tx);
    }

    void clear() {
        entries.clear();
        accounts.clear();
    }
};

} // namespace speedex
