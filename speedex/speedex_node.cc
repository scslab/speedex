#include "speedex_node.h"

#include "utils/debug_macros.h"
#include "utils/save_load_xdr.h"

namespace speedex {

//returns true if successfully makes block from mempool.
bool
SpeedexNode::produce_block() {
	auto start_time = init_time_measurement();

	std::lock_guard lock(operation_mtx);

	assert_state(BLOCK_PRODUCER);

	uint64_t prev_block_number = prev_block.block.blockNumber;
	
	BLOCK_INFO("Starting production on block %lu", prev_block_number + 1);

	auto measurements_base = new_measurements();

	measurements_base.blockNumber = prev_block_number + 1;
	auto& current_measurements = measurements_base.results.productionResults();
	
	auto mempool_push_ts = init_time_measurement();
	mempool.push_mempool_buffer_to_mempool();
	current_measurements.mempool_push_time = measure_time(mempool_push_ts);

	BlockStateUpdateStatsWrapper state_update_stats;
	size_t block_size = 0;

	current_measurements.total_init_time = measure_time_from_basept(start_time);

	BLOCK_INFO("mempool size: %lu", mempool.size());
	{
		auto timestamp = init_time_measurement();
		current_measurements.last_block_added_to_mempool 
			= mempool.latest_block_added_to_mempool.load(std::memory_order_relaxed);

		block_size = block_producer.build_block(
			mempool, 
			TARGET_BLOCK_SIZE, 
			current_measurements.block_creation_measurements, 
			state_update_stats);

		current_measurements
			.block_creation_measurements
			.block_building_time = measure_time(timestamp);

		current_measurements
			.block_creation_measurements
			.number_of_transactions = block_size;

		BLOCK_INFO(
			"block build time: %lf", 
			current_measurements.block_creation_measurements.block_building_time);
	}

	mempool_worker.do_mempool_cleaning();

	current_measurements.total_block_build_time = measure_time_from_basept(start_time);

	HashedBlock new_block = speedex_block_creation_logic(
		prices,
		management_structures,
		tatonnement_structs,
		prev_block,
		current_measurements,
		state_update_stats);

	prev_block = new_block;

	highest_block = prev_block.block.blockNumber;
	
	current_measurements.total_block_commitment_time = measure_time_from_basept(start_time);

	auto timestamp = init_time_measurement();

	auto output_tx_block = persist_critical_round_data(
		management_structures, 
		prev_block, 
		current_measurements.data_persistence_measurements, 
		true,
		true);
	current_measurements.data_persistence_measurements.total_critical_persist_time = measure_time(timestamp);

	current_measurements.total_critical_persist_time = measure_time_from_basept(start_time);

	BLOCK_INFO("finished block production, starting to send to other nodes");
	block_forwarder.send_block(prev_block, std::move(output_tx_block));
	BLOCK_INFO("send time: %lf", measure_time(timestamp));

	current_measurements.total_block_send_time = measure_time_from_basept(start_time);

	BLOCK_INFO("done sending to other nodes");

	auto async_ts = init_time_measurement();
	if (prev_block.block.blockNumber % PERSIST_BATCH == 0) {
		async_persister.do_async_persist(
			std::make_unique<PersistenceMeasurementLogCallback>(measurements_log, prev_block.block.blockNumber));
	}
	current_measurements.data_persistence_measurements.async_persist_wait_time = measure_time(async_ts);

	current_measurements.total_block_persist_time = measure_time_from_basept(start_time);

	current_measurements.state_update_stats = state_update_stats.get_xdr();

	auto mempool_wait_ts = init_time_measurement();

	current_measurements.block_creation_measurements.mempool_clearing_time = mempool_worker.wait_for_mempool_cleaning_done();
	current_measurements.mempool_wait_time = measure_time(mempool_wait_ts);
	
	current_measurements.total_time_from_basept = measure_time_from_basept(start_time);

	current_measurements.total_time = measure_time(start_time);

	measurements_log.add_measurement(measurements_base);


	return block_size > 1000;
}

template bool SpeedexNode::validate_block(const HashedBlock& header, std::unique_ptr<SerializedBlock> block);
template bool SpeedexNode::validate_block(const HashedBlock& header, std::unique_ptr<AccountModificationBlock> block);

template<typename TxListType>
bool SpeedexNode::validate_block(const HashedBlock& header, std::unique_ptr<TxListType> block) {
	uint64_t prev_block_number = prev_block.block.blockNumber;

	std::lock_guard lock(operation_mtx);

	assert_state(BLOCK_VALIDATOR);

	auto measurements_base = new_measurements();
	measurements_base.blockNumber = prev_block_number + 1;

	auto& current_measurements = measurements_base.results.validationResults();

	auto timestamp = init_time_measurement();

	auto logic_timestamp = init_time_measurement();

	auto res =  speedex_block_validation_logic( 
		management_structures,
		block_validator,
		current_measurements,
		prev_block,
		header,
		*block);

	if (!res) {
		return false;
	}

	current_measurements.validation_logic_time = measure_time(logic_timestamp);
	
	auto persistence_start = init_time_measurement();
	
	persist_critical_round_data(
		management_structures, header, 
		current_measurements.data_persistence_measurements,
		false,
		true,
		1000000);

	current_measurements.total_persistence_time = measure_time(persistence_start);

	current_measurements.total_time = measure_time(timestamp);

	prev_block = header;

	block_forwarder.send_block(prev_block, std::move(block));
	highest_block = prev_block.block.blockNumber;

	if (prev_block.block.blockNumber % PERSIST_BATCH == 0) {
		async_persister.do_async_persist(
			std::make_unique<PersistenceMeasurementLogCallback>(measurements_log, prev_block.block.blockNumber));
	}

	measurements_log.add_measurement(measurements_base);

	return true;
}


TaggedSingleBlockResults
SpeedexNode::new_measurements() const
{
	TaggedSingleBlockResults res;
	res.results.type(state);
	return res;
}
/*
void SpeedexNode::set_current_measurements_type() {
	measurement_results
		.block_results
		.at(prev_block.block.blockNumber % MEASUREMENT_PERSIST_FREQUENCY)
		.results
		.type(state);
	measurement_results
		.block_results
		.at(prev_block.block.blockNumber % MEASUREMENT_PERSIST_FREQUENCY)
		.blockNumber = prev_block.block.blockNumber + 1;
}

BlockDataPersistenceMeasurements& 
SpeedexNode::get_persistence_measurements(uint64_t block_number) {
	switch(state) {
		case NodeType::BLOCK_PRODUCER:
			return measurement_results
				.block_results
				.at((block_number - 1) % MEASUREMENT_PERSIST_FREQUENCY)
				.results
				.productionResults()
				.data_persistence_measurements;
		case NodeType::BLOCK_VALIDATOR:
			return measurement_results
				.block_results
				.at((block_number - 1) % MEASUREMENT_PERSIST_FREQUENCY)
				.results
				.validationResults()
				.data_persistence_measurements;
	}
	throw std::runtime_error("invalid state");
}

BlockStateUpdateStats& 
SpeedexNode::get_state_update_stats(uint64_t block_number) {
	switch(state) {
		case NodeType::BLOCK_PRODUCER:
			return measurement_results
				.block_results
				.at((block_number - 1) % MEASUREMENT_PERSIST_FREQUENCY)
				.results
				.productionResults()
				.state_update_stats;
		case NodeType::BLOCK_VALIDATOR:
			return measurement_results
				.block_results
				.at((block_number - 1) % MEASUREMENT_PERSIST_FREQUENCY)
				.results
				.validationResults()
				.state_update_stats;
	}
	throw std::runtime_error("invalid state");
}
*/
void 
SpeedexNode::assert_state(NodeType required_state) {
	if (state != required_state) {
		std::string errstr = std::string("Expected ") + state_to_string(required_state) + ", but was in state " + state_to_string(state);
		throw std::runtime_error(errstr);
	}
}

std::string 
SpeedexNode::state_to_string(NodeType query_state) {
	switch(query_state) {
		case BLOCK_PRODUCER:
			return "BLOCK_PRODUCER";
		case BLOCK_VALIDATOR:
			return "BLOCK_VALIDATOR";
	}
	throw std::runtime_error("Invalid State!");
}

ExperimentResultsUnion 
SpeedexNode::get_measurements() {
	std::lock_guard lock(confirmation_mtx);
	return get_measurements_nolock();
}

// should own confirmation_mtx before calling.
ExperimentResultsUnion
SpeedexNode::get_measurements_nolock() {

	async_persister.wait_for_async_persist();
	ExperimentResultsUnion out = measurements_log.get_measurements();

	uint64_t num_measurements = highest_block;

	if (num_measurements == 0) {
		BLOCK_INFO("returned no measurements.  Is this ok?");
	}

	if (out.block_results.back().blockNumber != num_measurements) {
		BLOCK_INFO("Expected end of block_results to have block_number %lu, got %lu",
			num_measurements, out.block_results.back().blockNumber);
		throw std::runtime_error("block measurement accounting error");
	}

	if (out.block_results.front().blockNumber != 1) {
		throw std::runtime_error("didn't start measuring from 1");
	}

	return out;
}

void 
SpeedexNode::write_measurements() {
	std::lock_guard lock(confirmation_mtx);
	BLOCK_INFO("write measurements called");

	auto filename = overall_measurement_filename();

	auto out = get_measurements_nolock();

	if (save_xdr_to_file(out, filename.c_str())) {
		BLOCK_INFO("failed to save measurements file %s", filename.c_str());
	}

	BLOCK_INFO(
		"Wrote %lu measurements entries (make sure this is correct)",
		out.block_results.size());
}

void 
SpeedexNode::add_txs_to_mempool(
	std::vector<SignedTransaction>&& txs, 
	uint64_t latest_block_number) {

	assert_state(BLOCK_PRODUCER);	

	for(size_t i = 0; i <= txs.size() / mempool.TARGET_CHUNK_SIZE; i ++) {
		std::vector<SignedTransaction> chunk;
		size_t min_idx = i * mempool.TARGET_CHUNK_SIZE;
		size_t max_idx = std::min(txs.size(), (i + 1) * mempool.TARGET_CHUNK_SIZE);
		chunk.insert(
			chunk.end(),
			std::make_move_iterator(txs.begin() + min_idx),
			std::make_move_iterator(txs.begin() + max_idx));
		mempool.add_to_mempool_buffer(std::move(chunk));
	}
	mempool.latest_block_added_to_mempool.store(
		latest_block_number, std::memory_order_relaxed);
}


} /* edce */
