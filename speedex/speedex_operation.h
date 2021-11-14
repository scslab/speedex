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

Does not set overall_measurements.state_update_stats
*/
HashedBlock
speedex_block_creation_logic(
	std::vector<Price>& price_workspace,
	SpeedexManagementStructures& management_structures,
	TatonnementManagementStructures& tatonnement,
	const HashedBlock& prev_block,
	OverallBlockProductionMeasurements& overall_measurements,
	BlockStateUpdateStatsWrapper& state_update_stats);


/*!
Runs block validation logic.

Call sodium_init() before usage.

Does set overall_validation_stats.state_update_stats
*/
template <typename TxLogType>
bool speedex_block_validation_logic( 
	SpeedexManagementStructures& management_structures,
	BlockValidator& validator,
	OverallBlockValidationMeasurements& overall_validation_stats,
	const HashedBlock& prev_block,
	const HashedBlock& expected_next_block,
	const TxLogType& transactions);


} /* speedex */
