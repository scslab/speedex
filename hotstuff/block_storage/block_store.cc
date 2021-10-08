#include "hotstuff/block_storage/block_store.h"

#include "hotstuff/block_storage/io_utils.h"

#include "utils/debug_macros.h"
#include "utils/debug_utils.h"

namespace hotstuff {

void
BlockStore::write_to_disk(const Hash& hash) {
	std::lock_guard lock(mtx);

	auto it = block_cache.find(hash);

	if (it->second.on_disk) {
		HOTSTUFF_INFO("second write to disk of %s", debug::array_to_str(hash.data(), hash.size()));
		return;
	}

	if (it == block_cache.end()) {
		throw std::runtime_error("could not find block that needs to go to disk!");
	}

	save_block(*(it->second.block));
	it->second.on_disk = true;
}

bool
BlockStore::insert_block(block_ptr_t block)
{
	std::lock_guard lock(mtx);

	auto const& parent = block->get_parent_hash();
	auto it = block_cache.find(parent);
	
	if (it == block_cache.end()) {
		HOTSTUFF_INFO("failed to find parent for %s", debug::array_to_str(parent.data(), parent.size()));
		return false;
	}

	block -> set_parent(it->second.block);

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