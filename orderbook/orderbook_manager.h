#pragma once 

#include <cstdint>
#include <mutex>
#include <vector>

#include "modlog/account_modification_log.h"

#include "orderbook/lmdb.h"
#include "orderbook/offer_clearing_params.h"
#include "orderbook/orderbook.h"
#include "orderbook/utils.h"

#include "stats/block_update_stats.h"
#include "utils/fixed_point_value.h"

#include "xdr/types.h"

namespace speedex {


/*! Maintains a list of orderbooks.

The vast majority of the operations on the orderbooks are some form of iteration
over all orderbooks.  Most of the methods in this class are tbb loops 
over calls to individual orderbook methods.  Loops are done serially when
necessary (i.e. the persistence is done in one background thread, so as
to avoid using too many tbb threads in lmdb sync).

Trade categories (asset pairs) are mapped to integer indices.
Many operations require looking up these indices in advance.
Adding other types of operations would entail extending this category-index
correspondence.


Block validation workflow:

SerialTransactionValidator runs through offer lists, 
autoclears anything below partial execution threshold listed in block.  
Logs amount of supply activated.
All new offers added are marked as rollbackable.


Next, we call commit_for_validation.  This merges tries together 
(needed to compute hashes), starts persistence thunk preparation.

THen we call tentative_clear_offers_for_validation.  This is responsible for 
marking cleared entries as deleted, logging supply activation, and making the 
partial exec modifications.
This returns true if supply activations add up.  Also finishes construction 
of the persistence thunks.

Then we do external validity checks.  Check hashes, clearing, 
supply activations add up, etc.

If those pass, we call finalize_validation.  
If fails, we call rollback_validation.

Rollback validation clears the persistence thunk that was just built.

*/
class OrderbookManager {

	std::vector<Orderbook> orderbooks;

	uint16_t num_assets;
	
	template<auto func, typename... Args>
	void generic_map(Args... args);

	template<auto func, typename... Args>
	void generic_map_serial(Args... args);

	template<auto func, typename... Args>
	void generic_map_loading(uint64_t current_block_number, Args... args);

	//primarily useful for reducing fsanitize=thread false positives
	mutable std::mutex mtx;

	using trie_t = OrderbookTrie::TrieT;

	BackgroundDeleter<trie_t> thunk_garbage_deleter;

	OrderbookManagerLMDB lmdb;

public:

	using prefix_t = OrderbookTriePrefix;

	OrderbookManager(
		uint16_t num_new_assets)
		: orderbooks()
		, num_assets(0) {
		increase_num_traded_assets(num_new_assets);
		num_assets = num_new_assets;
	}

	OrderbookManager(const OrderbookManager& other) = delete;
	OrderbookManager(OrderbookManager&& other) = delete;

	void clear_();

	//! Add a set of offers to a particular orderbook index.  Index
	//! should be looked up in advance.
	void add_offers(int idx, OrderbookTrie&& trie) {
		orderbooks[idx].add_offers(std::move(trie));
	}

	//! Mark an existing offer for deletion.
	//! Returns nullopt if offer did not exist.
	std::optional<Offer> mark_for_deletion(int idx, const prefix_t& key) {
		return orderbooks[idx].mark_for_deletion(key);
	}

	//! Unmark an offer for deletion.
	void unmark_for_deletion(int idx, const prefix_t& key) {
		orderbooks[idx].unmark_for_deletion(key);
	}

	//! Get the persistence round of orderbook index \a idx.
	uint64_t get_persisted_round_number(int idx) {
		return orderbooks[idx].get_persisted_round_number();
	}

	//! Get the lowest persistence round of all orderbooks.
	uint64_t get_min_persisted_round_number() {
		uint64_t min = UINT64_MAX;
		for (const auto& orderbook : orderbooks) {
			min = std::min(min, orderbook.get_persisted_round_number());
		}
		return min;
	}

	//! Get the highest persistence round of all orderbooks.
	uint64_t get_max_persisted_round_number() {
		uint64_t max = 0;
		for (const auto& orderbook : orderbooks) {
			max = std::max(max, orderbook.get_persisted_round_number());
		}
		return max;
	}

	//! Change the number of assets traded by the orderbooks managed here.
	void increase_num_traded_assets(uint16_t new_asset_count);

