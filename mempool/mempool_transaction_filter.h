#pragma once

/*! \file mempool_transaction_filter.h

	Filter transactions from a mempool that have already been
	confirmed, or who have had successors (by seq num) already
	confirmed.  Main usage is in maintaining a reasonably
	valid mempool in validators.
*/

#include "mempool/mempool.h"

#include "speedex/speedex_management_structures.h"

#include "xdr/transaction.h"

namespace speedex {

//! Filter mempool for committed txs (i.e. txs committed by another node).
//! Should NOT be used while speedex state is modified by block production or validation.
//! Mempool lock should be acquired
//! One instance of class can be concurrently used (this doesn't hold any state of its own).
class MempoolTransactionFilter {

	SpeedexManagementStructures const& management_structures;

public:

	MempoolTransactionFilter(SpeedexManagementStructures const& management_structures)
		: management_structures(management_structures) 
		{}

	//! return true if the transaction is definitely committed already or uncommittable
	//! return false if tx should stay in mempool
	bool check_transaction(const SignedTransaction& tx) const;
};

class MempoolFilterExecutor : public AsyncWorker {

	using AsyncWorker::mtx;
	using AsyncWorker::cv;

	std::atomic<bool> cancel_background_filter;

	bool do_work;

	bool exists_work_to_do() override final {
		return do_work;
	}

	void run();

	MempoolTransactionFilter filter;
	Mempool& mempool;

public:

	MempoolFilterExecutor(SpeedexManagementStructures& management_structures, Mempool& mempool);

	void start_filter();

	void stop_filter();
};



} /* speedex */
