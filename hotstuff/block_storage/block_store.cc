#include "hotstuff/block_storage/block_store.h"

#include "hotstuff/block_storage/io_utils.h"

#include "utils/debug_macros.h"
#include "utils/debug_utils.h"

namespace hotstuff {

void
BlockStore::write_to_disk(const Hash& hash) {
	std::lock_guard lock(mtx);

	auto it = block_cache.find(hash);


	if (it == block_cache.end()) {
		throw std::runtime_error("could not find block that needs to go to disk!");
	}

	it -> second.block.write_to_disk();
}

bool
BlockStore::insert_block(block_ptr_t block)
{
	std::lock_guard lock(mtx);

	auto const& parent = block->get_parent_hash();
	auto it = block_cache.find(parent);
	
	if (parent_it == block_cache.end()) {
		HOTSTUFF_INFO("failed to find parent for %s", debug::array_to_str(parent.data(), parent.size()));
		return false;
	}

	auto const& justify_hash = block -> get_justify_hash();
	auto justify_it = block_cache.find(justify_hash);

	if (justify_it == block_cache.end()) {
		HOTSTUFF_INFO("failed to find justify for %s", debug::array_to_str(justify_hash.data(), justify_hash.size()));
		return false;
	}

	block -> set_parent(parent_it -> second.block);
	block -> set_justify(justify_it -> second.block);

	block_cache.emplace(block -> get_hash(), BlockContext{.block = block, .on_disk = false});
	return true;
}

block_ptr_t 
BlockStore::get_block(const Hash& block_hash)
{
	std::lock_guard lock(mtx);

	auto it = block_cache.find(block_hash);
	if (it == block_cache.end()) {
		HOTSTUFF_INFO("failed to find block %s", debug::array_to_str(block_hash.data(), block_hash.size()));
		return nullptr;
	}

	return it -> second.block;
}




} /* hotstuff */