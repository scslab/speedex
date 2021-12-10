#include "speedex/vm/speedex_vm.h"

#include "hotstuff/lmdb.h"

#include "speedex/speedex_management_structures.h"
#include "speedex/speedex_operation.h"

#include "utils/hash.h"
#include "utils/save_load_xdr.h"

#include "xdr/experiments.h"

namespace speedex {

SpeedexVM::SpeedexVM(
		const ExperimentParameters& params,
		const SpeedexOptions& options,
		std::string measurement_output_folder)
	: PERSIST_BATCH(options.persistence_frequency)
	, management_structures(options.num_assets, options.get_approx_params())
	, operation_mtx()
	, confirmation_mtx()
	, proposal_base_block()
	, last_committed_block() // genesis
	, async_persister(management_structures)
	, measurements_log(params)
	, measurement_output_folder(measurement_output_folder)
	, options(options)
	, params(params)
	, TARGET_BLOCK_SIZE(options.block_size)
	, MEMPOOL_TARGET_SIZE(options.mempool_target)
	, tatonnement_structs(management_structures.orderbook_manager)
	, mempool_structs(management_structures, MEMPOOL_CHUNK_SIZE, MEMPOOL_TARGET_SIZE)
	, log_merge_worker(management_structures.account_modification_log)
	, block_producer(management_structures, log_merge_worker)
	, block_validator(management_structures, log_merge_worker)
	{
		size_t num_assets = options.num_assets;
		prices.resize(num_assets);
		for (auto i = 0u; i < num_assets; i++) {
			prices[i] = price::from_double(1.0);
		}

		mkdir_safe(measurement_output_folder.c_str());
	}

void 
SpeedexVM::rewind_structs_to_committed_height() 
{
	uint64_t committed_round_number = last_committed_block.block.blockNumber;

	management_structures.db.commit_persistence_thunks(committed_round_number);
	management_structures.db.force_sync();
	management_structures.db.clear_persistence_thunks_and_reload(committed_round_number);

	management_structures.orderbook_manager.persist_lmdb(committed_round_number);
	management_structures.orderbook_manager.rollback_thunks(committed_round_number);

	management_structures.account_modification_log.detached_clear();

	management_structures.block_header_hash_map.persist_lmdb(committed_round_number);
	management_structures.block_header_hash_map.rollback_to_committed_round(committed_round_number);
	proposal_base_block = last_committed_block;
}

void
SpeedexVM::log_commitment(const block_id& id) {
	std::lock_guard lock(confirmation_mtx);
	if (id.value) {
		last_committed_block = *(id.value);
		auto last_committed_block_number = last_committed_block.block.blockNumber;
		if (last_committed_block_number % PERSIST_BATCH == 0) {
			async_persister.do_async_persist(
				std::make_unique<PersistenceMeasurementLogCallback>(measurements_log, last_committed_block_number));
		}
	}
}

TaggedSingleBlockResults
new_measurements(NodeType state)
{
	TaggedSingleBlockResults res;
	res.results.type(state);
	auto ts = init_time_measurement();
	res.startTimeStamp = std::chrono::duration_cast<std::chrono::milliseconds>(ts.time_since_epoch()).count();
	return res;
}

void
SpeedexVM::exec_block(const block_type& blk) {

	std::lock_guard lock(operation_mtx);
	std::lock_guard lock2(confirmation_mtx);

	if (last_committed_block.block.blockNumber + 1 != blk.hashedBlock.block.blockNumber) {
		BLOCK_INFO("incorrect block height appended to speedex vm chain -- no-op, except incrementing blockNumber");
		return;
	}

	auto const& new_header = blk.hashedBlock;

	mempool_structs.pre_validation_stop_background_filtering();

	uint64_t prev_block_number = last_committed_block.block.blockNumber;

	auto measurements_base = new_measurements(BLOCK_VALIDATOR);
	measurements_base.blockNumber = prev_block_number + 1;

	auto& current_measurements = measurements_base.results.validationResults();

	auto timestamp = init_time_measurement();

	auto logic_timestamp = init_time_measurement();

	auto [corrected_next_block, res] =  speedex_block_validation_logic( 
		management_structures,
		block_validator,
		current_measurements,
		last_committed_block,
		new_header,
		blk.txList);
	

	last_committed_block.block = corrected_next_block;
	last_committed_block.hash = hash_xdr(corrected_next_block);

	if (proposal_base_block.block.blockNumber < last_committed_block.block.blockNumber) {
		proposal_base_block = last_committed_block;
	}
	
	if (!res) {
		mempool_structs.post_validation_cleanup();
		return;
	}

	current_measurements.validation_logic_time = measure_time(logic_timestamp);
	
	auto persistence_start = init_time_measurement();
	
	persist_critical_round_data(
		management_structures, 
		new_header, 
		current_measurements.data_persistence_measurements, 
		false, 
		false);

	current_measurements.total_persistence_time = measure_time(persistence_start);

	current_measurements.total_time = measure_time(timestamp);

	//last_committed_block = new_header;

	measurements_log.add_measurement(measurements_base);

	mempool_structs.post_validation_cleanup();
}

size_t
SpeedexVM::assemble_block(TaggedSingleBlockResults& measurements_base, BlockStateUpdateStatsWrapper& state_update_stats)
{
	auto& current_measurements = measurements_base.results.productionResults();
	auto timestamp = init_time_measurement();

	auto mempool_push_ts = init_time_measurement();
	mempool_structs.pre_production_stop_background_filtering();
	current_measurements.mempool_push_time = measure_time(mempool_push_ts);

	current_measurements.last_block_added_to_mempool 
		= mempool_structs.mempool.latest_block_added_to_mempool.load(std::memory_order_relaxed);

	size_t block_size = block_producer.build_block(
		mempool_structs.mempool, 
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

	mempool_structs.during_production_post_tx_select_start_cleaning();
	return block_size;
}

void write_tx_data(SignedTransactionList& tx_data, const AccountModificationBlock& mod_block) {
	for (auto const& log : mod_block)
	{
		for (auto const& tx : log.new_transactions_self) {
			tx_data.push_back(tx);
		}
	}
}

std::unique_ptr<SpeedexVM::block_type>
SpeedexVM::propose()
{
	auto start_time = init_time_measurement();

	std::lock_guard lock(operation_mtx);

	uint64_t prev_block_number = proposal_base_block.block.blockNumber;
	
	BLOCK_INFO("Starting production on block %lu", prev_block_number + 1);

	auto measurements_base = new_measurements(BLOCK_PRODUCER);

	measurements_base.blockNumber = prev_block_number + 1;
	auto& current_measurements = measurements_base.results.productionResults();
	
	BlockStateUpdateStatsWrapper state_update_stats;

	current_measurements.total_init_time = measure_time_from_basept(start_time);

	BLOCK_INFO("mempool size: %lu", mempool_structs.mempool.size());

	size_t block_size = assemble_block(measurements_base, state_update_stats);

	experiment_done = (block_size < 100);

	current_measurements.total_block_build_time = measure_time_from_basept(start_time);

	HashedBlock new_block = speedex_block_creation_logic(
		prices,
		management_structures,
		tatonnement_structs,
		proposal_base_block,
		current_measurements,
		state_update_stats);

	proposal_base_block = new_block;
	
	current_measurements.total_block_commitment_time = measure_time_from_basept(start_time);

	auto timestamp = init_time_measurement();

	auto output_tx_block = persist_critical_round_data(
		management_structures, 
		proposal_base_block, 
		current_measurements.data_persistence_measurements, 
		true,
		false);

	current_measurements.data_persistence_measurements.total_critical_persist_time = measure_time(timestamp);
	current_measurements.total_critical_persist_time = measure_time_from_basept(start_time);
	current_measurements.total_block_persist_time = measure_time_from_basept(start_time);
	current_measurements.state_update_stats = state_update_stats.get_xdr();

	auto mempool_wait_ts = init_time_measurement();

	current_measurements.block_creation_measurements.mempool_clearing_time = mempool_structs.post_production_cleanup();
	current_measurements.mempool_wait_time = measure_time(mempool_wait_ts);
	
	auto out = std::make_unique<block_type>();
	out->hashedBlock = proposal_base_block;
	out -> txList.reserve(block_size);
	write_tx_data(out->txList, *output_tx_block);

	current_measurements.serialize_time = measure_time(mempool_wait_ts);

	current_measurements.total_time_from_basept = measure_time_from_basept(start_time);
	current_measurements.total_time = measure_time(start_time);

	measurements_log.add_measurement(measurements_base);

	return out;
}

ExperimentResultsUnion 
SpeedexVM::get_measurements() {
	std::lock_guard lock(confirmation_mtx);
	return get_measurements_nolock();
}

// should own confirmation_mtx before calling.
ExperimentResultsUnion
SpeedexVM::get_measurements_nolock() {

	async_persister.wait_for_async_persist();
	ExperimentResultsUnion out = measurements_log.get_measurements();

	if (out.block_results.size() == 0) {
		BLOCK_INFO("returned no measurements.  Is this ok?");
	}

	return out;
}

void 
SpeedexVM::write_measurements() {
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
SpeedexVM::rewind_to_last_commit() {
	std::lock_guard lock(operation_mtx);
	std::lock_guard lock2(confirmation_mtx);
	rewind_structs_to_committed_height();
}

uint64_t 
SpeedexVM::get_lead_block_height() {
	std::lock_guard lock(operation_mtx);
	std::lock_guard lock2(confirmation_mtx);
	return proposal_base_block.block.blockNumber;
}

} /* speedex */
