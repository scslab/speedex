#include "speedex/reload_from_hotstuff.h"

#include "block_processing/block_validator.h"

#include "hotstuff/log_access_wrapper.h"

#include "speedex/speedex_management_structures.h"
#include "speedex/speedex_operation.h"
#include "speedex/speedex_persistence.h"
#include "speedex/vm/speedex_vm.h"

#include "utils/debug_macros.h"
#include "utils/hash.h"

#include "xdr/block.h"

namespace speedex {

using xdr::operator==;

//! Replay round based on tx block data on disk
//! Used for catch up on data from trusted sources
//! or from data logged on disk (i.e. if db crashed
//! before fsync)
void speedex_replay_trusted_round_success(
	SpeedexManagementStructures& management_structures,
	HashedBlockTransactionListPair const& replay_data) {

	auto const& header = replay_data.hashedBlock;
	auto const& tx_block = replay_data.txList;

	uint64_t round_number = header.block.blockNumber;

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
	header_hash_map.insert_for_loading(header.block, true);

	//persist data
	if (management_structures.db.get_persisted_round_number() < round_number) {
		management_structures.db.commit_values();
		management_structures.db.persist_lmdb(round_number);
	}

	management_structures.orderbook_manager.persist_lmdb_for_loading(round_number);

	if (management_structures.block_header_hash_map.get_persisted_round_number() < round_number) {
		management_structures.block_header_hash_map.persist_lmdb(round_number);
	}
}

void speedex_replay_trusted_round_failed(
	SpeedexManagementStructures& management_structures,
	HashedBlock const & prev_block,
	HashedBlock const& next_block)
{
	uint64_t round_number = prev_block.block.blockNumber + 1;

	Block correction = ensure_sequential_block_numbers(prev_block, next_block);

	auto header_hash_map = LoadLMDBHeaderMap(round_number, management_structures.block_header_hash_map);
	header_hash_map.insert_for_loading(correction, false);
}

std::pair<Block, bool>
try_replay_saved_block(
	SpeedexManagementStructures& management_structures,
	BlockValidator& validator,
	HashedBlock const& prev_block,
	HashedBlockTransactionListPair const& replay_data) {

	OverallBlockValidationMeasurements measurements; //unused

	auto[corrected_next_block, validation_res] = speedex_block_validation_logic(
		management_structures,
		validator,
		measurements,
		prev_block,
		replay_data.hashedBlock,
		replay_data.txList);

	if (validation_res) {

		//clears account mod log & creates memory database persistence thunk
		persist_critical_round_data(
			management_structures, 
			replay_data.hashedBlock, 
			measurements.data_persistence_measurements, 
			false, 
			false);

		return {corrected_next_block, true};
	}

	return {corrected_next_block, false};
}

HashedBlock
speedex_load_persisted_data(
	SpeedexManagementStructures& management_structures,
	BlockValidator& validator,
	hotstuff::LogAccessWrapper const& decided_block_cache) {

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
			static_cast<uint64_t>(1),
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

	//auto cursor = decided_block_cache.forward_cursor();

	uint64_t cur_block = start_round;

	HashedBlock top_block;

	auto iter = decided_block_cache.begin();

	uint64_t nonempty_block_index = 0; // speedex vm height

	for (; iter != decided_block_cache.end(); ++iter) {
		auto [hs_hash, block_id] = iter.get_hs_hash_and_vm_data();

		if (block_id) {
			nonempty_block_index ++;

			if (nonempty_block_index != cur_block) {
				continue;
			}

			// TODO check whether block_id matches loaded block header? unnecessary unless disk tampered with or something unrecoverable happened
			// begs the question of whether hotstuff even needs to store block ids in lmdb (still needs them in speculative execution gadget).

			auto correct_header = management_structures.block_header_hash_map.get(nonempty_block_index);
			if (!correct_header) {
				throw std::runtime_error("failed to load expected hash from header hash map");
			}

			HashedBlockTransactionListPair speedex_data = decided_block_cache.template load_vm_block<SpeedexVMBlock>(hs_hash).data;

			if (correct_header->validation_success) {
				speedex_replay_trusted_round_success(management_structures, speedex_data);

				top_block = speedex_data.hashedBlock;	
			} else {
				speedex_replay_trusted_round_failed(management_structures, top_block, speedex_data.hashedBlock);

				auto new_block = ensure_sequential_block_numbers(top_block, speedex_data.hashedBlock);
				top_block.block = new_block;
				top_block.hash = hash_xdr(new_block);
			}
			cur_block++;

			if (cur_block > end_round) {
				break;
			}
		}
	}

	//replay remaining blocks, untrusted

	for (;iter != decided_block_cache.end(); ++iter) {
		auto [hs_hash, block_id] = iter.get_hs_hash_and_vm_data();

		if (block_id) {
			auto speedex_data = decided_block_cache.template load_vm_block<SpeedexVMBlock>(hs_hash).data;

			auto [corrected_next_block, _] = try_replay_saved_block(management_structures, validator, top_block, speedex_data);
			top_block.block = corrected_next_block;
			top_block.hash = hash_xdr(corrected_next_block);
		}
	}

	persist_after_loading(management_structures, top_block.block.blockNumber);

	return top_block;
}


} /* speedex */
