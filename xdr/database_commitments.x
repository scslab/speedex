
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




namespace edce {
	
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


}