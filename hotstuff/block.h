#pragma once

#include "hotstuff/crypto.h"
#include "hotstuff/replica_config.h"

#include "utils/debug_macros.h"

#include "xdr/hotstuff.h"

#include <cstdint>

#include <xdrpp/marshal.h>

namespace hotstuff {

class HotstuffBlock;

using block_ptr_t = std::shared_ptr<HotstuffBlock>;


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

	//delayed parse
	//std::optional<HeaderDataPair> parsed_block_body;

	//derived from header, with help of block store
	uint64_t block_height;
	block_ptr_t parent_block_ptr;
	block_ptr_t justify_block_ptr; // block pointed to by qc

	//fields to build for this block
	QuorumCertificate self_qc;

	//status fields
	bool decided;
	bool applied;
	std::atomic_flag written_to_disk;

	//TODO if you get qc on block that's not self produced, leader knows it was demoted.
	// State machine recovery to be done in that case.
	bool self_produced;

public:

	HotstuffBlock(HotstuffBlockWire&& _wire_block);

	//make genesis block
	HotstuffBlock();

	// copies wire_block, unfortunately
	HotstuffBlockWire to_wire() const {
		return wire_block;
	}

	bool has_body() const;

	bool has_been_decided() const {
		return decided;
	}

	bool has_been_applied() const {
		return applied;
	}

	void mark_applied() {
		applied = true;
	}

	void decide() {
		decided = true;
	}

	bool is_self_produced() const {
		return self_produced;
	}

	void set_self_produced() {
		self_produced = true;
	}

	uint64_t get_height() const {
		return block_height;
	}

	/*
	 * Checks that the block passes basic Hotstuff validity checks.
	 * (1) hash(wire_block.body) == wire_block.body_hash
	 * (2) quorum cert is valid
	 */
	bool validate_hotstuff(const ReplicaConfig& config) const;

	/*
	 * Try to parse body into a speedex (header, txs) pair.
	 * body.size() == 0 is considered to be no speedex block (which is valid for speedex.  Just ignored.)
	 * parse failures are considered an invalid speedex block. (but valid for hotstuff)
	 */
	template<typename ParseType>
	std::optional<ParseType>
	try_vm_parse() 
	{
		if (!has_body()) {
			return std::nullopt;
		}
		auto parsed_block_body = std::make_optional<ParseType>();

		try {
			xdr::xdr_from_opaque(wire_block.body, *parsed_block_body);
		} catch(...) {
			HOTSTUFF_INFO("block parse failed");
			return std::nullopt;
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

	static block_ptr_t 
	mint_block(xdr::opaque_vec<>&& body, QuorumCertificateWire const& qc_wire, speedex::Hash const& parent_hash);
};



} /* hotstuff */