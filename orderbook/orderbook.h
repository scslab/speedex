#pragma once

#include <cstdint>
#include <iomanip>
#include <sstream>

#include <xdrpp/marshal.h>

#include "memory_database/memory_database.h"

#include "modlog/account_modification_log.h"

#include "orderbook/commitment_checker.h"
#include "orderbook/helpers.h"
#include "orderbook/lmdb.h"
#include "orderbook/metadata.h"
#include "orderbook/offer_clearing_logic.h"
#include "orderbook/offer_clearing_params.h"
#include "orderbook/thunk.h"
#include "orderbook/typedefs.h"

#include "stats/block_update_stats.h"

#include "trie/merkle_trie.h"

#include "utils/big_endian.h"
#include "utils/price.h"

namespace speedex {

typedef __int128 int128_t;
typedef unsigned __int128 uint128_t;

class Orderbook {

	const OfferCategory category;

	using prefix_t = OrderbookTriePrefix;
	
	OrderbookTrie committed_offers;

	OrderbookTrie uncommitted_offers;
	OrderbookLMDB lmdb_instance;

	struct FuncWrapper {
		static Price eval(const prefix_t& buf) {
			return price::read_price_big_endian(buf);
		}
	};

	using IndexType = IndexedMetadata
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

	void persist_lmdb(uint64_t current_block_number);

	void add_offers(OrderbookTrie&& offers) {
		INFO("merging in to \"%d %d\"", category.sellAsset, category.buyAsset);
		uncommitted_offers.merge_in(std::move(offers));
	}

	std::optional<Offer> mark_for_deletion(const prefix_t key) {
		return committed_offers.mark_for_deletion(key);
	}

	std::optional<Offer> unmark_for_deletion(const prefix_t key) {
		return committed_offers.unmark_for_deletion(key);
	}

	friend class MerkleWorkUnitManager;

	void rollback_validation();
	void finalize_validation();

	void rollback_thunks(uint64_t current_block_number);

	std::string get_lmdb_env_name() {
		return std::string(ROOT_DB_DIRECTORY) 
		+ std::string(OFFER_DB) 
		+ std::to_string(category.sellAsset) 
		+ "_" 
		+ std::to_string(category.buyAsset) 
		+ std::string("/");
	}

	std::string get_lmdb_db_name() {
		return "offers";
	}

	void load_lmdb_contents_to_memory();

public:
	Orderbook(OfferCategory category)
	: category(category), 
	  committed_offers(),
	  uncommitted_offers(),
	  lmdb_instance(), 
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


	void process_clear_offers(
		const OrderbookClearingParams& params, 
		const Price* prices, 
		const uint8_t& tax_rate, 
		MemoryDatabase& db,
		SerialAccountModificationLog& serial_account_log,
		SingleOrderbookStateCommitment& clearing_commitment_log,
		BlockStateUpdateStatsWrapper& state_update_stats);
	
	//make the inserted things marked with some kind of metadata, which is deletable
	bool tentative_clear_offers_for_validation(
		MemoryDatabase& db,
		SerialAccountModificationLog& serial_account_log,
		SingleValidationStatistics& validation_statistics,
		const SingleOrderbookStateCommitmentChecker& local_clearing_log,
		const OrderbookStateCommitmentChecker& clearing_commitment_log,
		BlockStateUpdateStatsWrapper& state_update_stats);

	void open_lmdb_env() {
		lmdb_instance.open_env(get_lmdb_env_name());
	}

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

	std::pair<Price, Price> get_execution_prices(const Price* prices, const uint8_t smooth_mult) const;
	std::pair<Price, Price> get_execution_prices(Price sell_price, Price buy_price, const uint8_t smooth_mult) const;

	EndowAccumulator get_metadata(Price p) const;
	//GetMetadataTask coro_get_metadata(Price p, EndowAccumulator& endow_out, DemandCalcScheduler& scheduler) const;

	void calculate_demands_and_supplies(
		const Price* prices, 
		uint128_t* demands_workspace, 
		uint128_t* supplies_workspace,
		const uint8_t smooth_mult);

	void calculate_demands_and_supplies_from_metadata(
		const Price* prices, 
		uint128_t* demands_workspace,
		uint128_t* supplies_workspace,
		const uint8_t smooth_mult, 
		const EndowAccumulator& metadata_partial,
		const EndowAccumulator& metadata_full);

	uint8_t max_feasible_smooth_mult(int64_t amount, const Price* prices) const;
	double max_feasible_smooth_mult_double(int64_t amount, const Price* prices) const;

	size_t num_open_offers() const;

	std::pair<uint64_t, uint64_t> get_supply_bounds(
		const Price* prices, const uint8_t smooth_mult) const;
	std::pair<uint64_t, uint64_t> get_supply_bounds(
		Price sell_price, Price buy_price, const uint8_t smooth_mult) const;

	const std::vector<IndexType>& get_indexed_metadata() const {
		return indexed_metadata;
	}

	size_t get_index_nnz() const {
		return indexed_metadata.size() - 1; // first entry of index is 0, so ignore
	}

	OfferCategory get_category() const {
		return category;
	}

	size_t size() const {
		return committed_offers.size();
	}
};

} /* namespace speedex */
