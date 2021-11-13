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

namespace speedex {

struct ExperimentParameters {
	unsigned int num_threads;
	unsigned int num_assets;
	string account_list_filename<200>;
	unsigned int persistence_frequency;
	unsigned int num_blocks;
};

typedef AccountID AccountIDList<>;

typedef SignedTransaction ExperimentBlock<>;
 
enum NodeType {
	BLOCK_PRODUCER = 0,
	BLOCK_VALIDATOR = 1
};

union SingleBlockResultsUnion switch(NodeType type) {
	case BLOCK_PRODUCER:
		OverallBlockProductionMeasurements productionResults;
	case BLOCK_VALIDATOR:
		OverallBlockValidationMeasurements validationResults;
};

struct TaggedSingleBlockResults {
	SingleBlockResultsUnion results;
	uint64 blockNumber;
};

struct ExperimentResultsUnion {
	ExperimentParameters params;
	TaggedSingleBlockResults block_results<>;
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

} /* speedex */
