
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

namespace speedex {

const MAX_TRANSACTIONS_PER_BLOCK = 10000000;
const MAX_NUM_ASSETS = 1000;

const MAX_NUM_WORK_UNITS = 1000000; //NUM_OFFER_TYPES * (MAX_NUM_ASSETS * MAX_NUM_ASSETS);

typedef SignedTransaction SignedTransactionList<MAX_TRANSACTIONS_PER_BLOCK>;

// payload is SignedTransactionList
typedef opaque SerializedBlock<>;

struct SingleOrderbookStateCommitment {
	Hash rootHash;
	FractionalSupply fractionalSupplyActivated;
	OfferKeyType partialExecThresholdKey;
	int32 thresholdKeyIsNull;
	FractionalSupply partialExecOfferActivationAmount;
};

typedef SingleOrderbookStateCommitment OrderbookStateCommitment<MAX_NUM_WORK_UNITS>;

struct InternalHashes {
	Hash dbHash;
	OrderbookStateCommitment clearingDetails;
	Hash modificationLogHash;
	Hash blockMapHash;
};

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

struct HashedBlockTransactionListPair {
	HashedBlock hashedBlock;
	SignedTransactionList txList;
};

struct BlockHeaderHashValue {
	Hash hash;
	uint32 validation_success;
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
	float header_write_time; //
	float account_db_checkpoint_time; //
	float account_db_checkpoint_finish_time; //	
	float offer_checkpoint_time; //
	float account_log_write_time; //
	float block_hash_map_checkpoint_time; //
	float wait_for_persist_time; //
	float account_db_checkpoint_sync_time; //
	float total_critical_persist_time; //
	float async_persist_wait_time; //
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

struct TxProcessingMeasurements {
	float process_time;
	float finish_time;
};

struct OverallBlockProductionMeasurements {
	TxProcessingMeasurements processing_measurements<>;
	BlockCreationMeasurements block_creation_measurements;
	BlockDataPersistenceMeasurements data_persistence_measurements;
	float state_commitment_time;
	BlockProductionHashingMeasurements production_hashing_measurements;
	float format_time;
	int num_txs;
	float total_time;
	BlockStateUpdateStats state_update_stats;
	uint64 last_block_added_to_mempool;
	float mempool_push_time;
	float mempool_wait_time;
	float total_init_time;
	float total_block_build_time;
	float total_block_creation_time;
	float total_block_commitment_time;
	float total_block_persist_time;
	float total_time_from_basept;
	float total_block_send_time;
	float total_self_confirm_time;
	float total_critical_persist_time;
	float serialize_time;
};

struct OverallBlockValidationMeasurements {
	BlockValidationMeasurements block_validation_measurements;
	BlockDataPersistenceMeasurements data_persistence_measurements;
	float total_time;
	float total_persistence_time;
	float block_load_time;
	float validation_logic_time;
	BlockStateUpdateStats state_update_stats;
};


} /* speedex */
