#pragma once

/*! \file block_producer.h

Produce a block of transactions (of an approximate target size),
given a mempool of uncommitted (new) transactions.
*/

#include "modlog/log_merge_worker.h"

#include "speedex/speedex_management_structures.h"

#include "mempool/mempool.h"

#include "stats/block_update_stats.h"
#include "xdr/block.h"

namespace speedex {

/*! 
Interface for producing valid block of transactions.
Owns a background thread used for merging account modification logs
in the background. */
class BlockProducer {

	SpeedexManagementStructures& management_structures;
	LogMergeWorker worker;

public:
	//! Create a new block producer.
	BlockProducer(SpeedexManagementStructures& management_structures)
		: management_structures(management_structures)
		, worker(management_structures.account_modification_log) {}

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