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

#include "orderbook/orderbook_manager.h"

#include "modlog/account_modification_log.h"

#include "orderbook/commitment_checker.h"
#include "orderbook/offer_clearing_params.h"

#include "speedex/speedex_static_configs.h"

#include "stats/block_update_stats.h"

#include "utils/debug_macros.h"	

namespace speedex {

class NullDB;

OrderbookManager::OrderbookManager(
		uint16_t num_new_assets)
		: orderbooks()
		, num_assets(0)
		, lmdb(get_num_orderbooks_by_asset_count(num_new_assets))
	{	
		increase_num_traded_assets(num_new_assets);
		num_assets = num_new_assets;
	}

void OrderbookManager::increase_num_traded_assets(
	uint16_t new_asset_count) 
{
	if (new_asset_count < num_assets) {
		throw std::runtime_error("Cannot decrease number of assets");
	}
	std::vector<Orderbook> new_orderbooks;
	int new_orderbooks_count 
		= get_num_orderbooks_by_asset_count(new_asset_count);
	new_orderbooks.reserve(new_orderbooks_count);
	for (int i = 0; i < new_orderbooks_count; i++) {
		OfferCategory category = category_from_idx(i, new_asset_count);
		if (category.buyAsset < num_assets && category.sellAsset < num_assets) {
			int old_idx = category_to_idx(category, num_assets);
			new_orderbooks.push_back(std::move(orderbooks[old_idx]));
		} else {
			new_orderbooks.emplace_back(category, lmdb);
		}
	}
	orderbooks = std::move(new_orderbooks);
	num_assets = new_asset_count;
}

template<auto func, typename... Args>
void OrderbookManager::generic_map(Args... args) {
	auto num_orderbooks = orderbooks.size();
	tbb::parallel_for(
		tbb::blocked_range<size_t>(0, num_orderbooks),
		[this, &args...] (auto r) {
			for (unsigned int i = r.begin(); i < r.end(); i++) {
				(orderbooks[i].*func)(args...);
			}
		});
}

template<auto func, typename... Args>
void OrderbookManager::generic_map_serial(Args... args) {
	auto num_orderbooks = orderbooks.size();
	for (unsigned int i = 0; i < num_orderbooks; i++) {
		(orderbooks[i].*func)(args...);
	}
}

template<auto func, typename... Args>
void OrderbookManager::generic_map_loading(uint64_t current_block_number, Args... args) {

	//TODO parallelize loop if this ever is necessary.
	for (auto i = 0u; i < lmdb.get_num_base_instances(); i++) {
		auto [start, end] = lmdb.get_base_instance_range(i);
		auto& local_lmdb = lmdb.get_base_instance_by_index(i);
		if (local_lmdb.get_persisted_round_number() < current_block_number) {
			tbb::parallel_for(
				tbb::blocked_range<size_t>(start, end),
					[this, current_block_number, &args...] (auto r) {
						for (unsigned int j = r.begin(); j < r.end(); j++) {
							(orderbooks[j].*func)(current_block_number, args...);
						}
				});
		}
	}
}

void OrderbookManager::commit_for_loading(uint64_t current_block_number) {
	generic_map_loading<&Orderbook::tentative_commit_for_validation>(
		current_block_number);
}

void OrderbookManager::finalize_for_loading(uint64_t current_block_number) {
	generic_map_loading<&Orderbook::finalize_validation_for_loading>(
		current_block_number);
}

void OrderbookManager::persist_lmdb_for_loading(uint64_t current_block_number) {

	for (auto i = 0u; i < lmdb.get_num_base_instances(); i++) {
		auto [start, end] = lmdb.get_base_instance_range(i);
		auto& local_lmdb = lmdb.get_base_instance_by_index(i);

		if (local_lmdb.get_persisted_round_number() < current_block_number) {

			ThunkGarbage<OrderbookTrie::TrieT> garbage;

			auto wtx = local_lmdb.wbegin();

			for (auto j = start; j < end; j++) {
				auto orderbook_garbage 
					= orderbooks[j].persist_lmdb(current_block_number, wtx);
				if (orderbook_garbage != nullptr) {
					garbage.add(orderbook_garbage -> release());
				}
			}

			thunk_garbage_deleter.call_delete(garbage.release());

			local_lmdb.commit_wtxn(wtx, current_block_number);
		}
	}
}

void OrderbookManager::create_lmdb() {
	lmdb.create_db();
	generic_map_serial<&Orderbook::create_lmdb>();
}

void OrderbookManager::open_lmdb() {
	lmdb.open_db();
	generic_map_serial<&Orderbook::open_lmdb>();
}

void OrderbookManager::commit_for_production(uint64_t current_block_number) {
	std::lock_guard lock(mtx);
	generic_map<&Orderbook::commit_for_production>(current_block_number);
}

void OrderbookManager::commit_for_validation(
	uint64_t current_block_number) {
	std::lock_guard lock(mtx);
	generic_map<&Orderbook::tentative_commit_for_validation>(
		current_block_number);
}


void OrderbookManager::rollback_thunks(uint64_t current_block_number) {
	std::lock_guard lock(mtx);
	generic_map<&Orderbook::rollback_thunks>(current_block_number);
}

void OrderbookManager::persist_lmdb(uint64_t current_block_number) {
	//orderbooks manage their own thunk threadsafety for persistence thunks

	for (auto i = 0u; i < lmdb.get_num_base_instances(); i++) {
		auto [start, end] = lmdb.get_base_instance_range(i);
		auto& local_lmdb = lmdb.get_base_instance_by_index(i);
		ThunkGarbage<OrderbookTrie::TrieT> garbage;

		auto wtx = local_lmdb.wbegin();

		for (auto j = start; j < end; j++) {

			auto orderbook_garbage 
				= orderbooks[j].persist_lmdb(current_block_number, wtx);

			if (orderbook_garbage != nullptr) {
				garbage.add(orderbook_garbage->release());
			}
		}

		if constexpr (!DISABLE_LMDB)
		{
			local_lmdb.commit_wtxn(wtx, current_block_number);
		}
		
		thunk_garbage_deleter.call_delete(garbage.release());
	}
}

uint64_t 
OrderbookManager::get_min_persisted_round_number() {
	uint64_t min = UINT64_MAX;
	for (const auto& orderbook : orderbooks) {
		min = std::min(min, orderbook.get_persisted_round_number());
	}
	return min;
}

uint64_t 
OrderbookManager::get_max_persisted_round_number() {
	uint64_t max = 0;
	for (const auto& orderbook : orderbooks) {
		max = std::max(max, orderbook.get_persisted_round_number());
	}
	return max;
}

void OrderbookManager::open_lmdb_env() {
	lmdb.open_lmdb_env();
}

void OrderbookManager::finalize_validation() {
	std::lock_guard lock(mtx);
	generic_map<&Orderbook::finalize_validation>();
}

void OrderbookManager::rollback_validation() {
	std::lock_guard lock(mtx);
	generic_map<&Orderbook::rollback_validation>();
}

void OrderbookManager::load_lmdb_contents_to_memory() {
	generic_map<&Orderbook::load_lmdb_contents_to_memory>();
}

void OrderbookManager::generate_metadata_indices() {
	std::lock_guard lock(mtx);
	generic_map<&Orderbook::generate_metadata_index>();
}

void OrderbookManager::hash(OrderbookStateCommitment& clearing_details) {
	std::lock_guard lock(mtx);
	tbb::parallel_for(
		tbb::blocked_range<size_t>(0, orderbooks.size()),
		[&clearing_details, this] (auto r) {
			for (unsigned int i = r.begin(); i < r.end(); i++) {
				orderbooks[i].hash(clearing_details[i].rootHash);
			}
	});
}

size_t OrderbookManager::num_open_offers() const {
	std::lock_guard lock(mtx);

	std::atomic<size_t> num_offers = 0;
	auto num_orderbooks = orderbooks.size();
	tbb::parallel_for(
		tbb::blocked_range<std::size_t>(0, num_orderbooks),
		[this, &num_offers] (auto r) {
			for (unsigned int i = r.begin(); i < r.end(); i++) {
				num_offers.fetch_add(
					orderbooks[i].num_open_offers(), std::memory_order_relaxed);
			}
		});

	return num_offers.load(std::memory_order_relaxed);

}

template<typename DB>
struct ClearOffersForProductionData {
	const ClearingParams& params;
	Price* prices;
	DB& db;
	OrderbookStateCommitment& clearing_details_out;

