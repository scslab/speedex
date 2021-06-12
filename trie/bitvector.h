#pragma once

#include "utils/big_endian.h"

namespace speedex {
/*!

Bitvector of size 16.  Used for storing which of a
list of trie node children pointers are actove.

*/
class BitVector {
	uint16_t bv = 0;
public:

	BitVector(uint16_t bv) : bv(bv) {}
	BitVector() : bv(0) {}

	void add(unsigned char branch_bits) {
		bv |= ((uint16_t)1)<<branch_bits;
	}
	//! Pop lowest valued bit from bitvector.
	//! Invalid results if bitvector is empty.
	unsigned char pop() {
		unsigned char loc = __builtin_ffs(bv) - 1;
		bv -= ((uint16_t)1)<<loc;
		return loc;
	}

	void erase(uint8_t loc) {
		bv &= (~(((uint16_t)1)<<loc));
	}

	unsigned char lowest() const {
		unsigned char loc = __builtin_ffs(bv) - 1;
		return loc;
	}
	unsigned int size() const {
		return __builtin_popcount(bv);
	}
	//! Number of bytes needed to write bitvector (used in trie proofs)
	constexpr unsigned int needed_bytes() {
		return 2;
	}
	void write_to(unsigned char* ptr) {
		write_unsigned_big_endian(ptr, bv);
	}
	void write(std::vector<unsigned char>& vec) {
		append_unsigned_big_endian(vec, bv);
	}

	bool contains(uint8_t loc) const {
		return ((((uint16_t)1)<<loc) & bv) != 0;
	}

	//! Drop all entries below the input value.
	BitVector drop_lt(uint8_t bb) const {
		uint16_t out = (UINT16_MAX << bb) & bv;
		return BitVector{out};
	}

	bool empty() const {
		return bv == 0;
	}

	bool operator==(const BitVector& other) const {
		return bv == other.bv;
	}

	void clear() {
		bv = 0;
	}

	uint16_t get() const {
		return bv;
	}
};
}