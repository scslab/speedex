#include "cda/serial_ob_experiment.h"

#include "memory_database/memory_database.h"
#include "memory_database/memory_database_view.h"


using xdr::operator==;

namespace speedex {

void
SerialOrderbookExperiment::exec_one_offer(Offer const& offer)
{

	Price max_price = (((uint128_t)1) << (2*price::PRICE_RADIX)) / offer.minPrice;

	BufferedMemoryDatabaseView view(db);

	auto* user = view.lookup_user(offer.owner);

	if (user == nullptr)
	{
		throw std::runtime_error("wtf");
	}

	auto res = view.reserve_sequence_number(user, offer.offerId & 0xFFFF'FFFF'FFFF'FF00);

	if (res != TransactionProcessingStatus::SUCCESS)
	{
		throw std::runtime_error("should not happen in experiments");
		view.unwind();
		return;
	}

	res = view.escrow(user, offer.category.sellAsset, offer.amount);

	if (res != TransactionProcessingStatus::SUCCESS)
	{
		view.unwind();
		return;
	}

	if (offer.category == a_to_b_cat)
	{
		auto[a_sold, b_recv] = a_to_b.try_execute(max_price, offer.amount, view);

		if (a_sold > offer.amount)
		{
			throw std::runtime_error("invalid");
		}

		view.transfer_available(user, a_to_b_cat.buyAsset, b_recv);

		Offer to_add = offer;

		to_add.amount -= a_sold;

		if (to_add.amount > 0)
		{
			b_to_a.add_offer(to_add);
		}
	} 
	else
	{
		auto[b_sold, a_recv] = b_to_a.try_execute(max_price, offer.amount, view);

		if (b_sold > offer.amount)
		{
			throw std::runtime_error("invalid");
		}

		view.transfer_available(user, b_to_a_cat.buyAsset, a_recv);

		Offer to_add = offer;

		to_add.amount -= b_sold;

		if (to_add.amount > 0)
		{
			a_to_b.add_offer(to_add);
		}
	}
	view.commit();
}



} /* speedex */
