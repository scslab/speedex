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



#if defined(XDRC_PXDI) || defined(XDRC_PXD)
%from types_includes cimport *
#endif

#if defined(XDRC_PYX)
%from types_xdr cimport *
#endif


namespace speedex
{

typedef unsigned int uint32;
typedef int int32;

typedef unsigned hyper uint64;
typedef hyper int64;

// Todo add signatures to accountIDS
typedef uint64 AccountID;

typedef uint32 AssetID;

typedef opaque uint128[16];

typedef opaque uint256[32];

typedef opaque Signature[64]; //ed25519 sig len is 512 bits
typedef opaque PublicKey[32]; //ed25519 key len is 256 bits
typedef opaque Hash[32]; // 256 bit hash, i.e. output of sha256
typedef opaque SecretKey[64]; //ed25519 secret key len is 64 bytes, at least on libsodium

typedef uint32 ReplicaID;
const MAX_REPLICAS = 32; // max length of bitmap.  More replicas => longer bitmap

//Valid replicas have IDs 0-31, inclusive
const UNKNOWN_REPLICA = 32;

enum OfferType
{
	SELL = 0
};
const NUM_OFFER_TYPES = 1;

struct OfferCategory
{
	AssetID sellAsset;
	AssetID buyAsset;
	OfferType type;
};

// sell order executes if current price >= given price;
// Interpreted as price/2^radix
typedef uint64 Price;

//44 bytes
struct Offer
{
	OfferCategory category;
	uint64 offerId; // the operation number (sequence number + lowbits idx) that created this offer
	AccountID owner;
	uint64 amount;
	Price minPrice;
};

const OFFER_KEY_LEN_BYTES = 22; // Number of bytes in offer key

typedef opaque OfferKeyType[OFFER_KEY_LEN_BYTES];

typedef uint128 FractionalSupply;

} /* speedex */
