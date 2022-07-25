#pragma once

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
