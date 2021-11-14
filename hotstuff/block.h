#pragma once

#include "config/replica_config.h"

#include "hotstuff/crypto.h"

#include "utils/debug_macros.h"

#include "xdr/hotstuff.h"

#include <cstdint>
#include <optional>

#include <xdrpp/marshal.h>

namespace hotstuff {

class HotstuffBlock;

using block_ptr_t = std::shared_ptr<HotstuffBlock>;	

struct load_from_disk_block_t {};
struct genesis_block_t {};

/*
 * Typical workflow:
 * (1) validate_hotstuff() -- ensures hotstuff block is well formed
 * (2) validate parent exists (i.e. through BlockStore)
 * (3) At exec time, try_delayed_parse (and then exec)
 */
class HotstuffBlock {

	HotstuffBlockWire wire_block;

	//genesis_block lacks this
	std::optional<QuorumCertificate> parsed_qc;

	speedex::ReplicaID proposer;  //0 (starting proposer) for genesis block

	//derived from header, with help of block store
	uint64_t block_height;
	block_ptr_t parent_block_ptr;
	block_ptr_t justify_block_ptr; // block pointed to by qc

	//fields to build for this block
	QuorumCertificate self_qc;

	//status fields
	bool decided;
	std::atomic_flag written_to_disk;

	//caching hash of body == header.body_hash
	mutable bool hash_checked;
	mutable bool hash_valid;

	// garbage collection
	bool flushed_from_memory;

	HotstuffBlock(genesis_block_t);

	//load block
	HotstuffBlock(HotstuffBlockWire&& _wire_block, load_from_disk_block_t);

	HotstuffBlock(HotstuffBlockWire&& _wire_block, speedex::ReplicaID proposer);

public:

	// copies wire_block, unfortunately
	HotstuffBlockWire to_wire() const {
		return wire_block;
	}

	bool has_body() const;

	bool has_been_decided() const {
		return decided;
	}

	void decide() {
		decided = true;
	}

	speedex::ReplicaID 
	get_proposer() const {
		return proposer;
	}

	uint64_t get_height() const {
		return block_height;
	}

	bool supports_nonempty_child_proposal(const speedex::ReplicaID self_id, int depth = 3) const;

	bool validate_hash() const;

	/*
	 * Checks that the block passes basic Hotstuff validity checks.
	 * (1) hash(wire_block.body) == wire_block.body_hash
	 * (2) quorum cert is valid
	 */
	bool validate_hotstuff(const speedex::ReplicaConfig& config) const;

	/*
	 * Try to parse body into a speedex (header, txs) pair.
	 * body.size() == 0 is considered to be no speedex block (which is valid for speedex.  Just ignored.)
	 * parse failures are considered an invalid speedex block. (but valid for hotstuff)
	 */
	template<typename ParseType>
	std::unique_ptr<ParseType>
	try_vm_parse() 
	{
		if (!has_body()) {
			return nullptr;
		}
		auto parsed_block_body = std::make_unique<ParseType>();

		try {
			xdr::xdr_from_opaque(wire_block.body, *parsed_block_body);
		} catch(...) {
			HOTSTUFF_INFO("block parse failed");
			return nullptr;
		}
		return parsed_block_body;
	}

	const speedex::Hash& 
	get_hash() const {
		return self_qc.get_obj_hash();
	}

	const speedex::Hash& 
	get_justify_hash() const {
		return parsed_qc->get_obj_hash();
	}

	const speedex::Hash& 
	get_parent_hash() const {
		return wire_block.header.parent_hash;
	}

	block_ptr_t 
	get_parent() const {
		return parent_block_ptr;
	}

	block_ptr_t
	get_justify() const {
		return justify_block_ptr;
	}

	const QuorumCertificate&
	get_justify_qc() const {
		return *parsed_qc;
	}

	QuorumCertificate&
	get_self_qc() {
		return self_qc;
	}

	void set_parent(block_ptr_t parent_block);
	void set_justify(block_ptr_t justify_block);

	void write_to_disk();

	void flush_from_memory();

	bool
	is_flushed_from_memory() const {
		return flushed_from_memory;
	}

	static block_ptr_t
	genesis_block();

	static block_ptr_t
	receive_block(
		HotstuffBlockWire&& body,
		speedex::ReplicaID source);

	static block_ptr_t 
	mint_block(
		xdr::opaque_vec<>&& body, 
		QuorumCertificateWire const& qc_wire, 
		speedex::Hash const& parent_hash, 
		speedex::ReplicaID proposer);

	static block_ptr_t
	load_decided_block(
		speedex::Hash const& hash); 
};

} /* hotstuff */