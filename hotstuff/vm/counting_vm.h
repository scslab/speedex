#pragma once

#include "utils/debug_macros.h"

#include <compare>
#include <cstdint>
#include <optional>

#include <xdrpp/marshal.h>

namespace hotstuff {

struct CountingVMBlockID {
	std::optional<uint64_t> value;

	std::strong_ordering operator<=>(const CountingVMBlockID& other) const;

	bool operator==(const CountingVMBlockID&) const = default;
};


class CountingVM {
	uint64_t state = 0;
	uint64_t last_committed_state = 0;

public:

	using block_type = uint64_t;
	using block_id = CountingVMBlockID;

	static block_id nonempty_block_id(block_type const& blk) {
		return CountingVMBlockID{blk};
	}
	static block_id empty_block_id() {
		return CountingVMBlockID{std::nullopt};
	}

	std::unique_ptr<block_type> propose() {
		state++;
		HOTSTUFF_INFO("VM: proposing value %lu", state);
		return std::make_unique<block_type>(state);
	}

	// Main workflow for non-proposer is exec_block (called indirectly
	// by update) immediately followed by log_commitment
	// Proposer skips the exec_block call.
	void exec_block(const block_type& blk);

	void log_commitment(const block_id& id);

	void rewind_to_last_commit();
};

} /* hotstuff */
