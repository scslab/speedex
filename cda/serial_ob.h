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
