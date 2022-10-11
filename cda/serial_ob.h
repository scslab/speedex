#pragma once

#include "orderbook/typedefs.h"

#include "mtt/trie/metadata.h"
#include "mtt/trie/merkle_trie.h"

namespace speedex
{

class BufferedMemoryDatabaseView;

class SerialOrderbook
{

	typedef trie::CombinedMetadata<
			trie::DeletableMixin>
	SerialOBMetadata;

	using ob_t = trie::MerkleTrie<OrderbookTriePrefix, OfferWrapper, SerialOBMetadata, false>;

	ob_t ob;

public:

	void add_offer(Offer const& offer);
	bool cancel_offer(const Price& mp, const AccountID& account, const uint64_t seqno);

	// returns [sold amount, bought amount]
	// exec never "fails", it just might not trade (at which point we need to add the offer
	// to the other orderbook)
	std::pair<uint64_t, uint64_t>
	try_execute(const Price& max_price, const uint64_t sell_amount, BufferedMemoryDatabaseView& db);
};


} /* speedex */