	void operator() (
		const tbb::blocked_range<std::size_t>& r, 
		std::vector<Orderbook>& orderbooks, 
		SerialAccountModificationLog& local_log,
		BlockStateUpdateStatsWrapper& state_update_stats) {
		
		for (auto i = r.begin(); i < r.end(); i++) {
			orderbooks.at(i).process_clear_offers(
				params.orderbook_params.at(i),
				prices, 
				params.tax_rate, 
				db, 
				local_log, 
				clearing_details_out.at(i), 
				state_update_stats);
		}
	}
};

struct TentativeClearOffersForValidationData {

	MemoryDatabase& db;
	ThreadsafeValidationStatistics& validation_statistics;
	const OrderbookStateCommitmentChecker& clearing_commitment_log;
	std::atomic_flag& exists_failure;

	void operator() (
		const tbb::blocked_range<std::size_t>& r, 
		std::vector<Orderbook>& orderbooks, 
		SerialAccountModificationLog& local_log,
		BlockStateUpdateStatsWrapper& state_update_stats) {


		for (auto i = r.begin(); i < r.end(); i++) {
			auto res = orderbooks[i].tentative_clear_offers_for_validation(
						db, 
						local_log, 
						validation_statistics[i], 
						clearing_commitment_log[i], 
						clearing_commitment_log,
						state_update_stats);
			if (!res) {
				std::printf("one unit failed\n");
				exists_failure.test_and_set();
				return;
			}
		}
	}
};




template<typename ClearingData>
class ClearOffersReduce {

