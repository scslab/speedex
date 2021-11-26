#include "orderbook/orderbook.h"

#include "utils/debug_macros.h"
#include "utils/debug_utils.h"

#include <tbb/task_arena.h>

namespace speedex {

//! Lambda which, when applied to an offer trie, fully clears
//! all of the offers in said trie.
class CompleteClearingFunc {
	const Price sellPrice;
	const Price buyPrice;
	const uint8_t tax_rate;
	MemoryDatabase& db;
	SerialAccountModificationLog& serial_account_log;
public:
	CompleteClearingFunc(
		Price sellPrice, 
		Price buyPrice,
		uint8_t tax_rate, 
		MemoryDatabase& db, 
		SerialAccountModificationLog& serial_account_log) 
	: sellPrice(sellPrice)
	, buyPrice(buyPrice)
	, tax_rate(tax_rate)
	, db(db)
	, serial_account_log(serial_account_log) {}

	void operator() (const Offer& offer) {

		if (price::a_over_b_lt_c(sellPrice, buyPrice, offer.minPrice)) {
			std::printf(
				"%f %f\n", 
				price::to_double(sellPrice)/price::to_double(buyPrice), 
				price::to_double(offer.minPrice));
			std::fflush(stdout);
			throw std::runtime_error("Trying to clear an offer with bad price");
		}
		if (offer.amount == 0) {
			throw std::runtime_error("offer amount was 0!");
		}
		UserAccount* idx = db.lookup_user(offer.owner);
		//auto result = db.lookup_user_id(offer.owner, &idx);
		if (idx == nullptr) {
			throw std::runtime_error("Offer in manager from nonexistent account");
		}


		clear_offer_full(offer, sellPrice, buyPrice, tax_rate, db, idx);

		serial_account_log.log_self_modification(offer.owner, offer.offerId);
	}
};

void Orderbook::tentative_commit_for_validation(uint64_t current_block_number) {
	{
		auto lock = lmdb_instance.lock();
		//std::lock_guard lock(*lmdb_instance.mtx);
		auto& thunk = lmdb_instance.add_new_thunk_nolock(current_block_number);
		thunk.uncommitted_offers_vec 
			= uncommitted_offers.accumulate_values<std::vector<Offer>>();
		auto& accumulate_deleted_keys = thunk.deleted_keys;
		committed_offers.perform_marked_deletions(accumulate_deleted_keys);
	}
	committed_offers.merge_in(std::move(uncommitted_offers));
	uncommitted_offers.clear();
}

void Orderbook::undo_thunk(OrderbookLMDBCommitmentThunk& thunk) {
	std::printf("starting thunk undo\n");
	for (auto& kv : thunk.deleted_keys.deleted_keys) {
		committed_offers.insert(kv.first, OfferWrapper(kv.second));
	}

	thunk.cleared_offers.clean_singlechild_nodes(thunk.partial_exec_key);

	committed_offers.merge_in(std::move(thunk.cleared_offers));

	for (auto& offer : thunk.uncommitted_offers_vec) {
		prefix_t key;
		generate_orderbook_trie_key(offer, key);
		committed_offers.mark_for_deletion(key);
	}
	committed_offers.perform_marked_deletions();

	if (thunk.get_exists_partial_exec()) {
		auto bytes_array = thunk.partial_exec_key.get_bytes_array();
		std::printf(
			"key:%s\n", 
			debug::array_to_str(
				bytes_array.data(), bytes_array.size()).c_str());
		committed_offers.insert(
			thunk.partial_exec_key, 
			OfferWrapper(thunk.preexecute_partial_exec_offer));
	}
	std::printf("done thunk undo\n");
}

void Orderbook::finalize_validation() {
	committed_offers.clear_rollback();  //TODO on a higher powered machine, maybe parallelize?

	if (uncommitted_offers.size() != 0) {
		throw std::runtime_error("shouldn't have uncommitted_offers nonempty when calling finalize");
	}
}

//rolls back tentative_commit_for_validation, along with transaction side effects
// also rolls back tentative_clear_offers_for_validation via undo_thunk.
void Orderbook::rollback_validation() {
	
	uncommitted_offers.clear();
	committed_offers.do_rollback(); // takes care of new round's uncommitted offers, so we can safely clear them from the thunk.

	auto lock = lmdb_instance.lock();
	//std::lock_guard lock(*lmdb_instance.mtx);

	auto& thunk = lmdb_instance.get_top_thunk_nolock();

	thunk.uncommitted_offers_vec.clear();

	undo_thunk(thunk);

	lmdb_instance.pop_top_thunk_nolock();
}

void Orderbook::commit_for_production(uint64_t current_block_number) {
	tentative_commit_for_validation(current_block_number);
	generate_metadata_index();
}

void Orderbook::generate_metadata_index() {
	indexed_metadata 
		= committed_offers
			.metadata_traversal<EndowAccumulator, Price, FuncWrapper>(
				price::PRICE_BIT_LEN);
}


std::unique_ptr<ThunkGarbage<typename OrderbookTrie::TrieT>>
__attribute__((warn_unused_result))
Orderbook::persist_lmdb(uint64_t current_block_number, dbenv::wtxn& wtx) {

	if (!lmdb_instance) {
		return nullptr;
	}
	return lmdb_instance.write_thunks(current_block_number, wtx);
}

/*
//old version
WorkUnitMetadata MerkleWorkUnit::get_metadata(Price p) {
	unsigned char price_bytes[PriceUtils::PRICE_BYTES];
	PriceUtils::write_price_big_endian(price_bytes, partial_exec_p);

	return committed_offers.metadata_query(
		price_bytes, PriceUtils::PRICE_BIT_LEN);
}
*/

Price divide_prices(Price sell_price, Price buy_price) {
	uint8_t extra_bits_len = (64-price::PRICE_RADIX);
	uint128_t ratio = (((uint128_t)sell_price)<<64) / buy_price;
	ratio >>= extra_bits_len;
	return ratio & UINT64_MAX;
}

double 
Orderbook::max_feasible_smooth_mult_double(
	int64_t amount, const Price* prices) const 
{
	size_t start = 1;
	size_t end = indexed_metadata.size() - 1;

	Price sell_price = prices[category.sellAsset];
	Price buy_price = prices[category.buyAsset];

	Price exact_exchange_rate = divide_prices(sell_price, buy_price);

	if (end <= 0) {
		return 0;
	}

	if (amount > indexed_metadata[end].metadata.endow) {
		return 0;
	}

	Price max_activated_price = 0;

	int mp = (end + start) /2;
	while (true) {
		if (end == start) {
			if (indexed_metadata[end].metadata.endow > amount) {
				max_activated_price = indexed_metadata[end].key;
			} else {
				if (end + 1 == indexed_metadata.size()) {
					return 0;
				}
				max_activated_price = indexed_metadata[end+1].key;
			}
			break;
		}

		if (amount >= indexed_metadata[mp].metadata.endow) {
			start = mp + 1;
		} else {
			end = mp;
		}
		mp = (start + end) / 2;
	}

	Price raw_difference = exact_exchange_rate - max_activated_price;
	if (exact_exchange_rate <= max_activated_price) {
		//Should never happen, but maybe if there's some rounding error I'm not accounting for.
		return 0;
	}

	return price::to_double(raw_difference) 
		/ price::to_double(exact_exchange_rate);
}

uint8_t
Orderbook::max_feasible_smooth_mult(
	int64_t amount, const Price* prices) const 
{
	size_t start = 1;
	size_t end = indexed_metadata.size() - 1;

	Price sell_price = prices[category.sellAsset];
	Price buy_price = prices[category.buyAsset];

	Price exact_exchange_rate = divide_prices(sell_price, buy_price);

	if (end <= 0) {
		return UINT8_MAX;
	}

	if (amount > indexed_metadata[end].metadata.endow) {
		return UINT8_MAX;
	}

	Price max_activated_price = 0;

	int mp = (end + start) /2;
	while (true) {
		if (end == start) {
			if (indexed_metadata[end].metadata.endow > amount) {
				max_activated_price = indexed_metadata[end].key;
			} else {
				if (end + 1 == indexed_metadata.size()) {
					return UINT8_MAX;
				}
				max_activated_price = indexed_metadata[end+1].key;
			}
			break;
		}

		if (amount >= indexed_metadata[mp].metadata.endow) {
			start = mp + 1;
		} else {
			end = mp;
		}
		mp = (start + end) / 2;
	}

	Price raw_difference = exact_exchange_rate - max_activated_price;
	if (exact_exchange_rate <= max_activated_price) {
		//Should never happen, but maybe if there's some rounding error I'm not accounting for.
		return UINT8_MAX;
	}

	uint8_t out = 0;
	while (raw_difference <= (exact_exchange_rate >> out)) {
		out++;
	}
	if (out > 0) {
		return out - 1;
	}
	return 0;
}

size_t 
Orderbook::num_open_offers() const {
	return committed_offers.size();
}


/*

GetMetadataTask
MerkleWorkUnit::coro_get_metadata(Price p, EndowAccumulator& endow_out, DemandCalcScheduler& scheduler) const {

	using awaiter_t = DemandCalcAwaiter<const Price, DemandCalcScheduler>;

	int start = 1;
	int end = indexed_metadata.size() - 1;

	if (end <= 0) {
		endow_out = EndowAccumulator{};
		co_return;
	}
	if (p > indexed_metadata[end].key) {
		endow_out = indexed_metadata[end].metadata;
		co_return;
	}

	int mp = (end + start) / 2;
	while(true) {
		if (end == start) {
			endow_out = indexed_metadata[end - 1].metadata;
			co_return;
		}

		const Price compare_val = co_await awaiter_t{&(indexed_metadata[mp].key), scheduler};

		if (p >= compare_val) {
			start = mp + 1;
		} else {
			end = mp;
		}
		mp = (start + end) / 2;
	}
}
*/

EndowAccumulator 
Orderbook::get_metadata(Price p) const{
	int start = 1;
	int end = indexed_metadata.size()-1;
	DEMAND_CALC_INFO("committed_offers_sz:%d", committed_offers.size());
	DEMAND_CALC_INFO("indexed_metadata_sz:%d, end:%d", indexed_metadata.size(), end);
	if (end <= 0) {
		DEMAND_CALC_INFO("empty work unit, outputting 0");
		return EndowAccumulator{};
	}
	if (p > indexed_metadata[end].key) {
		DEMAND_CALC_INFO("outputting end");
		DEMAND_CALC_INFO("%lu, %lu", p, indexed_metadata[end].key);
		return indexed_metadata[end].metadata;
	}
	int mp = (end+start)/2;
	
	while (true) {
		if (end==start) {
			DEMAND_CALC_INFO("outputting idx %d, key %f", end, price::to_double(indexed_metadata[end - 1].key));
			DEMAND_CALC_INFO("supply:%lu", indexed_metadata[end - 1].metadata.endow);
			return indexed_metadata[end - 1].metadata;
		}
		if (p >= indexed_metadata[mp].key) {
			start = mp + 1;
		} else {
			end = mp;
		}
		mp = (end + start)/2;
	}
}

std::pair<Price, Price> 
Orderbook::get_execution_prices(
	Price sell_price, 
	Price buy_price, 
	const uint8_t smooth_mult) const 
{
	uint8_t extra_bits_len = (64-price::PRICE_RADIX);
	uint128_t ratio = (((uint128_t)sell_price)<<64) / buy_price;
	ratio >>= extra_bits_len;

	Price upper_bound_price = ratio & UINT64_MAX;
	Price lower_bound_price = upper_bound_price;
	if (smooth_mult) {
		lower_bound_price = upper_bound_price - (upper_bound_price>>smooth_mult);
	}
	return std::make_pair(lower_bound_price, upper_bound_price);
}

std::pair<Price, Price> 
Orderbook::get_execution_prices(
	const Price* prices, const uint8_t smooth_mult) const 
{
	auto sell_price = prices[category.sellAsset];
	auto buy_price = prices[category.buyAsset];
	return get_execution_prices(sell_price, buy_price, smooth_mult);
}

std::pair<uint64_t, uint64_t>
Orderbook::get_supply_bounds(
	const Price* prices, const uint8_t smooth_mult) const {	
	
	auto [lower_bound_price, upper_bound_price] 
		= get_execution_prices(prices, smooth_mult);

	uint64_t upper_bound = get_metadata(upper_bound_price).endow;
	uint64_t lower_bound = get_metadata(lower_bound_price).endow;

	return std::make_pair(lower_bound, upper_bound);
}

std::pair<uint64_t, uint64_t>
Orderbook::get_supply_bounds(
	Price sell_price, 
	Price buy_price, 
	const uint8_t smooth_mult) const 
{
	auto [lower_bound_price, upper_bound_price] 
		= get_execution_prices(sell_price, buy_price, smooth_mult);

	uint64_t upper_bound = get_metadata(upper_bound_price).endow;
	uint64_t lower_bound = get_metadata(lower_bound_price).endow;
	
	return std::make_pair(lower_bound, upper_bound);
}

void 
Orderbook::calculate_demands_and_supplies_times_prices(
	const Price* prices, 
	uint128_t* demands_workspace, 
	uint128_t* supplies_workspace,
	const uint8_t smooth_mult) {

	auto [full_exec_p, partial_exec_p] = get_execution_prices(prices, smooth_mult);
	
	auto sell_price = prices[category.sellAsset];
	auto buy_price = prices[category.buyAsset];

	auto metadata_partial = get_metadata(partial_exec_p);
	auto metadata_full = metadata_partial;
	if (smooth_mult) /* partial_exec_p != full_exec_p */{
		metadata_full = get_metadata(full_exec_p);
	}

	calculate_demands_and_supplies_times_prices_from_metadata(prices, demands_workspace, supplies_workspace, smooth_mult, metadata_partial, metadata_full);
}

void Orderbook::calculate_demands_and_supplies_times_prices_from_metadata(
	const Price* prices,
	uint128_t* demands_workspace,
	uint128_t* supplies_workspace,
	const uint8_t smooth_mult,
	const EndowAccumulator& metadata_partial,
	const EndowAccumulator& metadata_full) {


	Price sell_price = prices[category.sellAsset];
	Price buy_price = prices[category.buyAsset];

	//demands_workspace & supplies_workspace will output quantities of (endowment * price)
	// same "units" as partial exec metadata.  That is, radix of PRICE_RADIX, and are (64 + PRICE_BIT_LEN)-bit integers.

	uint64_t full_exec_endow = metadata_full.endow;
	uint64_t partial_exec_endow = metadata_partial.endow - full_exec_endow;

	uint128_t full_exec_endow_times_price = metadata_full.endow_times_price;
	uint128_t partial_exec_endow_times_price = metadata_partial.endow_times_price - full_exec_endow_times_price;
	
	if (metadata_full.endow_times_price > metadata_partial.endow_times_price) {
		throw std::runtime_error("This should absolutely never happen, and means indexed_metadata or binary search is broken (or maybe an overflow)");
	}

	uint128_t full_exec_trade_volume = static_cast<uint128_t>(full_exec_endow) * static_cast<uint128_t>(sell_price);
	uint128_t partial_exec_trade_volume = 0;

	auto wide_multiply_safe = [] (uint128_t const& endow_times_limit_price, uint64_t const& price) -> uint128_t {
		uint128_t upper = endow_times_limit_price >> 64;
		uint128_t lower = endow_times_limit_price & UINT64_MAX;

		upper *= price;
		lower *= price;

		return (upper << (64 - price::PRICE_RADIX)) + (lower >> price::PRICE_RADIX);
	};

	if (smooth_mult > 0) {
		// REQUIRE: smooth_mult + price::PRICE_BIT_LEN <= 63
		// e.g. smooth_mult <= 15

		uint128_t part1 = (static_cast<uint128_t>(sell_price) * static_cast<uint128_t>(partial_exec_endow));
		uint128_t part2 = wide_multiply_safe(partial_exec_endow_times_price, buy_price);

		if (part1 < part2) {
			throw std::runtime_error("arithmetic error");
		}
		partial_exec_trade_volume = (part1 - part2) << smooth_mult;
	}

	uint128_t total_trade_volume = full_exec_trade_volume + partial_exec_trade_volume;

	demands_workspace[category.buyAsset] += total_trade_volume;
	supplies_workspace[category.sellAsset] += total_trade_volume;


}

void Orderbook::calculate_demands_and_supplies_from_metadata(
	const Price* prices, 
	uint128_t* demands_workspace,
	uint128_t* supplies_workspace,
	const uint8_t smooth_mult, 
	const EndowAccumulator& metadata_partial,
	const EndowAccumulator& metadata_full) {

	auto sell_price = prices[category.sellAsset];
	auto buy_price = prices[category.buyAsset];

	uint64_t full_exec_endow = metadata_full.endow;

	uint64_t partial_exec_endow = metadata_partial.endow - full_exec_endow;//radix:0

	uint128_t full_exec_endow_times_price = metadata_full.endow_times_price; // radix:PRICE_RADIX
	uint128_t partial_exec_endow_times_price = metadata_partial.endow_times_price - full_exec_endow_times_price; //radix:PRICE_RADIX
	if (metadata_full.endow_times_price > metadata_partial.endow_times_price) {
		throw std::runtime_error("This should absolutely never happen, and means indexed_metadata or binary search is broken (or maybe an overflow)");
	}

	uint128_t partial_sell_volume = 0; //radix:PRICE_RADIX
	uint128_t partial_buy_volume = 0;  //radix:PRICE_RADIX


	if (smooth_mult) {

		//net endow times ratio is at most (partial exec endow) * sell_over_buy.
		uint128_t endow_over_epsilon = (partial_exec_endow) << smooth_mult; // radix:0

		uint128_t endow_times_price_over_epsilon = (partial_exec_endow_times_price)<<smooth_mult; //radix:PRICE_RADIX


		uint128_t sell_wide_multiply_result = price::wide_multiply_val_by_a_over_b(endow_times_price_over_epsilon, buy_price, sell_price); //radix:PRICE_RADIX

		partial_sell_volume = (endow_over_epsilon<<price::PRICE_RADIX) - (sell_wide_multiply_result); //radix:PRICE_RADIX
		if ((endow_over_epsilon<<price::PRICE_RADIX) < (sell_wide_multiply_result)) {
			throw std::runtime_error("this should not happen unless something has begun to overflow");
		}

		uint128_t buy_wide_multiply_result = price::wide_multiply_val_by_a_over_b(
			endow_over_epsilon<<price::PRICE_RADIX,
			sell_price,
			buy_price);
		partial_buy_volume = (buy_wide_multiply_result - endow_times_price_over_epsilon); // radix:PRICE_RADIX

		if (buy_wide_multiply_result < endow_times_price_over_epsilon) {
			partial_buy_volume = 0;
			std::printf("weird2 %lf\n", (double) (endow_times_price_over_epsilon - buy_wide_multiply_result));

			throw std::runtime_error("this should not happen unless something has begun to overflow");
			//sanity check
		}
	}

	uint128_t full_sell_volume = partial_sell_volume 
		+ (((uint128_t)full_exec_endow)<<price::PRICE_RADIX);


	auto full_buy_volume = partial_buy_volume 
		+ price::wide_multiply_val_by_a_over_b(((uint128_t)full_exec_endow)<<price::PRICE_RADIX, sell_price, buy_price);

	demands_workspace[category.buyAsset] += full_buy_volume;
	supplies_workspace[category.sellAsset] += full_sell_volume;
}

void Orderbook::calculate_demands_and_supplies(
	const Price* prices, 
	uint128_t* demands_workspace, 
	uint128_t* supplies_workspace,
	const uint8_t smooth_mult) {

	auto [full_exec_p, partial_exec_p] = get_execution_prices(prices, smooth_mult);
	
	auto sell_price = prices[category.sellAsset];
	auto buy_price = prices[category.buyAsset];

	auto metadata_partial = get_metadata(partial_exec_p);
	auto metadata_full = metadata_partial;
	if (smooth_mult) /* partial_exec_p != full_exec_p */{
		metadata_full = get_metadata(full_exec_p);
	}

	uint64_t full_exec_endow = metadata_full.endow;

	uint64_t partial_exec_endow = metadata_partial.endow - full_exec_endow;//radix:0

	uint128_t full_exec_endow_times_price = metadata_full.endow_times_price; // radix:PRICE_RADIX
	uint128_t partial_exec_endow_times_price = metadata_partial.endow_times_price - full_exec_endow_times_price; //radix:PRICE_RADIX
	if (metadata_full.endow_times_price > metadata_partial.endow_times_price) {
		throw std::runtime_error("This should absolutely never happen, and means indexed_metadata or binary search is broken (or maybe an overflow)");
	}

	uint128_t partial_sell_volume = 0; //radix:PRICE_RADIX
	uint128_t partial_buy_volume = 0;  //radix:PRICE_RADIX


	if (smooth_mult) {


		//net endow times ratio is at most (partial exec endow) * sell_over_buy.
		uint128_t endow_over_epsilon = (partial_exec_endow) << smooth_mult; // radix:0

		uint128_t endow_times_price_over_epsilon = (partial_exec_endow_times_price)<<smooth_mult; //radix:PRICE_RADIX

		uint128_t sell_wide_multiply_result = price::wide_multiply_val_by_a_over_b(endow_times_price_over_epsilon, buy_price, sell_price); //radix:PRICE_RADIX

		partial_sell_volume = (endow_over_epsilon<<price::PRICE_RADIX) - (sell_wide_multiply_result); //radix:PRICE_RADIX
		if ((endow_over_epsilon<<price::PRICE_RADIX) < (sell_wide_multiply_result)) {
			throw std::runtime_error("this should not happen unless something has begun to overflow");
		}

		uint128_t buy_wide_multiply_result = price::wide_multiply_val_by_a_over_b(
			endow_over_epsilon<<price::PRICE_RADIX,
			sell_price,
			buy_price);
		partial_buy_volume = (buy_wide_multiply_result - endow_times_price_over_epsilon); // radix:PRICE_RADIX
		if (buy_wide_multiply_result < endow_times_price_over_epsilon) {
			partial_buy_volume = 0;
			std::printf("weird2 %lf\n", (double) (endow_times_price_over_epsilon - buy_wide_multiply_result));

			throw std::runtime_error("this should not happen unless something has begun to overflow");
			//sanity check
		}
	}

	uint128_t full_sell_volume = partial_sell_volume 
		+ (((uint128_t)full_exec_endow)<<price::PRICE_RADIX);


	auto full_buy_volume = partial_buy_volume 
		+ price::wide_multiply_val_by_a_over_b(((uint128_t)full_exec_endow)<<price::PRICE_RADIX, sell_price, buy_price);


	demands_workspace[category.buyAsset] += full_buy_volume;
	supplies_workspace[category.sellAsset] += full_sell_volume;
}

bool Orderbook::tentative_clear_offers_for_validation(
	MemoryDatabase& db,
	SerialAccountModificationLog& serial_account_log,
	SingleValidationStatistics& validation_statistics,
	const SingleOrderbookStateCommitmentChecker& local_clearing_log,
	const OrderbookStateCommitmentChecker& clearing_commitment_log,
	BlockStateUpdateStatsWrapper& state_update_stats){

	prefix_t partialExecThresholdKey(local_clearing_log.partialExecThresholdKey);

	int64_t endow_below_partial_exec_key = committed_offers.endow_lt_key(local_clearing_log.partialExecThresholdKey);
	validation_statistics.activated_supply += FractionalAsset::from_integral(endow_below_partial_exec_key);

	Price sellPrice = clearing_commitment_log.prices[category.sellAsset];
	Price buyPrice = clearing_commitment_log.prices[category.buyAsset];

	CompleteClearingFunc func(sellPrice, buyPrice, clearing_commitment_log.tax_rate, db, serial_account_log);

	unsigned char zero_buf[ORDERBOOK_KEY_LEN];
	memset(zero_buf, 0, ORDERBOOK_KEY_LEN);

	auto partial_exec_offer_opt = committed_offers.perform_deletion(local_clearing_log.partialExecThresholdKey);

	//if no partial exec offer, partial exec threshold key in block header must be all zeros.
	if (!partial_exec_offer_opt) {
		INFO("no partial exec offer");
		

		if (memcmp(local_clearing_log.partialExecThresholdKey.data(), zero_buf, ORDERBOOK_KEY_LEN)!= 0) {
			std::printf("key was not zero\n");
			return false;
		}
		if (local_clearing_log.partialExecOfferActivationAmount() != FractionalAsset::from_integral(0)) {
			std::printf("partial activate amt was nonzero\n");
			return false;
		}

		validation_statistics.activated_supply += FractionalAsset::from_integral(committed_offers.get_root_metadata().endow);
		try {
			committed_offers.apply(func);
		} catch (...) {
			std::printf("failed apply WHEN NO PARTIAL EXEC OFFER in category sell %u buy %u with endow_below_partial_exec_key %ld\n", category.sellAsset, category.buyAsset, endow_below_partial_exec_key);
			std::printf("committed offers sz was %lu\n", committed_offers.size());
			std::fflush(stdout);
			throw;
		}
		state_update_stats.fully_clear_offer_count += committed_offers.size();

		{

			auto lock = lmdb_instance.lock();
			//std::lock_guard lock(*lmdb_instance.mtx);
			auto& thunk = lmdb_instance.get_top_thunk_nolock();
			thunk.set_no_partial_exec();
			thunk.cleared_offers = std::move(committed_offers);
			committed_offers.clear();
		}

		INFO("no partial exec correct exit");
		return true;
	}


	auto partial_exec_offer = *partial_exec_offer_opt;

	serial_account_log.log_self_modification(partial_exec_offer.owner, partial_exec_offer.offerId);

	int64_t partial_exec_sell_amount, partial_exec_buy_amount;

	UserAccount* db_idx = db.lookup_user(partial_exec_offer.owner);

	//if (!db.lookup_user_id(partial_exec_offer.owner, &db_idx)) {
	if (db_idx == nullptr) {
		std::printf("couldn't lookup user\n");
		committed_offers.insert(local_clearing_log.partialExecThresholdKey, partial_exec_offer);
		return false;
	}

	clear_offer_partial(
		partial_exec_offer, 
		clearing_commitment_log.prices[partial_exec_offer.category.sellAsset],
		clearing_commitment_log.prices[partial_exec_offer.category.buyAsset],
		clearing_commitment_log.tax_rate,
		local_clearing_log.partialExecOfferActivationAmount(),
		db, 
		db_idx,
		partial_exec_sell_amount,
		partial_exec_buy_amount);

	if ((uint64_t)partial_exec_sell_amount > partial_exec_offer.amount) {
		std::printf("sell amount too high: partial_exec_sell_amount %ld partial_exec_offer.amount %ld\n", partial_exec_sell_amount, partial_exec_offer.amount);
		committed_offers.insert(local_clearing_log.partialExecThresholdKey, partial_exec_offer);
		return false;
	}

	{
		auto lock = lmdb_instance.lock();
		//std::lock_guard lock(*lmdb_instance.mtx);
		auto& thunk = lmdb_instance.get_top_thunk_nolock();

		thunk.set_partial_exec(
			local_clearing_log.partialExecThresholdKey, partial_exec_sell_amount, partial_exec_offer);

		// achieves same effect as a hypothetical committed_offers.split_lt_key

		thunk.cleared_offers = committed_offers.endow_split(endow_below_partial_exec_key);

		thunk.cleared_offers.apply(func);

		state_update_stats.fully_clear_offer_count += thunk.cleared_offers.size();
	}

	partial_exec_offer.amount -= partial_exec_sell_amount;


	//committed_offers.mark_subtree_lt_key_for_deletion(local_clearing_log.partialExecThresholdKey);
	//committed_offers.apply_lt_key(func, local_clearing_log.partialExecThresholdKey);

	if (partial_exec_offer.amount != 0) {
		//added if statement 4/22/2021
		committed_offers.insert(local_clearing_log.partialExecThresholdKey, partial_exec_offer);
		state_update_stats.partial_clear_offer_count ++;
	}
	return true;
}

void Orderbook::process_clear_offers(
	const OrderbookClearingParams& params, 
	const Price* prices, 
	const uint8_t& tax_rate, 
	MemoryDatabase& db,
	SerialAccountModificationLog& serial_account_log,
	SingleOrderbookStateCommitment& clearing_commitment_log,
	BlockStateUpdateStatsWrapper& state_update_stats) {
	

	auto clear_amount = params.supply_activated.floor();
	if (clear_amount > INT64_MAX) {
		throw std::runtime_error("trying to clear more than there should exist");
	}

	write_unsigned_big_endian(
		clearing_commitment_log.fractionalSupplyActivated, 
		params.supply_activated.value);

	auto fully_cleared_trie = committed_offers.endow_split(clear_amount);

	Price sellPrice = prices[category.sellAsset];
	Price buyPrice = prices[category.buyAsset];

	CompleteClearingFunc func(sellPrice, buyPrice, tax_rate, db, serial_account_log);

/*	tbb::this_task_arena::isolate([&func, &fully_cleared_trie]() {
		fully_cleared_trie.parallel_apply(func);
	});
*/
	//todo see if coroutines still don't make sense
	try {
		fully_cleared_trie.apply(func);
	} catch (...) {
		fully_cleared_trie._log("fully cleared trie: ");

		std::fflush(stdout);
		throw;
	}
	state_update_stats.fully_clear_offer_count += fully_cleared_trie.size();

	
	auto remaining_to_clear = params.supply_activated 
		- FractionalAsset::from_integral(fully_cleared_trie.get_root_metadata().endow);

	{
		auto lock = lmdb_instance.lock();
		//std::lock_guard lock(*lmdb_instance.mtx);
		lmdb_instance.get_top_thunk_nolock().cleared_offers = std::move(fully_cleared_trie);
		fully_cleared_trie.clear();
	}
	
	write_unsigned_big_endian(clearing_commitment_log.partialExecOfferActivationAmount, remaining_to_clear.value);

	std::optional<prefix_t> partial_exec_key = committed_offers.get_lowest_key();

	if ((!partial_exec_key) && remaining_to_clear != FractionalAsset::from_integral(0)) {
		throw std::runtime_error("null partial_exec_key (lowest offer key) but remaining to clear is nonzero");
	}

	if (!partial_exec_key) {
		INFO("partial exec key is nullptr");
		INTEGRITY_CHECK("remaining offers size: %d", committed_offers.size());
		//no committed offers remain
		auto lock = lmdb_instance.lock();
		//std::lock_guard lock(*lmdb_instance.mtx);
		lmdb_instance.get_top_thunk_nolock().set_no_partial_exec();
		clearing_commitment_log.partialExecThresholdKey.fill(0);
		clearing_commitment_log.thresholdKeyIsNull = 1;


		return;
	}
	clearing_commitment_log.thresholdKeyIsNull = 0;

	auto try_delete = (committed_offers.perform_deletion(*partial_exec_key));

	if (!try_delete) {
		throw std::runtime_error("couldn't find partial exec offer!!!");
	}

	auto partial_exec_offer = *try_delete;

	serial_account_log.log_self_modification(
		partial_exec_offer.owner, partial_exec_offer.offerId);

	int64_t buy_amount, sell_amount;

	UserAccount* idx = db.lookup_user(partial_exec_offer.owner);
	//auto result = db.lookup_user_id(partial_exec_offer.owner, &idx);
	//if (!result) {
	if (idx == nullptr) {
		throw std::runtime_error(
			"(partialexec) Offer in manager from nonexistent account");
	}

	clear_offer_partial(partial_exec_offer, sellPrice, buyPrice, tax_rate, remaining_to_clear, db, idx, sell_amount,buy_amount);

	if (partial_exec_offer.amount < (uint64_t) sell_amount) {
		throw std::runtime_error(
			"should not have been partially clearing this offer");
	}

	auto lock = lmdb_instance.lock();
	//std::lock_guard lock(*lmdb_instance.mtx);
	lmdb_instance.get_top_thunk_nolock().set_partial_exec(
		*partial_exec_key, sell_amount, partial_exec_offer);
	
	clearing_commitment_log.partialExecThresholdKey = partial_exec_key->get_bytes_array();


	partial_exec_offer.amount -= sell_amount;

	//db.transfer_escrow(idx, category.sellAsset, -sell_amount);
	//db.transfer_available(idx, category.buyAsset, buy_amount);

	if (partial_exec_offer.amount > 0) {
		//std::printf("starting last committed offers insert\n");
		//committed_offers._log("committed offers ");
		committed_offers.insert(*partial_exec_key, partial_exec_offer);
		//std::printf("ending last committed offers insert\n");
		state_update_stats.partial_clear_offer_count++;
	} else if (partial_exec_offer.amount < 0) {
		throw std::runtime_error("how on earth did partial_exec_offer.amount become less than 0");
	}

	//TODO partial exec rebate policy?
}

void Orderbook::rollback_thunks(uint64_t current_block_number) {
	//std::lock_guard lock(*lmdb_instance.mtx);
	auto lock = lmdb_instance.lock();
	if (current_block_number < lmdb_instance.get_persisted_round_number()) {
		throw std::runtime_error("can't rollback persisted objects");
	}

	auto& thunks = lmdb_instance.get_thunks_ref();

	for (size_t i = 0; i < thunks.size();) {
		if (thunks[i].current_block_number > current_block_number) {

			undo_thunk(thunks[i]);

			thunks.erase(thunks.begin() + i);
		} else {
			i++;
		}
	}
}

void Orderbook::load_lmdb_contents_to_memory() {
	auto rtx = lmdb_instance.rbegin();
	auto cursor = rtx.cursor_open(lmdb_instance.get_data_dbi());

	prefix_t key_buf;

	for (auto kv : cursor) {
		Offer offer;
		dbval_to_xdr(kv.second, offer);
		generate_orderbook_trie_key(offer, key_buf);
		if (offer.amount <= 0) {

			std::printf("offer.owner = %lu offer.amount = %ld offer.offerId = %lu sellAsset %u buyAsset %u\n",
					offer.owner,
					offer.amount,
					offer.offerId,
					offer.category.sellAsset,
					offer.category.buyAsset);
			std::fflush(stdout);
			throw std::runtime_error("invalid offer amount present in database!");
		}
		committed_offers.insert(key_buf, OfferWrapper(offer));
	}

	generate_metadata_index();
}

} /* speedex */
