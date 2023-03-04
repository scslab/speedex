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

#include "cda/serial_ob.h"

#include "memory_database/memory_database_view.h"

namespace speedex
{

void 
SerialOrderbook::add_offer(Offer const& offer)
{
	OrderbookTriePrefix prefix;

	generate_orderbook_trie_key(offer, prefix);

	ob.insert(prefix, OfferWrapper(offer));
}


bool 
SerialOrderbook::cancel_offer(const Price& mp, const AccountID& account, const uint64_t seqno)
{
	OrderbookTriePrefix prefix;

	generate_orderbook_trie_key(mp, account, seqno, prefix);

	return ((bool) ob.perform_deletion(prefix));
}

std::pair<uint64_t, uint64_t>
SerialOrderbook::try_execute(const Price& max_price, const uint64_t sell_amount, BufferedMemoryDatabaseView& db)
{
	uint64_t remaining_sell = sell_amount;
	uint64_t bought_amount = 0;
	Offer final_offer;

	size_t num_changed = 0;

	for (auto it = ob.begin(); !it.at_end(); ++it)
	{
		if (remaining_sell == 0)
		{
			break;	
		}

		auto const& offer = (*it).second.get();

		//std::printf("mp = %lu max_price = %lu\n", offer.minPrice, max_price);

		if (offer.minPrice <= max_price)
		{

			// offer selling X units of B
			// we are selling Y units of A

			// offer mp is pB/pA
			// we can buy up to Y/mp units of X
			// offer sells min(X, Y/mp)
			// alternatively, offer sells min(X *mp, Y) / mp

			uint128_t max_sell_consumed = ((uint128_t) offer.minPrice) * ((uint128_t) offer.amount);
			uint128_t remaining_sell_consumed = ((uint128_t) remaining_sell) << price::PRICE_RADIX;

			//std::printf("max sell consumed = %lf remaining_sell_consumed = %lf\n", (double) max_sell_consumed, (double) remaining_sell_consumed);

			uint128_t realized_sell_consumed = std::min(max_sell_consumed, remaining_sell_consumed);

			uint64_t amount_offer_consumed = realized_sell_consumed / offer.minPrice;
			uint64_t amount_remaining_sell_consumed = price::round_up_price_times_amount(realized_sell_consumed);

		//	std::printf("remaining_sell = %lu amount_remaining_sell_consumed = %lu\n", remaining_sell, amount_remaining_sell_consumed);
		//	std::printf("amount_offer_consumed = %lu offer.amount %lu\n", amount_offer_consumed, offer.amount);

			if (amount_remaining_sell_consumed > remaining_sell)
			{
				throw std::runtime_error("sell amount mismatch");
			}

			remaining_sell -= amount_remaining_sell_consumed;
			bought_amount += amount_offer_consumed;

			auto* user = db.lookup_user(offer.owner);

			if (user == nullptr)
			{
				throw std::runtime_error("wtf");
			}

			//db.transfer_available(user, offer.category.sellAsset, amount_offer_consumed);
			db.transfer_available(user, offer.category.buyAsset, amount_remaining_sell_consumed, "cda transfer");

			if (amount_offer_consumed == offer.amount)
			{
				ob.mark_for_deletion((*it).first);
				num_changed++;
			} 
			else if (amount_offer_consumed > offer.amount)
			{
				throw std::runtime_error("invalid offer consume");
			}
			else {
				if (remaining_sell != 0)
				{
					throw std::runtime_error("invalid partial exec");
				}
				final_offer = offer;
				final_offer.amount -= amount_offer_consumed;
			}
		} 
		else
		{
			break;
		}
	}

	if (final_offer.amount != 0)
	{
		OrderbookTriePrefix prefix;

		generate_orderbook_trie_key(final_offer, prefix);

		ob.insert(prefix, OfferWrapper(final_offer));
	//	std::printf("inserting final offer\n");
	}

	ob.perform_marked_deletions();
//	std::printf("deleted %lu entries\n", num_changed);

	return 
		std::make_pair(
			sell_amount - remaining_sell,
			bought_amount
		);
}

} /* speedex */