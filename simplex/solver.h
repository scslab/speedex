#pragma once

#include "simplex/simplex.h"

namespace speedex {

class LPSolver : public TUSimplex {
	const size_t num_assets;
	const size_t num_orderbooks;

	// var layout: [y_ij e_ij s_a t_a]
	const size_t start_orderbook_slack_vars;
	const size_t start_asset_slack_vars;
	const size_t start_feasibility_slack_vars;

	std::vector<int128_t> solution;

	// constraint layout:
	// <asset constraints>
	// <orderbook constraints>

	// asset constraints are BUY - SELL == 0
	void add_asset_constraint(AssetID sell);

	//void set_feasibility_objective_value();
	void set_feasibility_objective_coeffs();
	void normalize_asset_constraints();

	void adjust_asset_constraint(AssetID asset, int128_t amount);
	uint16_t get_slack_var_idx(AssetID asset);
	uint16_t get_feasibility_var_idx(AssetID asset);

	void set_asset_constraint_slacks_active(AssetID asset);

public:

	LPSolver(size_t _num_assets) 
		: TUSimplex(2 * get_num_orderbooks_by_asset_count(_num_assets) + 2 * _num_assets)
		, num_assets(_num_assets)
		, num_orderbooks(get_num_orderbooks_by_asset_count(num_assets))
		, start_orderbook_slack_vars(num_orderbooks)
		, start_asset_slack_vars(start_orderbook_slack_vars + num_orderbooks)
		, start_feasibility_slack_vars(start_asset_slack_vars + num_assets)
		, solution()
		{
			for (auto i = 0u; i < num_assets; i++) {
				add_asset_constraint(i);
			}
		}

	void add_orderbook_constraint(const int128_t& lower_bound, const int128_t& upper_bound, const OfferCategory& category);

	bool check_feasibility();
};

} /* speedex */
