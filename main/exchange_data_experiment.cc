
#include "memory_database/memory_database.h"

#include "modlog/account_modification_log.h"

#include "orderbook/orderbook_manager.h"
#include "orderbook/orderbook_manager_view.h"

#include "speedex/approximation_parameters.h"
#include "speedex/speedex_management_structures.h"

#include "stats/block_update_stats.h"

#include "utils/debug_macros.h"
#include "utils/manage_data_dirs.h"
#include "utils/save_load_xdr.h"

#include "xdr/cryptocoin_experiment.h"

#include <atomic>
#include <cstdint>
#include <vector>

using namespace speedex;

void add_one_round_of_events(OrderbookManager& manager, size_t start_idx, size_t end_idx, xdr::xvector<ExchangeEvent> const& events)
{
	ProcessingSerialManager serial_manager(manager);

	end_idx = std::min(end_idx, events.size());
	size_t failed_cancels = 0, successful_cancels = 0, new_offers = 0;

	for (size_t i = start_idx; i < end_idx; i++)
	{
		auto const& event = events[i];
		int dummyarg = 0;

		if (event.v() == 0)
		{
			//new offer
			auto idx = manager.look_up_idx(event.newOffer().category);
			serial_manager.add_offer(idx, event.newOffer(), dummyarg, dummyarg);
			new_offers++;
		} 
		else 
		{
			//cancel
			auto idx = manager.look_up_idx(event.cancel().category);
			auto res = serial_manager.delete_offer(idx, event.cancel().cancelledOfferPrice, 0, event.cancel().cancelledOfferId);
			if (!res) {
				failed_cancels++;
			//	std::printf("failed to cancel offer %lu %lu\n", event.cancel().cancelledOfferPrice, event.cancel().cancelledOfferId);
			} else {
				successful_cancels++;
			}
		}
	}
	std::printf("new offers %lu, successful cancels: %lu, failed cancels: %lu\n",new_offers, successful_cancels, failed_cancels);
}

int main(int argc, char const *argv[])
{


	ExchangeExperiment experiment;

	if (load_xdr_from_file(experiment, "exchange_formatted_results")) {
		throw std::runtime_error("failed to load data");
	}


	size_t num_assets = 8;

	size_t batch_size = 50'000;


	OrderbookManager manager(num_assets);

	clear_orderbook_lmdb_dir();
	make_orderbook_lmdb_dir();

	manager.open_lmdb_env();
	manager.create_lmdb();

	for (auto const& snapshot : experiment.initial_snapshots)
	{
		size_t idx = manager.look_up_idx(snapshot.category);

		std::printf("selling %" PRIu32 " buying %" PRIu32 "\n", snapshot.category.sellAsset, snapshot.category.buyAsset);

		ProcessingSerialManager serial_manager(manager);

		int dummyarg = 0;

		for (auto const& offer : snapshot.offers)
		{
			serial_manager.add_offer(idx, offer, dummyarg, dummyarg);
		//	std::printf("adding offer minPrice %lu account %lu id %lu\n", offer.minPrice, offer.owner, offer.offerId);
		}


		serial_manager.finish_merge();
	}

	uint64_t round_number = 1;

	manager.commit_for_production(round_number);
	round_number++;

	uint32_t cur_idx = 0;

	TatonnementManagementStructures tatonnement(manager);

	ApproximationParameters approx_params {
		.tax_rate = 10,
		.smooth_mult = 15
	};

	auto const& events = experiment.event_stream;

	std::vector<Price> prices;
	prices.resize(num_assets);
	for (size_t i = 0; i < prices.size(); i++) {
		prices[i] = price::from_double(1.0);
	}

	uint32_t timeouts = 0, successes = 0;

	while(cur_idx < events.size()) {
		std::printf("current idx: %" PRIu32 " events.size() %lu\n", cur_idx, events.size());
		add_one_round_of_events(manager, cur_idx, cur_idx + batch_size, events);
		cur_idx += batch_size;

		manager.commit_for_production(round_number);

		std::atomic<bool> cancel_timeout_thread = false;
		std::atomic<bool> timeout_flag = false;

		BlockStateUpdateStatsWrapper state_update_stats;

		auto th = tatonnement.oracle.launch_timeout_thread(3000, timeout_flag, cancel_timeout_thread);

		auto res = tatonnement.oracle.compute_prices_grid_search(prices.data(), approx_params);

		cancel_timeout_thread = true;

		if (th)
		{
			th->join();
		}
		auto lp_results = tatonnement.lp_solver.solve(prices.data(), approx_params, !timeout_flag);

		if (!timeout_flag) {
			successes++;
			std::printf("time per round (micros): %lf\n", res.runtime * 1'000'000.0 / (res.num_rounds * 1.0));
		} else {
			timeouts++;
		}

		auto feasible_first = manager.get_max_feasible_smooth_mult(lp_results, prices.data());
		std::printf("feasible smooth mult:%u\n", feasible_first);

		lp_results.check_clearing(prices);

		double vol_metric = manager
			.get_weighted_price_asymmetry_metric(lp_results, prices);
		
		bool use_lower_bound = !timeout_flag;

		if (!use_lower_bound) {
			BLOCK_INFO("tat timed out!");
		}	

		BLOCK_INFO("regular Tat vol metric: timeout %lu %lf", 
			!use_lower_bound, vol_metric);

		auto [satisfied, lost] = manager
			.satisfied_and_lost_utility(lp_results, prices.data());

		BLOCK_INFO("satisfied and lost utility: timeout %lu satisfied %lf lost %lf",
			!use_lower_bound, satisfied, lost);

		tatonnement.rolling_averages.update_averages(
			lp_results, prices.data());

		OrderbookStateCommitment clearing_details;

		NullModificationLog null_log;
		NullDB null_db;

		manager.clear_offers_for_production(
			lp_results, 
			prices.data(), 
			null_db, 
			null_log, 
			clearing_details, 
			state_update_stats);

		manager.persist_lmdb(round_number);
		round_number++;

		std::printf("after round %" PRIu64 ": successes %" PRIu32 " timeouts %" PRIu32 "\n", round_number, successes, timeouts);
	}

}