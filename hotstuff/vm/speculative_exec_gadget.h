#pragma once

#include "hotstuff/block.h"

#include <cstdint>
#include <forward_list>
#include <optional>
#include <mutex>

namespace hotstuff {

/**
 * Acquire lock before use to make threadsafe
 * (on_commit_hotstuff acquires lock internally,
 * other methods do not).
 * Additionally, take care to ensure
 * that VM stays in sync with these commands.
 * VM will have some command queue, presumably.
 * If we cancel the pending items in this gadget,
 * we need to also roll back the VM.
 * The main race condition would be if we cancel
 * gadget contents while the VM is producing a block
 * based off the canceled contents.
 * This motivates the strict sequentiality and
 * the tracking of speculation_head_hotstuff_height.
 * The check for hotstuff_height == speculation_head_hotstuff_height
 * catches this.
 * Liveness gadget should take care elsewhere to ensure
 * to only propose nonempty blocks on self-proposals.
 * That say, the VM doesn't stack a proposal on top of a
 * conflicting proposal.
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

	bool empty() const {
		return last_elt_iter == height_map.end();
	}

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

	std::lock_guard<std::mutex> lock() {
		return {mtx, std::adopt_lock};
	}

	const value_t& 
	get_lowest_speculative_hotstuff_height() const {
		if (empty()) {
			throw std::runtime_error("can't access from empty list!");
		}
		return *height_map.begin();
	}
};

template<typename VMValueType>
void 
SpeculativeExecGadget<VMValueType>::add_height_pair(uint64_t hotstuff_height, VMValueType vm_value)
{
	if (speculation_head_hotstuff_height != hotstuff_height) {
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
		throw std::runtime_error("committing on empty map");
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
	height_map.clear();
	last_elt_iter = height_map.end();

	speculation_head_hotstuff_height = highest_committed_height + 1;
}

} /* hotstuff */
