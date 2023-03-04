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
	UserAccount* const source_account_idx;
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

private:
	bool was_committed_or_unwound = false;

	void assert_committed_or_unwound()
	{
		if (!was_committed_or_unwound)
		{
			throw std::runtime_error("OperationMetadata was not committed or unwound!");
		}
	}

public:

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

	~OperationMetadata()
	{
		assert_committed_or_unwound();
	}

	//! Call when commiting the overall transaction.
	//! Merges in local transaction stats and commits db view.
	void commit(BlockStateUpdateStatsWrapper& stats) {
		stats += local_stats;
		db_view.commit();
		was_committed_or_unwound = true;
	}
	
	//! Unwind the contained db view.
	void unwind() {
		db_view.unwind();
		was_committed_or_unwound = true;
	}

	//! no actual need to unwind anything during validation,
	//! or when already unwinding a tx
	void set_no_unwind()
	{
		was_committed_or_unwound = true;
	}
};

} /* speedex */