#include "filtering/account_filter_entry.h"

#include "memory_database/memory_database.h"

namespace speedex
{

using xdr::operator==;
using xdr::operator!=;

AccountFilterEntry::AccountFilterEntry(AccountID account)
    : account(account)
    , min_seq_no(UINT64_MAX)
    , initialized(true)
    , txs()
    , required_assets()
{}

void
AccountFilterEntry::assert_initialized() const
{
    if (!initialized)
    {
        std::printf("uninitialized\n");
        throw std::runtime_error("uninit");
    }
}

void
AccountFilterEntry::add_req(AssetID const& asset, int64_t amount)
{
	//std::printf("add req %u amt %ld\n", asset, amount);
    if (amount < 0)
    {
    //	std::printf("negative\n");
        return;
    }

    if (found_error)
    {
        return;
    }

    if (__builtin_add_overflow_p(
            amount, required_assets[asset], static_cast<int64_t>(0)))
    {
    //	std::printf("got error\n");
        required_assets[asset] = INT64_MAX;
        found_error = true;
        return;
    }

    required_assets[asset] += amount;
}

void
AccountFilterEntry::compute_reqs()
{
    for (auto const& [_, tx] : txs)
    {
        for (auto const& op : tx.transaction.operations)
        {
            switch (op.body.type())
            {
                case CREATE_ACCOUNT:
                    add_req(MemoryDatabase::NATIVE_ASSET,
                            op.body.createAccountOp().startingBalance);
                    break;
                case CREATE_SELL_OFFER:
                    add_req(op.body.createSellOfferOp().category.sellAsset,
                            op.body.createSellOfferOp().amount);
                    break;
                case CANCEL_SELL_OFFER:
                    break;
                case PAYMENT:
       //         	std::printf("payment tx\n");
                    add_req(op.body.paymentOp().asset,
                            op.body.paymentOp().amount);
                    break;
                case MONEY_PRINTER:
                    break;
                default:
                    throw std::runtime_error("filtering unknown optype");
            }
        }
      //  std::printf("native asset\n");
        add_req(MemoryDatabase::NATIVE_ASSET, tx.transaction.fee);
    }
}

void
AccountFilterEntry::add_tx(SignedTransaction const& tx,
                           MemoryDatabase const& db)
{
//	std:printf("add tx\n");
    if (found_error)
    {
    //	std::printf("already error\n");
        return;
    }

    if (min_seq_no == UINT64_MAX)
    {
        auto const* acc = db.lookup_user(account);

        if (acc == nullptr)
        {
        //	std::printf("no account\n");
            log_reqs_invalid();
            return;
        }
        min_seq_no = acc->get_last_committed_seq_number();
    }

    uint64_t seqno = tx.transaction.metadata.sequenceNumber;

    if (seqno <= min_seq_no)
    {
    	//std::printf("low seqno, ignoring seqno=%lu min=%lu\n", seqno, min_seq_no);
        return;
    }

    auto it = txs.find(seqno);
    if (it != txs.end())
    {
        auto const& tx_old = it->second;

        if (!(tx_old == tx))
        {
        //	std::printf("dup seqno mismatch, error\n");
            found_error = true;
            return;
        }
    }
    txs.emplace(seqno, tx);
}

void
AccountFilterEntry::log_reqs_invalid()
{
    checked_reqs_cached = true;
    found_error = true;
}

void
AccountFilterEntry::log_reqs_valid()
{
    checked_reqs_cached = true;
}

void
AccountFilterEntry::compute_validity(MemoryDatabase const& db)
{
//	std::printf("compute_validity\n");
    if (checked_reqs_cached)
    {
        std::printf("double check valid\n");
        throw std::runtime_error("double check valid");
    }

    if (found_error)
    {
        log_reqs_invalid();
        return;
    }

    assert_initialized();

    compute_reqs();

    auto const* acc = db.lookup_user(account);
    if (acc == nullptr)
    {
        log_reqs_invalid();
        return;
    }

    for (auto const& [asset, req] : required_assets)
    {
        int64_t avail = acc->lookup_available_balance(asset);
       // std::printf("asset %lu req %ld avail %ld\n", asset, req, avail);
        if (avail < req)
        {
            log_reqs_invalid();
            return;
        }
    }
    log_reqs_valid();
}

bool
AccountFilterEntry::check_valid() const
{
    if (!checked_reqs_cached)
    {
        throw std::runtime_error("check before computing valid or not");
    }

    return !found_error;
}

void
AccountFilterEntry::merge_in(AccountFilterEntry& other)
{
    min_seq_no = std::min(min_seq_no, other.min_seq_no);
    found_error = found_error || other.found_error;

    if (found_error)
    {
        return;
    }

    other.assert_initialized();
    initialized = true;

    if (checked_reqs_cached)
    {
        throw std::runtime_error("improper merge in post check");
    }

    txs.merge(other.txs);

    for (auto const& [seqno, tx] : other.txs)
    {
        auto it = txs.find(seqno);
        if (it == txs.end())
        {
            throw std::runtime_error("error in map merge");
        }

        if (it->second != tx)
        {
            found_error = true;
            return;
        }
    }
}

} // namespace speedex
