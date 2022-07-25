#include "cda/serial_ob_experiment.h"

#include "memory_database/memory_database.h"


using xdr::operator==;

namespace speedex {

void
SerialOrderbookExperiment::exec_one_offer(Offer const& offer)
{

	Price max_price = (((uint128_t)1) << (2*price::PRICE_RADIX)) / offer.minPrice;

	//std::printf("maxPrice = %lu minPrice = %lu\n", max_price, offer.minPrice);

	if (offer.category == a_to_b_cat)
	{
		auto[a_sold, b_recv] = a_to_b.try_execute(max_price, offer.amount, db);

		if (a_sold > offer.amount)
		{
			throw std::runtime_error("invalid");
		}

		auto* user = db.lookup_user(offer.owner);

		if (user == nullptr)
		{
			throw std::runtime_error("wtf");
		}

		db.transfer_available(user, a_to_b_cat.buyAsset, b_recv);

		Offer to_add = offer;

		to_add.amount -= a_sold;

		if (to_add.amount > 0)
		{
			b_to_a.add_offer(to_add);
		}
	} 
	else
	{
		auto[b_sold, a_recv] = b_to_a.try_execute(max_price, offer.amount, db);

		if (b_sold > offer.amount)
		{
			throw std::runtime_error("invalid");
		}

		auto* user = db.lookup_user(offer.owner);

		if (user == nullptr)
		{
			throw std::runtime_error("wtf");
		}

		db.transfer_available(user, b_to_a_cat.buyAsset, a_recv);

		Offer to_add = offer;

		to_add.amount -= b_sold;

		if (to_add.amount > 0)
		{
			a_to_b.add_offer(to_add);
		}
	}
}



} /* speedex */
