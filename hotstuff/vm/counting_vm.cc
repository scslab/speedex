#include "hotstuff/vm/counting_vm.h"

namespace hotstuff {

/*
std::strong_ordering 
CountingVMBlockID::operator<=>(const CountingVMBlockID& other) const {
	if ((!value) && (!other.value)) {
		return std::strong_ordering::equal;
	}
	if ((!value) && (other.value)) {
		return std::strong_ordering::less;
	}
	if ((value) && (!other.value)) {
		return std::strong_ordering::greater;
	}
	return (*value) <=> (*other.value);
}*/

void 
CountingVM::exec_block(const block_type& blk) {
	if (blk == state + 1) {
		state ++;
		HOTSTUFF_INFO("VM: applied update, now at %lu", state);
	} else {
		HOTSTUFF_INFO("VM: got invalid input state, no op");
	}
}

void 
CountingVM::log_commitment(const block_id& id) {
	if (id.value) {
		last_committed_state = *(id.value);
		HOTSTUFF_INFO("VM: confirmed up to %lu", last_committed_state);
	}
}

void
CountingVM::rewind_to_last_commit() {
	HOTSTUFF_INFO("VM: rewind to %lu", last_committed_state);
	state = last_committed_state;
}

} /* hotstuff */