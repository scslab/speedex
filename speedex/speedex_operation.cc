#include "speedex/speedex_operation.h"

#include "orderbook/utils.h"

#include "speedex/autorollback_validation_structures.h"

#include "utils/hash.h"
#include "utils/header_persistence.h"
#include "utils/time.h"
#include "utils/save_load_xdr.h"

namespace speedex {

void speedex_make_state_commitment(
	InternalHashes& hashes,
	SpeedexManagementStructures& management_structures,
	BlockProductionHashingMeasurements& measurements) {
	{
		auto timestamp = init_time_measurement();

		management_structures.db.produce_state_commitment(
			hashes.dbHash, management_structures.account_modification_log);
		measurements.db_state_commitment_time = measure_time(timestamp);
	}

	{
		auto timestamp = init_time_measurement();
		management_structures.orderbook_manager.hash(hashes.clearingDetails);
		measurements.work_unit_commitment_time = measure_time(timestamp);
	}

	auto timestamp = init_time_measurement();
	management_structures.account_modification_log.hash(
		hashes.modificationLogHash);
	measurements.account_log_hash_time = measure_time(timestamp);

	management_structures.block_header_hash_map.hash(hashes.blockMapHash);
}

void speedex_format_hashed_block(
	HashedBlock& block_out,
	const HashedBlock& prev_block,
	const std::vector<Price>& price_workspace,
	const uint8_t tax_rate) {

	block_out.block.blockNumber = prev_block.block.blockNumber + 1;
	block_out.block.prevBlockHash = prev_block.hash;
	block_out.block.feeRate = tax_rate;


	for (unsigned int i = 0; i < price_workspace.size(); i++) {
		block_out.block.prices.push_back(price_workspace[i]);
	}

	block_out.hash = hash_xdr(block_out.block);
}


HashedBlock
speedex_block_creation_logic(
	std::vector<Price>& price_workspace,
	SpeedexManagementStructures& management_structures,
	TatonnementManagementStructures& tatonnement,
	const HashedBlock& prev_block,
	OverallBlockProductionMeasurements& overall_measurements,
	BlockStateUpdateStatsWrapper& state_update_stats)
{

	auto& stats = overall_measurements.block_creation_measurements;

	HashedBlock new_block;

	auto prev_block_number = prev_block.block.blockNumber;

	uint64_t current_block_number = prev_block_number + 1;

	BLOCK_INFO("starting block creation");
	
	// Allocate a file descriptor/file for persisting account modification log.
	management_structures.account_modification_log.prepare_block_fd(
		current_block_number);

	auto timestamp = init_time_measurement();

	auto& db = management_structures.db;
	auto& orderbook_manager = management_structures.orderbook_manager;

	//Push new accounts into main db (also updates database merkle trie).
	db.commit_new_accounts(current_block_number);
	stats.initial_account_db_commit_time = measure_time(timestamp);

	BLOCK_INFO("initial accountdb commit duration: %fs", 
		stats.initial_account_db_commit_time);

	//! Commits newly created offers, preps orderbooks for Tatonnement
	orderbook_manager.commit_for_production(current_block_number);

	stats.initial_offer_db_commit_time = measure_time(timestamp);

	BLOCK_INFO("initial offerdb commit duration: %fs", 
		stats.initial_offer_db_commit_time);
	BLOCK_INFO("Database size:%lu", db.size());

	std::atomic<bool> tatonnement_timeout = false;
	std::atomic<bool> cancel_timeout = false;

	auto timeout_th = tatonnement.oracle.launch_timeout_thread(
		2000, tatonnement_timeout, cancel_timeout);

	auto tat_res = tatonnement.oracle.compute_prices_grid_search(
		price_workspace.data(), 
		management_structures.approx_params, 
		tatonnement.rolling_averages.get_formatted_avgs());
	
	//After running tatonnement, signal the timeout thread to cancel
	cancel_timeout = true;

	stats.tatonnement_time = measure_time(timestamp);
	BLOCK_INFO("price computation took %fs", stats.tatonnement_time);
	stats.tatonnement_rounds = tat_res.num_rounds;

	BLOCK_INFO("time per tat round:%lf microseconds", 
		1'000'000.0 * stats.tatonnement_time / tat_res.num_rounds);

	// Did tatonnement timeout or not?
	// If it timed out, prices are not mu-approximate.  Hence,
	// supply activation lower bounds aren't feasible.
	// Drop these lower bounds, clear as much trade activity as possible.
	bool use_lower_bound = !tatonnement_timeout;

	if (!use_lower_bound) {
		BLOCK_INFO("tat timed out!");
	}

	auto lp_results = tatonnement.lp_solver.solve(
		price_workspace.data(), 
		management_structures.approx_params,
		use_lower_bound);
	
	stats.lp_time = measure_time(timestamp);
	BLOCK_INFO("lp solving took %fs", stats.lp_time);


	// Experimental diagnostic - rerun tatonnement with much longer timeout
	// to see how equilibrium changes with more time.
	constexpr bool rerun_tatonnement = false;

	if (rerun_tatonnement && !use_lower_bound) {
		BLOCK_INFO("rerunning tatonnement");
		std::vector<Price> price_copy = price_workspace;

		tatonnement.oracle.wait_for_all_tatonnement_threads();

		timeout_th.join();
		tatonnement_timeout = false;
		cancel_timeout = false;
			
		timeout_th = tatonnement.oracle.launch_timeout_thread(
			50'000, tatonnement_timeout, cancel_timeout); 

		auto tat_rerun_res = tatonnement.oracle.compute_prices_grid_search(
			price_copy.data(), 
			management_structures.approx_params, 
			tatonnement.rolling_averages.get_formatted_avgs());
		
		bool use_lower_bound_2 = !tatonnement_timeout;
		auto lp_res_2 = tatonnement.lp_solver.solve(
				price_copy.data(), management_structures.approx_params, use_lower_bound_2);

		auto clearing_check = lp_res_2.check_clearing(price_copy);
		
		if (!clearing_check) {
			std::fflush(stdout);
			throw std::runtime_error(
				"The prices we computed in long tatonnement did not result in clearing!!!");
		}

		management_structures.orderbook_manager.get_max_feasible_smooth_mult(
			lp_res_2, price_copy.data());
		double vol_metric = management_structures.orderbook_manager
			.get_weighted_price_asymmetry_metric(lp_res_2, price_copy);

		BLOCK_INFO("long run Tat vol metric: %lf", vol_metric);
		BLOCK_INFO("done rerunning");
	}

	//TODO could be removed later.  Good rn as a sanititimeouttheck.
	auto clearing_check = lp_results.check_clearing(price_workspace);

	stats.clearing_check_time = measure_time(timestamp);
	BLOCK_INFO("clearing sanity check took %fs", stats.clearing_check_time);

	double vol_metric = management_structures.orderbook_manager
		.get_weighted_price_asymmetry_metric(lp_results, price_workspace);
	
	BLOCK_INFO("regular Tat vol metric: timeout %lu %lf", 
		!use_lower_bound, vol_metric);

	if (!clearing_check) {
		std::fflush(stdout);
		throw std::runtime_error("The prices we computed did not result in clearing!!!");
	}

	tatonnement.rolling_averages.update_averages(
		lp_results, price_workspace.data());

	stats.num_open_offers = orderbook_manager.num_open_offers();
	BLOCK_INFO("num open offers is %lu", stats.num_open_offers);

	auto& clearing_details = new_block.block.internalHashes.clearingDetails;

	orderbook_manager.clear_offers_for_production(
		lp_results, 
		price_workspace.data(), 
		db, 
		management_structures.account_modification_log, 
		clearing_details, 
		state_update_stats);

	stats.offer_clearing_time = measure_time(timestamp);
	BLOCK_INFO("clearing offers took %fs", stats.offer_clearing_time);

	if (!db.check_valid_state(management_structures.account_modification_log)) {
		throw std::runtime_error("DB left in invalid state!!!");
	}

	stats.db_validity_check_time = measure_time(timestamp);
	BLOCK_INFO("db validity check took %fs", stats.db_validity_check_time);

	db.commit_values(management_structures.account_modification_log);

	stats.final_commit_time = measure_time(timestamp);
	BLOCK_INFO("final commit took %fs", stats.final_commit_time);
	BLOCK_INFO("finished block creation");

	//management_structures.block_header_hash_map.insert_for_production(
	//	prev_block_number, prev_block.hash);
	
	uint8_t achieved_feerate = lp_results.tax_rate;
	
	stats.achieved_feerate = achieved_feerate;
	stats.achieved_smooth_mult 
		= management_structures.orderbook_manager.get_max_feasible_smooth_mult(
			lp_results, price_workspace.data());
	stats.tat_timeout_happened = !use_lower_bound;

	BLOCK_INFO("achieved approx params tax %lu smooth %lu", 
		stats.achieved_feerate, stats.achieved_smooth_mult);

	if ((!stats.tat_timeout_happened) 
		&& stats.achieved_smooth_mult + 1 < management_structures.approx_params.smooth_mult) {
		BLOCK_INFO("lower bound dropped from numerical precision challenge in lp solving");
	}

	auto& hashing_measurements 
		= overall_measurements.production_hashing_measurements;

	speedex_make_state_commitment(
		new_block.block.internalHashes,
		management_structures,
		hashing_measurements);

	overall_measurements.state_commitment_time = measure_time(timestamp);

	speedex_format_hashed_block(
		new_block,
		prev_block,
		price_workspace,
		achieved_feerate);

	overall_measurements.format_time = measure_time(timestamp);

	tatonnement.oracle.wait_for_all_tatonnement_threads();
	timeout_th.join();

	management_structures.block_header_hash_map.insert(new_block.block.blockNumber, new_block.hash);

	return new_block;
}

void debug_hash_discrepancy(
	const HashedBlock& expected_next_block,
	const Block& comparison_next_block,
	AccountModificationLog& mod_log) {
	BLOCK_INFO("incorrect hash");

	if (memcmp(
			comparison_next_block.prevBlockHash.data(), 
			expected_next_block.block.prevBlockHash.data(), 
			32) != 0) {
		BLOCK_INFO("discrepancy in prevBlockHash");
	}
	if (comparison_next_block.blockNumber != expected_next_block.block.blockNumber) {
		BLOCK_INFO("discrepancy in blockNumber");
	}

	auto current_block_number =  expected_next_block.block.blockNumber;
	
	if (comparison_next_block.prices.size() != expected_next_block.block.prices.size()) {
		BLOCK_INFO("different numbers of prices");
	}
	for (unsigned int i = 0; 
		i < std::min(
				comparison_next_block.prices.size(), 
				expected_next_block.block.prices.size());
		i++) 
	{
		if (comparison_next_block.prices[i] != expected_next_block.block.prices[i]) {
			BLOCK_INFO("discrepancy at price %u", i);
		}
	}
	if (comparison_next_block.feeRate != expected_next_block.block.feeRate) {
		BLOCK_INFO("discrepancy in feeRate");
	}

	if (memcmp(
			comparison_next_block.internalHashes.dbHash.data(), 
			expected_next_block.block.internalHashes.dbHash.data(), 
			32) != 0) {
		BLOCK_INFO("discrepancy in dbHash");
	}

	for (unsigned int i = 0; 
		i < expected_next_block.block.internalHashes.clearingDetails.size(); 
		i++) {
		if (memcmp(
				comparison_next_block.internalHashes.clearingDetails[i].rootHash.data(), 
				expected_next_block.block.internalHashes.clearingDetails[i].rootHash.data(), 
				32) != 0) {
			BLOCK_INFO("discrepancy in work unit %lu", i);
		}
	}

	if (memcmp(
			comparison_next_block.internalHashes.modificationLogHash.data(), 
			expected_next_block.block.internalHashes.modificationLogHash.data(), 
			32) != 0) {
		BLOCK_INFO("mod log discrepancy");
		mod_log.diff_with_prev_log(current_block_number);
		// persist anyways for now, for comparison purposes later 
		mod_log.persist_block(current_block_number + 1000000, false, true); 
	}
	if (memcmp(
			comparison_next_block.internalHashes.blockMapHash.data(), 
			expected_next_block.block.internalHashes.blockMapHash.data(), 
			32) != 0) {
		BLOCK_INFO("header hash map discrepancy");
	}
}


template<typename TxLogType>
bool speedex_block_validation_logic( 
	SpeedexManagementStructures& management_structures,
	BlockValidator& validator,
	OverallBlockValidationMeasurements& overall_validation_stats,
	const HashedBlock& prev_block,
	const HashedBlock& expected_next_block,
	const TxLogType& transactions) {
	
	uint64_t current_block_number = prev_block.block.blockNumber + 1;
	BLOCK_INFO("starting block validation for block %lu", current_block_number);

	if (current_block_number != expected_next_block.block.blockNumber) {
		BLOCK_INFO("invalid block number");
		return false;
	}

	management_structures.account_modification_log.prepare_block_fd(
		current_block_number + 1000000);

	auto num_assets = management_structures.orderbook_manager.get_num_assets();
	auto num_orderbooks = management_structures.orderbook_manager.get_num_orderbooks();

	ThreadsafeValidationStatistics validation_stats(num_orderbooks);

	BlockStateUpdateStatsWrapper state_update_stats;

	std::vector<Price> prices;

	if (expected_next_block.block.prices.size() != num_assets) {
		BLOCK_INFO("incorrect number of prices in expected_next_block");
		return false;
	}

	if (expected_next_block.block.internalHashes.clearingDetails.size() != num_orderbooks) {
		BLOCK_INFO("invalid clearingdetails (size: %lu, expected %lu", 
			expected_next_block.block.internalHashes.clearingDetails.size(), 
			num_orderbooks);
		return false;
	}

	prices = expected_next_block.block.prices;

	//for (unsigned i = 0; i < options.num_assets; i++) {
	//	prices.push_back(expected_next_block.block.prices[i]);
	//}

	if (expected_next_block.block.feeRate + 1 < management_structures.approx_params.tax_rate) {
		BLOCK_INFO("invalid fee rate (got %u, expected %u", 
			expected_next_block.block.feeRate, 
			management_structures.approx_params.tax_rate);
		return false;
	}

	if (memcmp(
			expected_next_block.block.prevBlockHash.data(), 
			prev_block.hash.data(), 
			32) != 0) {
		BLOCK_INFO("next block doesn't point to prev block");
		return false;
	}

	OrderbookStateCommitmentChecker commitment_checker(
		expected_next_block.block.internalHashes.clearingDetails, 
		prices, 
		expected_next_block.block.feeRate);

	INFO("commitment checker log:");
	INFO_F(commitment_checker.log());

	SpeedexManagementStructuresAutoRollback autorollback_structures(
		management_structures,
		current_block_number,
		commitment_checker);

	auto& db_autorollback = autorollback_structures.db;
	auto& manager_autorollback = autorollback_structures.orderbook_manager;
	//auto& header_map_autorollback 
	//	= autorollback_structures.block_header_hash_map;

	auto timestamp = init_time_measurement();

	auto& stats = overall_validation_stats.block_validation_measurements;

	BLOCK_INFO("checking clearing params");
	auto clearing_param_res = commitment_checker.check_clearing();

	stats.clearing_param_check = measure_time(timestamp);

	if (!clearing_param_res) {

		BLOCK_INFO("clearing params invalid");
		return false;
	}

	auto res = validator.validate_transaction_block(
		transactions, 
		commitment_checker, 
		validation_stats,
		stats,
		state_update_stats); // checks db in valid state.

	INFO_F(validation_stats.log());

	stats.tx_validation_time = measure_time(timestamp);
	BLOCK_INFO("block validation time: %lf",stats.tx_validation_time);

	if (!res) {

		BLOCK_INFO("validation error");
		return false;
	}

	db_autorollback.tentative_commit_for_validation();
	std::printf("done tentative commit for validation\n");
	manager_autorollback.tentative_commit_for_validation(current_block_number);
	std::printf("done tentative commit workunits\n");

	stats.tentative_commit_time = measure_time(timestamp);

	auto clearings_valid 
		= manager_autorollback.tentative_clear_offers_for_validation(
			management_structures,
			validation_stats,
			state_update_stats);

	std::printf("done tentative clearing\n");

	stats.check_workunit_validation_time = measure_time(timestamp);

	if (!clearings_valid) {
		BLOCK_INFO("clearings invalid");
		return false;
	}

	if (!commitment_checker.check_stats(validation_stats)) {
		BLOCK_INFO("clearing stats mismatch");
		return false;
	}

	//TODO check optimality of LP solution ?

	//if (!header_map_autorollback.tentative_insert_for_validation(
	//	prev_block.block.blockNumber, prev_block.hash)) {
	//	BLOCK_INFO("couldn't insert block hash");
	//	return false;
	//}

	//TODO currently ignoring header map mod times, which should be basically 0.
	measure_time(timestamp);

	Block comparison_next_block;

	comparison_next_block.prevBlockHash = prev_block.hash;
	comparison_next_block.blockNumber = current_block_number;
	comparison_next_block.prices = expected_next_block.block.prices;
	comparison_next_block.feeRate = expected_next_block.block.feeRate;

	stats.get_dirty_account_time = measure_time(timestamp);
	db_autorollback.tentative_produce_state_commitment(
		comparison_next_block.internalHashes.dbHash,
		management_structures.account_modification_log);

	stats.db_tentative_commit_time = measure_time(timestamp);
	BLOCK_INFO("db tentative_commit_time = %lf", stats.db_tentative_commit_time);
	
	//copy expected clearing state, except we overwrite the hashes later.
	comparison_next_block.internalHashes.clearingDetails 
		= expected_next_block.block.internalHashes.clearingDetails;
	management_structures.orderbook_manager.hash(
		comparison_next_block.internalHashes.clearingDetails);

	stats.workunit_hash_time = measure_time(timestamp);

	management_structures.account_modification_log.hash(
		comparison_next_block.internalHashes.modificationLogHash);

	management_structures.block_header_hash_map.hash(
		comparison_next_block.internalHashes.blockMapHash);

	Hash final_hash = hash_xdr(comparison_next_block);

	auto hash_result = memcmp(
		final_hash.data(), expected_next_block.hash.data(), 32);

	if (hash_result != 0) {
		BLOCK_INFO("incorrect hash");

		debug_hash_discrepancy(
			expected_next_block, 
			comparison_next_block, 
			management_structures.account_modification_log);
		return false;
	} else {
		BLOCK_INFO("block hash match");
	}

	autorollback_structures.finalize_commit(current_block_number, stats);

	management_structures.block_header_hash_map.insert(expected_next_block.block.blockNumber, expected_next_block.hash);

	overall_validation_stats.state_update_stats = state_update_stats.get_xdr();

	return true;
}

/*
If successful, returns true and all state is committed to next block.
If fails, no-op.
*/
template bool speedex_block_validation_logic( 
	SpeedexManagementStructures& management_structures,
	BlockValidator& validator,
	OverallBlockValidationMeasurements& overall_validation_stats,
	const HashedBlock& prev_block,
	const HashedBlock& expected_next_block,
	const SerializedBlock& transactions);

template bool speedex_block_validation_logic( 
	SpeedexManagementStructures& management_structures,
	BlockValidator& validator,
	OverallBlockValidationMeasurements& overall_validation_stats,
	const HashedBlock& prev_block,
	const HashedBlock& expected_next_block,
	const TransactionData& transactions);

template bool speedex_block_validation_logic( 
	SpeedexManagementStructures& management_structures,
	BlockValidator& validator,
	OverallBlockValidationMeasurements& overall_validation_stats,
	const HashedBlock& prev_block,
	const HashedBlock& expected_next_block,
	const AccountModificationBlock& transactions);

template bool speedex_block_validation_logic( 
	SpeedexManagementStructures& management_structures,
	BlockValidator& validator,
	OverallBlockValidationMeasurements& overall_validation_stats,
	const HashedBlock& prev_block,
	const HashedBlock& expected_next_block,
	const SignedTransactionList& transactions);




uint64_t speedex_load_persisted_data(
	SpeedexManagementStructures& management_structures) {


	management_structures.db.load_lmdb_contents_to_memory();
	management_structures.orderbook_manager.load_lmdb_contents_to_memory();
	management_structures.block_header_hash_map.load_lmdb_contents_to_memory();

	auto db_round = management_structures.db.get_persisted_round_number();

	auto max_orderbook_round = management_structures.orderbook_manager.get_max_persisted_round_number();
	auto min_orderbook_round = management_structures.orderbook_manager.get_min_persisted_round_number();

	if (max_orderbook_round > db_round) {
		throw std::runtime_error("can't reload if workunit persists without db (bc of the cancel offers thing)");
	}

	BLOCK_INFO("db round: %lu manager max round: %lu hashmap %lu", 
		db_round, 
		max_orderbook_round, 
		management_structures.block_header_hash_map.get_persisted_round_number());

	auto start_round = std::min(
		{
			db_round, 
			min_orderbook_round, 
			management_structures.block_header_hash_map.get_persisted_round_number()
		});
	auto end_round = std::max(
		{
			db_round, 
			max_orderbook_round, 
			management_structures.block_header_hash_map.get_persisted_round_number()
		});

	BLOCK_INFO("replaying rounds [%lu, %lu]", start_round, end_round);

	for (auto i = start_round; i <= end_round; i++) {
		speedex_replay_trusted_round(management_structures, i);
	}
	management_structures.db.commit_values();
	return end_round;
}


//! Replay round based on tx block data on disk
//! Used for catch up on data from trusted sources
//! or from data logged on disk (i.e. if db crashed
//! before fsync)
void speedex_replay_trusted_round(
	SpeedexManagementStructures& management_structures,
	const uint64_t round_number) {

	if (round_number == 0) {
		//There is no block number 0.
		return;
	}

	auto header = load_header(round_number);
	AccountModificationBlock tx_block;
	auto block_filename = tx_block_name(round_number);
	auto res = load_xdr_from_file(tx_block, block_filename.c_str());
	if (res != 0) {
		throw std::runtime_error((std::string("can't load tx block ") + block_filename).c_str());
	}

	BLOCK_INFO("starting to replay transactions of round %lu", round_number);
	replay_trusted_block(management_structures, tx_block, header);
	if (management_structures.db.get_persisted_round_number() < round_number) {
		management_structures.db.commit_new_accounts(round_number);
	}
	BLOCK_INFO("replayed txs in block %lu", round_number);

	//not actually used or checked.
	ThreadsafeValidationStatistics validation_stats(
		management_structures.orderbook_manager.get_num_orderbooks());

	std::vector<Price> prices;
	for (unsigned i = 0; i < header.block.prices.size(); i++) {
		prices.push_back(header.block.prices[i]);
	}
	OrderbookStateCommitmentChecker commitment_checker(
		header.block.internalHashes.clearingDetails, prices, header.block.feeRate);

	management_structures.orderbook_manager.commit_for_loading(round_number);

	NullModificationLog no_op_modification_log{};

	management_structures.orderbook_manager.clear_offers_for_data_loading(
		management_structures.db, no_op_modification_log, validation_stats, commitment_checker, round_number);

	management_structures.orderbook_manager.finalize_for_loading(round_number);

	auto header_hash_map = LoadLMDBHeaderMap(round_number, management_structures.block_header_hash_map);
	header_hash_map.insert_for_loading(round_number, header.hash);

	if (management_structures.db.get_persisted_round_number() < round_number) {
		management_structures.db.persist_lmdb(round_number);
	}
	management_structures.orderbook_manager.persist_lmdb_for_loading(round_number);
	if (management_structures.block_header_hash_map.get_persisted_round_number() < round_number) {
		management_structures.block_header_hash_map.persist_lmdb(round_number);
	}
} 

} /* speedex */
