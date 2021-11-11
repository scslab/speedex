#include "hotstuff/block.h"
#include "hotstuff/block_storage/io_utils.h"


#include "utils/hash.h"

namespace hotstuff {

using speedex::Hash;
using speedex::ReplicaID;
using speedex::ReplicaConfig;

HotstuffBlock::HotstuffBlock(HotstuffBlockWire&& _wire_block, ReplicaID proposer)
	: wire_block(std::move(_wire_block))
	, parsed_qc(wire_block.header.qc)
	, proposer(proposer)
	, block_height(0)
	, parent_block_ptr(nullptr)
	, self_qc(speedex::hash_xdr(wire_block.header))
	, decided(false)
	, written_to_disk()
	, hash_checked(false)
	, hash_valid(false)
	, flushed_from_memory(false)
	{}

HotstuffBlock::HotstuffBlock(HotstuffBlockWire&& _wire_block, load_from_disk_block_t _)
	: wire_block(std::move(_wire_block))
	, parsed_qc(wire_block.header.qc)
	, proposer(speedex::UNKNOWN_REPLICA)
	, block_height(0)
	, parent_block_ptr(nullptr)
	, self_qc(speedex::hash_xdr(wire_block.header))
	, decided(true)
	, written_to_disk()
	, hash_checked(true)
	, hash_valid(true)
	, flushed_from_memory(false)
	{
		written_to_disk.test_and_set();
	}


// genesis block
HotstuffBlock::HotstuffBlock(genesis_block_t)
	: wire_block()
	, parsed_qc(std::nullopt)
	, proposer(speedex::UNKNOWN_REPLICA)
	, block_height(0)
	, parent_block_ptr(nullptr)
	, self_qc(speedex::Hash())
	, decided(true)
	, written_to_disk()
	, hash_checked(true)
	, hash_valid(true)
	, flushed_from_memory(true)
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


bool
HotstuffBlock::has_body() const {
	if (flushed_from_memory) {
		throw std::runtime_error("can't query body if flushed from memory");
	}
	return wire_block.body.size() > 0;
}

bool
HotstuffBlock::supports_nonempty_child_proposal(const ReplicaID self_id, int depth) const {
	if (depth <= 0) return true;
	if (self_id != proposer) return false;
	if (!justify_block_ptr) return false;
	if (!parent_block_ptr) return false;
	if (get_justify_hash() != get_parent_hash()) return false;
	return parent_block_ptr -> supports_nonempty_child_proposal(self_id, depth - 1);
}

bool
HotstuffBlock::validate_hash() const {
	if (hash_checked) return hash_valid;
	auto hash = speedex::hash_xdr(wire_block.body);
	hash_checked = true;
	if (hash != wire_block.header.body_hash) {
		HOTSTUFF_INFO("mismatch between hash(wire_block.body) and wire_block.body_hash");
		hash_valid = false;
		return false;
	}
	hash_valid = true;
	return true;
}

bool 
HotstuffBlock::validate_hotstuff(const ReplicaConfig& config) const {
	if  (!validate_hash()) {
		return false;
	}

	return parsed_qc->verify(config);
}

void
HotstuffBlock::write_to_disk() {
	bool already_written = written_to_disk.test_and_set();

	if (already_written) {
		return;
	}

	// output filename (should be) equal to get_hash()
	// or equivalently, hash(wire_block.header)
	save_block(wire_block);
	if (parent_block_ptr != nullptr) {
		parent_block_ptr -> write_to_disk();
	}
}

void
HotstuffBlock::flush_from_memory() {
	if (flushed_from_memory) return;
	flushed_from_memory = true;

	wire_block.body.clear();
}


block_ptr_t
HotstuffBlock::genesis_block()
{
	return std::shared_ptr<HotstuffBlock>(new HotstuffBlock(genesis_block_t{}));
}

block_ptr_t
HotstuffBlock::receive_block(HotstuffBlockWire&& body, ReplicaID source_id)
{
	return std::shared_ptr<HotstuffBlock>(new HotstuffBlock(std::move(body), source_id));
}

block_ptr_t 
HotstuffBlock::mint_block(xdr::opaque_vec<>&& body, QuorumCertificateWire const& qc_wire, Hash const& parent_hash, ReplicaID self_id)
{
	HotstuffBlockWire wire_block;
	wire_block.header.parent_hash = parent_hash;
	wire_block.header.qc = qc_wire;
	wire_block.header.body_hash = speedex::hash_xdr(body);
	wire_block.body = std::move(body);

	return std::shared_ptr<HotstuffBlock>(new HotstuffBlock(std::move(wire_block), self_id));
}

block_ptr_t
HotstuffBlock::load_decided_block(Hash const& hash) {
	auto loaded = load_block(hash);
	if (!loaded) {
		throw std::runtime_error("failed to load an expected block!");
	}

	return std::shared_ptr<HotstuffBlock>(new HotstuffBlock(std::move(*loaded), load_from_disk_block_t{}));
} 

} /* hotstuff */