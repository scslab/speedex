#include "price_computation/normalization_rolling_average.h"

namespace speedex {

double
NormalizationRollingAverage::relative_volume_calc(
	const FractionalAsset& max, 
	const FractionalAsset& supply) {
	double candidate_out = max.to_double() / supply.to_double();
	if (candidate_out >= MAX_RELATIVE_VOLUME) {
		return MAX_RELATIVE_VOLUME;
	}
	return candidate_out;
}


void 
NormalizationRollingAverage::update_formatted_avgs() {
	for (size_t i = 0; i < num_assets; i++) {
		formatted_rolling_avgs[i] 
			= RELATIVE_VOLUME_BASEPT * rolling_averages[i];
		if (formatted_rolling_avgs[i] == 0) {
			formatted_rolling_avgs[i] = 1;
		}
		if (rolling_averages[i] >= UINT16_MAX / RELATIVE_VOLUME_BASEPT) {
			formatted_rolling_avgs[i] = UINT16_MAX;
		}
	}
}

void 
NormalizationRollingAverage::add_to_average(double* current_normalizers) {
	for (size_t i = 0; i < num_assets; i++) {
		rolling_averages[i] 
			= std::pow(rolling_averages[i], keep_amt) 
				* std::pow(current_normalizers[i], new_amt);
	}
	update_formatted_avgs();
}

void 
NormalizationRollingAverage::update_averages(
	const ClearingParams& params, const Price* prices) {

	auto num_orderbooks = params.orderbook_params.size();

	FractionalAsset supplies[num_assets];

	for (size_t i = 0; i < num_orderbooks; i++) {
		auto category = category_from_idx(i, num_assets);
		supplies[category.sellAsset] 
			+= params.orderbook_params[i].supply_activated 
				* prices[category.sellAsset];
	}

	FractionalAsset avg, max;
	double new_factors[num_assets];

	for (size_t i = 0; i < num_assets; i++) {
		max = std::max(max, supplies[i]);
		avg += supplies[i];
	}

	avg.value /= num_assets;

	for (size_t i = 0; i < num_assets; i++) {
		if (supplies[i].value > 0) {
			new_factors[i] = relative_volume_calc(max, supplies[i]);
		} else {
			new_factors[i] = relative_volume_calc(max, avg);
		}
	}

	add_to_average(new_factors);
}

} /* speedex */