#pragma once

namespace speedex {

class ObjectiveRow {
	using int128_t = __int128;

	std::vector<int8_t> matrix_entries;

	int128_t row_value;

public:

	ObjectiveRow(size_t num_cols)
		: matrix_entries()
		, row_value(0)
		{
			matrix_entries.resize(num_cols, 0);
		}

	template<typename ConstraintRow>
	void subtract(const ConstraintRow& row, size_t pivot_col) {
		int8_t coeff = matrix_entries[pivot_col];

		for (size_t i = 0; i < matrix_entries.size(); i++) {
			matrix_entries[i] -= coeff * row[i];
		}
	}

	int8_t& operator[](size_t idx) {
		return matrix_entries[idx];
	}

	const int8_t& operator[](size_t idx) const {
		return matrix_entries[idx];
	}

	std::string to_string() const {
		std::string out("objective:");
		for (size_t i = 0u; i < matrix_entries.size(); i++) {
			out += std::string(" ") + std::to_string(matrix_entries[i]);
		}
		return out;
	}
};

} /* speedex */
