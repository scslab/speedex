
#if defined(XDRC_HH) || defined(XDRC_SERVER)
%#include "xdr/types.h"
%#include "xdr/transaction.h"
#endif

#if defined(XDRC_PXDI) || defined(XDRC_PXD)
%from types_xdr cimport *
%from transaction_xdr cimport *
%from block_includes cimport *
#endif

#if defined(XDRC_PYX)
%from types_xdr cimport *
%from transaction_xdr cimport *
%from block_includes cimport *
#endif

namespace edce {

const MAX_TRANSACTIONS_PER_BLOCK = 1000000;
const MAX_NUM_ASSETS = 1000;

const TRANSACTIONS_PER_MINIBLOCK = 10000;
const MINIBLOCK_SELECTOR_BYTES = 1250; //tx/miniblock over 8

const MAX_NUM_WORK_UNITS = 1000000; //NUM_OFFER_TYPES * (MAX_NUM_ASSETS * MAX_NUM_ASSETS);
	
struct TransactionData {
	SignedTransaction transactions<MAX_TRANSACTIONS_PER_BLOCK>;
};

typedef SignedTransaction SignedTransactionList<MAX_TRANSACTIONS_PER_BLOCK>;

// payload is SignedTransactionList
typedef opaque SerializedBlock<>;

//struct PartialMiniblock {
//	union switch(bool exists) {
//		case 0:
//			SignedTransaction transaction;
//		default:
//			void;
//	} transactions<TRANSACTIONS_PER_MINIBLOCK>;
//	SignedTransaction transactions<TRANSACTIONS_PER_MINIBLOCK>;
//	uint32 missingSlots<TRANSACTIONS_PER_MINIBLOCK>;
//	Hash transactionHash;
//	uint64 creator;
//	Signature sig;
//};

//struct TransactionMiniblock {
//	union switch(bool parsed) {
//		case TRUE:
//			SignedTransaction transactions<TRANSACTIONS_PER_MINIBLOCK>;
//		case FALSE:
//			opaque data<>;
//	} body;
//	Hash transactionHash;
//	uint64 creator;
//	Signature sig;
//};

//struct MiniblockSelector {
//	uint64 creator;
//	Hash transactionHash;
//	opaque inclusionBits<MINIBLOCK_SELECTOR_BYTES>;
//};
//
//struct BlockSpecification {
//	MiniblockSelector selections<>;
//};


const OFFER_KEY_LEN_BYTES = 22; // MerkleWorkUnit::WORKUNIT_KEY_LEN; 

typedef uint128 FractionalSupply;

typedef opaque OfferKeyType[OFFER_KEY_LEN_BYTES];

struct SingleWorkUnitStateCommitment {
	Hash rootHash;
	FractionalSupply fractionalSupplyActivated;
	OfferKeyType partialExecThresholdKey;
	int32 thresholdKeyIsNull;
	FractionalSupply partialExecOfferActivationAmount;
};

typedef SingleWorkUnitStateCommitment WorkUnitStateCommitment<MAX_NUM_WORK_UNITS>;

struct InternalHashes {
	Hash dbHash;
	WorkUnitStateCommitment clearingDetails;
	Hash modificationLogHash;
	Hash blockMapHash;
};

//typedef opaque PriceBuffer[6];

struct Block {
	Hash prevBlockHash;
	uint64 blockNumber;
	Price prices<MAX_NUM_ASSETS>;
	uint32 feeRate;
	InternalHashes internalHashes;
};

struct HashedBlock {
	Block block;
	Hash hash;
};


struct TatonnementMeasurements {
	float runtime;
	uint32 step_radix;
	uint32 num_rounds;
	uint32 achieved_fee_rate;
	uint32 achieved_smooth_mult;
};

struct BlockStateUpdateStats {
	uint32 new_offer_count;
	uint32 cancel_offer_count;
	uint32 fully_clear_offer_count;
	uint32 partial_clear_offer_count;
	uint32 payment_count;
	uint32 new_account_count;
};

struct BlockCreationMeasurements {
	float block_building_time;
	float initial_account_db_commit_time;
	float initial_offer_db_commit_time;
	float tatonnement_time;
	float lp_time;
	float clearing_check_time;
	float offer_clearing_time;
	float db_validity_check_time;
	float final_commit_time;
	float mempool_clearing_time;

	uint32 number_of_transactions;
	uint32 tatonnement_rounds;
	uint32 achieved_feerate;
	uint32 achieved_smooth_mult;
	uint32 tat_timeout_happened; // 1 if yes, 0 if no
	uint32 num_open_offers;
	float offer_merge_time;
	float reserved_space8;
	float reserved_space9;
	float reserved_space0;
};

struct BlockDataPersistenceMeasurements {
	float header_write_time;
	float account_db_checkpoint_time;
	float account_db_checkpoint_finish_time;	
	float offer_checkpoint_time;
	float account_log_write_time;
	float block_hash_map_checkpoint_time;
	float wait_for_persist_time;
	float account_db_checkpoint_sync_time;
	float total_critical_persist_time;
	float async_persist_wait_time;
	float reserved_space3;
	float reserved_space4;
	float reserved_space5;
	float reserved_space6;
	float reserved_space7;
	float reserved_space8;
	float reserved_space9;
	float reserved_space0;
};

struct BlockProductionHashingMeasurements {
	float db_state_commitment_time;
	float work_unit_commitment_time;
	float account_log_hash_time;

	float reserved_space1;
	float reserved_space2;
	float reserved_space3;
	float reserved_space4;
	float reserved_space5;
	float reserved_space6;
	float reserved_space7;
	float reserved_space8;
	float reserved_space9;
	float reserved_space0;
};

struct BlockValidationMeasurements {
	float clearing_param_check;
	float tx_validation_time;
	float tx_validation_trie_merge_time;
	float tx_validation_processing_time;
	float tx_validation_offer_merge_time;
	float tentative_commit_time;
	float check_workunit_validation_time;
	float get_dirty_account_time;
	float db_tentative_commit_time;
	float workunit_hash_time;
	float hash_time;
	float db_finalization_time;
	float workunit_finalization_time;
	float account_log_finalization_time;
	float header_map_finalization_time;

	float reserved_space1;
	float reserved_space2;
	float reserved_space3;
	float reserved_space4;
	float reserved_space5;
	float reserved_space6;
	float reserved_space7;
	float reserved_space8;
	float reserved_space9;
	float reserved_space0;
};


}
