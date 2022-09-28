#pragma once

/*! \file orderbook_manager_view.h

When processing/validating transactions, speedex does not directly operate
on the orderbook manager.  Instead, it uses one of these manager views to buffer
a set of changes to the manager.

(Marking offers as deleted is not buffered locally, but marked in the main
manager's tries, and the actual trie manipulations to delete offers are done
later).

In this design, the memory database must be persisted before the orderbooks.
When an offer is cancelled, the amount the offer had for sale is returned to
the offer's owner.  If an offer is cancelled (and thus deleted from disk)
before this capital return is persisted in the account database,
a crash could result in an unrecoverable state.
*/
#include <cstdint>

#include "orderbook/typedefs.h"
#include "orderbook/utils.h"

#include "mtt/trie/utils.h"

#include "orderbook/commitment_checker.h"

namespace speedex {

struct SpeedexManagementStructures;
class OrderbookManager;

/*! Mock around an orderbook manager when replaying a block from disk.
Operations are no-op if the corresponding orderbook already reflects
the current block number's state changes
*/
class LoadLMDBManagerView {

	const uint64_t current_block_number;
	OrderbookManager& main_manager;
	
	using prefix_t = OrderbookTriePrefix;
	using trie_t = OrderbookTrie;
public:


	LoadLMDBManagerView(
		uint64_t current_block_number, OrderbookManager& main_manager);

	void add_offers(int idx, trie_t&& trie);

	std::optional<Offer> 
	mark_for_deletion(int idx, const prefix_t& key) ;

	void unmark_for_deletion (int idx, const prefix_t& key);

	unsigned int get_num_orderbooks() const;
	int get_num_assets() const;
	int look_up_idx(const OfferCategory& id) const;

};

/*! Base class for local offer buffers in both block production and validation.

Injects a LoadLMDBManagerView based on the template argument.
*/
template<typename ManagerType = OrderbookManager>
class BaseSerialManager {
	
	using prefix_t = OrderbookTriePrefix;
	using trie_t = OrderbookTrie;
protected:
	using main_manager_t 
		= typename std::conditional<
			std::is_same<LoadLMDBManagerView, ManagerType>::value,
			LoadLMDBManagerView, 
			OrderbookManager&>::type;

	main_manager_t main_manager;
	//! Uses same indexing scheme as in orderbook manager
	std::vector<trie_t> new_offers;

	prefix_t key_buf;

	//! Ensures new_offers is sufficiently large.
	//! TODO use vector.resize() instead.
	void ensure_suffient_new_offers_sz(unsigned int idx) {
		while (idx >= new_offers.size()) {
			new_offers.emplace_back();
		}
	}

	//! Args are empty except when using LoadLMDBViewManager.
	template<typename... Args>
	BaseSerialManager(OrderbookManager& main_manager, Args... args)
		: main_manager(args..., main_manager)
		, new_offers()
		, key_buf() {
		};

public:

	//! Merge contents of view into main orderbook manager.
	//! Does not parallelize well.  Only used in e.g. replaying trusted blocks.
	void finish_merge() {
		auto new_offers_sz = std::min<size_t>(
			new_offers.size(), main_manager.get_num_orderbooks());
		for (size_t i = 0; i < new_offers_sz; i++) {
			main_manager.add_offers(i, std::move(new_offers[i]));
		}
		new_offers.clear();
	}

	//! Merge the changes associated with orderbook index \a idx 
	//! into the main trie.
	void partial_finish(size_t idx) {
		if (new_offers.size() > idx) {
			main_manager.add_offers(idx, std::move(new_offers[idx]));
		}
	}

	//! Call after looping over partial_finish.
	//! Does essentially nothing for in block production (at the moment),
	//! but in validation mode, this commits the local validation stats.
	void partial_finish_conclude() {
		new_offers.clear();
	}

	//! Mark an offer in the main orderbook manager as deleted.
	std::optional<Offer> delete_offer(
		const int idx, 
		const Price min_price, 
		const AccountID owner, 
		const uint64_t offer_id) {
		ensure_suffient_new_offers_sz(idx);
		generate_orderbook_trie_key(min_price, owner, offer_id, key_buf);

		//can't delete an uncommitted offer, so we don't check
		//uncommitted buffer

		return main_manager.mark_for_deletion(idx, key_buf);
	}

	int look_up_idx(const OfferCategory& id) {
		return main_manager.look_up_idx(id);
	}

	const uint16_t get_num_assets() {
		return main_manager.get_num_assets();
	}

	void clear() {
		new_offers.clear();
	}

	//! Validate that an input offer category is well formed.
	bool validate_category(const OfferCategory& category) {
		return validate_category_(category, main_manager.get_num_assets());
	}
};

/*! Local view of the orderbook manager when producing a new block.
*/
class ProcessingSerialManager : public BaseSerialManager<OrderbookManager> {

public:

	ProcessingSerialManager(OrderbookManager& manager) 
		: BaseSerialManager(manager) {}

	//! Flag to tell SerialTransactionProcessor whether to bother
	//! building an account modification log (which is unnecessary when
	//! replaying a trusted block from disk).
	constexpr static bool maintain_account_log = true;


	/*! Undo a call to delete_offer */
	void undelete_offer(
		const int idx, 
		const Price min_price, 
		const AccountID owner, 
		const uint64_t offer_id);

	//! Undo a call do add_offer
	void unwind_add_offer(int idx, const Offer& offer);

