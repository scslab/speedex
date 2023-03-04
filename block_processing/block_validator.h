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

/*! \file block_validator.h

Convenience wrapper around logic for validating a block of transactions.

Only does the actual iteration over transactions.  Does not do 
offer clearing/validation checks.
*/

#include "xdr/block.h"
#include "xdr/transaction.h"
#include "xdr/database_commitments.h"

namespace speedex
{

class BlockStateUpdateStatsWrapper;
class LogMergeWorker;
class OrderbookStateCommitmentChecker;
class SpeedexManagementStructures;
class ThreadsafeValidationStatistics;

/*! 
Interface for producing valid block of transactions.

Each of the validation methods is essentially the same functionality.
Each accepts a transaction block written in a different format.

*/
class BlockValidator {

	SpeedexManagementStructures& management_structures;
	LogMergeWorker& worker;

	//! Validate a batch of transactions
	template<typename WrappedType>
	bool validate_transaction_block_(
		const WrappedType& transactions,
		const OrderbookStateCommitmentChecker& clearing_commitment,
		ThreadsafeValidationStatistics& main_stats,
		BlockValidationMeasurements& measurements,
		BlockStateUpdateStatsWrapper& stats);

public:
	//! Create a new block validator.
	BlockValidator(SpeedexManagementStructures& management_structures,
		LogMergeWorker& log_merge_worker)
		: management_structures(management_structures)
		, worker(log_merge_worker) {}

	bool validate_transaction_block(
		const AccountModificationBlock& transactions,
		const OrderbookStateCommitmentChecker& clearing_commitment,
		ThreadsafeValidationStatistics& main_stats,
		BlockValidationMeasurements& measurements,
		BlockStateUpdateStatsWrapper& state_update_stats);

	bool validate_transaction_block(
		const SignedTransactionList& transactions,
		const OrderbookStateCommitmentChecker& clearing_commitment,
		ThreadsafeValidationStatistics& main_stats,
		BlockValidationMeasurements& measurements,
		BlockStateUpdateStatsWrapper& state_update_stats);

	bool validate_transaction_block(
		const SerializedBlock& transactions,
		const OrderbookStateCommitmentChecker& clearing_commitment,
		ThreadsafeValidationStatistics& main_stats,
		BlockValidationMeasurements& measurements,
		BlockStateUpdateStatsWrapper& state_update_stats);

};

//! Replay a block loaded from disk
// TODO alternate fn calls depending on save format
void replay_trusted_block(
	SpeedexManagementStructures& management_structures,
	const SignedTransactionList& block,
	const HashedBlock& header);

} /* speedex */
