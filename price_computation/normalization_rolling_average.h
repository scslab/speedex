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

/*! \file normalization_rolling_average.h

Stores a running average of the (price-weighted) trade volumes
for use as a tatonnement preconditioner.
*/

#include <cmath>
#include <cstdint>

#include "orderbook/offer_clearing_params.h"
#include "orderbook/utils.h"

#include "utils/fixed_point_value.h"

namespace speedex {

/*! Track a rolling average of trade volumes as a tatonnement preconditioning
heuristic.

None of these numbers are sent to consensus, so floating-point error is 
not a concern.  It's a rough heuristic.  Future work is too improve
this preconditioning.
*/
class NormalizationRollingAverage {

	/*! For numerical precision, we use RELATIVE_VOLUME_BASEPOINT
	as the "1" in our calculations.  If all assets traded at the
	same volume, then all rolling averages would be RELATIVE_VOLUME_BASEPOINT

	Specifically, in every round, we compute the max traded asset,
	and then the factor for the asset is its volume relative to the
	maximum (multiplied by RELATIVE_VOLUME_BASEPT).
	*/
	constexpr static uint16_t RELATIVE_VOLUME_BASEPT = 16;

	//! Max ratio we can handle using 16 bit outputs
	constexpr static double MAX_RELATIVE_VOLUME 
		= ((double) UINT16_MAX) / ((double) RELATIVE_VOLUME_BASEPT);

	//! The number of assets tracked.
	const size_t num_assets;

	//! For convenience, store rolling averages internally as doubles.
	double* rolling_averages;

	//! Rolling averages for use in Tatonnement.  These
	//! will be kept as rolling_averages * RELATIVE_VOLUME_BASEPT.
	uint16_t* formatted_rolling_avgs;

	//! Rolling averages are a weighted geometric mean
	//! keep_amt is the weight of the previous value
	constexpr static double keep_amt = 1.0/2.0; // 2/3 is good too, possibly
	//! weight of the new value in the rolling average calculation
	constexpr static double new_amt = 1.0 - keep_amt;

	//! Keep formatted_rolling_avgs in sync with rolling_avgs
	void update_formatted_avgs();

	//! Calculate relative volume for one asset.
	//! Supply should be nonzero.
	static double
	relative_volume_calc(
		const FractionalAsset& max, 
		const FractionalAsset& supply);

	//! Update running average with new relative volumes
	void add_to_average(double* current_normalizers);

public:
	//! init rolling average calculation tracking a given number of assets.
	NormalizationRollingAverage(size_t num_assets)
		: num_assets(num_assets) {
			rolling_averages = new double[num_assets];
			formatted_rolling_avgs = new uint16_t[num_assets];
			for (size_t i = 0; i < num_assets; i++) {
				rolling_averages[i] = 1.0;
			}
		}

	~NormalizationRollingAverage() {
		delete[] formatted_rolling_avgs;
		delete[] rolling_averages;
	}

	//! Returns preconditioning data for Tatonnement.
	const uint16_t* get_formatted_avgs() {
		return formatted_rolling_avgs;
	}

	//! Update rolling averages with new clearing information
	void update_averages(const ClearingParams& params, const Price* prices);
};

	
} /* speedex */

