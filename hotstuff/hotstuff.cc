#include "hotstuff/hotstuff.h"

#include "hotstuff/crypto.h"

namespace hotstuff {

void
HotstuffAppBase::do_vote(block_ptr_t block, ReplicaID proposer) 
{
	PartialCertificate cert(block -> get_hash(), secret_key);

	//forwards vote to 'proposer' (which is usually, but 
	// doesn't have to be, the proposer of the block being voted on)
	protocol_manager.send_vote_to(block, cert, proposer);
}

void
HotstuffAppBase::do_propose(xdr::opaque_vec<>&& body)
{
	std::lock_guard lock(proposal_mutex);

	auto newly_minted_block = HotstuffBlock::mint_block(std::move(body), hqc.second, b_leaf->get_hash());

	if (!block_store.insert_block(newly_minted_block)) {
		throw std::runtime_error("failed to insert newly minted block?");
	}

	b_leaf = newly_minted_block;

	protocol_manager.broadcast_proposal(newly_minted_block);
}


} /* hotstuff */