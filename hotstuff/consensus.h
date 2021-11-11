/**
 * File based on libhotstuff/include/consensus.h
 * Original copyright notice follows
 *
 * Copyright 2018 VMware
 * Copyright 2018 Ted Yin
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include "hotstuff/block.h"
#include "hotstuff/crypto.h"
#include "hotstuff/lmdb.h"

#include <mutex>

namespace hotstuff {

class HotstuffCore {
    block_ptr_t genesis_block;                              /** the genesis block */
    /* === state variables === */
    /** block containing the QC for the highest block having one */
protected:
    std::pair<block_ptr_t, QuorumCertificateWire> hqc; 		/**< highest QC */
private:
    block_ptr_t b_lock;                         			/**< locked block */
    block_ptr_t b_exec;                         			/**< last executed block */
    uint32_t vheight;          								/**< height of the block last voted for */

protected:
    /* === auxilliary variables === */
    speedex::ReplicaID self_id;           					/**< replica id of self in ReplicaConfig */
    speedex::ReplicaConfig config;	                 		/**< replica configuration */
    block_ptr_t b_leaf;										/**< highest tail block.  Build on this block */

	mutable std::mutex proposal_mutex; 						/**< lock access to b_leaf and hqc */

	HotstuffLMDB decided_hash_index;

	block_ptr_t get_genesis() const {
		return genesis_block;
	}

	// only for initialization from lmdb
	void reload_state_from_index();
	virtual block_ptr_t find_block_by_hash(speedex::Hash const& hash) = 0;

private:
	//qc_block is the block pointed to by qc
	void update_hqc(block_ptr_t const& qc_block, const QuorumCertificate& qc);

	void update(const block_ptr_t& nblk);

public:

	HotstuffCore(const speedex::ReplicaConfig& config, speedex::ReplicaID self_id);

	const speedex::ReplicaConfig& get_config() {
		return config;
	}

	void 
	on_receive_vote(
		const PartialCertificate& partial_cert, 
		block_ptr_t certified_block, 
		speedex::ReplicaID voterid);

	void 
	on_receive_proposal(block_ptr_t bnew, speedex::ReplicaID proposer);

	speedex::ReplicaID 
	get_last_proposer() const {
		std::lock_guard lock(proposal_mutex);
		return hqc.first -> get_proposer();
	}

	speedex::ReplicaID 
	get_self_id() const {
		return self_id;
	}

protected:

	// should send vote to block proposer
	virtual void do_vote(block_ptr_t block, speedex::ReplicaID proposer) = 0;

	// called on getting a new qc.  Input is hash of qc'd obj.
	virtual void on_new_qc(speedex::Hash const& hash) = 0;

	virtual void apply_block(block_ptr_t block, HotstuffLMDB::txn& tx) = 0;

	virtual void notify_vm_of_commitment(block_ptr_t blk) = 0;

	virtual void notify_vm_of_qc_on_nonself_block(block_ptr_t b_other) = 0;

	virtual void notify_ok_to_prune_blocks(uint64_t committed_hotstuff_height) = 0;
};


} /* hotstuff */
