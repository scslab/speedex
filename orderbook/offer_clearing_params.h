#pragma once

/*! \file offer_clearing_params.h

Record for each orderbook the amount of supply
that trades.
*/

#include <cstdint>
#include <vector>

#include "utils/fixed_point_value.h"

#include "xdr/types.h"

namespace speedex {

/*! Specify how much of the available supply in one orderbook
is activated.
*/
struct OrderbookClearingParams {
	FractionalAsset supply_activated;
};

struct ClearingParams {
	uint8_t tax_rate;
	std::vector<OrderbookClearingParams> orderbook_params;

	//! Check whether a given set of prices clears the market with
	//! these clearing params (i.e. supply exceeds taxed demand).
	bool check_clearing(const std::vector<Price>& prices) const;
};

} /* speedex */