#pragma once

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