	//! Return a reference to the list of orderbooks.
	std::vector<Orderbook>& get_orderbooks() {
		return orderbooks;
	}

	size_t get_num_orderbooks() const {
		return get_num_orderbooks_by_asset_count(num_assets);
	}

	size_t get_work_unit_size(int idx) const {
		return orderbooks[idx].size();
	}

	int look_up_idx(const OfferCategory& id) const {
		return category_to_idx(id, num_assets);
	}

	uint16_t get_num_assets() const {
		return num_assets;
	}

	bool validate_category(const OfferCategory& category) {
		return validate_category_(category, num_assets);
	}

	size_t get_total_nnz() const;

	//! Clear a set of offers, when operating in block production mode.
	void clear_offers_for_production(
		const ClearingParams& params, 
		Price* prices, 
		MemoryDatabase& db, 
		AccountModificationLog& account_log, 
		OrderbookStateCommitment& clearing_details_out,
		BlockStateUpdateStatsWrapper& state_update_stats);

	//! Clear a set of offers, when operating in block validation mode.
	bool tentative_clear_offers_for_validation(
		MemoryDatabase& db,
		AccountModificationLog& account_modification_log,
		ThreadsafeValidationStatistics& validation_statistics,
		const OrderbookStateCommitmentChecker& clearing_commitment_log,
		BlockStateUpdateStatsWrapper& state_update_stats);

	//! Clear a set of offers, when operating in data reloading mode.
	//! This one is essentially the same as for validation (it's replaying
	//! an existing block) but it throws an error if there's some validation
	//! error (this should only be run on trusted blocks known to be committed).
	void clear_offers_for_data_loading(
		MemoryDatabase& db,
		AccountModificationLog& account_modification_log,
		ThreadsafeValidationStatistics& validation_statistics,
		const OrderbookStateCommitmentChecker& clearing_commitment_log,
		const uint64_t current_block_number);

	//! Commit orderbooks when operating in block production mode.
	void commit_for_production(uint64_t current_block_number);

	//! Tentatively commit when operating in block validation mode.
	//! Only difference from commit_for_production() is that this
	//! does not generate a metadata index for each orderbook.
	void commit_for_validation(uint64_t current_block_number);

	//! Commit when in data loading mode.  Same logic as 
	//! for validation, but only runs if disk state does not reflect
	//! current_block_number's inputs yet.
	void commit_for_loading(uint64_t current_block_number);

	//! Call when a validation check fails.  Rolls back orderbooks to
	//! previously committed state.
	//! Only when in validation mode.
	void rollback_validation();
	//! Finalizes the changes that were made tentatively when validationg
	//! a block.  Call after all validation checks pass when in validation
	//! mode.
	void finalize_validation();

	void finalize_for_loading(uint64_t current_block_number);

	//! Load persisted data contents to memory.
	void load_lmdb_contents_to_memory();

	//! Rollback many persistence thunks.  Cannot rollback thunks that
	//! were persisted to disks.
	//! Only relevant if there were a block reorganization.
	void rollback_thunks(uint64_t current_block_number);

	//! Linear pass over orderbooks to accumulate endow/endow*price running
	//! sums.  A tatonnement preprocessing step.
	void generate_metadata_indices();

	void create_lmdb();
	//! Persist lmdb thunks up to \a current_block_humber
	void persist_lmdb(uint64_t current_block_number);

	//! Persist lmdb thunks, when operating in data loading mode.
	//! (i.e. is a no-op if lmdb already reflects current_block_number).
	void persist_lmdb_for_loading(uint64_t current_block_number);
	void open_lmdb_env();
	void open_lmdb();

	//! Hash all of the orderbooks, storing results in clearing_details.
	void hash(OrderbookStateCommitment& clearing_details);

	uint8_t get_max_feasible_smooth_mult(
		const ClearingParams& clearing_params, Price* prices);

	//! Compute a volume-weighted price asymmetry metric.
	//! Used when tatonnement times out to quantify efficienty loss.
	double
	get_weighted_price_asymmetry_metric(
		const ClearingParams& clearing_params,
		const std::vector<Price>& prices)  const;

	size_t num_open_offers() const;
};

} /* speedex */
