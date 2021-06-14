#pragma once

#include "block_processing/serial_transaction_processor.h"

#include "modlog/log_merge_worker.h"

#include "orderbook/commitment_checker.h"

#include "speedex/speedex_management_structures.h"

#include "stats/block_update_stats.h"

#include "xdr/transaction.h"
#include "xdr/ledger.h"
#include "xdr/block.h"

namespace speedex {
/*! 
Interface for producing valid block of transactions.
Owns a background thread used for merging account modification logs
in the background. 

Each of the validation methods is essentially the same functionality.
Each accepts a transaction block written in a different format.

*/
class BlockValidator {

	SpeedexManagementStructures& management_structures;
	LogMergeWorker worker;

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
	BlockValidator(SpeedexManagementStructures& management_structures)
		: management_structures(management_structures)
		, worker(management_structures.account_modification_log) {}

	bool validate_transaction_block(
		const AccountModificationBlock& transactions,
		const OrderbookStateCommitmentChecker& clearing_commitment,
		ThreadsafeValidationStatistics& main_stats,
		BlockValidationMeasurements& measurements,
		BlockStateUpdateStatsWrapper& state_update_stats);

	bool validate_transaction_block(
		const TransactionData& transactions,
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

	//! Replay a block loaded from disk
	//! TODO alternate fn calls depending on save format
	void replay_trusted_block(
		const AccountModificationBlock& block,
		const HashedBlock& header);

};



} /* speedex */
