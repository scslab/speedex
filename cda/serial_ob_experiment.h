#pragma once

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

#include "xdr/types.h"

#include "cda/serial_ob.h"

namespace speedex
{

class MemoryDatabase;

class SerialOrderbookExperiment
{
	SerialOrderbook a_to_b, b_to_a;

	MemoryDatabase& db;

	const static inline OfferCategory a_to_b_cat = OfferCategory(0, 1, OfferType::SELL);
	const static inline OfferCategory b_to_a_cat = OfferCategory(1, 0, OfferType::SELL);

	void exec_one_offer(Offer const& offer);

public:

	SerialOrderbookExperiment(MemoryDatabase& db)
		: a_to_b()
		, b_to_a()
		, db(db)
		{}

	void exec_offers(std::vector<Offer> const& offers)
	{
		for (auto const& offer : offers)
		{
			exec_one_offer(offer);
		}
	}


};

} /* speedex */
