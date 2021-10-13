#pragma once

#include "utils/big_endian.h"

#include <compare>
#include <cstdint>
#include <vector>

namespace speedex {
/*!

Bitvector template of varying size

*/
namespace detail {
template<typename uint_type>
struct BVManipFns {
};

template<>
struct BVManipFns<uint16_t> {

	BVManipFns() = delete;

	static Constexpr uint8_t get_lowest(uint16_t bv) {
		return __builtin_ffs(bv) - 1;
	}

	static Constexpr uint8_t size(uint16_t bv) {
		return __builtin_popcount(bv);
	}

	static Constexpr size_t size_in_bytes = 2;

	static Constexpr uint16_t max_value = UINT16_MAX;
};

template<>
struct BVManipFns<uint32_t> {

	BVManipFns() = delete;

	static Constexpr uint8_t get_lowest(uint32_t bv) {
		return __builtin_ffsl(bv) - 1;
	}

	static Constexpr uint8_t size(uint32_t bv) {
		return __builtin_popcountl(bv);
	}

	static Constexpr size_t size_in_bytes = 4;

	static Constexpr uint32_t max_value = UINT32_MAX;
};

template<>
struct BVManipFns<uint64_t> {

	BVManipFns() = delete;

	static Constexpr uint8_t get_lowest(uint64_t bv) {
		return __builtin_ffsll(bv) - 1;
	}

	static Constexpr uint8_t size(uint64_t bv) {
		return __builtin_popcountll(bv);
	}

	static Constexpr size_t size_in_bytes = 8;

	static Constexpr uint64_t max_value = UINT64_MAX;
};

} /* detail */

template<typename uint_type>
class BitVector {
	uint_type bv;

public:
	BitVector(uint_type bv) : bv(bv) {}
	BitVector() : bv(0) {}

	void add(uint8_t loc) {
		bv |= static_cast<uint_type>(1) << loc;
	}

	//pops lowest from bv
	// ub if bv is empty
	uint8_t pop() {
		uint8_t loc = detail::BVManipFns<uint_type>::get_lowest(bv);
		bv -= static_cast<uint_type>(1) << loc;
		return loc;
	}

	void erase(uint8_t loc) {
		bv &= (~(static_cast<uint_type>(1) << loc));
	}

	uint8_t lowest() const {
		return detail::BVManipFns<uint_type>::get_lowest(bv);
	}

	uint8_t size() const {
		return detail::BVManipFns<uint_type>::size(bv);
	}

	constexpr uint8_t needed_bytes() {
		return detail::BVManipFns<uint_type>::size_in_bytes;
	}

	void write(std::vector<uint8_t>& vec) {
		append_unsigned_big_endian(vec, bv);
	}

	bool contains(uint8_t loc) const {
		return ((static_cast<uint_type>(1) << loc) & bv) != 0;
	}

	//! drop all entries below the input value
	BitVector drop_lt(uint8_t lowest_remaining) const {
		return BitVector{
			static_cast<uint_type>(((detail::BVManipFns<uint_type>::max_value) << lowest_remaining) & bv)
		};
	}

	bool empty() const {
		return bv == 0;
	}

	std::strong_ordering operator<=>(const BitVector& other) const {
		return bv <=> other.bv;
	}

	bool operator==(const BitVector& other) const = default;

	void clear() {
		bv = 0;
	}

	uint_type get() const {
		return bv;
	}
};

} /* speedex */
