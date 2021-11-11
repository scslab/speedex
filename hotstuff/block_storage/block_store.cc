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

	it -> second.block->write_to_disk();
}


BlockStore::MissingDependencies
BlockStore::insert_block(block_ptr_t block)
{
	std::lock_guard lock(mtx);

	MissingDependencies missing_dependencies;

	auto const& parent = block->get_parent_hash();
	auto parent_it = block_cache.find(parent);
	
	if (parent_it == block_cache.end()) {
		HOTSTUFF_INFO("failed to find parent for %s", debug::array_to_str(parent.data(), parent.size()).c_str());
		missing_dependencies.parent_hash = parent;
	}

	auto const& justify_hash = block -> get_justify_hash();
	auto justify_it = block_cache.find(justify_hash);

	if (justify_it == block_cache.end()) {
		HOTSTUFF_INFO("failed to find justify for %s", debug::array_to_str(justify_hash.data(), justify_hash.size()).c_str());
		missing_dependencies.justify_hash = justify_hash;
	}

	if (missing_dependencies) {
		return missing_dependencies;
	}

	block -> set_parent(parent_it -> second.block);
	block -> set_justify(justify_it -> second.block);

	block_cache.emplace(block -> get_hash(), BlockContext{.block = block});
	gc_collector.add_block(block);
	return missing_dependencies;
}

block_ptr_t 
BlockStore::get_block(const Hash& block_hash)
{
	std::lock_guard lock(mtx);

	auto it = block_cache.find(block_hash);
	if (it == block_cache.end()) {
		HOTSTUFF_INFO("failed to find block %s", debug::hash_to_str(block_hash).c_str());
		return nullptr;
	}

	return it -> second.block;
}

void 
BlockStore::prune_below_height(const uint64_t prune_height) {
	gc_collector.invoke_gc(prune_height);	
}





} /* hotstuff */