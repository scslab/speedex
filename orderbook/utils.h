#pragma once

/*! \file utils.h

Miscellaneous utility functions for working with orderbooks.
*/

#include "xdr/types.h"

namespace speedex {


//! Map an offer type to an integer.
//! In theory, with many offer types, we would have one orderbook
//! per (sell asset, buy asset, type) tuple, and our indexing functions
//! support this, but only one offer type is implemented at the moment.
static int map_type_to_int(OfferType type) {
	switch(type) {
		case SELL:
			return 0;
	}
	throw std::runtime_error("invalid offer type");
}

//! int to offer type.  For the inverse mapping, also not currently useful.
static OfferType map_int_to_type(int type) {
	switch(type) {
		case 0:
			return OfferType::SELL;
	}
	return static_cast<OfferType>(-1);
}

//! Map an offer category to an orderbook index
[[maybe_unused]]
static int 
category_to_idx(
	const OfferCategory& id, const unsigned int asset_count) {

	if (id.sellAsset >= asset_count || id.buyAsset >= asset_count) {
		std::printf(
			"asset_count %u sellAsset %u buyAsset %u\n", 
			asset_count, id.sellAsset, id.buyAsset);
		throw std::runtime_error("invalid asset number");
	}
	int units_per_order_type = asset_count * (asset_count - 1);
	int idx_in_order_type 
		= id.sellAsset * (asset_count - 1) 
		+ (id.buyAsset - (id.buyAsset > id.sellAsset? 1:0));

	int idx_out 
		= units_per_order_type * map_type_to_int(id.type) + idx_in_order_type;

	return idx_out;
}

//! Map an offer index to a (sell asset, buy asset) tuple.
[[maybe_unused]]
static OfferCategory category_from_idx(const int idx, const int asset_count) {
	int units_per_order_type = asset_count * (asset_count - 1);
	OfferType type = map_int_to_type(idx/units_per_order_type);
	int remainder = idx % units_per_order_type;

	AssetID sell_asset = remainder / (asset_count - 1);
	AssetID buy_asset = remainder % (asset_count - 1);
	if (buy_asset >= sell_asset) {
		buy_asset += 1;
	}
	
	OfferCategory output;
	output.type = type;
	output.buyAsset = buy_asset;
	output.sellAsset = sell_asset;
	return output;
}

//! Validate than an offer category is well-formed.
[[maybe_unused]]
static bool validate_category_(
	const OfferCategory& id, const unsigned int asset_count) 
{
	if (id.sellAsset == id.buyAsset) return false;
	if (id.sellAsset >= asset_count || id.buyAsset >= asset_count) return false;
	if (id.type != OfferType::SELL) return false;
	return true;
}

//! Get the total number of valid (sell, buy) pairs.
[[maybe_unused]]
static unsigned int 
get_num_orderbooks_by_asset_count(unsigned int asset_count) {
	return NUM_OFFER_TYPES * (asset_count * (asset_count - 1));
}


} /* speedex */