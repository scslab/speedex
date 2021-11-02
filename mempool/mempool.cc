#include "mempool/mempool.h"

#include "mempool/mempool_transaction_filter.h"

#include <tbb/parallel_for.h>
namespace speedex {

uint64_t MempoolChunk::remove_confirmed_txs() {
	uint64_t num_removed = 0;

	for (size_t i = 0; i < confirmed_txs_to_remove.size();) {
		if (confirmed_txs_to_remove[i]) {
			confirmed_txs_to_remove[i] = confirmed_txs_to_remove.back();
			txs[i] = txs.back();
			txs.pop_back();
			confirmed_txs_to_remove.pop_back();
			num_removed ++;
		} else {
			i++;
		}
	}
	return num_removed;
}

uint64_t
MempoolChunk::filter(MempoolTransactionFilter const& filter) {
	uint64_t num_removed = 0;

	for (size_t i = 0; i < txs.size();) {
		if (filter.check_transaction(txs[i])) {
			txs[i] = std::move(txs.back());
			txs.pop_back();
			num_removed++;
		} else {
			i++;
		}
	}
	return num_removed;
}

void 
Mempool::add_to_mempool_buffer(std::vector<SignedTransaction>&& chunk) {
	buffer_size.fetch_add(chunk.size(), std::memory_order_relaxed);
	MempoolChunk to_add(std::move(chunk));
	std::lock_guard lock(buffer_mtx);
	buffered_mempool.emplace_back(std::move(to_add));
}

void 
Mempool::add_to_mempool_buffer_nolock(std::vector<SignedTransaction>&& chunk) {
	buffer_size.fetch_add(chunk.size(), std::memory_order_relaxed);
	MempoolChunk to_add(std::move(chunk));
	buffered_mempool.emplace_back(std::move(to_add));
}

void 
Mempool::chunkify_and_add_to_mempool_buffer(std::vector<SignedTransaction>&& txs) {
	std::lock_guard lock(buffer_mtx);
	for(size_t i = 0; i <= txs.size() / TARGET_CHUNK_SIZE; i++) {
		std::vector<SignedTransaction> chunk;
		size_t min_idx = i * TARGET_CHUNK_SIZE;
		size_t max_idx = std::min(txs.size(), (i + 1) * TARGET_CHUNK_SIZE);
		chunk.insert(
			chunk.end(),
			std::make_move_iterator(txs.begin() + min_idx),
			std::make_move_iterator(txs.begin() + max_idx));
		add_to_mempool_buffer_nolock(std::move(chunk));
	}
}

void Mempool::push_mempool_buffer_to_mempool() {
	std::lock_guard lock (buffer_mtx);
	std::lock_guard lock2 (mtx);
	//TODO consider limiting number of chunks or total number of txs in mempool

	while (buffered_mempool.size() > 0) {
		auto cur_sz = mempool_size.fetch_add(buffered_mempool.front().size(), std::memory_order_relaxed);
		mempool.emplace_back(std::move(buffered_mempool.front()));
		buffered_mempool.pop_front();

		if (cur_sz > MAX_MEMPOOL_SIZE) {
			return;
		}
	}
}

void Mempool::join_small_chunks() {
	std::lock_guard lock(mtx);

	//ensures that the average chunk size is at least TARGET/2
	for (size_t i = 0; i < mempool.size() - 1;) {
		if (mempool[i].size() + mempool[i+1].size() < TARGET_CHUNK_SIZE) {
			mempool[i].join(std::move(mempool[i+1]));
			mempool[i+1] = std::move(mempool.back());
			mempool.pop_back();
		} else {
			i++;
		}
	}
}

void Mempool::log_tx_removal(uint64_t removed_count) {
	mempool_size.fetch_sub(removed_count, std::memory_order_relaxed);
}

void Mempool::remove_confirmed_txs() {
	std::lock_guard lock(mtx);

	tbb::parallel_for(tbb::blocked_range<size_t>(0, mempool.size()),
		[this] (const auto& r) {

			int64_t deleted = 0;
			for (auto i = r.begin(); i < r.end(); i++) {
				deleted += mempool[i].remove_confirmed_txs();
			}
			log_tx_removal(deleted);
		});
}

void Mempool::drop_txs(size_t num_to_drop) {
	std::lock_guard lock(mtx);

	size_t dropped = 0;
	while (dropped < num_to_drop && mempool.size() > 0) {
		dropped += mempool.front().size();
		mempool.erase(mempool.begin());
	}
	log_tx_removal(dropped);
}

} /* speedex */
