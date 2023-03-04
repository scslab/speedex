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


#if defined(XDRC_HH) || defined(XDRC_SERVER)
%#include "xdr/types.h"
%#include "xdr/transaction.h"
#endif

#if defined(XDRC_PXDI) || defined(XDRC_PXD)
%from types_xdr cimport *
%from transaction_xdr cimport *
%from database_commitments_includes cimport *
#endif

#if defined(XDRC_PYX)
%from types_xdr cimport *
%from transaction_xdr cimport *
%from database_commitments_includes cimport *
#endif

namespace speedex {
	
const MAX_NUMBER_DISTINCT_ASSETS = 256;

struct AssetCommitment {
	AssetID asset;
	uint64 amount_available;
};

struct AccountCommitment {
	AccountID owner;
	AssetCommitment assets<MAX_NUMBER_DISTINCT_ASSETS>;
	uint64 last_committed_id;
	PublicKey pk;
};

struct TxIdentifier {
	AccountID owner;
	uint64 sequence_number;
};

struct AccountModificationTxList {
	AccountID owner;
	SignedTransaction new_transactions_self<>; //transactions
	uint64 identifiers_self<>; // sequence numbers, in addition
	TxIdentifier identifiers_others<>;
};

typedef AccountModificationTxList AccountModificationBlock<>;


} /* speedex */