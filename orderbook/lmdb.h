#pragma once

/*! \file lmdb.h

LMDB instance and related methods for persisting orderbook data.
*/

#include <cstdint>
#include <memory>
#include <mutex>
#include <vector>

#include "config.h"

#include "lmdb/lmdb_wrapper.h"

#include "orderbook/thunk.h"
#include "orderbook/typedefs.h"

#include "utils/background_deleter.h"


namespace speedex {

/*! Persisting thunks leaves large offer tries (i.e. the offers that cleared),
and freeing tries can take a nonzero amount of time (lots of memory
accesses).  We run this in a background thread.

This class encapsulates a list of pointers to delete.  Deletes any contents
left inside the class when going out of scope, which should cut down 
on accidental memory leaks.
*/

class ThunkGarbage {
	using trie_t = OrderbookTrie::TrieT;
	std::vector<trie_t*> to_delete;

public:
	ThunkGarbage() : to_delete() {}

	~ThunkGarbage() {
		for (auto* ptr : to_delete) {
			delete ptr;
		}
	}

	void add(trie_t* garbage) {
		to_delete.push_back(garbage);
	}

	//! Add a vector of garbage pointers
	//! (i.e. the result of release() on another garbage object).
	void add(std::vector<trie_t*> ptrs) {
		to_delete.insert(to_delete.end(), ptrs.begin(), ptrs.end());
	}

	//! Release the list of pointers (Caller becomes responsible for deleting
	//! these pointers).
	std::vector<trie_t*>
	release() {
		auto out = std::move(to_delete);
		to_delete.clear();
		return out;
	}
};

class OrderbookManagerLMDB {

	//External iface is set up so that we could introduce limited sharding here
	BaseLMDBInstance base_instance;

	const size_t num_orderbooks;

	std::string get_lmdb_env_name() {
		return std::string(ROOT_DB_DIRECTORY) 
		+ std::string(OFFER_DB);
	}

public:

	/*! Initialize lmdb with mapsize of 2^30 (somewhat arbitrary choice)
	Roughly 1 billion.
	*/
	OrderbookManagerLMDB(size_t num_orderbooks)
		: base_instance{0x40000000}
		, num_orderbooks(num_orderbooks)
		{}

	BaseLMDBInstance& get_base_instance(OfferCategory const& category) {
		return base_instance;
	}

	size_t get_num_base_instances() {
		return 1;
	}

	std::pair<size_t, size_t> get_base_instance_range(size_t instance_number) {
		return {0, num_orderbooks};
	}

	BaseLMDBInstance& get_base_instance_by_index(size_t idx) {
		return base_instance;
	}

	void open_lmdb_env() {
		auto name = get_lmdb_env_name();
		base_instance.open_env(name.c_str());
	}
};

/*! Wrapper around lmdb instance.  Also includes 
persistence thunks (and manages those thunks).
*/
class OrderbookLMDB : public SharedLMDBInstance {
	using thunk_t = OrderbookLMDBCommitmentThunk;
	using prefix_t = OrderbookTriePrefix;

	std::vector<thunk_t> thunks;

	using trie_t = OrderbookTrie::TrieT;

	//! Use lock to make sure that during normal processing/validation,
	//! async flushes to lmdb don't conflict with operations of new blocks.
	std::unique_ptr<std::mutex> mtx;

public:
	
	OrderbookLMDB(OfferCategory const& category, OrderbookManagerLMDB& manager_lmdb);
	
	std::lock_guard<std::mutex> lock() {
		mtx -> lock();
		return {*mtx, std::adopt_lock};
	}

	/*! Persist accumulated thunks to disk, up to current_block_number.
		Take care to not migrate wtx across threads.

		Erases thunks that were committed to disk from memory.

		Returns a list of pointers which the caller is responsible for deleting.
	*/
	ThunkGarbage
	__attribute__((warn_unused_result)) write_thunks(
		const uint64_t current_block_number,
		dbenv::wtxn& wtx,
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
	add_new_thunk_nolock(uint64_t current_block_number);

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
