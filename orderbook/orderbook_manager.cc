#include "orderbook/orderbook_manager.h"

namespace speedex {

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
			new_orderbooks.emplace_back(category);
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
	auto num_orderbooks = orderbooks.size();
	tbb::parallel_for(
		tbb::blocked_range<size_t>(0, num_orderbooks),
		[this, current_block_number, &args...] (auto r) {
			for (unsigned int i = r.begin(); i < r.end(); i++) {
				if (orderbooks[i].get_persisted_round_number() < current_block_number) {
					(orderbooks[i].*func)(current_block_number, args...);
				}
			}
		});
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
	generic_map_loading<&Orderbook::persist_lmdb>(
		current_block_number);
}

void OrderbookManager::clear_() {
	generic_map_serial<&Orderbook::clear_>();
}

void OrderbookManager::create_lmdb() {
	generic_map_serial<&Orderbook::create_lmdb>();
}

void OrderbookManager::open_lmdb() {
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
	ThunkGarbage garbage;
	auto num_orderbooks = orderbooks.size();
	for (unsigned int i = 0; i < num_orderbooks; i++) {
		auto orderbook_garbage 
			= orderbooks[i].persist_lmdb(current_block_number);
		garbage.add(orderbook_garbage.release());
	}
	thunk_garbage_deleter.call_delete(garbage.release());
	//generic_map_serial<&Orderbook::persist_lmdb>(current_block_number);
}

void OrderbookManager::open_lmdb_env() {
	generic_map_serial<&Orderbook::open_lmdb_env>();
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

struct ClearOffersForProductionData {
	const ClearingParams& params;
	Price* prices;
	MemoryDatabase& db;
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

void OrderbookManager::clear_offers_for_production(
	const ClearingParams& params, 
	Price* prices, 
	MemoryDatabase& db, 
	AccountModificationLog& account_log, 
	OrderbookStateCommitment& clearing_details_out,
	BlockStateUpdateStatsWrapper& state_update_stats) {
	
	std::lock_guard lock(mtx);

	auto num_orderbooks = orderbooks.size();//get_num_orderbooks();

	clearing_details_out.resize(num_orderbooks);

	const size_t work_units_per_batch = 3;
	
	ClearOffersForProductionData data{params, prices, db, clearing_details_out};


	ClearOffersReduce<ClearOffersForProductionData> reduction(account_log, orderbooks, data);

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
