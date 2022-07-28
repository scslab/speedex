#pragma once

#include "hotstuff/hotstuff_debug_macros.h"
#include "hotstuff/vm/vm_block_id.h"
#include "hotstuff/vm/vm_base.h"

#include <compare>
#include <cstdint>
#include <optional>
#include <vector>

#include <xdrpp/marshal.h>

namespace hotstuff {

class LogAccessWrapper;

//! Demo class for vm types + block ids.
//! Important: block_id should be in bijection with
//! block_type (up to computational limits; e.g. block_id = sha256(block_type) is fine)

//typedef VMBlockID<uint64_t> CountingVMBlockID;

struct CountingVMBlock : public VMBlock
{
	uint64_t value;

	CountingVMBlock(uint64_t value)
		: value(value)
		{}

	CountingVMBlock(xdr::opaque_vec<> const& v)
		: value(0)
	{
		xdr::xdr_from_opaque(v, value);
	}

	VMBlockID get_id() const override final
	{
		return VMBlockID(xdr::xdr_to_opaque(value));
	}

	xdr::opaque_vec<> serialize() const override final
	{
		return xdr::xdr_to_opaque(value);
	}
};

class CountingVM : public VMBase {
	uint64_t state = 0;
	uint64_t last_committed_state = 0;

public:

	//using block_type = uint64_t;
	//using block_id = CountingVMBlockID;
/*
	static block_id nonempty_block_id(block_type const& blk) {
		return CountingVMBlockID{blk};
	}
	static block_id empty_block_id() {
		return CountingVMBlockID{};
	} */

	uint64_t get_last_committed_height() const
	{
		return last_committed_state;
	}
	uint64_t get_speculative_height() const
	{
		return state;
	} 

	void init_clean() override final {
		state = 0;
		last_committed_state = 0;
	}

	void init_from_disk(LogAccessWrapper const& lmdb) override final;

	std::unique_ptr<VMBlock> propose() override final {
		state++;
		HOTSTUFF_INFO("VM: proposing value %lu", state);
		return std::make_unique<CountingVMBlock>(state);
	}

	std::unique_ptr<VMBlock>
	try_parse(xdr::opaque_vec<> const& body) override final
	{
		uint64_t res;
		try {
			xdr::xdr_from_opaque(body, res);
		} catch (...)
		{
			return nullptr;
		}

		return std::make_unique<CountingVMBlock>(res);
	}

	// Main workflow for non-proposer is exec_block (called indirectly
	// by update) immediately followed by log_commitment
	// Proposer skips the exec_block call.
	void exec_block(const VMBlock& blk) override final;

	void log_commitment(const VMBlockID& id) override final;

	void rewind_to_last_commit() override final;

	~CountingVM() override final {}
};

} /* hotstuff */
