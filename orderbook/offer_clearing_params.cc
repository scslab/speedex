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

#include "orderbook/offer_clearing_params.h"

#include "orderbook/utils.h"

#include "utils/debug_macros.h"
#include "utils/price.h"

namespace speedex {


bool 
ClearingParams::check_clearing(const std::vector<Price>& prices) const {

	auto num_assets = prices.size();

	auto num_orderbooks = orderbook_params.size();

	FractionalAsset supplies[num_assets], demands[num_assets];

	CLEARING_INFO("tax rate:%u", tax_rate);


	for (unsigned int i = 0; i < num_orderbooks; i++) {
		auto category = category_from_idx(i, num_assets);
		auto& supply_activated = orderbook_params[i].supply_activated;

		supplies[category.sellAsset] += FractionalAsset::from_integral(supply_activated.ceil());
		auto demanded = price::wide_multiply_val_by_a_over_b(
			supply_activated.value, 
			prices[category.sellAsset], 
			prices[category.buyAsset]);
		demands[category.buyAsset] += FractionalAsset::from_raw(demanded);
	}

	CLEARING_INFO("rounded asset results");
	CLEARING_INFO("Asset\tsupply\tdemand\tprice");
	for (unsigned int i = 0; i < num_assets; i++) {
		//FractionalAsset taxed_demand = demands[i].tax_and_round(tax_rate);
		FractionalAsset taxed_demand = FractionalAsset::from_integral(demands[i].tax_and_round(tax_rate));
		if (supplies[i] < taxed_demand) {
			std::printf("failed on %d %f %f (delta %f)\n", 
				i, supplies[i].to_double(), taxed_demand.to_double(), (supplies[i]-taxed_demand).to_double());
			return false;
		}
		CLEARING_INFO("%d %f %f %f", 
			i, 
			supplies[i].to_double(), 
			taxed_demand.to_double(), 
			price::to_double(prices[i]));
	}

	return true;
}


} /* speedex */