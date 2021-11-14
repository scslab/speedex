#pragma once

class hotstuff::HotstuffLMDB;

namespace speedex {

struct SpeedexManagementStructures;

//! loads persisted data, repairing lmdbs if necessary.
//! at end, disk should be in a consistent state.
HashedBlock 
speedex_load_persisted_data(
	SpeedexManagementStructures& management_structures,
	hotstuff::HotstuffLMDB const& decided_block_cache);

};
