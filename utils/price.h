#pragma once


/*! \file price.h Various utility functions for working with fixed-point price values.

	Prices stored using PRICE_BYTES many bytes.  The real value signified
	(by the underlying integral value) is 2^(value) / 2^(PRICE_RADIX).
	
*/

#include <concepts>
#include <cmath>
#include "xdr/types.h"


namespace speedex {

typedef __int128 int128_t;
typedef unsigned __int128 uint128_t;

namespace price {

namespace {
constexpr static uint64_t price_lowbits_mask = 0x0000'00FF'FFFF;
constexpr static uint64_t price_highbits_mask = 0xFFFF'FF00'0000;
}

//! Number of bits below the decimal point
constexpr static uint8_t PRICE_RADIX = 24;
//! Number of bits to represent a price value.
constexpr static uint8_t PRICE_BIT_LEN = 2 * PRICE_RADIX;
//! Number of bytes to represent a price value.
constexpr static uint8_t PRICE_BYTES = (PRICE_BIT_LEN) / 8;

static_assert(PRICE_BYTES * 8 == PRICE_BIT_LEN, 
	"using a non-integral number of bytes complicates the arithmetic");

//! Maximum value which a price can take.
constexpr static Price MAX_PRICE = (((uint64_t) 1) << (PRICE_BIT_LEN)) - 1; // 0xffffff.ffffff

static_assert(PRICE_RADIX % 4 == 0, 
	"be warned that making price len a fractional number of bytes makes working with prices harder");

//! The price value that corresponds to a double 1.0.
constexpr static Price PRICE_ONE = ((uint64_t)1)<<PRICE_RADIX;

//! Fixed-point representation of a price to double representation
inline static double 
to_double(const Price& price) {
	return (double)price / (double) ((uint64_t)1<<PRICE_RADIX);
}

[[maybe_unused]]
inline static double
amount_times_price_to_double(const uint128_t& amount_times_price)
{
	return ((double) amount_times_price) /  ((double) ((uint64_t)1<<PRICE_RADIX));
}

//! Double representation of a price to a fixed-point representation
//! (rounding away the low bits from the double).
inline static Price 
from_double(const double price_d) {
	return (uint64_t)(price_d * (((uint64_t)1)<<PRICE_RADIX));
}

//! Writes a price (in big endian) to \a buf.
//! Uses PRICE_BYTES bytes in buf.
template<typename ArrayType>
inline static void 
write_price_big_endian(ArrayType& buf, const Price& price) {

	for (uint8_t loc = 0; loc < PRICE_BYTES; loc++) {
		uint8_t offset = ((PRICE_BYTES - loc - 1) * 8);
		buf.at(loc) = (price >> offset) & 0xFF;
	}
}

//! Writes a price (in big endian) to \a buf.
//! Overwrites from buf to buf + (PRICE_BYTES -1), inclusive.
inline static void 
write_price_big_endian(unsigned char* buf, const Price& price) {
	for (uint8_t loc = 0; loc < PRICE_BYTES; loc++) {
		uint8_t offset = ((PRICE_BYTES - loc - 1) * 8);
		buf[loc] = (price >> offset) & 0xFF;
	}
}

//! Constrains a uint128 value to lie between 1 and MAX_PRICE.
//! Does not do any radix conversions.
inline static Price 
impose_price_bounds(const uint128_t& val) {
	if (val > MAX_PRICE) {
		return MAX_PRICE;
	}
	if (val == 0) {
		return 1;
	}
	return val;
}

//! Checks whether the input is within the bounds of a valid
//! price value.
inline static bool 
is_valid_price(const Price& price) {
	return price <= MAX_PRICE && price != 0;
}

//! Read in price from array, which should be in a big-endian format.
template<size_t ARRAY_LEN>
inline static Price 
read_price_big_endian(const std::array<unsigned char, ARRAY_LEN>& buf) {
	static_assert(ARRAY_LEN >= PRICE_BYTES, "not enough bytes to read price");
	return read_price_big_endian(buf.data());
}

//! Read in price from array, which should be in big-endian format.
inline static Price 
read_price_big_endian(const unsigned char* buf) {
	Price p = 0;
	for (uint8_t loc = 0; loc < PRICE_BYTES; loc++) {
		p <<=8;
		p += buf[loc];
	}
	return p;
}

//! Read in price from an array, which should be in big-endian format.
//! Should work with any type that has an operator[] and accepts
//! operator[0] through operator[PRICE_BYTES - 1]
template<typename ArrayType>
inline static Price 
read_price_big_endian(const ArrayType& buf) {
	Price p = 0;
	for (uint8_t loc = 0; loc < PRICE_BYTES; loc++) {
		p <<= 8;
		p += buf[loc];
	}
	return p;
}

//! Convert a uint128 amount to a double, viewing the amount
//! as a fixed-point fraction with a given radix.
inline static double 
amount_to_double(const uint128_t& value, unsigned int radix = 0) {
	uint64_t top = value >> 64;
	uint64_t bot = value & UINT64_MAX;
	return ((((double) top * (((double) (UINT64_MAX)) + 1)) + ((double) bot))) / (double) (((uint128_t)1)<<radix);
}


//! Convert a tax rate (stored as a -log_2(real rate)) to its
//! double-value equivalent.
inline static double 
tax_to_double(const uint8_t tax_rate) {
	double tax_d = tax_rate;
	return (1.0) - std::exp2f(-tax_d);
}


//! subtract 1/2^smooth_mult
inline static Price 
smooth_mult(const Price& price, const uint8_t smooth_mult) {
	return (price - (price>>smooth_mult));
}

//! Decide if price a / price b <= price c.
//! Save from overflows.
inline static bool 
a_over_b_leq_c(const Price& a, const Price& b, const Price& c) {
	return (((uint128_t)a)<<PRICE_RADIX) <= ((uint128_t) b) * ((uint128_t) c);
}

//! Decide if price a / price b < price c.
//! Save from overflows.
inline static bool 
a_over_b_lt_c(const Price& a, const Price& b, const Price &c) {
	return (((uint128_t)a)<<PRICE_RADIX) < ((uint128_t) b) * ((uint128_t) c);
}

//! Multiply some value by price a / price b.  Save from overflows on the
//! multiplication, assuming the result can fit into a 128 bit output.
inline static uint128_t 
wide_multiply_val_by_a_over_b(
	const uint128_t value, const Price& a, const Price& b) 
{
	uint128_t denom = b;
	uint128_t numer = a;
	uint128_t modulo = (value/denom) * numer;
	uint128_t remainder = ((value % denom) * numer) / denom;
	return modulo + remainder;
}

//! it's not 100% accurate - there's some carries that get lost, but ah well.
//! Only use case is computing volume heuristics for Tatonnement,
//! so small errors are not a problem.
inline static Price 
safe_multiply_and_drop_lowbits(
	const uint128_t& first, 
	const uint128_t& second, 
	const uint64_t& lowbits_to_drop) {

	if (lowbits_to_drop < 64 || lowbits_to_drop > 196) {
		throw std::runtime_error("unimplemented");
	}

	uint128_t first_low = first & UINT64_MAX;
	uint128_t first_high = (first >> 64) & UINT64_MAX;
	uint128_t second_low = second & UINT64_MAX;
	uint128_t second_high = (second >> 64) & UINT64_MAX;

	uint128_t low_low = first_low * second_low;

	uint128_t low_high = first_low * second_high;
	uint128_t high_low = first_high * second_low;
	uint128_t high_high = first_high * second_high;

	uint128_t out = 0;

	if (lowbits_to_drop < 128) {
		out += (low_low >> lowbits_to_drop);
	}
	uint64_t lowhigh_offset = lowbits_to_drop - 64;

	out += (low_high >> lowhigh_offset) + (high_low >> lowhigh_offset);

	if (lowbits_to_drop <= 128) {

		uint64_t used_lowbits = 128-lowbits_to_drop;
		uint64_t used_highbits 
			= (used_lowbits <= PRICE_BIT_LEN) ? PRICE_BIT_LEN - used_lowbits : 0;

		uint64_t max_highbits = (((uint64_t)1) << used_highbits) - 1;
		
		if (high_high > max_highbits) {
			high_high = max_highbits;
		}

		out += (high_high << (used_lowbits));

		if (out > MAX_PRICE) {
			out = MAX_PRICE;
		}
	} else {
		uint64_t highbits_offset = lowbits_to_drop - 128;

		out += (high_high >> highbits_offset);

		if (out > MAX_PRICE) {
			out = MAX_PRICE;
		}

	}
	return out;
}

// breaks if amount could overflow a uint64_t
inline static
uint64_t
round_up_price_times_amount(uint128_t p_times_amount)
{
	if (p_times_amount & price_lowbits_mask)
	{
		return (p_times_amount >> PRICE_RADIX) + 1;
	}
	return p_times_amount >> PRICE_RADIX;
}

} /* price */

} /* speedex */
