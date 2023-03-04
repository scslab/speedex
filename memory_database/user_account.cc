/**
 * SPEEDEX: A Scalable, Parallelizable, and Economically Efficient Decentralized Exchange
 * Copyright (C) 2023 Geoffrey Ramseyer

 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "memory_database/user_account.h"

#include <xdrpp/marshal.h>
#include <cinttypes>


namespace speedex {

UserAccount::UserAccount(AccountID owner, PublicKey public_key)
	: uncommitted_assets_mtx()
	, owned_assets()
	, uncommitted_assets()
	, seq_tracker(0)
	, owner(owner)
	, pk(public_key)
{}

UserAccount::UserAccount()
	: uncommitted_assets_mtx()
	, owned_assets()
	, uncommitted_assets()
	, seq_tracker(UINT64_MAX)
	, owner()
	, pk()
{}

TransactionProcessingStatus 
UserAccount::reserve_sequence_number(
	uint64_t sequence_number) {

	return seq_tracker.reserve_sequence_number(sequence_number);
}

void UserAccount::release_sequence_number(
	uint64_t sequence_number) {

	seq_tracker.release_sequence_number(sequence_number);
}

void UserAccount::commit_sequence_number(
	uint64_t sequence_number) {
	seq_tracker.commit_sequence_number(sequence_number);
}

void UserAccount::commit() {
	std::lock_guard lock(uncommitted_assets_mtx);

	for (auto iter = owned_assets.begin(); iter != owned_assets.end(); iter++) {
		iter->commit();
	}
	int uncommitted_size = uncommitted_assets.size();
	for (int i = 0; i < uncommitted_size; i++) {
		owned_assets.emplace_back(uncommitted_assets[i].commit());
	}

	uncommitted_assets.clear();
	seq_tracker.commit();
	//last_committed_id += get_seq_num_increment(
	//	sequence_number_vec.load(std::memory_order_relaxed));
	//sequence_number_vec.store(0, std::memory_order_relaxed);
}

void UserAccount::rollback() {
	std::lock_guard lock(uncommitted_assets_mtx);

	for (auto iter = owned_assets.begin(); iter != owned_assets.end(); iter++) {
		iter->rollback();
	}
	uncommitted_assets.clear();

	seq_tracker.rollback();

	//sequence_number_vec.store(0, std::memory_order_relaxed);
}

bool UserAccount::in_valid_state() {
	std::lock_guard lock(uncommitted_assets_mtx);

	for (auto iter = owned_assets.begin(); iter != owned_assets.end(); iter++) {
		if (!iter->in_valid_state()) {
			return false;
		}
	}
	for (auto iter = uncommitted_assets.begin();
	 	iter != uncommitted_assets.end(); 
	 	iter++) 
	{
		if (!iter->in_valid_state()) {
			return false;
		}
	}
	return true;
}

AccountCommitment UserAccount::produce_commitment() const {
	std::lock_guard lock(uncommitted_assets_mtx);

	AccountCommitment output;
	output.owner = owner;
	for (uint8_t i = 0; i < owned_assets.size(); i++) {
		output.assets.push_back(owned_assets[i].produce_commitment(i));
	}
	output.last_committed_id = seq_tracker.produce_commitment();
//	output.last_committed_id = last_committed_id;
	output.pk = pk;
	return output;
}

AccountCommitment UserAccount::tentative_commitment() const {
	std::lock_guard lock(uncommitted_assets_mtx);

	AccountCommitment output;
	output.owner = owner;
	for (uint8_t i = 0; i < owned_assets.size(); i++) {
		output.assets.push_back(owned_assets[i].tentative_commitment(i));
	}
	for (uint8_t i = 0; i < uncommitted_assets.size(); i++) {
		output.assets.push_back(
			uncommitted_assets[i].tentative_commitment(i + owned_assets.size()));
	}

	output.last_committed_id = seq_tracker.tentative_commitment();
	//output.last_committed_id = 
	//	last_committed_id 
	//	+ get_seq_num_increment(sequence_number_vec.load(std::memory_order_relaxed));
	output.pk = pk;

	return output;
}

AccountID UserAccount::read_lmdb_key(const lmdb::dbval& key) {
	return key.uint64(); // don't copy the database across systems with diff endianness
}

void 
UserAccount::set_owner(AccountID _owner, PublicKey const& _pk, uint64_t _last_committed_id)
{
	owner = _owner;
	pk = _pk;
	seq_tracker.set_last_committed_id(_last_committed_id);
	//last_committed_id = _last_committed_id;
}

UserAccount::UserAccount(UserAccount&& other)
	: uncommitted_assets_mtx()
	, owned_assets(std::move(other.owned_assets))
	, uncommitted_assets(std::move(other.uncommitted_assets))
	, seq_tracker(std::move(other.seq_tracker))
	, owner(other.owner)
	, pk(other.pk) {}

UserAccount::UserAccount(const AccountCommitment& commitment) 
	: owned_assets()
	, uncommitted_assets()
	, seq_tracker(commitment.last_committed_id)
	, owner(commitment.owner)
	, pk(commitment.pk) {

		for (unsigned int i = 0; i < commitment.assets.size(); i++) {
			if (commitment.assets[i].asset < owned_assets.size()) {
				throw std::runtime_error(
					"assets in commitment should be sorted");
			}
			while (owned_assets.size() < commitment.assets[i].asset) {
				owned_assets.emplace_back(0);
			}
			owned_assets.emplace_back(
				commitment.assets[i].amount_available);
		}
	}

UserAccount& 
UserAccount::operator=(UserAccount&& other) {
	owned_assets = std::move(other.owned_assets);
	uncommitted_assets = std::move(other.uncommitted_assets);
	seq_tracker = std::move(other.seq_tracker);

	owner = other.owner;
	pk = other.pk;
	return *this;
}

void 
UserAccount::log() const
{
	for (uint32_t i = 0; i < owned_assets.size(); i++) {
		std::printf(
			"%" PRIu32 "=%" PRId64 " ", i, owned_assets[i].lookup_available_balance());
	}
	std::printf("\n");
}

} /* speedex */
