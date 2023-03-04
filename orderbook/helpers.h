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

#include <cstdint>

#include "orderbook/metadata.h"

namespace speedex {

//! Accumulate total endow and endow times price when
//! scanning over list of open trade offers.
struct EndowAccumulator {
	using int128_t = __int128;

	int64_t endow;
	int128_t endow_times_price;

	EndowAccumulator(const Price& price, const OrderbookMetadata& metadata) 
		: endow(metadata.endow), 
		endow_times_price(
			(((int128_t) metadata.endow) * ((int128_t) price))) {}

	EndowAccumulator()
		: endow(0), 
		endow_times_price(0) {}

	EndowAccumulator& operator+=(const EndowAccumulator& other) {
		endow += other.endow;
		endow_times_price += other.endow_times_price;
		return *this;
	}
};

} /* speedex */