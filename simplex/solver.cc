#include "simplex/solver.h"

namespace speedex {

uint16_t
SimplexLPSolver::get_slack_var_idx(AssetID asset) {
	return start_asset_slack_vars + asset;
}

uint16_t
SimplexLPSolver::get_feasibility_var_idx(AssetID asset) {
	return start_feasibility_slack_vars + asset;
}

void
SimplexLPSolver::set_feasibility_objective_coeffs() {
	for (uint16_t asset = 0; asset < num_assets; asset++) {
		objective_row.set_idx(get_feasibility_var_idx(asset), -1);
	}
}

void 
SimplexLPSolver::add_asset_constraint(AssetID sell)
{
	add_new_constraint_row();

	//auto& row = constraint_rows.back();

	size_t row_idx = sell;

	//row.set_pos(get_slack_var_idx(sell));
	//row.set_neg(get_feasibility_var_idx(sell));
	set_entry(row_idx, get_slack_var_idx(sell), 1);
	set_entry(row_idx, get_feasibility_var_idx(sell), -1);

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

			set_entry(row_idx, sell_buy_idx, -1);
			set_entry(row_idx, buy_sell_idx, 1);
			//row.set_neg(sell_buy_idx);
			//row.set_pos(buy_sell_idx);
		}
	}
}

void 
SimplexLPSolver::adjust_asset_constraint(AssetID asset, int128_t amount) {
	auto& row = tableau.rows[asset];

	row.set_value(row.get_value() + amount);

	if (row.get_value() < 0) {
		active_basis[asset] = get_feasibility_var_idx(asset);
	} else {
		active_basis[asset] = get_slack_var_idx(asset);
	}
}

void
SimplexLPSolver::set_asset_constraint_slacks_active(AssetID asset) {
	active_cols[get_slack_var_idx(asset)] = true;
	active_cols[get_feasibility_var_idx(asset)] = true;
}

void 
SimplexLPSolver::add_orderbook_constraint(const int128_t& lower_bound, const int128_t& upper_bound, const OfferCategory& category)
{
	if (lower_bound > upper_bound) {
		throw std::runtime_error("invalid bounds");
	}

	if (lower_bound == upper_bound) {
		std::printf("identical bounds, returning\n");
		return;
	}

	add_new_constraint_row();
	auto& row = tableau.rows.back();

	size_t row_idx = tableau.rows.size() - 1;

	row.set_value(upper_bound - lower_bound);

	auto idx = category_to_idx(category, num_assets);
	
	set_entry(row_idx, idx, 1);
	active_cols[idx] = true;
	set_entry(row_idx, start_orderbook_slack_vars + idx, 1);
	active_cols[start_orderbook_slack_vars + idx] = true;
	
	active_basis.push_back(start_orderbook_slack_vars + idx);

	set_asset_constraint_slacks_active(category.sellAsset);
	set_asset_constraint_slacks_active(category.buyAsset);

	adjust_asset_constraint(category.buyAsset, -lower_bound);
	adjust_asset_constraint(category.sellAsset, lower_bound);
}

void
SimplexLPSolver::normalize_asset_constraints() {
	for (uint16_t asset = 0; asset < num_assets; asset++) {
		auto& row = tableau.rows[asset];

		auto feasibility_idx = get_feasibility_var_idx(asset);

		if (active_basis[asset] == feasibility_idx) {

			row.negate();

			//std::printf("feas constr on %u\n", asset);
			//objective_row.print();
			//tableau.print("before subtract");

			//std::printf("row in question:\n");
			//tableau.print_row(asset);

			objective_row.subtract_sparse(row, feasibility_idx);

			//objective_row.print();
		}
	}
}

bool
SimplexLPSolver::check_feasibility() {
	set_feasibility_objective_coeffs();
	normalize_asset_constraints();
	run_simplex();
	return objective_row.get_value() == 0;
}

} /* speedex */
