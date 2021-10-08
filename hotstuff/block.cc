#include "hotstuff/block.h"

#include "utils/debug_macros.h"
#include "utils/hash.h"

namespace hotstuff {

HotstuffBlock::HotstuffBlock(HotstuffBlockWire&& _wire_block)
	: wire_block(std::move(_wire_block))
	, parsed_qc(wire_block.header.qc)
	, parsed_block_body(std::nullopt)
	, block_height(0)
	, parent_block_ptr(nullptr)
	, self_qc(speedex::hash_xdr(wire_block.header)) {}


void
HotstuffBlock::set_parent(block_ptr_t parent_block) {
	parent_block_ptr = parent_block;
	block_height = parent_block->block_height + 1;
}

bool HotstuffBlock::has_body() const {
	return wire_block.body.size() > 0;
}

bool 
HotstuffBlock::validate_hotstuff(const ReplicaConfig& config) const {
	auto hash = speedex::hash_xdr(wire_block.body);
	if (hash != wire_block.header.body_hash) {
		HOTSTUFF_INFO("mismatch between hash(wire_block.body) and wire_block.body_hash");
		return false;
	}

	return parsed_qc.verify(config);
}

bool 
HotstuffBlock::try_delayed_parse() 
{
	if (!has_body()) {
		parsed_block_body = std::nullopt;
		return true;
	}
	parsed_block_body = std::make_optional<HeaderDataPair>();

	try {
		xdr::xdr_from_opaque(wire_block.body, *parsed_block_body);
	} catch(...) {
		HOTSTUFF_INFO("speedex block parse failed");
		parsed_block_body = std::nullopt;
		return false;
	}
	return true;
}

} /* hotstuff */