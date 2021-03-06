#include "simplex/bitcompressed_row.h"

#include <cstdio>

namespace speedex {

void normalize(uint64_t& base) {
	constexpr uint64_t SMEAR_CONSTANT = 0x5555'5555'5555'5555;
	uint64_t adjust = ((base & (SMEAR_CONSTANT << 1)) >> 1) ^ (base & SMEAR_CONSTANT);
	adjust |= (adjust << 1);
	base &= adjust;
}

void add_bitwise(uint64_t& base, const uint64_t value) {
	
	base += value;
	normalize(base);
}

BitcompressedRow& 
BitcompressedRow::operator+=(const BitcompressedRow& other)
{
	uint64_t* data = matrix_entries.data();
	const uint64_t* other_data = other.matrix_entries.data();
	for (auto i = 0u; i < num_words; i++) {
		add_bitwise(data[i], other_data[i]);
	}
	row_value += other.row_value;
	return *this;
}

SparseRow&
SparseRow::operator+=(const SparseRow& other) {
	for (auto const& [word_idx, data] : other.entries) {
		add_bitwise(entries[word_idx], data);
	}
	row_value += other.row_value;
	return *this;
}

void 
BitcompressedRow::negate() {
	uint64_t* data = matrix_entries.data();
	for (auto i = 0u; i < num_words; i++) {
		data[i] = ~data[i];
		normalize(data[i]);
	}
	row_value *= -1;
}

void
SparseRow::negate() {
	for (auto& [_, word] : entries) {
		word = ~word;
		normalize(word);
	}
	row_value *= -1;
}

void
BitcompressedRow::set_pos(uint16_t idx) {
	uint16_t word_idx = idx / 32;
	uint16_t offset = idx % 32;

	uint64_t one_bit = static_cast<uint64_t>(1) << (2 * offset);
	matrix_entries[word_idx] |= one_bit;
}

void
SparseRow::set_pos(uint16_t idx) {
	uint16_t word_idx = idx / 32;
	uint16_t offset = idx % 32;

	uint64_t one_bit = static_cast<uint64_t>(1) << (2 * offset);
	entries[word_idx] |= one_bit;
}


void
BitcompressedRow::set_neg(uint16_t idx) {
	uint16_t word_idx = idx / 32;
	uint16_t offset = idx % 32;

	uint64_t neg_bit = static_cast<uint64_t>(1) << ((2 * offset) + 1);

	matrix_entries[word_idx] |= neg_bit;
}

void
SparseRow::set_neg(uint16_t idx) {
	uint16_t word_idx = idx / 32;
	uint16_t offset = idx % 32;

	uint64_t neg_bit = static_cast<uint64_t>(1) << ((2 * offset) + 1);

	entries[word_idx] |= neg_bit;
}

int8_t
BitcompressedRow::operator[](size_t idx) const
{
	uint16_t word_idx = idx / 32;
	uint16_t offset = idx % 32;

	uint64_t idx_val = (static_cast<uint64_t>(3) << (2 * offset)) & matrix_entries[word_idx];
	idx_val >>= (2 * offset);

	return idx_val == 0 ? 0 : (idx_val == 1 ? 1 : -1);
}

int8_t
SparseRow::operator[](size_t idx) const
{
	uint16_t word_idx = idx / 32;
	uint16_t offset = idx % 32;

	auto iter = entries.find(word_idx);
	if (iter == entries.end()) {
		return 0;
	}

	uint64_t idx_val = (static_cast<uint64_t>(3) << (2 * offset)) & (iter->second);
	idx_val >>= (2 * offset);

	return idx_val == 0 ? 0 : (idx_val == 1 ? 1 : -1);
}



} /* speedex */
