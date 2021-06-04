
#if defined(XDRC_HH) || defined(XDRC_SERVER)
%#include "xdr/types.h"
#endif

#if defined(XDRC_PXDI) || defined(XDRC_PXD)
%from types_includes cimport *
%from types_xdr cimport *
%from transaction_includes cimport *
#endif

#if defined(XDRC_PYX)
%from types_xdr cimport *
%from transaction_includes cimport *
#endif

namespace edce
{

enum TransactionProcessingStatus {
	SUCCESS = 0,
	SOURCE_ACCOUNT_NEXIST = 1,
	INVALID_OPERATION_TYPE = 2,
	SEQ_NUM_TOO_LOW = 3,
	SEQ_NUM_TOO_HIGH = 4,
	SEQ_NUM_TEMP_IN_USE = 5,
	STARTING_BALANCE_TOO_LOW = 6,
	NEW_ACCOUNT_ALREADY_EXISTS = 7,
	NEW_ACCOUNT_TEMP_RESERVED = 8,
	INSUFFICIENT_BALANCE = 9,
	INVALID_TX_FORMAT = 10,
	INVALID_OFFER_CATEGORY = 11,
	INVALID_PRICE = 12,
	CANCEL_OFFER_TARGET_NEXIST = 13,
	RECIPIENT_ACCOUNT_NEXIST = 14,
	INVALID_PRINT_MONEY_AMOUNT = 15,
	INVALID_AMOUNT = 16
};

enum OperationType
{
	CREATE_ACCOUNT = 0,
	CREATE_SELL_OFFER = 1,
	CANCEL_SELL_OFFER = 2,
	PAYMENT = 3,
	MONEY_PRINTER = 4
};

//Payment amounts are int64, not uint64, so we don't have to worry
//about overflows when negating values.

const CREATE_ACCOUNT_MIN_STARTING_BALANCE = 100;

struct CreateAccountOp
{
	// balance is in the native base currency, supplied by the account creator
	int64 startingBalance;
	AccountID newAccountId;
	PublicKey newAccountPublicKey;
};

struct CreateSellOfferOp
{
	// everything in an Offer type except the offerId, which has not yet been assigned.
	OfferCategory category;
	int64 amount;
	Price minPrice;
};

struct CancelSellOfferOp
{
	OfferCategory category;
	uint64 offerId;
	Price minPrice;
};

struct PaymentOp
{
	AccountID receiver;
	AssetID asset;
	int64 amount;
};

struct MoneyPrinterOp
{
	AssetID asset;
	int64 amount;
};

struct Operation {
	union switch (OperationType type) {
	case CREATE_ACCOUNT:
		CreateAccountOp createAccountOp;
	case CREATE_SELL_OFFER:
		CreateSellOfferOp createSellOfferOp;
	case CANCEL_SELL_OFFER:
		CancelSellOfferOp cancelSellOfferOp;
	case PAYMENT:
		PaymentOp paymentOp;
	case MONEY_PRINTER:
		MoneyPrinterOp moneyPrinterOp;
	} body;


};

const MAX_OPS_PER_TX = 256;
const RESERVED_SEQUENCE_NUM_LOWBITS = 255;

struct TransactionMetadata {
	AccountID sourceAccount;
	uint64 sequenceNumber;
};


struct Transaction {
	TransactionMetadata metadata;
	Operation operations<MAX_OPS_PER_TX>;
	uint32 fee;
};

struct SignedTransaction {
	Transaction transaction;
	Signature signature;
};


// Store any nondeterministic operation results
struct OperationResult {
	union switch (OperationType type) {
	default:
		void;
	} body;
};

struct TransactionResult {
	OperationResult results<MAX_OPS_PER_TX>;
};



}
