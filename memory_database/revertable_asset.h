#pragma once

/*! \file revertable_asset.h

Threadsafe (mostly) record of an amount of an asset.  Can be reverted to
a previously committed state.

*/

#include <cstdint>
#include <atomic>

#include <mutex>

#include <memory>

#include "xdr/types.h"
#include "xdr/database_commitments.h"

namespace speedex {

/*!
A threadsafe, revertable asset.

Cannot be moved while concurrently modified (i.e. be careful when 
resizing vectors of assets) but otherwise is fully threadsafe.

Use commit and rollback to finalize/revert any pending changes.

Stores amount of asset that is freely available at the moment.
Escrowed money (i.e. money locked up to back sell offers) is
not counted (no need to duplicate data).
*/
class RevertableAsset {
	
public:
	using amount_t = int64_t;

private:

	using atomic_amount_t = std::atomic<amount_t>;

	using pointer_t = std::unique_ptr<std::atomic<amount_t>>;

	atomic_amount_t available;
	
	amount_t committed_available;

	constexpr static auto read_order = std::memory_order_relaxed; 
	constexpr static auto write_order = std::memory_order_relaxed;

public:

	// Initialize asset with 0 balance.
	RevertableAsset() 
		: available(0),
		committed_available(0) {}

	RevertableAsset(amount_t amount)
		: available(amount),
		committed_available(amount) {}

	//! Cannot be called concurrently with anything else.  Be careful
	//! therefore when resizing vectors of these objects.
	RevertableAsset(RevertableAsset&& other)
		: available(other.available.load(read_order)),
		committed_available(other.committed_available) {}

	//! Converts some amount of available money into escrowed money.
	//! (decreases amount of available money).
	void escrow(const amount_t& amount) {
		available.fetch_add(-amount, write_order);
	}

	//! Adjust the amount of available money by amount (which can be positive
	//! or negative).
	void transfer_available(const amount_t& amount) {
		available.fetch_add(amount, write_order);
	}

	//! Attempt to escrow amount units of money.
	//! Fails if amount of available money is too small
	//! Negative inputs cannot fail (i.e. negative inputs mean releasing money
	//! from escrow).
	bool conditional_escrow(const amount_t& amount) {
		if (amount > 0) {
			auto result = conditional_transfer_available(-amount);
			return result;
		} else {
			transfer_available(-amount);
			return true;
		}
	}


	//! Attempt to change the amount of available money.
	//! Reductions of the amount below 0 will fail.
	//! Positive inputs represent increasing the amount of money in the account,
	//! which can never fail.
	//! Another approach would be to subtract, see if the original value you subtracted
	//! from is actually high enough, and apologize if not (undo).
	//! This creates the option to make txs that shouldn't fail fail, though.
	//! Unclear which causes less contention.

	bool conditional_transfer_available(const amount_t& amount) {

		if (amount > 0) {
			transfer_available(amount);
			return true;
		}
		while (true) {
			amount_t current_available
				= available.load(std::memory_order_relaxed);
			amount_t tentative_available = current_available + amount;
			if (tentative_available < 0) {
				return false;
			}
			bool result = available.compare_exchange_weak(
				current_available, 
				tentative_available, 
				write_order);
			if (result) {
				return true;
			}
			__builtin_ia32_pause();
		}	
	}

	//! Return the available balance (with current round's modifications
	//! applied).
	int64_t lookup_available_balance() {
		return available.load(read_order);
	}

	//! Produce a state commitment based on committed asset value (no
	//! modifications from current round).
	AssetCommitment produce_commitment(AssetID asset) const {
		return AssetCommitment(asset, committed_available);
	}

	//! Produce a state commitment based on asset value, including current
	//! round's modifications.
	AssetCommitment tentative_commitment(AssetID asset) const {
		return AssetCommitment{asset, available.load(read_order)};
	}

	//! Commit any in-flight modifications to the asset.
	//! Not threadsafe, so don't call commit and rollback and in_valid_state at
	//! the same time as each other or as escrow/transfer
	amount_t commit() {
		amount_t new_avail = available.load(read_order);
		committed_available = new_avail;
		return committed_available;
	}

	//! Rollback to previously committed value.
	void rollback() {
		available.store(committed_available, write_order);
	}

	//! Check that the amount of available money is nonnegative.
	bool in_valid_state() {
		int64_t available_load = available.load(read_order);
		return (available_load >= 0);
	}
};

}