#include "hotstuff/hotstuff.h"

#include "hotstuff/crypto.h"

namespace hotstuff {

using xdr::operator==;

void
HotstuffAppBase::do_vote(block_ptr_t block, ReplicaID proposer) 
{
	PartialCertificate cert(block -> get_hash(), secret_key);

	//forwards vote to 'proposer' (which is usually, but 
	// doesn't have to be, the proposer of the block being voted on)
	protocol_manager.send_vote_to(block, cert, proposer);
}

speedex::Hash
HotstuffAppBase::do_propose(xdr::opaque_vec<>&& body)
{
	std::lock_guard lock(proposal_mutex);

	auto newly_minted_block = HotstuffBlock::mint_block(std::move(body), hqc.second, b_leaf->get_hash());

	if (!block_store.insert_block(newly_minted_block)) {
		throw std::runtime_error("failed to insert newly minted block?");
	}

	b_leaf = newly_minted_block;

	protocol_manager.broadcast_proposal(newly_minted_block);
	return b_leaf -> get_hash();
}

void 
HotstuffAppBase::on_new_qc(speedex::Hash const& hash)
{
	std::lock_guard lock(qc_wait_mtx);
	latest_new_qc = hash;
	qc_wait_cv.notify_all();
}

bool
HotstuffAppBase::wait_for_new_qc(speedex::Hash const& expected_next_qc)
{
	std::unique_lock lock(qc_wait_mtx);
	if ((!latest_new_qc) && (!cancel_wait)) {
		qc_wait_cv.wait(lock, [this] () -> bool {
			return ((bool) latest_new_qc) || cancel_wait;
		});
	}
	if (cancel_wait) {
		cancel_wait = false;
		return false;
	}

	bool res = (*latest_new_qc == expected_next_qc);

	latest_new_qc = std::nullopt;

	return res;
}

void 
HotstuffAppBase::cancel_wait_for_new_qc() {
	std::lock_guard lock(qc_wait_mtx);
	cancel_wait = true;
	qc_wait_cv.notify_all();
}


} /* hotstuff */