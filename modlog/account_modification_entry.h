#pragma once

#include <compare>
#include <cstdint>
#include <set>

#include "xdr/database_commitments.h"
#include "xdr/transaction.h"

namespace speedex
{

[[maybe_unused]]
static std::strong_ordering
operator<=>(const TxIdentifier& a, const TxIdentifier& b)
{
	auto res = a.owner <=> b.owner;

	if (res != std::strong_ordering::equal)
	{
		return res;
	}
	return a.sequence_number <=> b.sequence_number;
}

namespace detail
{
struct TxComparator
{	
	// this gets around -Wsubobject-linkage, which
	// occurs if you do the recommended set<STx, decltype(cmp)>
	bool operator()(const SignedTransaction& a, const SignedTransaction& b) const
	{
		return a.transaction.metadata.sequenceNumber < b.transaction.metadata.sequenceNumber;
	}
};

} /* detail */


class AccountModificationEntry
{
	const AccountID owner;

	std::set<uint64_t> identifiers_self;
	std::set<TxIdentifier> identifiers_other;
	std::set<SignedTransaction, detail::TxComparator> new_transactions_self;

public:

	AccountModificationEntry(AccountID const& owner)
		: owner(owner)
		, identifiers_self()
		, identifiers_other()
		, new_transactions_self()
		{}

	void add_identifier_self(uint64_t id);
	void add_identifier_other(TxIdentifier const& id);
	void add_tx_self(const SignedTransaction& tx);

	void merge_value(AccountModificationEntry& other_value);

	std::pair<uint8_t*, size_t>
	serialize_xdr() const;

	void copy_data(std::vector<uint8_t>& buf) const
	{
		auto [ptr, sz] = serialize_xdr();
		buf.insert(buf.end(), ptr, ptr + sz);
	}
};


}