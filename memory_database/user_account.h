#pragma once

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

/*! \file user_account.h

Manage the account state for one user.
*/

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <set>
#include <unordered_map>
#include <atomic>

#include "memory_database/revertable_asset.h"
#include "memory_database/sequence_tracker.h"

#include "xdr/types.h"
#include "xdr/transaction.h"
#include "xdr/database_commitments.h"

#include "lmdb/lmdb_types.h"

#include "speedex/speedex_static_configs.h"

#include <sodium.h>

namespace speedex {

/*!
Stores a user's account information.

Currently, this consists of asset amounts and a public key.

Modifications to accounts are not threadsafe with commit/rollback.
*/
class UserAccount {

	static_assert(
		MAX_OPS_PER_TX == RESERVED_SEQUENCE_NUM_LOWBITS + 1, "ops mismatch");
	static_assert(
		__builtin_popcount(MAX_OPS_PER_TX) == 1, "should be power of two");

	using amount_t = typename RevertableAsset::amount_t;

	mutable std::mutex uncommitted_assets_mtx;

	// using a map here really slows things down.
	//! Assets owned by the account
	std::vector<RevertableAsset> owned_assets;
	//! Assets owned by the account, but which were not owned (i.e.
	//! memory was not allocated towards recording them) prior to this block.
	//! Separating new assets from prior assets avoids every op
	//! taking a loc on owned_assets.
	std::vector<RevertableAsset> uncommitted_assets;

	//! Bitvector of committed/reserved sequence numbers in the current block.
	//! Offsets are from last_committed_id.  I.e. to reserve sequence number
	//! last_committed_id + 3, set sequence_number_vec |= 1 << (3 + 1)
	//std::atomic<uint64_t> sequence_number_vec;

	//! Highest sequence number that has been committed from the account
	//uint64_t last_committed_id;

	SequenceTracker<MAX_SEQ_NUMS_PER_BLOCK> seq_tracker;

	/*! Apply some function to an asset.  Acquires a lock on uncommittted_assets
	    if the current account does not own the asset in question.
	*/
	template<typename return_type>
	return_type operate_on_asset(
		unsigned int asset, 
		amount_t amount, 
		return_type (*func)(RevertableAsset&, const amount_t&)) {
		unsigned int owned_assets_size = owned_assets.size();
		if (asset >= owned_assets_size) {
			std::lock_guard<std::mutex> lock(uncommitted_assets_mtx);
			while (asset >= owned_assets_size + uncommitted_assets.size()) {
				uncommitted_assets.emplace_back();
			}
			return func(uncommitted_assets[asset - owned_assets_size], amount);
		}
		return func(owned_assets[asset], amount);
	}

	AccountID owner;
	PublicKey pk;

	static_assert(
		sizeof(pk) == crypto_sign_PUBLICKEYBYTES, "pk size should be pkbytes");

public:

	//! Create a new account with a given public key.
	UserAccount(AccountID owner, PublicKey public_key);
	
	//! During genesis init, preallocate data and then later finish initialization with set_owner
	UserAccount();

	void set_owner(AccountID _owner, PublicKey const& _pk, uint64_t _last_committed_id);

	//!We cannot move UserAccounts and concurrently modify them.
	UserAccount(UserAccount&& other);

	//!Needed only for vector.erase, for some dumb reason
	UserAccount& operator=(UserAccount&& other);

	//! Initializes an account from an account database record.
	UserAccount(const AccountCommitment& commitment);

	//! Return the public key associated with the account.
	const PublicKey& get_pk() const {
		return pk;
	}

	//! Return the ID of the owner of the account.
	AccountID get_owner() const {
		return owner;
	}

	//! NOT threadsafe with commit.
	uint64_t get_last_committed_seq_number() const {
		return seq_tracker.produce_commitment();
	}


private:

	friend class MemoryDatabase;

	//! Transfer amount of asset to the account's (unescrowed) balance.
	//! Negative amounts mean a withdrawal.
	//! Unconditionally executes.
	void transfer_available(unsigned int asset, amount_t amount) {
		operate_on_asset<void>(
			asset,
			amount, 
			[] (RevertableAsset& asset, const amount_t& amount) {
				asset.transfer_available(amount);
			});
	}

	//! Escrow amount units of asset.
	void escrow(unsigned int asset, amount_t amount) {
		operate_on_asset<void>(asset, 
			amount, 
			[] (RevertableAsset& asset, const amount_t& amount) {
				asset.escrow(amount);
			});
	}

	//! Attempt to transfer amount units of asset to the account.
	//! Returns true on success.
	//! Can only fail if amount is negative (i.e. a withdrawal).
	bool conditional_transfer_available(unsigned int asset, amount_t amount) {
		return operate_on_asset<bool>(
			asset, 
			amount, 
			[] (RevertableAsset& asset, const amount_t& amount) {
				return asset.conditional_transfer_available(amount);
			});
	}


	//! Attempt to escrow amount units of asset to the account.
	//! Returns true on success.
	//! Can only fail if amount is positive (negative means release from
	//! escrow).
	bool conditional_escrow(unsigned int asset, amount_t amount) {
		return operate_on_asset<bool>(
			asset, 
			amount, 
			[] (RevertableAsset& asset, const amount_t& amount) {
				return asset.conditional_escrow(amount);
			});
	}

public:


	//! Returns an account's available balance of some asset.
	amount_t lookup_available_balance(unsigned int asset) const {

		if (asset >= owned_assets.size())
		{
			return 0;
		}
		return owned_assets[asset].lookup_available_balance();
		/*[[maybe_unused]]
		amount_t unused = 0;
		return operate_on_asset<amount_t>(
			asset, 
			unused, 
			[] (RevertableAsset& asset, [[maybe_unused]] const amount_t& _) {
				return asset.lookup_available_balance();
			});
			*/
		
	}

	//! Reserves a sequence number on this account
	TransactionProcessingStatus reserve_sequence_number(
		uint64_t sequence_number);

	//! Releases a sequence number reservation
	void release_sequence_number(
		uint64_t sequence_number);

	//! Commits a sequence number reservation
	void commit_sequence_number(
		uint64_t sequence_number);

	//! Commit the current round's modifications to user's account.
	void commit();
	//! Rollback current round's modifications to user's account
	void rollback();

	//! Check that this account is in a valid state (i.e. all asset
	//! balances are nonnegative).
	bool in_valid_state();
	
	//! Generate an account commitment (for hashing)
	//! based on committed account balances
	AccountCommitment produce_commitment() const;
	//! Generate an account commitment (for hashing)
	//! based on uncommitted account balances.
	AccountCommitment tentative_commitment() const;

	//! Convert an lmdb key (byte string) into an account id.
	static AccountID read_lmdb_key(const lmdb::dbval& key);
	
	void log() const;
};

}
