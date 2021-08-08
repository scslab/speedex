#pragma once

//! \file speedex_node.h Operates a speedex node

#include "block_processing/block_producer.h"
#include "block_processing/block_validator.h"

#include "mempool/mempool.h"
#include "mempool/mempool_cleaner.h"

#include "rpc/block_forwarder.h"

#include "speedex/speedex_management_structures.h"
#include "speedex/speedex_operation.h"
#include "speedex/speedex_options.h"
#include "speedex/speedex_persistence.h"

#include "utils/price.h"

#include "xdr/experiments.h"
#include "xdr/block.h"

#include <cstdint>
#include <mutex>

namespace speedex {

/*!
Operates a node within SPEEDEX.

Can be either in block production or block validation mode.

When in block production mode, an external system periodically adds
transactions to a pending mempool.  produce_block() assembles these transactions
into a new block.  Transactions identified as invalid or included in a block
are removed from the mempool in the background.

When in block validation mode, an external system inputs blocks of transactions,
which the node validates.  If successful, then the node applies state updates
to the database.

*/ 
class SpeedexNode {
	constexpr static size_t PERSIST_BATCH = 5;
	constexpr static size_t MEASUREMENT_PERSIST_FREQUENCY = 10000;
	
	SpeedexManagementStructures& management_structures;

	NodeType state;

	std::mutex confirmation_mtx;
	std::mutex operation_mtx;
	std::mutex measurement_mtx;

	HashedBlock prev_block;

	std::atomic<uint64_t> highest_block;

	AsyncPersister async_persister;
	
	ExperimentResultsUnion measurement_results;
	std::string measurement_output_prefix;

	const SpeedexOptions& options;

	BlockForwarder block_forwarder;

	constexpr static bool small = false;

	LogMergeWorker log_merge_worker;

	//block production related objects
	constexpr static size_t TARGET_BLOCK_SIZE = small ? 60'000 : 600'000;

	constexpr static size_t MEMPOOL_CHUNK_SIZE = small ? 1'000: 10'000;

	TatonnementManagementStructures tatonnement_structs;
	std::vector<Price> prices;
	Mempool mempool;
	MempoolCleaner mempool_worker;
	BlockProducer block_producer;

	//block validation related objects
	BlockValidator block_validator;

	//utility methods
	void set_current_measurements_type();

	BlockDataPersistenceMeasurements& 
	get_persistence_measurements(uint64_t block_number);
	BlockStateUpdateStats& get_state_update_stats(uint64_t block_number);

	void assert_state(NodeType required_state);
	std::string state_to_string(NodeType query_state);

	ExperimentResultsUnion get_measurements_nolock();


public:

	SpeedexNode(
		SpeedexManagementStructures& management_structures,
		const ExperimentParameters params,
		const SpeedexOptions& options,
		std::string measurement_output_prefix,
		NodeType state)
	: management_structures(management_structures)
	, state(state)
	, confirmation_mtx()
	, operation_mtx()
	, measurement_mtx()
	, prev_block()
	, highest_block(0)
	, async_persister(management_structures)
	, measurement_results()
	, measurement_output_prefix(measurement_output_prefix)
	, options(options) 
	, block_forwarder()
	, log_merge_worker(management_structures.account_modification_log)
	, tatonnement_structs(management_structures)
	, mempool(MEMPOOL_CHUNK_SIZE)
	, mempool_worker(mempool)
	, block_producer(management_structures, log_merge_worker)
	, block_validator(management_structures, log_merge_worker)
	{
		measurement_results.block_results.resize(MEASUREMENT_PERSIST_FREQUENCY);
		measurement_results.params = params;
		auto num_assets = management_structures.orderbook_manager.get_num_assets();
		//prices = new Price[num_assets];
		prices.resize(num_assets);
		for (size_t i = 0; i < num_assets; i++) {
			prices[i] = price::from_double(1.0);
		}
	}

	~SpeedexNode() {
		write_measurements();
	}

	ExperimentResultsUnion get_measurements();

	BlockForwarder& get_block_forwarder() {
		return block_forwarder;
	}

	std::string overall_measurement_filename() const {
		return measurement_output_prefix + "results";
	}

	bool produce_block();

	template<typename TxListType>
	bool validate_block(const HashedBlock& header, const std::unique_ptr<TxListType> block);

	void add_txs_to_mempool(std::vector<SignedTransaction>&& txs, uint64_t latest_block_number);
	size_t mempool_size() {
		assert_state(BLOCK_PRODUCER);
		return mempool.size();
	}

	void push_mempool_buffer_to_mempool() {
		assert_state(BLOCK_PRODUCER);
		mempool.push_mempool_buffer_to_mempool();
	}

	void write_measurements();
};


} /* speedex */
