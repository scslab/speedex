/**
 * SPEEDEX: A Scalable, Parallelizable, and Economically Efficient Decentralized Exchange
 * Copyright (C) 2023 Geoffrey Ramseyer

 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once
/*! \file fixed_point_value.h 

	Typesafe arithmetic with fixed-point fractional values.

*/

#include <type_traits>
#include <cstdint>
#include <cmath>
#include <concepts>

namespace speedex {

namespace
{

struct EmptyStruct {};
struct EmptyStruct2 {};

} // anonymous namespace

//! Represents a fractional value with a fixed radix.
//! Only works with unsigned underlying values.
template<unsigned int radix, std::unsigned_integral integral_type> 
struct FixedPrecision {

	using value_t = integral_type;
	
	constexpr static value_t get_lowbits_mask(unsigned int query_radix) {
		return (((value_t) 1) << query_radix) - ((value_t)1);
	}

	constexpr static value_t lowbits_mask = get_lowbits_mask(radix);

	value_t value;


private:
	//! Construct from raw value
	FixedPrecision(value_t value, EmptyStruct) : value(value) {}
	//! Construct from double (i.e. round double to fixed-point)
	FixedPrecision(double d_val, EmptyStruct2) : value(std::round(d_val * (((value_t) 1)<<radix))) {}
public:

	//! Construct with default value 0.
	FixedPrecision() : value(0) {}

	//! Construct from double, rounding up remaining precision bits
	static FixedPrecision round_up(double val) {
		return from_raw(std::ceil(val * ((value_t)1) << radix));
	}
	//! Construct from double, rounding down remaining precision bits
	static FixedPrecision round_down(double val) {
		return from_raw(std::floor(val * ((value_t) 1) << radix));
	}

	//! Construct from raw value (i.e. interpret input value as fixed point)
	static FixedPrecision from_raw(value_t value) {
		return FixedPrecision(value, EmptyStruct{});
	}
	//! Construct from integer
	static FixedPrecision from_integral(value_t value) {
		return FixedPrecision(value <<radix, EmptyStruct{});
	}

	//! Construct from double
	static FixedPrecision from_double(double val)
	{
		return FixedPrecision(val, EmptyStruct2{});
	}

	template<unsigned int other_radix>
	FixedPrecision<radix, value_t> operator+ (const FixedPrecision<other_radix, value_t>& other) const{
		static_assert(other_radix <= radix, "invalid radix combination");
		return FixedPrecision(value + (other.value<<(radix-other_radix)), EmptyStruct{});
	}

	template<unsigned int other_radix>
	FixedPrecision<radix, value_t>& operator+= (const FixedPrecision<other_radix, value_t>&  other) {
		static_assert(other_radix <= radix, "invalid radix combination");
		value += (other.value<<(radix-other_radix));
		return *this;
	}

	template<unsigned int other_radix>
	FixedPrecision<radix, value_t>& operator-= (const FixedPrecision<other_radix, value_t>&  other) {
		static_assert(other_radix <= radix, "invalid radix combination");
		value -= (other.value<<(radix-other_radix));
		return *this;
	}

	template<unsigned int other_radix>
	FixedPrecision<radix, value_t> operator- (const FixedPrecision<other_radix, value_t>& other) const {
		static_assert(other_radix <= radix, "invalid radix combination");
		return FixedPrecision(value - (other.value<<(radix-other_radix)), EmptyStruct{});
	}

	FixedPrecision<radix, value_t> operator* (const uint64_t& other_value) const {
		return FixedPrecision(value * other_value, EmptyStruct{});
	}

	std::strong_ordering operator<=> (const FixedPrecision<radix, value_t>& other) const {
		// doing proper comparisons between FixedPrecisions of other radixes requires more edge case nonsense
		// than I particularly care to deal with.
		return value <=> other.value;
	}

	bool operator==(const FixedPrecision<radix, value_t>& other) const = default;

	//does not shift radix
	FixedPrecision<radix, value_t>
	operator>>(unsigned int i) const {
		return FixedPrecision(value>>i, EmptyStruct{});
	}

	integral_type floor() const {
		return value >> radix;
	}

	integral_type ceil() const {
		return floor() + ((value & lowbits_mask) != 0);
	}

	double to_double() const {
		return ((double) value) / (double) (((value_t)1)<<radix);
	}

	//!Impose multiplicative tax (i.e. 2^-tax_rate),
	//! where precision lost (i.e. precision beyond the fixed-width of our 
	//! value) is rounded up.  Output is then rounded down to nearest integer.
	//! (i.e. tax is rounded up again).
	integral_type tax_and_round(uint8_t tax_rate) {
		auto tax_lowbits_mask = get_lowbits_mask(tax_rate);
		value_t tax = value >> tax_rate;
		if (value & tax_lowbits_mask) {
			tax++;
		}
		return (value - tax) >> radix; // (value - tax).floor();
	}


	//!Impose multiplicative tax (i.e. 2^-tax_rate),
	//! where precision lost (i.e. precision beyond the fixed-width of our 
	//! value) is rounded up.
	FixedPrecision<radix, value_t> tax(uint8_t tax_rate) {
		auto tax_lowbits_mask = get_lowbits_mask(tax_rate);
		value_t tax = value >> tax_rate;
		if (value & tax_lowbits_mask) {
			tax++;
		}
		return from_raw(value - tax);
	}
};

//! Fractional asset values (used internally within Tatonnement)
//! use 10 bits of extra precision and use 128 bits total
//! to avoid worrying about overflowing.
typedef FixedPrecision<10, unsigned __int128> FractionalAsset;

}
