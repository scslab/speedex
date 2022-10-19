#pragma once

#include <cstdint>

#include "xdr/block.h"

namespace speedex {

class SpeedexManagementStructures;
class BlockStateUpdateStatsWrapper;
class BlockValidator;
class TatonnementManagementStructures;

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
std::pair<Block, bool>
speedex_block_validation_logic( 
	SpeedexManagementStructures& management_structures,
	BlockValidator& validator,
	OverallBlockValidationMeasurements& overall_validation_stats,
	const HashedBlock& prev_block,
	const HashedBlock& expected_next_block,
	const SignedTransactionList& transactions);


Block 
ensure_sequential_block_numbers(const HashedBlock& prev_block, const HashedBlock& expected_next_block);

} /* speedex */
