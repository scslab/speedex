#pragma once

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <set>
#include <unordered_map>
#include <atomic>

#include "memory_database/revertable_asset.h"

#include "xdr/types.h"
#include "xdr/transaction.h"
#include "xdr/database_commitments.h"

#include "lmdb/lmdb_types.h"

#include <sodium.h>

namespace speedex {


/*!
Stores a user's account information.

Currently, this consists of asset amounts and a public key.

Modifications to accounts are not threadsafe with commit/rollback.
*/
class UserAccount {

	static_assert(MAX_OPS_PER_TX == RESERVED_SEQUENCE_NUM_LOWBITS + 1, "ops mismatch");
	static_assert(__builtin_popcount(MAX_OPS_PER_TX) == 1, "should be power of two");

	using amount_t = typename RevertableAsset::amount_t;

	mutable std::mutex uncommitted_assets_mtx;

	// using a map here really slows things down.
	std::vector<RevertableAsset> owned_assets;
	std::vector<RevertableAsset> uncommitted_assets;

	std::atomic<uint64_t> sequence_number_vec;


	uint64_t last_committed_id;

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

	void operate_on_asset(unsigned int asset, amount_t amount, void (*func)(RevertableAsset&, const amount_t&)) {
		unsigned int owned_assets_size = owned_assets.size();
		if (asset >= owned_assets_size) {
			std::lock_guard lock(uncommitted_assets_mtx);
			while (asset >= owned_assets_size + uncommitted_assets.size()) {
				uncommitted_assets.emplace_back();
			}
			func(uncommitted_assets[asset - owned_assets_size], amount);
			return;
		}
		func(owned_assets[asset], amount);
	}

	AccountID owner;
	PublicKey pk;

	static_assert(sizeof(pk) == crypto_sign_PUBLICKEYBYTES, "pk size should be pkbytes");

public:

	UserAccount(AccountID owner, PublicKey public_key) :
		uncommitted_assets_mtx(),
		last_committed_id(0),
		owner(owner)
		, pk(public_key)
	{}

	//!We cannot move UserAccounts and concurrently modify them.
	UserAccount(UserAccount&& other)
		: uncommitted_assets_mtx(),
		owned_assets(std::move(other.owned_assets)),
		uncommitted_assets(std::move(other.uncommitted_assets)),

		sequence_number_vec(other.sequence_number_vec.load(std::memory_order_acquire)),

		last_committed_id(other.last_committed_id),

		owner(other.owner)
		, pk(other.pk) {
		}


	//!Needed only for vector.erase, for some dumb reason
	UserAccount& operator=(UserAccount&& other) {
		owned_assets = std::move(other.owned_assets);
		uncommitted_assets = std::move(other.uncommitted_assets);
		sequence_number_vec = other.sequence_number_vec.load(std::memory_order_acquire);
		last_committed_id = other.last_committed_id;
		owner = other.owner;
		pk = other.pk;
		return *this;
	}

	//! Initializes an account from an account database record.
	UserAccount(const AccountCommitment& commitment) 
		: owned_assets()
		, uncommitted_assets()
		, sequence_number_vec(0)
		, last_committed_id(commitment.last_committed_id)
		, owner(commitment.owner)
		, pk(commitment.pk) {

			for (unsigned int i = 0; i < commitment.assets.size(); i++) {
				if (commitment.assets[i].asset < owned_assets.size()) {
					throw std::runtime_error("assets in commitment should be sorted");
				}
				while (owned_assets.size() < commitment.assets[i].asset) {
					owned_assets.emplace_back(0);
				}
				owned_assets.emplace_back(commitment.assets[i].amount_available);
			}
		}


	PublicKey get_pk() const {
		return pk;
	}

	AccountID get_owner() const {
		return owner;
	}

	void transfer_available(unsigned int asset, amount_t amount) {
		operate_on_asset(asset, amount, [] (RevertableAsset& asset, const amount_t& amount) {asset.transfer_available(amount);});
	}

	void transfer_escrow(unsigned int asset, amount_t amount) {
		operate_on_asset(asset, amount, [] (RevertableAsset& asset, const amount_t& amount) {asset.transfer_escrow(amount);});
	}

	void escrow(unsigned int asset, amount_t amount) {
		operate_on_asset(asset, amount, [] (RevertableAsset& asset, const amount_t& amount) {asset.escrow(amount);});
	}

	bool conditional_transfer_available(unsigned int asset, amount_t amount) {
		return operate_on_asset<bool>(asset, amount, [] (RevertableAsset& asset, const amount_t& amount) {return asset.conditional_transfer_available(amount);});
	}

	bool conditional_escrow(unsigned int asset, amount_t amount) {
		return operate_on_asset<bool>(asset, amount, [] (RevertableAsset& asset, const amount_t& amount) {return asset.conditional_escrow(amount);});
	}

	amount_t lookup_available_balance(unsigned int asset) {
		[[maybe_unused]]
		amount_t unused = 0;
		return operate_on_asset<amount_t>(asset, unused, [] (RevertableAsset& asset, [[maybe_unused]] const amount_t& amount) {return asset.lookup_available_balance();});
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

	void commit();
	void rollback();
	bool in_valid_state();
	
	AccountCommitment produce_commitment() const;
	AccountCommitment tentative_commitment() const;

	static dbval produce_lmdb_key(const AccountID& owner);
	static AccountID read_lmdb_key(const dbval& key);
	
	void log() {
		for (unsigned int i = 0; i < owned_assets.size(); i++) {
			std::printf ("%u=%ld ", i, owned_assets[i].lookup_available_balance());
		}
		std::printf("\n");
	}
};

}
