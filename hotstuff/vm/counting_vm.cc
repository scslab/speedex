#include "hotstuff/vm/counting_vm.h"

#include "hotstuff/lmdb.h"

namespace hotstuff {


CountingVMBlockID::CountingVMBlockID(std::vector<uint8_t> const& bytes) 
	: value(std::nullopt)
{
	if (bytes.size() > 0) {
		value = std::make_optional<uint64_t>();
		xdr::xdr_from_opaque(bytes, *value);
	}
}

void 
CountingVM::init_from_disk(HotstuffLMDB const& lmdb) {
	auto cursor = lmdb.forward_cursor();
	for (auto iter = cursor.begin(); iter != cursor.end(); ++iter) {
		auto [hash, id] = iter.template get_hs_hash_and_vm_data<block_id>();

		if (id) {
			auto loaded_block = lmdb.template load_vm_block<block_type>(hash);
			exec_block(loaded_block);
			log_commitment(id);
		}
	}
}

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