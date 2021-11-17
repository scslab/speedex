#pragma once

#include "simplex/sparse.h"

#include <vector>
#include <unordered_set>

namespace speedex {

class ObjectiveRow {
	using int128_t = __int128;

	std::vector<int8_t> matrix_entries;

	//SparseTUColumn nz_hints;
	std::vector<uint16_t> nz_hints;

	int128_t row_value;

public:

	ObjectiveRow(size_t num_cols)
		: matrix_entries(num_cols, 0)
		, row_value(0)
		{
		}

	void subtract_sparse(SignedTURow const& row, size_t pivot_col) {

		//print();
		int8_t coeff = matrix_entries[pivot_col];

		row_value -= coeff * row.get_value();

		if (row.is_negated()) {
			coeff *= -1;
		}

		for (auto const& p : row.pos) {
			matrix_entries[p] -= coeff;
			if (matrix_entries[p] > 0 && matrix_entries[p] <= -coeff) {
				nz_hints.push_back(p);
			}
		}
		for (auto const& n : row.neg) {
			matrix_entries[n] += coeff;
			if (matrix_entries[n] > 0  && matrix_entries[n] <= coeff) {
				nz_hints.push_back(n);
			}
		}


		//print();
	}

	void subtract_sparse(SparseTURow const& row, size_t pivot_col) {
		int8_t coeff = matrix_entries[pivot_col];
		for (size_t p_ = row.pos.size(); p_ > 0; p_--) {
			size_t p = p_ - 1; 
		//for (size_t p = 0; p < row.pos.size(); p++) {
			matrix_entries[row.pos[p]] -= coeff;
			if (matrix_entries[row.pos[p]] > 0 && matrix_entries[row.pos[p]] <= -coeff) {
				nz_hints.push_back(row.pos[p]);
			}
		}
		for (size_t n_ = row.neg.size(); n_ > 0; n_--) {
			size_t n = n_ - 1;
		//for (size_t n = 0; n < row.neg.size(); n++) {
			matrix_entries[row.neg[n]] += coeff;
			if (matrix_entries[row.neg[n]] > 0  && matrix_entries[row.neg[n]] <= coeff) {
				nz_hints.push_back(row.neg[n]);
			}
		}
		row_value -= coeff * row.get_value();
	}

	template<typename ConstraintRow>
	void subtract(const ConstraintRow& row, size_t pivot_col) {
		int8_t coeff = matrix_entries[pivot_col];

		for (size_t i = 0; i < matrix_entries.size(); i++) {
			matrix_entries[i] -= coeff * row[i];
		}
		row_value -= coeff * row.get_value();
	}

	void delta_value(int128_t delta) {
		row_value += delta;
	}

	void set_value(int128_t val) {
		row_value = val;
	}

	void set_idx(size_t idx, int8_t value) {
		matrix_entries[idx] = value;
		if (value > 0) {
			nz_hints.push_back(idx);
		}
	}

	std::optional<uint16_t>
	get_next_pos() {
		while (true) {
			if (nz_hints.empty()) {
				return std::nullopt;
			}

			//uint16_t val = nz_hints.pop_front();

			//auto iter = nz_hints.begin();
			//uint16_t val = *iter;
			//nz_hints.erase(iter);

			uint16_t val = nz_hints.back();
			nz_hints.pop_back();
			if (matrix_entries[val] > 0) {
				return val;
			}
		}
	}

	//int8_t& operator[](size_t idx) {
	//	return matrix_entries[idx];
	//}

	const int8_t& operator[](size_t idx) const {
		return matrix_entries[idx];
	}

	const int128_t& get_value() {
		return row_value;
	}

	std::string to_string() const {
		std::string out;
		for (size_t i = 0u; i < matrix_entries.size(); i++) {
			std::string prefix = std::string(" ");
			if (matrix_entries[i] == -1) {
				prefix = std::string ("");
			}
			out += prefix + std::to_string(matrix_entries[i]) + std::string(" ");
		}
		out += std::to_string((double) row_value);
		return out;
	}

	void print() const {
		auto str = to_string();
		std::printf("\n%s\n", str.c_str());
	}
};

} /* speedex */
