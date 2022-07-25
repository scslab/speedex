#pragma once

#include "orderbook/typedefs.h"

namespace speedex
{

class MemoryDatabase;

class SerialOrderbook
{

	typedef CombinedMetadata<
			DeletableMixin>
	SerialOBMetadata;

	using ob_t = MerkleTrie<OrderbookTriePrefix, OfferWrapper, SerialOBMetadata, false>;

	ob_t ob;

public:

	void add_offer(Offer const& offer);
	bool cancel_offer(const Price& mp, const AccountID& account, const uint64_t seqno);

	// returns [sold amount, bought amount]
	std::pair<uint64_t, uint64_t>
	try_execute(const Price& max_price, const uint64_t sell_amount, MemoryDatabase& db);
};


} /* speedex */
