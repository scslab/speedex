#pragma once

#include <cstdint>

#include "orderbook/metadata.h"

#include "mtt/trie/merkle_trie.h"
#include "mtt/trie/metadata.h"
#include "mtt/trie/prefix.h"
#include "mtt/trie/utils.h"

#include "mtt/utils/serialize_endian.h"

#include "utils/price.h"

#include "xdr/types.h"

#include <xdrpp/marshal.h>

namespace speedex {

struct OrderbookMethods
{
	// warning police: sidesteps anonymous namespace warnings -Wsubobject-linkage
	static std::vector<uint8_t> 
	serialize(const Offer& v)
	{
		return xdr::xdr_to_opaque(v);
	}
};

typedef trie::XdrTypeWrapper<Offer, &OrderbookMethods::serialize> OfferWrapper;

constexpr static size_t ORDERBOOK_KEY_LEN 
	= price::PRICE_BYTES + sizeof(AccountID) + sizeof(uint64_t);

static_assert(OFFER_KEY_LEN_BYTES == ORDERBOOK_KEY_LEN,
	"Accounting mismatch in offer key len!");

typedef trie::CombinedMetadata<
			trie::DeletableMixin, 
			trie::SizeMixin, 
			trie::RollbackMixin, 
			OrderbookMetadata>
	OrderbookTrieMetadata;

typedef  
	trie::ByteArrayPrefix<ORDERBOOK_KEY_LEN>
	OrderbookTriePrefix;

static void generate_orderbook_trie_key(
	const Price minPrice, 
	const AccountID owner, 
	const uint64_t offer_id,
	OrderbookTriePrefix& buf)
{
	size_t offset = 0;
	price::write_price_big_endian(buf, minPrice);
	offset += price::PRICE_BYTES;
	utils::write_unsigned_big_endian(buf, owner, offset);
	offset += sizeof(owner);
	utils::write_unsigned_big_endian(buf, offer_id, offset);
}

[[maybe_unused]]
static void 
generate_orderbook_trie_key(const Offer& offer, OrderbookTriePrefix& buf) {
	generate_orderbook_trie_key(
		offer . minPrice, offer . owner, offer . offerId, buf);
}

typedef  
	trie::MerkleTrie<OrderbookTriePrefix, OfferWrapper, OrderbookTrieMetadata, false>
	OrderbookTrie;


} /* speedex */