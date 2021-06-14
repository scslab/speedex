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

		supplies[category.sellAsset] += supply_activated;
		auto demanded = price::wide_multiply_val_by_a_over_b(
			supply_activated.value, 
			prices[category.sellAsset], 
			prices[category.buyAsset]);
		demands[category.buyAsset] += FractionalAsset::from_raw(demanded);
	}

	CLEARING_INFO("rounded asset results");
	CLEARING_INFO("Asset\tsupply\tdemand\tprice");
	for (unsigned int i = 0; i < num_assets; i++) {
		FractionalAsset taxed_demand = demands[i].tax(tax_rate);
		if (supplies[i] < taxed_demand) {
			CLEARING_INFO("failed on %d %f %f", 
				i, supplies[i].to_double(), taxed_demand.to_double());
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