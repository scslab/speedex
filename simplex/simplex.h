#pragma once

#include "orderbook/utils.h"

#include "simplex/bitcompressed_row.h"
#include "simplex/objective_row.h"
#include "simplex/sparse.h"

#include "xdr/types.h"

namespace speedex {

class SparseTUSimplex {
public:

	using int128_t = __int128;

protected:

	const uint16_t num_cols;

	//std::vector<SparseTURow> constraint_rows;
	//std::vector<SparseTUColumn> constraint_columns;

	SparseTableau tableau;

	ObjectiveRow objective_row;

	std::vector<bool> active_cols;
	std::vector<uint16_t> active_basis;

	SparseTUSimplex(size_t num_cols)
		: num_cols(num_cols)
		, tableau(num_cols)
		//, constraint_rows()
		//, constraint_columns(num_cols)
		, objective_row(num_cols)
		, active_cols(num_cols, false)
		, active_basis()
	{
	}

	std::optional<uint16_t> get_next_pivot_column();

	uint16_t get_next_pivot_row(uint16_t pivot_col) const;

	bool do_pivot();

	void set_entry(size_t row_idx, size_t col_idx, int8_t value);

	void run_simplex();

	void add_new_constraint_row() {
		tableau.add_row();
		//constraint_rows.emplace_back();
	}
};

// ub if constraint matrix isn't totally unimodular
class TUSimplex {

public:

	using constraint_row_t = BitcompressedRow;
	using int128_t = __int128;

protected:

	const uint16_t num_cols;

	ObjectiveRow objective_row;
	std::vector<constraint_row_t> constraint_rows;

	std::vector<bool> active_cols;
	std::vector<uint16_t> active_basis; // maps row idx to col idx

	std::optional<uint16_t> get_next_pivot_column() const;
	uint16_t get_next_pivot_row(uint16_t pivot_col) const;
	bool do_pivot();

	TUSimplex(const uint16_t num_cols);

	void run_simplex();

	void print_row(uint16_t row_idx) const {
		auto const& row = constraint_rows[row_idx];
		for (size_t i = 0u; i < num_cols; i++) {
			if (row[i] != -1) {
				std::printf(" ");
			}
			std::printf("%d ", row[i]);
		}
		std::printf("%lf\t%u\n", (double) row.get_value(), active_basis[row_idx]);
	}

	void print_tableau() const {
		std::printf("start tableau\n");
		auto obj_str = objective_row.to_string();
		std::printf("%s\n", obj_str.c_str());
		for (uint16_t row_idx = 0; row_idx < constraint_rows.size(); row_idx++) {
			print_row(row_idx);
		}
	}

	void set_entry(size_t row_idx, size_t col_idx, int8_t value) {
		if (value > 0) {
			constraint_rows[row_idx].set_pos(col_idx);
		} else {
			constraint_rows[row_idx].set_neg(col_idx);
		}
	}

	void add_new_constraint_row() {
		constraint_rows.emplace_back(num_cols);
	}


};

class TaxFreeSimplex : public TUSimplex {

	const size_t num_assets;
	const size_t num_orderbooks;

	// var layout: [y_ij e_ij s_a]
	const size_t start_orderbook_slack_vars;
	const size_t start_asset_slack_vars;

	std::vector<int128_t> solution;

	void add_asset_constraint(AssetID sell);

	void construct_solution();

public:

	TaxFreeSimplex(const size_t _num_assets)
		: TUSimplex(get_num_orderbooks_by_asset_count(_num_assets) * 2 + _num_assets)
		, num_assets(_num_assets)
		, num_orderbooks(get_num_orderbooks_by_asset_count(num_assets))
		, start_orderbook_slack_vars(num_orderbooks)
		, start_asset_slack_vars(num_orderbooks + num_orderbooks)
		{
			for (auto i = 0u; i < num_assets; i++) {
				add_asset_constraint(i);
			}
		}

	void add_orderbook_constraint(const int128_t& value, const OfferCategory& category);

	void solve();

	int128_t get_solution(const OfferCategory& category);
};

} /* speedex */
