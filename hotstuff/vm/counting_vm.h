#pragma once

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
		return std::make_unique<block_type>(state);
	}

	void exec_block(const block_type& blk);

	void log_confirmation(const block_id& id) {
		if (id.value) {
			last_committed_state = *(id.value);
		}
	}
};

/*
class CountingVM {

	uint64_t state = 0;

	uint64_t last_committed_state = 0;

	SpeculativeExecGadget<std::optional<uint64_t>> height_map;

	void revert_to_last_commitment();

public:

	CountingVM()
		: height_map() 
		{}

	void make_empty_proposal(uint64_t proposal_height);

	xdr::opaque_vec<> get_and_apply_next_proposal(uint64_t proposal_height);

	void apply_block(block_ptr_t block);

	void notify_vm_of_commitment(block_ptr_t blk);

};
 */

} /* hotstuff */
