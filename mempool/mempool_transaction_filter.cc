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

#include "mempool/mempool_transaction_filter.h"

#include "mempool/mempool.h"

#include "memory_database/memory_database.h"

#include "xdr/types.h"
#include "xdr/transaction.h"

namespace speedex {

// return true to remove
bool 
MempoolTransactionFilter::check_transaction(const SignedTransaction& tx) const {
	AccountID source_account = tx.transaction.metadata.sourceAccount;

	auto* idx = db.lookup_user(source_account);
	//if (!management_structures.db.lookup_user_id(source_account, &idx)) {
	if (idx == nullptr) {
		// don't remove, we haven't seen that account's creation tx yet
		//TODO possibly need a filter by age category here.
		return false;
	}

	uint64_t last_committed_seq_num = db.get_last_committed_seq_number(idx);

	// return true IFF we've already committed a seq number higher than this one on this account.
	return (last_committed_seq_num >= tx.transaction.metadata.sequenceNumber);
}

MempoolFilterExecutor::MempoolFilterExecutor(MemoryDatabase const& db, Mempool& mempool)
	: utils::AsyncWorker()
	, cancel_background_filter(false)
	, do_work(false)
	, filter(db)
	, mempool(mempool)
	{
		start_async_thread(
			[this] {run();}
		);
	}


void
MempoolFilterExecutor::start_filter() {
	std::lock_guard lock(mtx);

	do_work = true;
	cancel_background_filter = false;
	cv.notify_all();
}

void
MempoolFilterExecutor::stop_filter() {
	cancel_background_filter = true;

	wait_for_async_task();
}

void
MempoolFilterExecutor::run() {
	std::unique_lock lock(mtx);
	while(true) {
		if ((!done_flag) && (!exists_work_to_do())) {
		cv.wait(lock, 
			[this] () { return done_flag || exists_work_to_do();});
		}
		if (done_flag) return;

		auto mempool_lock = mempool.lock_mempool();

		tbb::parallel_for(
			tbb::blocked_range<size_t>(0, mempool.num_chunks()),
			[this] (auto r) {
				for (auto i = r.begin(); i < r.end(); i++) {
					if (cancel_background_filter.load(std::memory_order_relaxed)) {
						return;
					}
					auto& chunk = mempool[i];
					uint64_t num_removed = chunk.filter(filter);
					mempool.log_tx_removal(num_removed);
				}
			});
		do_work = false;
		cv.notify_all();
	}
}

} /* speedex */