	AccountModificationLog& main_log;
	std::vector<Orderbook>& orderbooks;
	ClearingData& func;

public:
	BlockStateUpdateStatsWrapper state_update_stats;

	void operator() (const tbb::blocked_range<size_t> r) {
		SerialAccountModificationLog local_log(main_log);
		func(r, orderbooks, local_log, state_update_stats);
	}

	ClearOffersReduce (ClearOffersReduce& x, [[maybe_unused]] tbb::split)
		: main_log(x.main_log)
		, orderbooks(x.orderbooks)
		, func(x.func)
		{}

	ClearOffersReduce(AccountModificationLog& main_log, std::vector<Orderbook>& orderbooks, ClearingData& func)
		: main_log(main_log)
		, orderbooks(orderbooks)
		, func(func)
		 {}

	void join(ClearOffersReduce& other) {
		state_update_stats += other.state_update_stats;
	}

};

template void OrderbookManager::clear_offers_for_production<MemoryDatabase>(
	const ClearingParams&,
	Price*,
	MemoryDatabase&,
	AccountModificationLog&,
	OrderbookStateCommitment&,
	BlockStateUpdateStatsWrapper&);

template void OrderbookManager::clear_offers_for_production<NullDB>(
	const ClearingParams&,
	Price*,
	NullDB&,
	AccountModificationLog&,
	OrderbookStateCommitment&,
	BlockStateUpdateStatsWrapper&);

template<typename DB>
void OrderbookManager::clear_offers_for_production(
	const ClearingParams& params, 
	Price* prices, 
	DB& db, 
	AccountModificationLog& account_log, 
	OrderbookStateCommitment& clearing_details_out,
	BlockStateUpdateStatsWrapper& state_update_stats) {
	
	std::lock_guard lock(mtx);

	auto num_orderbooks = orderbooks.size();//get_num_orderbooks();

	clearing_details_out.resize(num_orderbooks);

	const size_t work_units_per_batch = 3;
	
	ClearOffersForProductionData<DB> data{params, prices, db, clearing_details_out};


	ClearOffersReduce<ClearOffersForProductionData<DB>> reduction(account_log, orderbooks, data);

	tbb::parallel_reduce(
		tbb::blocked_range<size_t>(0, num_orderbooks, work_units_per_batch), 
		reduction);

	state_update_stats += reduction.state_update_stats;
	
	account_log.merge_in_log_batch();
}

bool 
OrderbookManager::tentative_clear_offers_for_validation(
	MemoryDatabase& db,
	AccountModificationLog& account_modification_log,
	ThreadsafeValidationStatistics& validation_statistics,
	const OrderbookStateCommitmentChecker& clearing_commitment_log,
	BlockStateUpdateStatsWrapper& state_update_stats) {

	std::lock_guard lock(mtx);

	auto num_orderbooks = orderbooks.size();;

	std::atomic_flag exists_failure = ATOMIC_FLAG_INIT;
	
	validation_statistics.make_minimum_size(num_orderbooks);

	TentativeClearOffersForValidationData data{db, validation_statistics, clearing_commitment_log, exists_failure};

	ClearOffersReduce<TentativeClearOffersForValidationData> reduction(account_modification_log, orderbooks, data);
	
	const size_t work_units_per_batch = 3;

	tbb::parallel_reduce(tbb::blocked_range<std::size_t>(0, num_orderbooks, work_units_per_batch), reduction);

	state_update_stats += reduction.state_update_stats;
	
	account_modification_log.merge_in_log_batch();

	return !exists_failure.test_and_set();
}

uint8_t 
OrderbookManager::get_max_feasible_smooth_mult(
	const ClearingParams& clearing_params, Price* prices) 
{
	uint8_t max = UINT8_MAX;
	for (size_t i = 0; i < orderbooks.size(); i++) {
		auto& orderbook = orderbooks[i];
		uint8_t candidate = orderbook.max_feasible_smooth_mult(
			clearing_params.orderbook_params[i].supply_activated.ceil(), prices);

		max = std::min(max, candidate);
	}
	return max;
}

std::pair<double, double>
OrderbookManager::satisfied_and_lost_utility(const ClearingParams& clearing_params, Price* prices) const
{
	double satisfied = 0, lost = 0;
	for (size_t i = 0; i < orderbooks.size(); i++) {
		auto& orderbook = orderbooks[i];
		auto [s, l] = orderbook.satisfied_and_lost_utility(
			clearing_params.orderbook_params[i].supply_activated.ceil(), prices);
		satisfied += s;
		lost += l;
	}
	return {satisfied, lost};
}

double 
OrderbookManager::get_weighted_price_asymmetry_metric(
	const ClearingParams& clearing_params,
	const std::vector<Price>& prices) const 
{
	auto num_orderbooks = orderbooks.size();

	double total_vol = 0;
	double weighted_vol = 0;

	for (size_t i = 0; i < num_orderbooks; i++) {
		double feasible_mult = orderbooks[i].max_feasible_smooth_mult_double(
			clearing_params.orderbook_params[i].supply_activated.ceil(), prices.data());
		auto category = orderbooks[i].get_category();
		Price sell_price = prices[category.sellAsset];

		double volume 
			= clearing_params
				.orderbook_params[i]
				.supply_activated
				.to_double() 
			* price::to_double(sell_price);
		
		total_vol += volume;

		weighted_vol += feasible_mult * volume;
	}
	return weighted_vol / total_vol;
}


void 
OrderbookManager::clear_offers_for_data_loading(
	MemoryDatabase& db,
	AccountModificationLog& account_modification_log,
	ThreadsafeValidationStatistics& validation_statistics,
	const OrderbookStateCommitmentChecker& clearing_commitment_log,
	const uint64_t current_block_number) {
	
	auto num_orderbooks = orderbooks.size();

	std::atomic_flag exists_failure = ATOMIC_FLAG_INIT;
	
	validation_statistics.make_minimum_size(num_orderbooks);

	tbb::parallel_for(
		tbb::blocked_range<std::size_t>(0, num_orderbooks),
		[this, &clearing_commitment_log, &account_modification_log, &db, &validation_statistics, &exists_failure, &current_block_number] (auto r) {
			SerialAccountModificationLog serial_account_log(account_modification_log);
			BlockStateUpdateStatsWrapper stats;
			for (auto i = r.begin(); i < r.end(); i++) {

				if (orderbooks[i].get_persisted_round_number() < current_block_number) {
					BLOCK_INFO("doing a tentative_clear_offers_for_validation while loading");

					auto res = orderbooks[i].tentative_clear_offers_for_validation(
						db, 
						serial_account_log, 
						validation_statistics[i], 
						clearing_commitment_log[i], 
						clearing_commitment_log,
						stats);
					if (res) {
						exists_failure.test_and_set();
						return;
					}
				}
			}
		});

	account_modification_log.merge_in_log_batch();
	
	if (exists_failure.test_and_set()) {
		throw std::runtime_error("failed to load block!");
	}
}

} /* speedex */
