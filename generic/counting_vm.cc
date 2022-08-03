#include "generic/counting_vm.h"

#include "hotstuff/log_access_wrapper.h"

namespace hotstuff {

void 
CountingVM::init_from_disk(LogAccessWrapper const& lmdb) {
	//auto cursor = lmdb.forward_cursor();
	for (auto iter = lmdb.begin(); iter != lmdb.end(); ++iter) {
		auto [hash, id] = iter.get_hs_hash_and_vm_data();

		if (id) {
			auto loaded_block = lmdb.template load_vm_block<CountingVMBlock>(hash);
			exec_block(loaded_block);
			log_commitment(id);
		}
	}
}

void 
CountingVM::exec_block(const VMBlock& blk) {
	const CountingVMBlock& blk_ = static_cast<const CountingVMBlock&>(blk);
	if (blk_.value == state + 1) {
		state ++;
//		HOTSTUFF_INFO("VM: applied update, now at %lu", state);
	} else {
//		HOTSTUFF_INFO("VM: got invalid input state, no op");
	}
}

void 
CountingVM::log_commitment(const VMBlockID& id) {
	if (id.value) {
		uint64_t res;
		xdr::xdr_from_opaque(*(id.value), res);
		last_committed_state = res;//*(id.value);
//		HOTSTUFF_INFO("VM: confirmed up to %lu", last_committed_state);
	}
}

void
CountingVM::rewind_to_last_commit() {
//	HOTSTUFF_INFO("VM: rewind to %lu", last_committed_state);
	state = last_committed_state;
}

} /* hotstuff */
