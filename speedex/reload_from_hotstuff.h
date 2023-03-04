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

namespace hotstuff {
	class LogAccessWrapper;
}

namespace speedex {

class BlockValidator;
struct HashedBlock;
struct SpeedexManagementStructures;

//! loads persisted data, repairing lmdbs if necessary.
//! at end, disk should be in a consistent state.
HashedBlock 
speedex_load_persisted_data(
	SpeedexManagementStructures& management_structures,
	BlockValidator& validator,
	hotstuff::LogAccessWrapper const& decided_block_cache);

} /* speedex */
