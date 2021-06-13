#pragma once

/*! \file commitment_checker.h

When validating a block of transactions, we optimistically clear offers
with minimum prices below the cutoff.

As we clear, we need to check to make sure the offers we cleared 
reflect the equilibrium stats stated in the block header.

That means, as we clear offers, add up the total supply of each asset
that we sold to get some other asset (tracked per orderbook).
*/
#include <vector>
#include <memory>
#include <mutex>

#include "utils/fixed_point_value.h"
#include "utils/big_endian.h"

#include "xdr/block.h"

namespace speedex {

/*! Clearing stats for one orderbook */
struct SingleValidationStatistics {
	FractionalAsset activated_supply;

	SingleValidationStatistics& operator+=(
		const SingleValidationStatistics& other);
};


/*! Overall clearing stats for all orderbooks. */
struct ValidationStatistics {
	std::vector<SingleValidationStatistics> stats;

	SingleValidationStatistics& operator[](size_t idx);
	SingleValidationStatistics& at(size_t idx);
	ValidationStatistics& operator+=(const ValidationStatistics& other);
	void make_minimum_size(size_t sz);

	void log() const;

	size_t size() const;
};

/*! Wrapper around validation statistics that adds threadsafety. */
struct ThreadsafeValidationStatistics : public ValidationStatistics {
	std::unique_ptr<std::mutex> mtx;
	ThreadsafeValidationStatistics(size_t minimum_size) 
		: ValidationStatistics(), mtx(std::make_unique<std::mutex>())
	{
		make_minimum_size(minimum_size);
	}

	ThreadsafeValidationStatistics& operator+=(
		const ValidationStatistics& other);
	void log();
};

/*! Convenience methods wrapping the equilibrium commitment object
read in from a block header, for one orderbook.
*/
struct SingleOrderbookStateCommitmentChecker
	: public SingleOrderbookStateCommitment {

	SingleOrderbookStateCommitmentChecker(
		const SingleOrderbookStateCommitment& internal) 
		: SingleOrderbookStateCommitment(internal) {}

	FractionalAsset fractionalSupplyActivated() const;

	FractionalAsset partialExecOfferActivationAmount() const;

	//! Commitment object has for each orderbook a minimum key
	//! and a flag saying whether that key is null.
	//! If that key is all 0s, then the threshold key is declared to be
	//! empty, which means that the whole orderbook has executed.
	//! Otherwise, only part of the orderbook has executed,
	//! and the key contains useful data.
	//! This checks the flag to make sure that the flag matches
	//! whether or not the key is empty. 
	//! The flag is easier to check when actually validating transactions
	//! than doing a whole memcmp.
	bool check_threshold_key() const;
};

/*! Wraps the equilibrium commitment object read from a block header.

Main method of import is check().  Compares equilibrium commitment against
observed validation statistics.
*/
struct OrderbookStateCommitmentChecker {

	const std::vector<SingleOrderbookStateCommitmentChecker> commitments;
	const std::vector<Price> prices;
	const uint8_t tax_rate;

	OrderbookStateCommitmentChecker(
		const OrderbookStateCommitment& internal, 
		const std::vector<Price> prices,
		const uint8_t tax_rate) 
	: commitments(internal.begin(), internal.end())
	, prices(prices)
	, tax_rate(tax_rate) {}

	const SingleOrderbookStateCommitmentChecker& operator[](size_t idx) const {
		return commitments[idx];
	}

	const SingleOrderbookStateCommitmentChecker& at(size_t idx) const {
		return commitments.at(idx);
	}

	void log() const;
	bool check(ThreadsafeValidationStatistics& fully_cleared_stats);

	size_t size() const {
		return commitments.size();
	}
};

} /* speedex */
