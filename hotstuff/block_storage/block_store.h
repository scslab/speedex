#pragma once

#include "xdr/types.h"
#include "xdr/hotstuff.h"

#include "hotstuff/block.h"
#include "hotstuff/block_storage/garbage_collector.h"
#include "hotstuff/block_storage/io_utils.h"

#include <map>
#include <mutex>
#include <optional>

namespace hotstuff {

class BlockStore {

	struct BlockContext {
		block_ptr_t block;
	};

	using Hash = speedex::Hash;

	std::map<Hash, BlockContext> block_cache;

	std::mutex mtx;

	BlockGarbageCollector gc_collector;

public:

	BlockStore(block_ptr_t genesis_block) 
		: block_cache()
		, mtx()
		, gc_collector()
		{
			block_cache.emplace(genesis_block->get_hash(), genesis_block);
			make_block_folder();
		}

	// Call before committing to any block (and executing it).
	void write_to_disk(const Hash& block_hash);

	struct MissingDependencies {
		std::optional<speedex::Hash> parent_hash;
		std::optional<speedex::Hash> justify_hash;

		operator bool() {
			return ((bool) parent_hash) || ((bool) justify_hash);
		}
	};

	// Sets height of block.
	// Parent block & justify block must exist in cache.  
	// Returns missing dependencies if not (and is no-op).
	// This could happen for two reasons: (1) is a byzantine block proposer
	// (i.e. propose with a good justify, but a bad parent)
	// (2) is we incorrectly pruned out the parent
	// (3) is we haven't gotten the parent.
	// (1) we can safely throw away, (2) is a bug we should fix (if it happens),
	// and (3) should not happen if we request parent on reception of proposal.
	// Sets the height of the new block and parent block ptr.
	// Note: casts to TRUE if insertion failed (True if there are missing dependencies)
	MissingDependencies
	insert_block(block_ptr_t block);

	//returns block from memory, if it exists.
	// Does not look to disk for blocks.
	block_ptr_t get_block(const Hash& block_hash);

	/* should not prune blocks at height >= committed height, but below that height is safe.

	Honest validators vote for a block only if
		- block extends b_lock OR
		- block.justfiy.height > b_lock.height

	and b_lock.height is always higher than the highest committed block's height.

	Below that, any block with a parent below the committed height is either executed (on disk)
	or no longer relevant, because there's a higher block with a quorum cert (and honest replicas will propose
	with this justification, and no quorum of honest replica will vote for a block with a justification or parent lower than
	the highest qc's height)
	So we can throw out that information and not respond to requests for said block.

	*/
	void prune_below_height(const uint64_t prune_height);

};


} /* hotstuff */
