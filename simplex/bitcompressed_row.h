#pragma once

#include <cstdint>
#include <vector>

namespace speedex {

class BitcompressedRow {

	using int128_t = __int128;
	
	const uint16_t num_words;

	std::vector<uint64_t> matrix_entries;

	int128_t row_value;

public:

	BitcompressedRow(size_t num_cols)
		: num_words(num_cols % 32 == 0 ? num_cols / 32 : 1 + (num_cols / 32))
		, matrix_entries()
		, row_value(0)
		{
			matrix_entries.resize(num_words, 0);
		}

	BitcompressedRow& operator+=(const BitcompressedRow& other);

	void negate();

	void set_pos(uint16_t idx);
	void set_neg(uint16_t idx);

	void set_value(const int128_t& value) {
		row_value = value;
	}
	const int128_t& get_value() const {
		return row_value;
	}

	int8_t operator[](size_t idx) const;
};

} /* speedex */
