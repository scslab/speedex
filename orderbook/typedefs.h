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

#pragma once

#include <cstdint>

#include "orderbook/metadata.h"

#include <mtt/trie/merkle_trie.h>
#include <mtt/trie/metadata.h>
#include <mtt/common/prefix.h>
#include <mtt/trie/utils.h>

#include <utils/serialize_endian.h>

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
