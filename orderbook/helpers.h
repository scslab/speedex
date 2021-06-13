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