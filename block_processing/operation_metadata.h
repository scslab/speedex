#pragma once

/*! \file operation_metadata.h

Utility class for storing metadata associated with
one speedex operation (one part of a transaction).
*/

#include <cstdint>

#include "memory_database/memory_database.h"

#include "stats/block_update_stats.h"

#include "xdr/types.h"
#include "xdr/transaction.h"

namespace speedex {

/*! Metadata associated with one operation (one part of a transaction).

DatabaseViewType is one of the MemoryDatabaseView objects.
*/
template<typename DatabaseViewType>
struct OperationMetadata {
	//! Metadata associated with the overall transaction
	const TransactionMetadata& tx_metadata;
	//! database index of the source account.
	//! (Saves repeated db lookups)
	UserAccount* source_account_idx;
	//const account_db_idx source_account_idx;
	//! View of the database for the transaction.
	//! Tx processor should call commit or unwind
	//! on this metadata object to commit/unwind the database view.
	DatabaseViewType db_view; 
	//! Id of the current operation (sequence number + operation index).
	//! Modified externally.
	uint64_t operation_id;
	//! Local stats object, will be merged in later to main stats object.
	BlockStateUpdateStats local_stats;

	//! Initialize metadata.  DBViewArgs are for mocking out db
	//! when loading from lmdb.  Empty parameter pack in regular operation.
	template<typename... DBViewArgs>
	OperationMetadata(
		const TransactionMetadata& tx_metadata,
		UserAccount* source_account_idx,
		//const account_db_idx source_account_idx,
		MemoryDatabase& db,
		DBViewArgs... args)
	: tx_metadata(tx_metadata)
	, source_account_idx(source_account_idx)
	, db_view(args..., db)
	, operation_id(0) {}

	//! Call when commiting the overall transaction.
	//! Merges in local transaction stats and commits db view.
	void commit(BlockStateUpdateStatsWrapper& stats) {
		stats += local_stats;
		db_view.commit();
	}
	
	//! Unwind the contained db view.
	void unwind() {
		db_view.unwind();
	}
};

} /* speedex */