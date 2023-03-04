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

/*! \file block_producer.h

Produce a block of transactions (of an approximate target size),
given a mempool of uncommitted (new) transactions.
*/

#include "modlog/log_merge_worker.h"

#include "mempool/mempool.h"

#include "xdr/block.h"

namespace speedex {

class BlockStateUpdateStatsWrapper;
class SpeedexManagementStructures;

/*! 
Interface for producing valid block of transactions.
*/
class BlockProducer {

	SpeedexManagementStructures& management_structures;
	//! Merge account mod logs in a background thread.
	LogMergeWorker& worker;

public:
	//! Create a new block producer.
	BlockProducer(
		SpeedexManagementStructures& management_structures,
		LogMergeWorker& log_merge_worker)
		: management_structures(management_structures)
		, worker(log_merge_worker) {}

	//! Mints a new block of transactions.
	//! output block is implicitly held within account_modification_log
	//! returns (somewhat redundantly) total number of txs in block
	uint64_t
	build_block(
		Mempool& mempool,
		int64_t max_block_size,
		BlockCreationMeasurements& measurements,
		BlockStateUpdateStatsWrapper& state_update_stats);

};

} /* speedex */