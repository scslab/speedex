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

#include <atomic>
#include <cstdint>
#include <sstream>

#include "mtt/trie/metadata.h"

#include "xdr/types.h"

namespace speedex {

struct AtomicOrderbookMetadata;


/*! Duplication of SizeMixin.

Stores an offer's available sell amount (it's "endowment").
*/
struct OrderbookMetadata {

	using AtomicT = AtomicOrderbookMetadata;
	int64_t endow;

	OrderbookMetadata(const Offer& offer) : endow(offer.amount) {}

	OrderbookMetadata() : endow(0) {}

	OrderbookMetadata(const OrderbookMetadata& v) : endow(v.endow) {}


	OrderbookMetadata& operator+=(const OrderbookMetadata& other) {
		endow += other.endow;
		return *this;
	}
	OrderbookMetadata& operator-=(const OrderbookMetadata& other) {
		endow -= other.endow;
		return *this;
	}

	bool operator==(const OrderbookMetadata& other) {
		return endow == other.endow;
	}

	std::string to_string() const {
		std::stringstream s;
		s << "endow:"<<endow<<" ";
		return s.str();
	}

	template<typename AtomicType>
	void unsafe_load_from(const AtomicType& s) {
		endow = s.endow.load(trie::load_order);
	}
};



struct AtomicOrderbookMetadata {

	using BaseT = OrderbookMetadata;

	std::atomic_int64_t endow;

	AtomicOrderbookMetadata(const Offer& offer) : endow(offer.amount) {}

	AtomicOrderbookMetadata() : endow(0) {}

	AtomicOrderbookMetadata(const OrderbookMetadata& v) : endow(v.endow) {}

	void operator+= (const OrderbookMetadata& other) {
		endow.fetch_add(other.endow, trie::store_order);
	}

	void operator-= (const OrderbookMetadata& other) {
		endow.fetch_sub(other.endow, trie::store_order);
	}

	void clear() {
		endow = 0;
	}

	void unsafe_store(const OrderbookMetadata& other) {
		endow.store(other.endow, trie::store_order);
	}

	std::string to_string() const {
		std::stringstream s;
		s << "endow:"<<endow<<" ";
		return s.str();
	}

};

} /* edce */
