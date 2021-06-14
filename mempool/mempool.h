#pragma once

/*! \file mempool.h
Simple implementation of a mempool, useful for block production experiments.

A mempool contains many mempool chunks.  These chunks are maintained at
approximately a fixed size.  After building a block, committed and failed
transactions are removed from the mempool and small chunks are merged into
larger chunks.
*/ 


#include <atomic>
#include <cstdint>
#include <mutex>
#include <vector>


#include "utils/async_worker.h"

#include "xdr/transaction.h"

namespace speedex {

/*! A chunk of transactions in the mempool.
Individual chunks have no synchronization primitives.  The larger mempool 
manages synchronization.
*/
struct MempoolChunk {

	//! Uncommitted transactions
	std::vector<SignedTransaction> txs;
	//! Flags indicating which transactions can be removed from the mempool
	//! Transactions removed if they are confirmed or if they fail
	//! in certain types of ways.
	std::vector<bool> confirmed_txs_to_remove;


	//! Initialize a mempool chunk with a given set of transactions
	MempoolChunk(std::vector<SignedTransaction>&& txs_input) 
		: txs(std::move(txs_input))
		, confirmed_txs_to_remove()
		{}

	//! Clear txs that were marked as finished.
	//! Returns the number of txs removed.
	uint64_t remove_confirmed_txs();

	//! Clear the remove tx flags.
	//! Not useful in our experiments.
	void clear_confirmed_txs_bitmap() {
		confirmed_txs_to_remove.clear();
	}

	//! Mark some of the transactions as confirmed or failed (and thus
	//! that they should be removed from the mempool)
	void set_confirmed_txs(std::vector<bool>&& bitmap) {
		confirmed_txs_to_remove = std::move(bitmap);
		if (confirmed_txs_to_remove.size() != txs.size()) {
			throw std::runtime_error("size mismatch: bitmap vs txs");
		}
	}

	size_t size() const {
		return txs.size();
	}

	//! Access a transaction in the chunk.
	const SignedTransaction& operator[](size_t idx) {
		return txs[idx];
	}

	//! Join one mempool chunk with another.
	void join(MempoolChunk&& other){
		txs.insert(txs.end(), 
			std::make_move_iterator(other.txs.begin()),
			std::make_move_iterator(other.txs.end()));
		confirmed_txs_to_remove.insert(
			confirmed_txs_to_remove.end(), 
			other.confirmed_txs_to_remove.begin(),
			other.confirmed_txs_to_remove.end());
	}
};
	

class Mempool {

	std::vector<MempoolChunk> mempool;

	std::vector<MempoolChunk> buffered_mempool;

	std::atomic<uint64_t> mempool_size;

	mutable std::mutex mtx;
	std::mutex buffer_mtx;


public:

	const size_t TARGET_CHUNK_SIZE;

	//! The number of the most recent batch of transactions added to the mempool
	//! Used in experiments - txs are streamed in batches from disk.
	std::atomic<uint64_t> latest_block_added_to_mempool = 0;

	Mempool(size_t target_chunk_size)
		: mempool()
		, buffered_mempool()
		, mempool_size(0)
		, mtx()
		, buffer_mtx()
       	, TARGET_CHUNK_SIZE(target_chunk_size) {}

    //! Add a set of transactions to the mempool
    //! These transactions do not go directly into the mempool, but instead
    //! into an internal buffer.  This buffer is merged into the main mempool
    //! by push_mempool_buffer_to_mempool().
	void add_to_mempool_buffer(std::vector<SignedTransaction>&& chunk);

	//! Pushes the internal tx buffer to the mempool.
	//! Requires acquiring the mempool lock.
	void push_mempool_buffer_to_mempool();

	//! Defragment the mempool.
	//! Threadsafe (can be done by background thread, no lock required).
	void join_small_chunks();

	uint64_t size() const {
		return mempool_size.load(std::memory_order_acquire);
	}

	//! Returns a lock on the mempool.  This lock should be held
	//! while iterating over the mempool.
	std::lock_guard<std::mutex> 
	lock_mempool() {
		mtx.lock();
		return {mtx, std::adopt_lock};
	}

	//! Remove confirmed transactions from mempool after
	//! block production.  Threadsafe (can be done by background worker).
	//! Internally acquires the mempool lock.
	void remove_confirmed_txs();


	//! Get the number of mempool chunks (for iterating over the pool).
	size_t num_chunks() const {
		return mempool.size();
	}

	//! Access a mempool chunk.
	//! References will be invalidated unless mempool is locked
	MempoolChunk& operator[](size_t idx) {
		return mempool.at(idx);
	}
};

} /* speedex */
