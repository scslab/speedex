#include "hotstuff/consensus.h"

namespace hotstuff {


HotstuffCore::HotstuffCore(const ReplicaConfig& config, ReplicaID self_id)
	: genesis_block(std::make_shared<HotstuffBlock>())
	, hqc({genesis_block, genesis_block->get_self_qc().serialize()})
	, b_lock(genesis_block)
	, b_exec(genesis_block)
	, vheight(0)
	, self_id(self_id)
	, config(config)
	, b_leaf(genesis_block)
	{}

//qc_block is the block pointed to by qc
void
HotstuffCore::update_hqc(block_ptr_t const& qc_block, const QuorumCertificate& qc)
{
	std::lock_guard lock(proposal_mutex);

	if (qc_block -> get_height() > hqc.first -> get_height()) {
		hqc = {qc_block, qc.serialize()};
		b_leaf = qc_block;

		if (!b_leaf -> is_self_produced()) {
			notify_vm_of_qc_on_nonself_block(qc_block);
		}
	}
}

void 
HotstuffCore::update(const block_ptr_t& nblk) {
    /* nblk = b*, blk2 = b'', blk1 = b', blk = b */
    
    /* honest quorum has voted once on blk2, twice on blk1, thrice on blk
	 * Honest node should have data on disk before commiting.
	 * That way, we guarantee that if the whole system crashes, we won't ever have
	 * lost data.
	 * This means that we need data on disk before we can send third vote on something.
	 * This means that when we send the second vote, we should at least start writing to disk.
	 * That means starting to write from blk1 onwards.
	 */


    /* three-step HotStuff */
    const auto blk2 = nblk->get_justify();
    if (blk2 == nullptr) return; // only occurs in case of genesis block
    
    /* decided blk could possible be incomplete due to pruning */
    if (blk2->has_been_decided()) return;
    
    update_hqc(blk2, nblk->get_justify_qc());

    const auto blk1 = blk2->get_justify();
    if (blk1 == nullptr) return;
    if (blk1->has_been_decided()) return;
    if (blk1->get_height() > b_lock->get_height()) b_lock = blk1;

    //TODO threadpool or async worker to do writing in background
    b_lock -> write_to_disk();

    const auto blk = blk1->get_justify();
    if (blk == nullptr) return;
    if (blk->has_been_decided()) return;

    /* commit requires direct parent */
    if (blk2->get_parent() != blk1 || blk1->get_parent() != blk) return;

    /* otherwise commit */
    std::vector<block_ptr_t> commit_queue;
    block_ptr_t b;

    // Once we have a 3-chain, we can commit everything in the (parent-linked) chain
    // NOT just the 3-chain.
    for (b = blk; b->get_height() > b_exec->get_height(); b = b->get_parent())  
    { 
        commit_queue.push_back(b);
    }
    if (b != b_exec)
        throw std::runtime_error("safety breached :( ");
    for (auto it = commit_queue.rbegin(); it != commit_queue.rend(); it++)
    {
        block_ptr_t blk = *it;
        blk -> decide();

        if (!blk->has_been_applied()) {
        	apply_block(blk);
        	blk->mark_applied();

        }

    }
    b_exec = blk;

    notify_vm_of_highest_commitment(b_exec);
}

void HotstuffCore::on_receive_vote(const PartialCertificate& partial_cert, block_ptr_t certified_block, ReplicaID voterid) {

    auto& self_qc = certified_block -> get_self_qc();

    bool had_quorum_before = self_qc.has_quorum(config);

    self_qc.add_partial_certificate(voterid, partial_cert);

    bool has_quorum_after = self_qc.has_quorum(config);

    if (has_quorum_after && !had_quorum_before)
    {
    	update_hqc(certified_block, self_qc);

    	on_new_qc(certified_block -> get_hash());
    	//callback here to pacemaker
    }
}


//assumes input bnew has been validated (for hotstuff criteria)
// That means it has a height in the block dag AND qc passes.
void
HotstuffCore::on_receive_proposal(block_ptr_t bnew, ReplicaID proposer)
{

	bool opinion = false;
	if (bnew -> get_height() > vheight)
	{
		auto justify_block = bnew -> get_justify();
		if (justify_block -> get_height() > b_lock -> get_height())
		{
			opinion = true;
		} 
		else
		{
			block_ptr_t b;
			uint64_t b_lock_height = b_lock -> get_height();
			for (b = bnew; b->get_height() > b_lock_height; b = b -> get_parent())
			{
				if (b == b_lock) {
					opinion = true;
				}
			}
		}
	}

	if (opinion)
	{
		vheight = bnew -> get_height();
		do_vote(bnew, proposer);
	}
}

} /* hotstuff */