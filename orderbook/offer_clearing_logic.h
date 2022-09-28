#pragma once

#include <cstdint>

#include "utils/fixed_point_value.h"
#include "utils/price.h"

#include "xdr/types.h"

namespace speedex {

class UserAccount;

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
		buy_amount_fractional.tax_and_round(tax_rate));
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
	
	db.transfer_available(db_idx, offer.category.buyAsset, out_buy_amount);
}

} /* speedex */