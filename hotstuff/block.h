#pragma once

#include "hotstuff/crypto.h"
#include "hotstuff/replica_config.h"

#include "xdr/hotstuff.h"

#include <cstdint>

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
	QuorumCertificate parsed_qc;

	//delayed parse
	std::optional<HeaderDataPair> parsed_block_body;

	//derived from header, with help of block store
	uint64_t block_height;
	block_ptr_t parent_block_ptr;

	//fields to build for this block
	QuorumCertificate self_qc;

public:

	HotstuffBlock(HotstuffBlockWire&& _wire_block);

	bool has_body() const;

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
	bool try_delayed_parse();

	const speedex::Hash& 
	get_hash() const {
		return self_qc.get_obj_hash();
	}

	const speedex::Hash& 
	get_justify_hash() const {
		return parsed_qc.get_obj_hash();
	}

	const speedex::Hash& 
	get_parent_hash() const {
		return wire_block.header.parent_hash;
	}

	void set_parent(block_ptr_t parent_block);
};


} /* hotstuff */