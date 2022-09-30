#pragma once

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
