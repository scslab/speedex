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
    ReplicaID self_id;           							/**< replica id of self in ReplicaConfig */
    ReplicaConfig config;	                 				/**< replica configuration */
    block_ptr_t b_leaf;										/**< highest tail block.  Build on this block */

	std::mutex proposal_mutex; 								/**< lock access to b_leaf and hqc */

	block_ptr_t get_genesis() const {
		return genesis_block;
	}

private:
	//qc_block is the block pointed to by qc
	void update_hqc(block_ptr_t const& qc_block, const QuorumCertificate& qc);

	void update(const block_ptr_t& nblk);

public:

	HotstuffCore(const ReplicaConfig& config, ReplicaID self_id);

	const ReplicaConfig& get_config() {
		return config;
	}

	void 
	on_receive_vote(
		const PartialCertificate& partial_cert, 
		block_ptr_t certified_block, 
		ReplicaID voterid);

	void 
	on_receive_proposal(block_ptr_t bnew, ReplicaID proposer);

protected:

	// should send vote to block proposer
	virtual void do_vote(block_ptr_t block, ReplicaID proposer) = 0;

	// called on getting a new qc.  Input is hash of qc'd obj.
	virtual void on_new_qc(speedex::Hash const& hash) = 0;

	virtual void apply_block(block_ptr_t block) = 0;

	virtual void notify_vm_of_commitment(block_ptr_t blk) = 0;

	virtual void notify_vm_of_qc_on_nonself_block(block_ptr_t b_other) = 0;
};


} /* hotstuff */
