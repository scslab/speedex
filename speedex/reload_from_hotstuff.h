#pragma once

namespace hotstuff {
	class HotstuffLMDB;
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
	hotstuff::HotstuffLMDB const& decided_block_cache);

} /* speedex */
