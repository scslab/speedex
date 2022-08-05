#pragma once

#include "filtering/account_filter_entry.h"

#include "trie/recycling_impl/trie.h"

#include "utils/threadlocal_cache.h"

namespace speedex
{

enum FilterResult : int32_t
{
    // success
    VALID_NO_TXS = 0,
    VALID_HAS_TXS = 1,

    // failure
    INVALID = -1,
};

class FilterLog
{
    using trie_t = AccountTrie<AccountFilterEntry>;
    using serial_trie_t = trie_t::serial_trie_t;

    using serial_cache_t = ThreadlocalCache<serial_trie_t>;

    trie_t entries;

public:
    void add_txs(std::vector<SignedTransaction> const& txs,
                 MemoryDatabase const& db);

    FilterResult check_valid_account(AccountID const& account) const;
};

} // namespace speedex
