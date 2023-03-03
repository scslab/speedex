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

/*! \file lp_solver.h

Utility methods for lp solving.

Given a set of prices, solve the trade-maximization lp (or check feasibility).
*/

#include <cstdint>
#include <memory>
#include <mutex>
#include <vector>

#include <glpk.h>

#include "orderbook/orderbook_manager.h"
#include "orderbook/offer_clearing_params.h"
#include "orderbook/utils.h"

#include "speedex/approximation_parameters.h"

#include "utils/fixed_point_value.h"
#include "utils/price.h"

#include "xdr/types.h"

namespace speedex {

/*! Convenience class around glpk structures for
one glpk problem instance.

Remembers to free problem when this object leaves scope.

Reuses structs from one round to the next.  Take care to call clear() before 
use.
*/

class LPInstance {

	int* ia;
	int* ja;
	double* ar;

	glp_prob* lp;
	
	const size_t nnz;

	LPInstance(const size_t nnz) : nnz(nnz) {
		ia = new int[nnz];
		ja = new int[nnz];
		ar = new double[nnz];
		lp = glp_create_prob();
	}

	void clear() {
		glp_erase_prob(lp);
	}

	friend class LPSolver;

public:

	~LPInstance() {
		delete[] ia;
		delete[] ja;
		delete[] ar;
		glp_delete_prob(lp);
	}
};

//! Lower and upper trade bounds for a given orderbook.
struct BoundsInfo {
	std::pair<uint64_t, uint64_t> bounds;
	OfferCategory category;
};

/*! Constructs and solves instances of the "trade-maximization" linear program.


GLPK is NOT threadsafe on its own.  This class is threadsafe.
*/
class LPSolver {

	OrderbookManager& manager;

	//! Add constraint to a problem stating bounding the trade volume
	//! from one asset to another.   Optionally drops the lower bound.
	void 
	add_orderbook_range_constraint(
		glp_prob* lp, 
		Orderbook& orderbook, 
		int idx, 
		Price* prices, 
		int *ia, int *ja, double *ar, 
		int& next_available_nnz,
		const ApproximationParameters approx_params,
		bool use_lower_bound);

	//! Adds trade volume constraint to problem, using precomputed
	//! bounds information.
	void
	add_orderbook_range_constraint(
		glp_prob* lp,
		BoundsInfo& bounds_info, 
		int idx, 
		Price* prices, 
		int *ia, int *ja, double *ar, 
		int& next_available_nnz,
		const uint8_t tax_rate,
		bool use_lower_bound);


	//! Get number of nnz values in lp.
	size_t get_nnz() const {
		return 1 + 2 * manager.get_orderbooks().size();
	}

	//! Computes the minimum tax rate required to clear one asset at
	//! a given supply/demand.  Throws error if tax rate
	//! would be more than one less than target_tax.
	uint8_t max_tax_param(
		FractionalAsset supply, 
		FractionalAsset demand, 
		const uint8_t target_tax);

	std::mutex mtx;

public:
	LPSolver(OrderbookManager& manager) : manager(manager) {}

	//! Solve the LP at input prices.
	ClearingParams 
	solve(
		Price* prices, 
		const ApproximationParameters approx_params, 
		bool use_lower_bound = true);
	
	//! Check LP feasibility at input prices.
	bool 
	check_feasibility(
		Price* prices, 
		std::unique_ptr<LPInstance>& instance, 
		const ApproximationParameters approx_params);


	//! for comparing LP speed ONLY
	bool
	unsafe_check_feasibility(
		Price* prices, 
		std::unique_ptr<LPInstance>& instance, 
		const ApproximationParameters approx_params,
		std::vector<BoundsInfo> & info,
		size_t num_assets);

	//! Produce a new lp solver instance.
	std::unique_ptr<LPInstance> make_instance() const;
};

}
