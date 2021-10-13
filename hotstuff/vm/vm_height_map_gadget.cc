#include "hotstuff/vm/vm_height_map_gadget.h"

#include <stdexcept>

namespace hotstuff {

void 
HeightMapGadget::add_height_pair(uint64_t hotstuff_height, uint64_t vm_height)
{
	if (last_elt_iter == height_map.end()) {
		last_elt_iter = height_map.insert_after(height_map.before_begin(), {hotstuff_height, vm_height});
		return;
	}
	last_elt_iter = height_map.insert_after(last_elt_iter, {hotstuff_height, vm_height});
}

std::optional<uint64_t> 
HeightMapGadget::on_commit_hotstuff(uint64_t hotstuff_height)
{
	auto const& front = height_map.front();
	if (hotstuff_height < front.first) {
		return std::nullopt;
	}
	if (hotstuff_height  == front.first) {
		uint64_t out = front.second;
		auto it = height_map.erase_after(height_map.before_begin());
		if (it == height_map.end()) {
			last_elt_iter = height_map.end();
		}
		return out;
	}
	throw std::runtime_error("invalid commit order! hotstuff_height > front.first");
}

void
HeightMapGadget::clear()
{
	height_map.clear();
	last_elt_iter = height_map.end();
}

} /* hotstuff */
