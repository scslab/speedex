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
