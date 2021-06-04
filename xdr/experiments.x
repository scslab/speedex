%#include "xdr/transaction.h"
%#include "xdr/block.h"

#if defined(XDRC_PXDI) || defined(XDRC_PXD)
%from experiments_includes cimport *
%from block_includes cimport *
%from transaction_includes cimport *
%from types_includes cimport *

%from types_xdr cimport *
%from transaction_xdr cimport *
%from block_xdr cimport *
#endif

#if defined(XDRC_PYX)
%from types_xdr cimport *
%from transaction_xdr cimport *
%from block_xdr cimport *

%from experiments_includes cimport *
#endif


namespace edce {

struct ExperimentParameters {
	int tax_rate;
	int smooth_mult;
	int num_threads;
	int num_assets;
	int num_accounts;
	int persistence_frequency;
	int num_blocks;
};

typedef AccountID AccountIDList<>;


typedef SignedTransaction ExperimentBlock<>;


//struct ExperimentBlock {
//	Transaction txs<>;
//};

/*struct Experiment {
	int tax_rate;
	int smooth_mult;
	int num_assets;
	int num_accounts;
	int persistence_frequency;
	ExperimentBlock blocks<>;
};*/

struct TxProcessingMeasurements {
	float process_time;
	float finish_time;
};

struct ExperimentBlockResults {
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
};

struct ExperimentResults {
	ExperimentBlockResults block_results<>;
	ExperimentParameters params;
};

struct ExperimentValidationBlockResults {
	BlockValidationMeasurements block_validation_measurements;
	BlockDataPersistenceMeasurements data_persistence_measurements;
	float total_time;
	float total_persistence_time;
	float block_load_time;
	float validation_logic_time;
	BlockStateUpdateStats state_update_stats;
};

struct ExperimentValidationResults {
	ExperimentParameters params;
	ExperimentValidationBlockResults block_results<>;
};

enum NodeType {
	BLOCK_PRODUCER = 0,
	BLOCK_VALIDATOR = 1
};

union SingleBlockResultsUnion switch(NodeType type) {
	case BLOCK_PRODUCER:
		ExperimentBlockResults productionResults;
	case BLOCK_VALIDATOR:
		ExperimentValidationBlockResults validationResults;
};

struct ExperimentResultsUnion {
	ExperimentParameters params;
	SingleBlockResultsUnion block_results<>;
};

//locked
struct PriceComputationSingleExperiment {
	uint32 num_assets;
	uint32 tax_rate;
	uint32 smooth_mult;
	uint32 num_txs;
	uint32 num_trials;
	TatonnementMeasurements results<>;
};

//locked
struct PriceComputationExperiment {
	
	PriceComputationSingleExperiment experiments<>;
	
};

typedef string ExperimentName<200>;

struct ExperimentConfig {
	ExperimentName name;
	ExperimentName out_name;
	Price starting_prices<>;
};

typedef ExperimentConfig ExperimentConfigList<>;

}