	/*! Add a newly created offer to the local database.
	Template arguments are irrelevant in the block production setting (and
	are ignored here) */
	template<typename OpMetadata, typename LogType>
	void add_offer(
		int idx, const Offer& offer, OpMetadata& metadata, LogType& log) 
	{
		ensure_suffient_new_offers_sz(idx);
		generate_orderbook_trie_key(offer, key_buf);

		//This always succeeds because we have a guarantee on uniqueness
		// of offerId (from uniqueness of sequence numbers
		// and from sequential impl of offerId lowbits)

		new_offers.at(idx).insert(key_buf, OfferWrapper(offer));
	}
};

/*! Local view of orderbook manager when validating an existing block.
LoadLMDBManagerView can be swapped in when replaying a block from disk.
*/
template<typename ManagerType = OrderbookManager>
class ValidatingSerialManager : public BaseSerialManager<ManagerType> {

	const OrderbookStateCommitmentChecker& clearing_commitment;
	ValidationStatistics activated_supplies;
	ThreadsafeValidationStatistics& main_stats;
	
	void ensure_suffient_new_offers_sz(unsigned int idx) {

		activated_supplies.make_minimum_size(idx);
		BaseSerialManager<ManagerType>::ensure_suffient_new_offers_sz(idx);
	}

	using BaseSerialManager<ManagerType>::key_buf;
	using BaseSerialManager<ManagerType>::new_offers;


public:
	
	//! No need to maintain account log when replaying trusted block.
	constexpr static bool maintain_account_log =
		 std::is_same<ManagerType, OrderbookManager>::value;

	template<typename ...Args>
	ValidatingSerialManager(
		OrderbookManager& main_manager, 
		const OrderbookStateCommitmentChecker& clearing_commitment, 
		ThreadsafeValidationStatistics& main_stats,
		 Args... args)
		: BaseSerialManager<ManagerType>(main_manager, args...)
		, clearing_commitment(clearing_commitment)
		, activated_supplies()
		, main_stats(main_stats) {}


	//! Local actions only unwound when undoing failed transaction in block
	//! production.  In validation, a failed transaction just reverts
	//! the whole block (i.e. throw out all buffered changes).
	//! Hence, no-op
	void undelete_offer(
		const int idx, 
		const Price min_price, 
		const AccountID owner, 
		const uint64_t offer_id) {
		//no op
	}

	//! Local actions only unwound when undoing failed transaction in block
	//! production.  In validation, a failed transaction just reverts
	//! the whole block (i.e. throw out all buffered changes).
	//! Hence, no-op
	void unwind_add_offer(int idx, const Offer& offer) {
		//no op
	}

	/*! Add an offer to the orderbook database.  Additionally, based on
		the market equilibrium specification, can clear offers immediately.
		This saves some trie manipulations later on.

		Metadata and LogType are useful parameters here.
		Metadata is the metadata associated with the transaction (i.e.
		source account, sequence number).  LogType is a serial account
		modification log.
	*/
	template<typename OpMetadata, typename LogType>
	void add_offer(
		int idx, const Offer& offer, OpMetadata& metadata, LogType& log)
	{
		ensure_suffient_new_offers_sz(idx);
		generate_orderbook_trie_key(offer, key_buf);

		// If theshold key is null, then all offers in the orderbook execute 
		// fully
		if (clearing_commitment[idx].thresholdKeyIsNull == 1) {
			auto sellPrice 
				= clearing_commitment.prices[offer.category.sellAsset];
			auto buyPrice 
				= clearing_commitment.prices[offer.category.buyAsset];
			
			clear_offer_full(
				offer, 
				sellPrice, 
				buyPrice, 
				clearing_commitment.tax_rate, 
				metadata.db_view, 
				metadata.source_account_idx);

			activated_supplies[idx].activated_supply 
				+= FractionalAsset::from_integral(offer.amount);
			log.log_self_modification(
				metadata.tx_metadata.sourceAccount, metadata.operation_id);
			return;
		}

		// Otherwise, compare the offer's key to the partial exec threshold.
		auto key_buf_bytes = key_buf.template get_bytes_array<xdr::opaque_array<ORDERBOOK_KEY_LEN>>();
		auto res = memcmp(
			clearing_commitment[idx].partialExecThresholdKey.data(), 
			key_buf_bytes.data(), 
			ORDERBOOK_KEY_LEN);

		if (res <= 0) {
			// Threshold key is less than or equal to offer's key
			// Hence, offer does not execute (might partial execute).
			// Partial execution is complicated, we'll handle that case
			// later.

			// Insertions are marked with rollback metadata, in case
			// we rollback the whole block later (for some unrelated reason).
			new_offers.at(idx).template insert<trie::RollbackInsertFn<OfferWrapper>> (
				key_buf, OfferWrapper(offer));
		} else {
			// Threshold key is strictly bigger than offer's key.
			// Hence, we can immediately clear the offer
			auto sellPrice 
				= clearing_commitment.prices.at(offer.category.sellAsset);
			auto buyPrice 
				= clearing_commitment.prices.at(offer.category.buyAsset);
			clear_offer_full(
				offer, 
				sellPrice, 
				buyPrice, 
				clearing_commitment.tax_rate, 
				metadata.db_view, 
				metadata.source_account_idx);
			activated_supplies.at(idx).activated_supply 
				+= FractionalAsset::from_integral(offer.amount);
			log.log_self_modification(
				metadata.tx_metadata.sourceAccount, metadata.operation_id);
		}
	}

	//! Also merge in local clearing stats when merging in this object's
	//! state updates to the orderbook manager.
	void partial_finish_conclude() {
		main_stats += activated_supplies;
		BaseSerialManager<ManagerType>::partial_finish_conclude();
	}
};

} /* speedex */