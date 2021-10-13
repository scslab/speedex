#pragma once

#include <cstdint>
#include <forward_list>
#include <optional>

namespace hotstuff {

class HeightMapGadget {
	// (hotstuff block height, vm value height)
	using value_t = std::pair<uint64_t, uint64_t>;
	using list_t = std::forward_list<value_t>;
	using iter_t = list_t::iterator;

	list_t height_map;
	iter_t last_elt_iter;

public:

	HeightMapGadget()
		: height_map()
		, last_elt_iter(height_map.end())
		{}

	void add_height_pair(uint64_t hotstuff_height, uint64_t vm_height);

	std::optional<uint64_t> on_commit_hotstuff(uint64_t hotstuff_height);

	void clear();
};

} /* hotstuff */
