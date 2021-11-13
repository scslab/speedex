#pragma once

/*! \file block_validator.h

Convenience wrapper around logic for validating a block of transactions.

Only does the actual iteration over transactions.  Does not do 
offer clearing/validation checks.
*/

#include "block_processing/serial_transaction_processor.h"

#include "modlog/log_merge_worker.h"

#include "orderbook/commitment_checker.h"

#include "speedex/speedex_management_structures.h"

#include "stats/block_update_stats.h"

#include "xdr/transaction.h"
#include "xdr/ledger.h"

namespace speedex {

struct SignedTransactionList;

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
