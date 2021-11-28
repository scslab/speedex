#include "hotstuff/hotstuff.h"

#include "hotstuff/block.h"
#include "hotstuff/crypto.h"

#include "utils/debug_utils.h"

namespace hotstuff {

using xdr::operator==;

using speedex::Hash;
using speedex::ReplicaConfig;
using speedex::ReplicaID;

HotstuffAppBase::HotstuffAppBase(const ReplicaConfig& config_, ReplicaID self_id, speedex::SecretKey sk)
	: HotstuffCore(config_, self_id)
	, block_store(get_genesis())
	, block_fetch_manager(block_store, config)
	, block_fetch_server(block_store)
	, event_queue(*this)
	, network_event_queue(event_queue, block_fetch_manager, block_store, config)
	, protocol_manager(event_queue, config, self_id)
	, protocol_server(network_event_queue, config)
	, secret_key(sk)
	, qc_wait_mtx()
	, qc_wait_cv()
	, latest_new_qc(std::nullopt)
	, cancel_wait(false)
	{
		block_fetch_manager.init_configs(network_event_queue);
	}

void
HotstuffAppBase::do_vote(block_ptr_t block, ReplicaID proposer) 
{
	PartialCertificate cert(block -> get_hash(), secret_key);

	//forwards vote to 'proposer' (which is usually, but 
	// doesn't have to be, the proposer of the block being voted on)
	protocol_manager.send_vote_to(block, cert, proposer);
}

void
HotstuffAppBase::notify_ok_to_prune_blocks(uint64_t committed_hotstuff_height) {
	const uint64_t keep_depth = 100;
	block_store.prune_below_height(committed_hotstuff_height > keep_depth ? committed_hotstuff_height - keep_depth : 0);
}

block_ptr_t 
HotstuffAppBase::find_block_by_hash(speedex::Hash const& hash) 
{
	auto out = block_store.get_block(hash);
	if (!out) {
		throw std::runtime_error("Failed to find block during initialization");
	}
	return out;
}


speedex::Hash
HotstuffAppBase::do_propose()
{
	std::lock_guard lock(proposal_mutex);

	uint64_t new_block_height = b_leaf -> get_height() + 1;

	HOTSTUFF_INFO("PROPOSE: new height %lu", new_block_height);

	auto body = get_next_vm_block(b_leaf -> supports_nonempty_child_proposal(self_id), new_block_height);

	auto newly_minted_block = HotstuffBlock::mint_block(std::move(body), hqc.second, b_leaf->get_hash(), self_id);

	if (block_store.insert_block(newly_minted_block)) {
		throw std::runtime_error("failed to insert newly minted block?");
	}

	b_leaf = newly_minted_block;

	protocol_manager.broadcast_proposal(newly_minted_block);
	return b_leaf -> get_hash();
}

speedex::Hash
HotstuffAppBase::do_empty_propose()
{
	std::lock_guard lock(proposal_mutex);

	uint64_t new_block_height = b_leaf -> get_height() + 1;

	HOTSTUFF_INFO("PROPOSE: new height %lu", new_block_height);

	auto body = get_next_vm_block(false, new_block_height);

	auto newly_minted_block = HotstuffBlock::mint_block(std::move(body), hqc.second, b_leaf->get_hash(), self_id);

	if (block_store.insert_block(newly_minted_block)) {
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

uint64_t
HotstuffAppBase::reload_decided_blocks() {
	
	uint64_t highest_decision = 0;
	{
		auto cursor = decided_hash_index.forward_cursor();

		for (auto [_, hash] : cursor)
		{
			HOTSTUFF_INFO("LOADING: block hash %s", debug::hash_to_str(hash).c_str());
			block_ptr_t blk = HotstuffBlock::load_decided_block(hash);
			auto res = block_store.insert_block(blk);
			if (res) {
				throw std::runtime_error("unable to properly load data into block store");
			}
			highest_decision = blk -> get_height();
		}
	}
	reload_state_from_index();
	return highest_decision;
}

} /* hotstuff */