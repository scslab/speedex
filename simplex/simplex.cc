#include "simplex/simplex.h"

#include <map>
#include <set>

/*

Columns:
[y_{ij} for each orderbook (sell i, buy j)] [slack vars, one per pair ij] [slack vars, one per asset]

Constraint rows:
[asset conservation, one per asset
 \Sum_{j} y_ij \geq \Sum_j y_{ji} -- amount sold >= amount bought
 This translates to 
 \Sum_j y_ij == \Sum_j y_ji + slack_i
 or 
 \Sum_j y_ij - \Sum_j y_ji - slack_i == 0]
[flow constraint, one per orderbook
 orderbook amount + orderbook slack == orderbook max amount]

*/

namespace speedex {


std::optional<uint16_t>
SparseTUSimplex::get_next_pivot_column() {

	//std::printf("%s\n", objective_row.to_string().c_str());

	while(true) {
		auto res = objective_row.get_next_pos();
		if (!res) {
	//		std::printf("got no result from row\n");
			//break;;
			return res;
		}
		if (active_cols[*res]) {
	//		std::printf("got res of %u\n", *res);
			//break;
			return res;
		}
	} 

	for (auto i = 0u; i < num_cols; i++) {
		if (active_cols[i] && (objective_row[i] > 0)) {
			std::printf("actual returned val was %u\n", i);
			return {i};
		}
	}
	std::printf("returning nullopt\n");
	return std::nullopt; 
}

uint16_t 
SparseTUSimplex::get_next_pivot_row(uint16_t pivot_col) const {
	return tableau.get_pivot_row(pivot_col);
	/*std::optional<size_t> row_out = std::nullopt;

	auto const& col = tableau.cols[pivot_col];

	int128_t value = 0;

	for (auto row_idx : col.nonzeros) {
	//for (size_t i = 0; i < constraint_rows.size(); i++) {
		int128_t const& constraint_value = tableau.rows[row_idx].get_value();

		if (tableau.rows[row_idx][pivot_col] > 0) {
			if ((!row_out) || value > constraint_value) {
				row_out = row_idx;
				value = constraint_value;
			}
		}
	
	}
	if (row_out) {
		return *row_out;
	}
	throw std::runtime_error("failed to find pivot row");*/
}

bool
SparseTUSimplex::do_pivot() {

	//objective_row.print();
	//tableau.print("pre pivot");

	//tableau.integrity_check();

	auto pivot_col_idx = get_next_pivot_column();
	if (!pivot_col_idx) {
		return false;
	}

	auto pivot_row = get_next_pivot_row(*pivot_col_idx);

	//std::printf("pivot on (%u, %u)\n", pivot_row, *pivot_col_idx);

	/*{
		auto& pivot_constraint = constraint_rows[pivot_row];

		int8_t coefficient = pivot_constraint[*pivot_col_idx];

		if (coefficient < 0) {
			throw std::runtime_error("invalid pivot constr");
			pivot_constraint.negate();
		}
	}*/

	auto const& pivot_constraint = tableau.rows[pivot_row];

	tableau.do_pivot(pivot_row, *pivot_col_idx);

	//objective_row.print();
	//tableau.print("post pivot");
/*
	auto& pivot_col = constraint_columns[*pivot_col_idx];


	auto const& nnz_list = pivot_col.nonzeros;
	//for (auto iter = nnz_list.begin(); iter != nnz_list.end(); iter++) {
	for (auto nonzero_row : nnz_list) {
		//auto nonzero_row = *iter;
		if (nonzero_row != pivot_row) {
			auto& constraint = constraint_rows[nonzero_row];
			int8_t row_coeff = constraint[*pivot_col_idx];
			if (row_coeff == 0) {
				throw std::runtime_error("invalid nnz");
			}
			constraint.add(pivot_constraint, nonzero_row, -row_coeff, *pivot_col_idx, constraint_columns);
		}
	}
	pivot_col.set_singleton(pivot_row);
	*/

	//objective_row.subtract(pivot_constraint, *pivot_col_idx);
	objective_row.subtract_sparse(pivot_constraint, *pivot_col_idx);

	active_basis[pivot_row] = *pivot_col_idx;
	return true;
}

void 
SparseTUSimplex::set_entry(size_t row_idx, size_t col_idx, int8_t value) {
	auto& row = tableau.rows[row_idx];
	auto& col = tableau.cols[col_idx];
	row.set(col_idx, value);
	col.set(row_idx, value);
}
















void 
TaxFreeSimplex::add_asset_constraint(AssetID sell)
{
	constraint_rows.emplace_back(num_cols);
	auto& row = constraint_rows.back();

	row.set_pos(start_asset_slack_vars + sell);
	active_basis.push_back(start_asset_slack_vars + sell);

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

void 
TaxFreeSimplex::add_orderbook_constraint( const int128_t& value, const OfferCategory& category)
{
	constraint_rows.emplace_back(num_cols);
	auto& row = constraint_rows.back();

	row.set_value(value);

	auto idx = category_to_idx(category, num_assets);
	row.set_pos(idx);
	active_cols[idx] = true;
	row.set_pos(start_orderbook_slack_vars + idx);
	active_cols[start_orderbook_slack_vars + idx] = true;
	active_basis.push_back(start_orderbook_slack_vars + idx);

	objective_row.set_idx(idx, 1);

	active_cols[start_asset_slack_vars + category.sellAsset] = true;
	active_cols[start_asset_slack_vars + category.buyAsset] = true;
}

void
TaxFreeSimplex::solve() {
	run_simplex();
	construct_solution();
}

void
TaxFreeSimplex::construct_solution()
{
	std::vector<bool> solved_cols;
	solved_cols.resize(num_orderbooks, false);

	solution.clear();
	solution.resize(num_orderbooks);

	for (auto col = 0u; col < num_orderbooks; col++) {
		if (!active_cols[col]) {
			solved_cols[col] = true;
			solution[col] = 0;
		}
	}

	for (auto row = 0u; row < active_basis.size(); row++) {
		auto col = active_basis[row];

		if (col < num_orderbooks) {
			solution[col] = constraint_rows[row].get_value();
			solved_cols[col] = true;
		}
	}

	auto offercmp = [] (const OfferCategory& a, const OfferCategory& b) -> bool {
		return a.sellAsset == b.sellAsset ? (a.buyAsset < b.buyAsset) : a.sellAsset < b.sellAsset;
	};

	std::map<AssetID, std::set<OfferCategory, decltype(offercmp)>> unsolved_var_map;

	for (auto col = 0u; col < num_orderbooks; col++) {
		if (!solved_cols[col]) {
			auto category = category_from_idx(col, num_assets);
			unsolved_var_map[category.sellAsset].insert(category);
			unsolved_var_map[category.buyAsset].insert(category);
		}
	}

	// At an optimal BFS, every yij is either maxed out (and already solved)
	// or not maxed out, due to the capacity constraint.
	// The graph of non-maxed out edges has no cycles (by optimality)
	// This identifies which assets are blocked by capacity constraints,
	// and peels off solvable edges iteratively.
	while (!unsolved_var_map.empty()) {
		bool made_progress = false;
		for (auto iter = unsolved_var_map.begin(); iter != unsolved_var_map.end();)
		{
			if (iter->second.size() == 1) {


				auto const& category = *(iter -> second.begin());

				AssetID asset = iter -> first;

				// the solvable edge leaves the "asset" node.
				// compute the inflow, then subtract solved outflow
				OfferCategory category_in;
				category_in.type = OfferType::SELL;
				category_in.buyAsset = asset;
				int128_t inflow = 0;
				for (AssetID flow_in = 0u; flow_in < num_assets; flow_in++) {
					if (flow_in != asset && flow_in != category.sellAsset) {
						category_in.sellAsset = flow_in;
						auto idx = category_to_idx(category_in, num_assets);
						inflow += solution[idx];
					}
				}
				OfferCategory category_out;
				category_out.type = OfferType::SELL;
				category_out.sellAsset = asset;
				for (AssetID flow_out = 0u; flow_out < num_assets; flow_out++) {
					if (flow_out != asset && flow_out != category.buyAsset) {
						category_out.buyAsset = flow_out;
						auto idx = category_to_idx(category_out, num_assets);
						inflow -= solution[idx];
					}
				}


				auto solved_idx = category_to_idx(category, num_assets);
				solution[solved_idx] = inflow;
				solved_cols[solved_idx] = true;
				

				made_progress = true;
				iter = unsolved_var_map.erase(iter);
			} else {
				iter++;
			}
		}

		if (!made_progress) {
			throw std::runtime_error("stuck");
		}
	}
}

TaxFreeSimplex::int128_t 
TaxFreeSimplex::get_solution(const OfferCategory& category) {
	auto idx = category_to_idx(category, num_assets);
	return solution[idx];
}


std::optional<uint16_t> 
TUSimplex::get_next_pivot_column() const {
	for (auto i = 0u; i < num_cols; i++) {
		if (active_cols[i] && (objective_row[i] > 0)) {
			return {i};
		}
	}
	return std::nullopt;
}
uint16_t 
TUSimplex::get_next_pivot_row(uint16_t pivot_col) const {
	std::optional<size_t> row_out = std::nullopt;

	int128_t value = 0;

	for (size_t i = 0; i < constraint_rows.size(); i++) {
		int128_t const& constraint_value = constraint_rows[i].get_value();

		if (constraint_rows[i][pivot_col] > 0) {
			if ((!row_out) || value > constraint_value) {
				row_out = i;
				value = constraint_value;
			}
		}
	
	}
	if (row_out) {
		return *row_out;
	}
	throw std::runtime_error("failed to find pivot row");
}

bool 
TUSimplex::do_pivot() {
	auto pivot_col = get_next_pivot_column();
	if (!pivot_col) {
		return false;
	}

	auto pivot_row = get_next_pivot_row(*pivot_col);

	{
		auto& pivot_constraint = constraint_rows[pivot_row];

		int8_t coefficient = pivot_constraint[*pivot_col];

		if (coefficient < 0) {
			pivot_constraint.negate();
		}
	}

	auto const& pivot_constraint = constraint_rows[pivot_row];

	for (size_t i = 0; i < constraint_rows.size(); i++) {
		if (i != pivot_row) {
			auto& constraint = constraint_rows[i];
			int8_t row_coeff = constraint[*pivot_col];

			if (row_coeff < 0) {
				constraint += pivot_constraint;
			}
			else if (row_coeff > 0) {
				// x - y = -(-x + y)
				constraint.negate();
				constraint += pivot_constraint;
				constraint.negate();
			}
		}
	}

	objective_row.subtract(pivot_constraint, *pivot_col);

	active_basis[pivot_row] = *pivot_col;
	return true;
}

TUSimplex::TUSimplex(const uint16_t num_cols)
	: num_cols(num_cols)
	, objective_row(num_cols)
	, constraint_rows()
	, active_cols()
	, active_basis()
	{
		active_cols.resize(num_cols, false);
	}

void 
TUSimplex::run_simplex() {
	while (do_pivot()) {}
}

void 
SparseTUSimplex::run_simplex() {
	while (do_pivot()) {}
}


} /* speedex */
