#pragma once

#include <cstdint>
#include <memory>
#include <mutex>
#include <vector>

#include "lmdb/lmdb_wrapper.h"

#include "orderbook/thunk.h"
#include "orderbook/typedefs.h"

#include "utils/background_deleter.h"


namespace speedex {
/*! Wrapper around lmdb instance.  Also includes 
persistence thunks (and manages those thunks).
*/
class OrderbookLMDB : public LMDBInstance {
	using thunk_t = OrderbookLMDBCommitmentThunk;
	using prefix_t = OrderbookTriePrefix;

	std::vector<thunk_t> thunks;

	using trie_t = OrderbookTrie::TrieT;
	BackgroundDeleter<trie_t> background_deleter;

public:
	
	//! Use lock to make sure that during normal processing/validation,
	//! async flushes to lmdb don't conflict with operations of new blocks.
	std::unique_ptr<std::mutex> mtx;

	/*! Initialize lmdb with mapsize of 2^30 (somewhat arbitrary choice)
	Roughly 1 billion.
	*/
	OrderbookLMDB() 
		: LMDBInstance{0x40000000}
		, mtx(std::make_unique<std::mutex>()) {}

	/*! Persist accumulated thunks to disk, up to current_block_number.
		Take care to not migrate wtx across threads.

		Erases thunks that were committed to disk from memory.
	*/
	void write_thunks(
		const uint64_t current_block_number,
		bool debug = false);

	//used for testing only, particularly wrt tatonnement_sim
	void clear_() {
		thunks.clear();
	}

	std::vector<thunk_t>& get_thunks_ref() {
		return thunks;
	}

	/*! Add new thunk, returning a reference.
		Should lock before calling and during lifetime of the reference,
		so that a background persistence does not invalidate the reference
		(via e.g. a vector resize).
	*/	
	thunk_t& 
	add_new_thunk_nolock(uint64_t current_block_number) {
		if (thunks.size()) {
			if (thunks.back().current_block_number + 1 
				!= current_block_number)
			{
				throw std::runtime_error("thunks in the wrong order!");
			}
		}
		thunks.emplace_back(current_block_number);
		return thunks.back();
	}
	/*! Get the most recent thunk 
		(typically, the thunk currently being created).
		
		Should lock before calling and during lifetime of the reference,
		so that a background persistence does not invalidate the reference
		(via e.g. a vector resize).
	*/
	thunk_t& 
	get_top_thunk_nolock() {
		if (thunks.size() == 0) {
			throw std::runtime_error(
				"can't get top thunk when thunks.size() == 0");
		}
		return thunks.back();
	}

	void pop_top_thunk() {
		std::lock_guard lock(*mtx);
		pop_top_thunk_nolock();
	}

	//! Erases the top thunk
	//! Use when block validation fails.
	void pop_top_thunk_nolock() {

		if (thunks.size() == 0) {
			throw std::runtime_error("can't pop_back from empty thunks list");
		}
		thunks.pop_back();
	}
};

} /* speedex */
