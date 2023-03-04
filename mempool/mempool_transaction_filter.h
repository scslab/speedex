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

/*! \file mempool_transaction_filter.h

	Filter transactions from a mempool that have already been
	confirmed, or who have had successors (by seq num) already
	confirmed.  Main usage is in maintaining a reasonably
	valid mempool in validators.
*/

#include <utils/async_worker.h>

namespace speedex {

class Mempool;
class MemoryDatabase;
struct SignedTransaction;

//! Filter mempool for committed txs (i.e. txs committed by another node).
//! Should NOT be used while speedex state is modified by block production or validation.
//! Mempool lock should be acquired
//! One instance of class can be concurrently used (this doesn't hold any state of its own).
class MempoolTransactionFilter {

	MemoryDatabase const& db;

public:

	MempoolTransactionFilter(MemoryDatabase const& db)
		: db(db) 
		{}

	//! return true if the transaction is definitely committed already or uncommittable
	//! return false if tx should stay in mempool
	bool check_transaction(const SignedTransaction& tx) const;
};

class MempoolFilterExecutor : public utils::AsyncWorker {

	std::atomic<bool> cancel_background_filter;

	bool do_work;

	bool exists_work_to_do() override final {
		return do_work;
	}

	void run();

	MempoolTransactionFilter filter;
	Mempool& mempool;

public:

	MempoolFilterExecutor(MemoryDatabase const& db, Mempool& mempool);

	void start_filter();

	void stop_filter();

	~MempoolFilterExecutor() {
		stop_filter();
		terminate_worker();
	}
};



} /* speedex */
