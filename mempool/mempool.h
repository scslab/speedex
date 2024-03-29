#pragma once

/**
 * SPEEDEX: A Scalable, Parallelizable, and Economically Efficient Decentralized Exchange
 * Copyright (C) 2023 Geoffrey Ramseyer

 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/*! \file mempool.h
Simple implementation of a mempool, useful for block production experiments.

A mempool contains many mempool chunks.  These chunks are maintained at
approximately a fixed size.  After building a block, committed and failed
transactions are removed from the mempool and small chunks are merged into
larger chunks.
*/ 


#include <atomic>
#include <cstdint>
#include <deque>
#include <mutex>
#include <vector>


#include "utils/async_worker.h"

#include "xdr/transaction.h"

namespace speedex {

class MempoolTransactionFilter;

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

	uint64_t filter(MempoolTransactionFilter const& filter);

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

	std::deque<MempoolChunk> buffered_mempool;

	std::atomic<uint64_t> mempool_size;

	std::atomic<uint64_t> buffer_size;

	mutable std::mutex mtx;
	std::mutex buffer_mtx;


	friend class MempoolFilterExecutor;

	//! update removed tx count
	void log_tx_removal(uint64_t removed_count);

	void add_to_mempool_buffer_nolock(std::vector<SignedTransaction>&& chunk);

public:

	const size_t TARGET_CHUNK_SIZE;

	const size_t MAX_MEMPOOL_SIZE;

	//! The number of the most recent batch of transactions added to the mempool
	//! Used in experiments - txs are streamed in batches from disk.
	std::atomic<uint64_t> latest_block_added_to_mempool = 0;

	Mempool(size_t target_chunk_size, size_t max_mempool_size)
		: mempool()
		, buffered_mempool()
		, mempool_size(0)
		, buffer_size(0)
		, mtx()
		, buffer_mtx()
       	, TARGET_CHUNK_SIZE(target_chunk_size)
       	, MAX_MEMPOOL_SIZE(max_mempool_size) {}

    //! Add a set of transactions to the mempool
    //! These transactions do not go directly into the mempool, but instead
    //! into an internal buffer.  This buffer is merged into the main mempool
    //! by push_mempool_buffer_to_mempool().
	void chunkify_and_add_to_mempool_buffer(std::vector<SignedTransaction>&& txs);

	//! Pushes the internal tx buffer to the mempool.
	//! Internally acquires all relevant locks
	void push_mempool_buffer_to_mempool();

	//! Defragment the mempool.
	//! Threadsafe (can be done by background thread, no lock required).
	void join_small_chunks();

	uint64_t size() const {
		return mempool_size.load(std::memory_order_acquire);
	}

	uint64_t total_size() const {
		return buffer_size.load(std::memory_order_relaxed) + size();
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


	//! For overlay mock tests
	//! Rounds up to nearest chunk
	void drop_txs(size_t num_to_drop);
};

} /* speedex */
