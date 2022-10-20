#pragma once

#include "xdr/transaction.h"
#include "xdr/types.h"

#include "mtt/trie/prefix.h"

#include <cstdint>
#include <map>
#include <set>

#include "filtering/error_code.h"

#include <utils/non_movable.h>

namespace speedex
{

class MemoryDatabase;

class AccountFilterEntry
{
    AccountID account;
    uint64_t min_seq_no;
    bool initialized = false;
    bool reqs_computed = false;

    std::map<uint64_t, SignedTransaction> txs;

    std::map<AssetID, int64_t> required_assets;

    std::set<uint64_t> consumed_cancel_ids;

    bool found_bad_duplicate = false;
    bool found_invalid_reqs = false;
    bool found_account_nexist = false;

    bool overflow_req = false;
    bool double_cancel = false;

    bool checked_reqs_cached = false;

    void add_req(AssetID const& asset, int64_t amount);
    void add_cancel_id(uint64_t id);

    void log_invalid_account();
    void log_bad_duplicate();
    void log_overflow_req();
    void log_double_cancel();
    void log_reqs_invalid();
    void log_reqs_checked();

    void compute_reqs();

    void assert_initialized() const;

    bool found_error() const
    {
    	return found_bad_duplicate || found_invalid_reqs || found_account_nexist || overflow_req || double_cancel;
    }

public:
    AccountFilterEntry()
        : account(0)
        , min_seq_no(UINT64_MAX)
        , initialized(false)
        , txs()
        , required_assets()
    {}

    AccountFilterEntry(AccountID account);

    void add_tx(SignedTransaction const& tx, MemoryDatabase const& db);

    void compute_validity(MemoryDatabase const& db);

    void merge_in(AccountFilterEntry& other);

    FilterResult check_valid() const;
};

struct AccountFilterInsertFn
{
    static AccountFilterEntry new_value(trie::UInt64Prefix const& prefix)
    {
        try
        {
            return AccountFilterEntry(prefix.uint64());
        }
        catch (...)
        {
            std::printf("wat2\n");
            std::fflush(stdout);
            throw;
        }
    }

    static void value_insert(
        AccountFilterEntry& to_modify,
        std::pair<SignedTransaction, const MemoryDatabase*> const& inserted)
    {
        try
        {
            to_modify.add_tx(inserted.first, *(inserted.second));
        }
        catch (...)
        {
            std::printf("wat\n");
            std::fflush(stdout);
            throw;
        }
    }
};

struct AccountFilterMergeFn
{
    template<typename metadata_t>
    static 
    metadata_t value_merge_recyclingimpl(AccountFilterEntry& into, AccountFilterEntry& from)
    {
        into.merge_in(from);
        return metadata_t::zero();
    }
};

} // namespace speedex
