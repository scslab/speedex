#include "modlog/account_modification_entry.h"

#include <xdrpp/marshal.h>

namespace speedex
{


void 
AccountModificationEntry::add_identifier_self(uint64_t id)
{
	identifiers_self.insert(id);
}


void
AccountModificationEntry::add_identifier_other(TxIdentifier const& id)
{
	identifiers_other.insert(id);
}


void
AccountModificationEntry::add_tx_self(const SignedTransaction& tx)
{
	new_transactions_self.insert(tx);
}

void
AccountModificationEntry::merge_value(AccountModificationEntry& other_value)
{
	identifiers_self.merge(other_value.identifiers_self);
	identifiers_other.merge(other_value.identifiers_other);
	new_transactions_self.merge(other_value.new_transactions_self);

	if (owner != other_value.owner)
	{
		throw std::runtime_error("log entry merge_value owner mismatch");
	}

	if (other_value.new_transactions_self.size() != 0)
	{
		throw std::runtime_error("seqno error: tx showed up in multiple values");
	}
}

//caller owns mem
std::pair<uint8_t*, size_t>
AccountModificationEntry::serialize_xdr() const
{
	auto ntx_sz = [this]
	{
		size_t out = 0;
		for (auto const& tx : new_transactions_self)
		{
			out += xdr::xdr_argpack_size(tx);
		}
		return out;
	};

	size_t total_size_bytes = 
		8 //owner
		+ 4 + ntx_sz()
		+ 4 + (8 * identifiers_self.size())
		+ 4 + (16 * identifiers_other.size());

	uint8_t* out = new uint8_t[total_size_bytes];

	xdr::xdr_put p(out, out + total_size_bytes);

	if (!owner)
	{
		throw std::runtime_error("owner not set prior to serialization");
	}

	p(static_cast<uint64_t>(*owner));

	p(static_cast<uint32_t>(new_transactions_self.size()));
	for (auto const& tx : new_transactions_self)
	{
		p(tx);
	}

	p(static_cast<uint32_t>(identifiers_self.size()));
	for (auto const& id : identifiers_self)
	{
		p(id);
	}

	p(static_cast<uint32_t>(identifiers_other.size()));
	for (auto const& id : identifiers_other)
	{
		p(id);
	}

	return {out, total_size_bytes};
}

} /* speedex */
