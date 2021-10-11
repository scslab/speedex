#include "hotstuff/block.h"
#include "hotstuff/block_storage/io_utils.h"

#include "utils/debug_macros.h"
#include "utils/hash.h"

namespace hotstuff {

HotstuffBlock::HotstuffBlock(HotstuffBlockWire&& _wire_block)
	: wire_block(std::move(_wire_block))
	, parsed_qc(wire_block.header.qc)
	, parsed_block_body(std::nullopt)
	, block_height(0)
	, parent_block_ptr(nullptr)
	, self_qc(speedex::hash_xdr(wire_block.header))
	, decided(false)
	, applied(false)
	, written_to_disk()
	, self_produced(false)
	{}

HotstuffBlock::HotstuffBlock()
	: wire_block()
	, parsed_qc(std::nullopt)
	, parsed_block_body(std::nullopt)
	, block_height(0)
	, parent_block_ptr(nullptr)
	, self_qc(speedex::Hash())
	, decided(true)
	, applied(true)
	, written_to_disk()
	, self_produced(false)
	{
		written_to_disk.test_and_set();
	}


void
HotstuffBlock::set_parent(block_ptr_t parent_block) {
	parent_block_ptr = parent_block;
	block_height = parent_block->block_height + 1;
}

void
HotstuffBlock::set_justify(block_ptr_t justify_block) {
	justify_block_ptr = justify_block;
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

	return parsed_qc->verify(config);
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

void
HotstuffBlock::write_to_disk() {
	bool already_written = written_to_disk.test_and_set();

	if (already_written) {
		return;
	}

	save_block(wire_block);
	if (parent_block_ptr != nullptr) {
		parent_block_ptr -> write_to_disk();
	}
}

block_ptr_t 
HotstuffBlock::mint_block(xdr::opaque_vec<>&& body, QuorumCertificateWire const& qc_wire, speedex::Hash const& parent_hash)
{
	HotstuffBlockWire wire_block;
	wire_block.header.parent_hash = parent_hash;
	wire_block.header.qc = qc_wire;
	wire_block.header.body_hash = speedex::hash_xdr(body);
	wire_block.body = std::move(body);

	auto out = std::make_shared<HotstuffBlock>(std::move(wire_block));

	out -> set_self_produced();

	return out;
}

} /* hotstuff */