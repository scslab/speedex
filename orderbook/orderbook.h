#pragma once

/*! \file orderbook.h

Manage a set of offers trading one fixed asset for another fixed asset.

*/


#include <cstdint>
#include <iomanip>
#include <sstream>

#include <xdrpp/marshal.h>

#include "utils/debug_macros.h"

#include "orderbook/helpers.h"
#include "orderbook/lmdb.h"
#include "orderbook/metadata.h"
#include "orderbook/offer_clearing_logic.h"
#include "orderbook/offer_clearing_params.h"
#include "orderbook/thunk.h"
#include "orderbook/typedefs.h"

namespace speedex {

typedef __int128 int128_t;
typedef unsigned __int128 uint128_t;

/*! One orderbooks is a collection of all offers trading one fixed asset
 for another fixed asset.

Workflow when producing a block:

1. Iterate over transactions, get new offers, etc.  Then call add_offers
on all those new offers.  These offers go into uncommitted_offers (no
additional metadata).
2. commit_for_production() merges uncommitted_offers into committed_offers.
This also runs generate_metadata_index(), which does a pass over all the offers
Also removes offers marked as deleted.
(Internally, it's a tentative_commit_for_validation call and then
a generate_metadata_index call).
as a Tatonnement preprocessing phase.
3. process_clear_offers() clears the set of open trade offers.

Workflow when validating a block:
1. Interate over nex transactions, get new offers etc.  Then call
add_offers on those new offers.  These offers are marked by RollbackMixin
metadata.
2. tentative_commit_for_validation merges in uncommitted_offers to 
committed_offers, removes offers marked as deleted.
Makes a persistence thunk, and offers deleted are recorded in the thunk
(useful for undoing a failed block).
3. tentative_clear_offers_for_validation clears offers, and performs validation
checks (make sure supply activation amounts add up, partial exec offer is
present, etc).  Returns true if these checks pass.
4. Additional validation checks (external)
5. If those checks pass, finalize_validation (clears rollback markers) 
If they fail, rollback_validation (undoes all state changes).

*/

class BlockStateUpdateStatsWrapper;
class OrderbookStateCommitmentChecker;
class MemoryDatabase;
class SerialAccountModificationLog;
class SingleValidationStatistics;
class SingleOrderbookStateCommitment;
class SingleOrderbookStateCommitmentChecker;

class Orderbook {

	const OfferCategory category;

	using prefix_t = OrderbookTriePrefix;
	
	//! Offers committed to the orderbook.
	OrderbookTrie committed_offers;

	OrderbookTrie uncommitted_offers;
	OrderbookLMDB lmdb_instance;

	struct FuncWrapper {
		static Price eval(const prefix_t& buf) {
			return price::read_price_big_endian(buf);
		}
	};

	using IndexType = trie::IndexedMetadata
					<
						EndowAccumulator,
						Price,
						FuncWrapper
					>;

	std::vector<IndexType> indexed_metadata;

	uint64_t get_persisted_round_number() const {
		return lmdb_instance.get_persisted_round_number();
	}

	void tentative_commit_for_validation(uint64_t current_block_number); //creates thunk
	void commit_for_production(uint64_t current_block_number); // creates lmdb thunk

	void generate_metadata_index();

	void undo_thunk(OrderbookLMDBCommitmentThunk& thunk);

	std::unique_ptr<ThunkGarbage<OrderbookTrie::TrieT>>
	__attribute__((warn_unused_result))
	persist_lmdb(uint64_t current_block_number, dbenv::wtxn& wtx);

	void add_offers(OrderbookTrie&& offers) {
		ORDERBOOK_INFO("merging in to \"%d %d\"", category.sellAsset, category.buyAsset);
		uncommitted_offers.merge_in(std::move(offers));
	}

	std::optional<Offer> mark_for_deletion(const prefix_t key) {
		return committed_offers.mark_for_deletion(key);
	}

	std::optional<Offer> unmark_for_deletion(const prefix_t key) {
		return committed_offers.unmark_for_deletion(key);
	}

	friend class OrderbookManager;

	void rollback_validation();
	void finalize_validation();

	//! Extra parameter is convenient for templating lmdb loading methods
	void finalize_validation_for_loading(uint64_t current_block_number) {
		finalize_validation();
	}

	void rollback_thunks(uint64_t current_block_number);

	std::string get_lmdb_db_name() {
		return std::to_string(category.sellAsset)
			 + std::string(" ")
			 + std::to_string(category.buyAsset); // TODO type
	}

