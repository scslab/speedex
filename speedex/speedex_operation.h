#pragma once

#include <cstdint>

#include "block_processing/block_validator.h"

#include "speedex/speedex_management_structures.h"

#include "stats/block_update_stats.h"

#include "xdr/block.h"

namespace speedex {


/*! Runs block creation logic.  Does not
assemble a new block of transactions, nor
does it persist data to disk.

Call sodium_init() before usage.
*/
HashedBlock
speedex_block_creation_logic(
	std::vector<Price>& price_workspace,
	SpeedexManagementStructures& management_structures,
	TatonnementManagementStructures& tatonnement,
	const HashedBlock& prev_block,
	OverallBlockProductionMeasurements& overall_measurements);

template <typename TxLogType>
bool speedex_block_validation_logic( 
	SpeedexManagementStructures& management_structures,
	BlockValidator& validator,
	OverallBlockValidationMeasurements& overall_validation_stats,
	const HashedBlock& prev_block,
	const HashedBlock& expected_next_block,
	const TxLogType& transactions);

/*
bool speedex_block_validation_logic( 
	SpeedexManagementStructures& management_structures,
	BlockValidator& validator,
	OverallBlockValidationResults& overall_validation_stats,
	const HashedBlock& prev_block,
	const HashedBlock& expected_next_block,
	const SerializedBlock& transactions);

bool speedex_block_validation_logic( 
	SpeedexManagementStructures& management_structures,
	BlockValidator& validator,
	OverallBlockValidationResults& overall_validation_stats,
	const HashedBlock& prev_block,
	const HashedBlock& expected_next_block,
	const TransactionData& transactions);

bool speedex_block_validation_logic( 
	SpeedexManagementStructures& management_structures,
	BlockValidator& validator,
	OverallBlockValidationResults& overall_validation_stats,
	const HashedBlock& prev_block,
	const HashedBlock& expected_next_block,
	const AccountModificationBlock& transactions);

bool speedex_block_validation_logic( 
	SpeedexManagementStructures& management_structures,
	BlockValidator& validator,
	OverallBlockValidationResults& overall_validation_stats,
	const HashedBlock& prev_block,
	const HashedBlock& expected_next_block,
	const SignedTransactionList& transactions);
*/
/*
//loads persisted data, repairing lmdbs if necessary.
//at end, disk should be in a consistent state.
uint64_t 
edce_load_persisted_data(
	EdceManagementStructures& management_structures);

void 
edce_replay_trusted_round(
	EdceManagementStructures& management_structures,
	const uint64_t round_number);
*/
} /* speedex */
