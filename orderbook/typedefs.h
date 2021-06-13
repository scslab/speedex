#pragma once

#include <cstdint>

#include "orderbook/metadata.h"

#include "trie/merkle_trie.h"
#include "trie/metadata.h"
#include "trie/prefix.h"
#include "trie/utils.h"

#include "utils/big_endian.h"
#include "utils/price.h"

#include "xdr/types.h"

namespace speedex {

typedef XdrTypeWrapper<Offer> OfferWrapper;

constexpr static size_t ORDERBOOK_KEY_LEN 
	= price::PRICE_BYTES + sizeof(AccountID) + sizeof(uint64_t);

static_assert(OFFER_KEY_LEN_BYTES == ORDERBOOK_KEY_LEN,
	"Accounting mismatch in offer key len!");

typedef CombinedMetadata<
			DeletableMixin, 
			SizeMixin, 
			RollbackMixin, 
			OrderbookMetadata>
	OrderbookTrieMetadata;

typedef  
	ByteArrayPrefix<ORDERBOOK_KEY_LEN>
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
	write_unsigned_big_endian(buf, owner, offset);
	offset += sizeof(owner);
	write_unsigned_big_endian(buf, offer_id, offset);
}

[[maybe_unused]]
static void 
generate_orderbook_trie_key(const Offer& offer, OrderbookTriePrefix& buf) {
	generate_orderbook_trie_key(
		offer . minPrice, offer . owner, offer . offerId, buf);
}

typedef  
	MerkleTrie<OrderbookTriePrefix, OfferWrapper, OrderbookTrieMetadata, false>
	OrderbookTrie;


} /* speedex */