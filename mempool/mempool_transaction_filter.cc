#include "mempool/mempool_transaction_filter.h"

namespace speedex {

// return true to remove
bool 
MempoolTransactionFilter::check_transaction(const SignedTransaction& tx) const {
	AccountID source_account = tx.transaction.metadata.sourceAccount;

	UserAccount* idx = management_structures.db.lookup_user(source_account);
	//if (!management_structures.db.lookup_user_id(source_account, &idx)) {
	if (idx == nullptr) {
		// don't remove, we haven't seen that account's creation tx yet
		//TODO possibly need a filter by age category here.
		return false;
	}

	uint64_t last_committed_seq_num = management_structures.db.get_last_committed_seq_number(idx);

	// return true IFF we've already committed a seq number higher than this one on this account.
	return (last_committed_seq_num >= tx.transaction.metadata.sequenceNumber);
}

MempoolFilterExecutor::MempoolFilterExecutor(SpeedexManagementStructures const& management_structures, Mempool& mempool)
	: AsyncWorker()
	, cancel_background_filter(false)
	, do_work(false)
	, filter(management_structures)
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
