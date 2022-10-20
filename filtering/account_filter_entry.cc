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
   // 	std::printf("negative\n");
        return;
    }

    if (found_error())
    {
    //	std::printf("found error, quick exit\n");
        return;
    }

    if (__builtin_add_overflow_p(
            amount, required_assets[asset], static_cast<int64_t>(0)))
    {
    //	std::printf("got overflow\n");
        required_assets[asset] = INT64_MAX;
	    log_overflow_req();
        return;
    }

    required_assets[asset] += amount;
}

void
AccountFilterEntry::add_cancel_id(uint64_t id)
{
    if (consumed_cancel_ids.find(id) != consumed_cancel_ids.end())
    {
        log_double_cancel();
        return;
    }
    consumed_cancel_ids.insert(id);
}

void
AccountFilterEntry::compute_reqs()
{
	if (reqs_computed)
	{
		throw std::runtime_error("double compute reqs");
	}
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
                    add_cancel_id(op.body.cancelSellOfferOp().offerId);
                    break;
                case PAYMENT:
                    add_req(op.body.paymentOp().asset,
                            op.body.paymentOp().amount);
                    break;
                case MONEY_PRINTER:
                    break;
                default:
                    throw std::runtime_error("filtering unknown optype");
            }
        }
        add_req(MemoryDatabase::NATIVE_ASSET, tx.transaction.maxFee);
    }
    reqs_computed = true;
}

void
AccountFilterEntry::add_tx(SignedTransaction const& tx,
                           MemoryDatabase const& db)
{
//	std:printf("add tx\n");
    if (found_error()) // short circuit a bad account
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
            log_bad_duplicate();
            return;
        }
    }
    txs.emplace(seqno, tx);
}

void
AccountFilterEntry::log_reqs_invalid()
{
    checked_reqs_cached = true;
    found_invalid_reqs = true;
}

void
AccountFilterEntry::log_reqs_checked()
{
    checked_reqs_cached = true;
}

void 
AccountFilterEntry::log_invalid_account()
{
	found_account_nexist = true;
}
void 
AccountFilterEntry::log_bad_duplicate()
{
	found_bad_duplicate = true;
}

void 
AccountFilterEntry::log_overflow_req()
{
	overflow_req = true;
}

void
AccountFilterEntry::log_double_cancel()
{
    double_cancel = true;
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

    if (found_error())
    {
        log_reqs_checked();
        return;
    }

    assert_initialized();

    compute_reqs();

    auto const* acc = db.lookup_user(account);
    if (acc == nullptr)
    {
        log_invalid_account();
        return;
    }

    for (auto const& [asset, req] : required_assets)
    {
        int64_t avail = acc->lookup_available_balance(asset);
    //    std::printf("asset %lu req %ld avail %ld\n", asset, req, avail);
        if (avail < req)
        {
        //	std::printf("found invalid\n");
            log_reqs_invalid();
            return;
        }
    }
    log_reqs_checked();
}

FilterResult
AccountFilterEntry::check_valid() const
{
    if (!checked_reqs_cached)
    {
        throw std::runtime_error("check before computing valid or not");
    }

    if (found_bad_duplicate)
	{
		return FilterResult::INVALID_DUPLICATE;
	}

	if (found_account_nexist)
	{
		return FilterResult::ACCOUNT_NEXIST;
	}

	if (found_invalid_reqs)
	{
		return FilterResult::MISSING_REQUIREMENT;
	}

	if (overflow_req)
	{
		return FilterResult::OVERFLOW_REQ;
	}
    return FilterResult::VALID_HAS_TXS;
}

void
AccountFilterEntry::merge_in(AccountFilterEntry& other)
{
    min_seq_no = std::min(min_seq_no, other.min_seq_no);
    found_bad_duplicate = found_bad_duplicate || other.found_bad_duplicate;
    found_invalid_reqs = found_invalid_reqs || other.found_invalid_reqs;
	found_account_nexist = found_account_nexist || other.found_account_nexist;
	overflow_req = overflow_req || other.overflow_req;
    
    if (found_error())
    {
        return;
    }

    other.assert_initialized();
    if (initialized)
    {
    	if (other.account != account)
    	{
    		throw std::runtime_error("invalid merge");
    	}
    }
    initialized = true;

    if (other.reqs_computed || reqs_computed)
    {
    	throw std::runtime_error("improper merge");
    }

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
        	log_bad_duplicate();
            return;
        }
    }
}

} // namespace speedex