	void load_lmdb_contents_to_memory();

public:
	Orderbook(OfferCategory category, OrderbookManagerLMDB& manager_lmdb)
	: category(category), 
	  committed_offers(),
	  uncommitted_offers(),
	  lmdb_instance(category, manager_lmdb), 
	  indexed_metadata() {
	}

	void clear_() {
		uncommitted_offers.clear();
		committed_offers.clear();
		indexed_metadata.clear();
		lmdb_instance.clear_();
	}

	void log() {
		committed_offers._log("committed_offers: ");
	}

	template<typename DB>
	void process_clear_offers(
		const OrderbookClearingParams& params, 
		const Price* prices, 
		const uint8_t& tax_rate, 
		DB& db,
		SerialAccountModificationLog& serial_account_log,
		SingleOrderbookStateCommitment& clearing_commitment_log,
		BlockStateUpdateStatsWrapper& state_update_stats);
	
	bool tentative_clear_offers_for_validation(
		MemoryDatabase& db,
		SerialAccountModificationLog& serial_account_log,
		SingleValidationStatistics& validation_statistics,
		const SingleOrderbookStateCommitmentChecker& local_clearing_log,
		const OrderbookStateCommitmentChecker& clearing_commitment_log,
		BlockStateUpdateStatsWrapper& state_update_stats);

	void create_lmdb() {
		auto name = get_lmdb_db_name();
		lmdb_instance.create_db(name.c_str());
	}

	void open_lmdb() {
		auto name = get_lmdb_db_name();
		lmdb_instance.open_db(name.c_str());
	}

	void hash(Hash& hash_buf) {
		committed_offers.hash(hash_buf);
	}

	//! Compute the price quotients at which trades happen in this block.
	//! Returns a pair: (full exec ratio, partial exec ratio).
	//! Minimum prices below full exec are guaranteed to fully trade,
	//! and those above partial exec never trade.
	std::pair<Price, Price> 
	get_execution_prices(
		const Price* prices, const uint8_t smooth_mult) const;

	std::pair<Price, Price> 
	get_execution_prices(
		Price sell_price, Price buy_price, const uint8_t smooth_mult) const;

	EndowAccumulator get_metadata(Price p) const;
	//GetMetadataTask coro_get_metadata(Price p, EndowAccumulator& endow_out, DemandCalcScheduler& scheduler) const;

	//! Calculate demand and supply at a given set of prices and a given
	//! smooth mult.
	void calculate_demands_and_supplies(
		const Price* prices, 
		uint128_t* demands_workspace, 
		uint128_t* supplies_workspace,
		const uint8_t smooth_mult);

	void calculate_demands_and_supplies_times_prices(
		const Price* prices, 
		uint128_t* demands_workspace, 
		uint128_t* supplies_workspace,
		const uint8_t smooth_mult);

	//! Calculate demand and supply at given set of prices,
	//! given that the endow calculations (the binary searches) have already
	//! been done.
	void calculate_demands_and_supplies_from_metadata(
		const Price* prices, 
		uint128_t* demands_workspace,
		uint128_t* supplies_workspace,
		const uint8_t smooth_mult, 
		const EndowAccumulator& metadata_partial,
		const EndowAccumulator& metadata_full);

	void calculate_demands_and_supplies_times_prices_from_metadata(
		const Price* prices,
		uint128_t* demands_workspace,
		uint128_t* supplies_workspace,
		const uint8_t smooth_mult,
		const EndowAccumulator& metadata_partial,
		const EndowAccumulator& metadata_full);

	uint8_t max_feasible_smooth_mult(
		int64_t amount, const Price* prices) const;
	double max_feasible_smooth_mult_double(
		int64_t amount, const Price* prices) const;

	std::pair<double, double> satisfied_and_lost_utility(
		int64_t amount, const Price* prices) const;

	size_t num_open_offers() const;

	std::pair<uint64_t, uint64_t> get_supply_bounds(
		const Price* prices, const uint8_t smooth_mult) const;
	std::pair<uint64_t, uint64_t> get_supply_bounds(
		Price sell_price, Price buy_price, const uint8_t smooth_mult) const;

	//! Returns the OfferCategory for this orderbook, which specifies
	//! the buy and sell assets for this orderbook.
	OfferCategory get_category() const {
		return category;
	}

	size_t size() const {
		return committed_offers.size();
	}
};

} /* namespace speedex */
