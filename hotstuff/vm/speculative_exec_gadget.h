#pragma once

#include "hotstuff/block.h"

#include <cstdint>
#include <forward_list>
#include <optional>
#include <mutex>

namespace hotstuff {

/**
 * Acquire lock before use to make threadsafe
 * (on_commit_hotstuff and clear acquire lock internally,
 * other methods do not).
 * Additionally, take care to ensure
 * that VM stays in sync with these commands.
 * VM will have some command queue, presumably.
 * If we cancel the pending items in this gadget,
 * we need to also roll back the VM.
 * The main race condition would be if we cancel
 * gadget contents while the VM is producing a block
 * based off the canceled contents.
 * ==Note== this race condtion is accounted for
 * by the explicit rewind condition in apply_block
 * ========
 * This motivates the strict sequentiality and
 * the tracking of speculation_head_hotstuff_height.
 * The check for hotstuff_height == speculation_head_hotstuff_height
 * catches this.
 * Liveness gadget should take care elsewhere to ensure
 * to only propose nonempty blocks on self-proposals.
 * That say, the VM doesn't stack a proposal on top of a
 * conflicting proposal.
 * ==Note== this last bit is not required from the VM's pov
 * but from hotstuff's.  It'd be incorrect to interleave
 * proposals from different VM's, without synchronizing the VMs
 * between proposals (i.e. VM1 proposes X on Z, VM2 proposes Y on Z,
 * and then hotstuff on machine 1 proposes X on Y).
 * ========
*/
template<typename VMValueType>
class SpeculativeExecGadget {
	// (hotstuff block height, vm value)
	using value_t = std::pair<uint64_t, VMValueType>;
	using list_t = std::forward_list<value_t>;
	using iter_t = list_t::iterator;

	list_t height_map;
	iter_t last_elt_iter;

	std::mutex mtx;

	uint64_t speculation_head_hotstuff_height;

	uint64_t highest_committed_height;

public:

	SpeculativeExecGadget()
		: height_map()
		, last_elt_iter(height_map.end())
		, speculation_head_hotstuff_height(1)
		, highest_committed_height(0)
		{}

	void add_height_pair(uint64_t hotstuff_height, VMValueType vm_value);

	VMValueType on_commit_hotstuff(uint64_t hotstuff_height);

	void clear();

	void init_from_disk(uint64_t highest_decided_height) {
		highest_committed_height = highest_decided_height;
		clear(); // set everything else
	}

	std::lock_guard<std::mutex> lock() {
		mtx.lock();
		return {mtx, std::adopt_lock};
	}

	const value_t& 
	get_lowest_speculative_hotstuff_height() const {
		if (empty()) {
			throw std::runtime_error("can't access from empty list!");
		}
		return *height_map.begin();
	}

	bool empty() const {
		return last_elt_iter == height_map.end();
	}
};

template<typename VMValueType>
void 
SpeculativeExecGadget<VMValueType>::add_height_pair(uint64_t hotstuff_height, VMValueType vm_value)
{
	if (speculation_head_hotstuff_height != hotstuff_height) {
		std::printf("WARN: speculation_head_hotstuff_height != hotstuff_height\n");
		return;
	}
	speculation_head_hotstuff_height ++ ;

	if (last_elt_iter == height_map.end()) {
		last_elt_iter = height_map.insert_after(height_map.before_begin(), {hotstuff_height, vm_value});
		return;
	}
	last_elt_iter = height_map.insert_after(last_elt_iter, {hotstuff_height, vm_value});
}

template<typename VMValueType>
VMValueType
SpeculativeExecGadget<VMValueType>::on_commit_hotstuff(uint64_t hotstuff_height)
{
	std::lock_guard lock(mtx);

	if (empty()) {
		throw std::runtime_error(std::string("committing on empty map at hs height " + std::to_string(hotstuff_height)));
	}

	auto const& front = height_map.front();
	if (hotstuff_height != front.first) {
		throw std::runtime_error("gap in commit log!");
	}
	auto out = front.second;
	highest_committed_height = front.first;

	auto it = height_map.erase_after(height_map.before_begin());
	if (it == height_map.end()) {
		last_elt_iter = height_map.end();
	}
	return out;

}

template<typename VMValueType>
void
SpeculativeExecGadget<VMValueType>::clear()
{
	std::lock_guard lock(mtx);
	height_map.clear();
	last_elt_iter = height_map.end();

	speculation_head_hotstuff_height = highest_committed_height + 1;
}

} /* hotstuff */
