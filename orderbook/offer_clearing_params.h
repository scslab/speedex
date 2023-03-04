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

	static ClearingParams
	get_null_clearing(uint8_t tax_rate, size_t num_orderbooks)
	{
		ClearingParams out;
		out.tax_rate = tax_rate;
		out.orderbook_params.resize(num_orderbooks);
		return out;
	}
};

} /* speedex */