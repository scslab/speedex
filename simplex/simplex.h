#pragma once

#include "orderbook/utils.h"

#include "simplex/bitcompressed_row.h"
#include "simplex/objective_row.h"

#include "xdr/types.h"

#include <optional>
#include <vector>

namespace speedex {


class TaxFreeSimplex {

	const size_t num_assets;
	const size_t num_orderbooks;
	const size_t start_orderbook_slack_vars;
	const size_t start_asset_slack_vars;
	const size_t num_cols;

	using constraint_row_t = BitcompressedRow;
	using int128_t = __int128;

	ObjectiveRow objective_row;
	std::vector<constraint_row_t> constraint_rows;
	std::vector<bool> active_cols;
	std::vector<uint16_t> active_basis; // maps row idx to col idx

	std::vector<int128_t> solution;

	void add_asset_constraint(AssetID sell);

	std::optional<size_t> get_next_pivot_column() const;
	size_t get_next_pivot_row(size_t pivot_col) const;
	bool do_pivot();
	void construct_solution();

	void print_row(const constraint_row_t& row) const {
		for (size_t i = 0u; i < num_cols; i++) {
			std::printf("%d ", row[i]);
		}
		std::printf("\n");
	}

	void print_tableau() const {
		std::printf("start tableau\n");
		for (auto const& constraint : constraint_rows) {
			print_row(constraint);
		}
	}

public:

	TaxFreeSimplex(const size_t num_assets)
		: num_assets(num_assets)
		, num_orderbooks(get_num_orderbooks_by_asset_count(num_assets))
		, start_orderbook_slack_vars(num_orderbooks)
		, start_asset_slack_vars(num_orderbooks + num_orderbooks)
		, num_cols(num_orderbooks + num_orderbooks + num_assets)
		, objective_row(num_cols)
		, constraint_rows()
		, active_cols()
		, active_basis()
		{
			active_cols.resize(num_cols, false);
			for (auto i = 0u; i < num_assets; i++) {
				add_asset_constraint(i);
			}
		}

	void add_orderbook_constraint(const int128_t& value, const OfferCategory& category);

	void solve();

	int128_t get_solution(const OfferCategory& category);
};


} /* speedex */
