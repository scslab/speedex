#include "simplex/solver.h"

namespace speedex {

uint16_t
LPSolver::get_slack_var_idx(AssetID asset) {
	return start_asset_slack_vars + asset;
}

uint16_t
LPSolver::get_feasibility_var_idx(AssetID asset) {
	return start_feasibility_slack_vars + asset;
}

void
LPSolver::set_feasibility_objective_coeffs() {
	for (uint16_t asset = 0; asset < num_assets; asset++) {
		objective_row[get_feasibility_var_idx(asset)] = -1;
	}
}

void 
LPSolver::add_asset_constraint(AssetID sell)
{
	constraint_rows.emplace_back(num_cols);
	auto& row = constraint_rows.back();

	row.set_pos(get_slack_var_idx(sell));
	row.set_neg(get_feasibility_var_idx(sell));
	active_basis.push_back(get_slack_var_idx(sell)); // by default, start with slack var active.
	//This will change if the feasibility slack becomes necessary

	OfferCategory category;
	category.type = OfferType::SELL;
	for (AssetID buy = 0; buy < num_assets; buy++) {
		if (buy != sell) {
			category.sellAsset = sell;
			category.buyAsset = buy;
			auto sell_buy_idx = category_to_idx(category, num_assets);
			category.sellAsset = buy;
			category.buyAsset = sell;
			auto buy_sell_idx = category_to_idx(category, num_assets);
			row.set_neg(sell_buy_idx);
			row.set_pos(buy_sell_idx);
		}
	}
}

void LPSolver::adjust_asset_constraint(AssetID asset, int128_t amount) {
	auto& row = constraint_rows[asset];

	row.set_value(row.get_value() + amount);

	if (row.get_value() < 0) {
		active_basis[asset] = get_feasibility_var_idx(asset);
	} else {
		active_basis[asset] = get_slack_var_idx(asset);
	}
}

void
LPSolver::set_asset_constraint_slacks_active(AssetID asset) {
	active_cols[get_slack_var_idx(asset)] = true;
	active_cols[get_feasibility_var_idx(asset)] = true;
}

void 
LPSolver::add_orderbook_constraint(const int128_t& lower_bound, const int128_t& upper_bound, const OfferCategory& category)
{
	constraint_rows.emplace_back(num_cols);
	auto& row = constraint_rows.back();

	row.set_value(upper_bound - lower_bound);

	auto idx = category_to_idx(category, num_assets);
	
	row.set_pos(idx);
	active_cols[idx] = true;
	row.set_pos(start_orderbook_slack_vars + idx);
	active_cols[start_orderbook_slack_vars + idx] = true;
	
	active_basis.push_back(start_orderbook_slack_vars + idx);

	set_asset_constraint_slacks_active(category.sellAsset);
	set_asset_constraint_slacks_active(category.buyAsset);

	adjust_asset_constraint(category.buyAsset, -lower_bound);
	adjust_asset_constraint(category.sellAsset, lower_bound);
}
/*
void
LPSolver::set_feasibility_objective_value() {
	objective_row.set_value(0);
	for (uint16_t asset = 0; asset < num_assets; asset++) {
		auto& row = constraint_rows[asset];

		if (active_basis[asset] == get_feasibility_var_idx(asset)) {
			objective_row.delta_value(row.get_value());
		}
	} 
} */

void
LPSolver::normalize_asset_constraints() {
	for (uint16_t asset = 0; asset < num_assets; asset++) {
		auto& row = constraint_rows[asset];

		auto feasibility_idx = get_feasibility_var_idx(asset);

		if (active_basis[asset] == feasibility_idx) {

			row.negate();

			objective_row.subtract(row, feasibility_idx);
		}
	}
}

bool
LPSolver::check_feasibility() {
	set_feasibility_objective_coeffs();
	normalize_asset_constraints();
	run_simplex();
	return objective_row.get_value() == 0;
}


} /* speedex */
