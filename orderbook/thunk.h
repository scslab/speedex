#pragma once

#include <cstdint>
#include <vector>

#include "orderbook/typedefs.h"

#include "xdr/types.h"

namespace speedex {

/*! Accumulate a list of deleted keys when running
perform_marked_deletions().
*/
struct AccumulateDeletedKeys {

	using prefix_t = OrderbookTriePrefix;

	//! Store deleted kv pairs.
	//! Keys are useful when deleting from LMDB.  Offers kept to
	//! enable thunk to be undone (i.e. if validation fails).
	std::vector<std::pair<prefix_t, Offer>> deleted_keys;

	AccumulateDeletedKeys() : deleted_keys() {}

	void operator() (const prefix_t& key, const Offer& offer) {
		deleted_keys.push_back(std::make_pair(key, offer));
	}
};


/*! Thunk storing changes to an orderbook, to be persisted to disk later.
*/
struct OrderbookLMDBCommitmentThunk {

	using prefix_t = OrderbookTriePrefix;

	//! Key equal to the offer that partially executes, if it exists.  
	//! 0xFF... otherwise.
	prefix_t partial_exec_key;

	int64_t partial_exec_amount = -1;

	Offer preexecute_partial_exec_offer;

	std::vector<Offer> uncommitted_offers_vec;

	OrderbookTrie cleared_offers; // used only for the rollback

	AccumulateDeletedKeys deleted_keys;

	bool exists_partial_exec;

	uint64_t current_block_number;

	OrderbookLMDBCommitmentThunk(uint64_t current_block_number)
		: current_block_number(current_block_number) {}

	void set_no_partial_exec() {
		exists_partial_exec = false;
		partial_exec_key.set_max();
	}

	const bool get_exists_partial_exec() {
		return exists_partial_exec;
	}

	void set_partial_exec(const prefix_t& buf, int64_t amount, Offer offer) {
		partial_exec_key = buf;
		partial_exec_amount = amount;
		preexecute_partial_exec_offer = offer;
		exists_partial_exec = true;
	}

	void reset_trie() {
		cleared_offers.clear_and_reset();
	}
};

} /* speedex */
