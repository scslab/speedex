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

#pragma once

#include <cstdint>

#include "utils/fixed_point_value.h"
#include "utils/price.h"

#include "xdr/types.h"

namespace speedex {

class UserAccount;

[[maybe_unused]]
static std::string 
make_offer_string(const Offer& offer)
{
	return std::string("owner= ")
		+ std::to_string(offer.owner)
		+ std::string(" minprice = ")
		+ std::to_string(offer.minPrice)
		+ std::string(" id = ")
		+ std::to_string(offer.offerId)
		+ std::string(" sell = ")
		+ std::to_string(offer.category.sellAsset)
		+ std::string(" buy = ")
		+ std::to_string(offer.category.buyAsset);
}

/*! Fully clear a trade offer.

Template argument is to allow passing either 
a raw MemoryDatabase or an UnbufferedMemoryDatabaseView
(in the case of block validation, when offers
are immediately cleared).
*/
template<typename Database>
[[maybe_unused]]
static void
clear_offer_full(
	const Offer& offer, 
	const Price& sellPrice,
	const Price& buyPrice, 
	const uint8_t tax_rate, 
	Database& db, 
	UserAccount* db_idx) {

	auto amount = FractionalAsset::from_integral(offer.amount);

	auto buy_amount_fractional = 
		FractionalAsset::from_raw(
			price::wide_multiply_val_by_a_over_b(
				amount.value, 
				sellPrice,
				buyPrice));

	db.transfer_available(
		db_idx, offer.category.buyAsset, 
		buy_amount_fractional.tax_and_round(tax_rate),
		(make_offer_string(offer) + " clear offer full").c_str());
}

/*! Partially clear an offer.

out_sell_amount is the amount deducted from the offer that partially
clears.
*/
template<typename Database>
[[maybe_unused]]
static void
clear_offer_partial(
	const Offer& offer, 
	const Price& sellPrice, 
	const Price& buyPrice,
	const uint8_t tax_rate,
	const FractionalAsset& remaining_to_clear, 
	Database& db,
	UserAccount* db_idx,
	int64_t& out_sell_amount, 
	int64_t& out_buy_amount) {

	auto buy_amount_fractional = FractionalAsset::from_raw(
		price::wide_multiply_val_by_a_over_b(
			remaining_to_clear.value, sellPrice, buyPrice));

	out_buy_amount = buy_amount_fractional.tax_and_round(tax_rate);
	out_sell_amount = remaining_to_clear.ceil();
	
	db.transfer_available(db_idx, offer.category.buyAsset, out_buy_amount,
		(make_offer_string(offer) + " clear partial sell_amount= " + std::to_string(out_sell_amount)).c_str());
}

} /* speedex */