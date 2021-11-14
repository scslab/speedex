#include "speedex/reload_from_hotstuff.h"

#include "hotstuff/lmdb.h"

#include "speedex/speedex_management_structures.h"
#include "speedex/speedex_persistence.h"

#include "utils/debug_macros.h"
#include "utils/hash.h"

#include "xdr/block.h"

namespace speedex {

using xdr::operator==;

//! Replay round based on tx block data on disk
//! Used for catch up on data from trusted sources
//! or from data logged on disk (i.e. if db crashed
//! before fsync)
void speedex_replay_trusted_round(
	SpeedexManagementStructures& management_structures,
	HashedBlockTransactionListPair const& replay_data) {

	auto const& header = replay_data.header;
	auto const& tx_block = replay_data.txList;

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

bool
try_replay_saved_block(
	SpeedexManagementStructures& management_structures,
	BlockValidator& validator,
	HashedBlock const& prev_block,
	HashedBlockTransactionPair const& replay_data) {

	OverallBlockValidationMeasurements measurements; //unused

	bool validation_res = speedex_block_validation_logic(
		management_structures,
		validator,
		measurements
		prev_block
		replay_data.hashedBlock,
		replay_data.txList);

	if (validation_res) {

		uint64_t current_round_number = replay_data.hashedBlock.block.blockNumber;

		//clears account mod log & creates memory database persistence thunk
		persist_critical_round_data(
			management_structures, 
			replay_data.hashedBlock, 
			current_measurements.data_persistence_measurements, 
			false, 
			false);

		//persist
		management_structures.db.persist_lmdb(current_block_number);
		management_structures.orderbook_manager.persist_lmdb(current_block_number);
		management_structures.block_header_hash_map.persist_lmdb(current_block_number);
		return true;
	}

	return false;

}

HashedBlock
speedex_load_persisted_data(
	SpeedexManagementStructures& management_structures,
	BlockValidator& validator,
	hotstuff::HotstuffLMDB const& decided_block_cache) {

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
			1,
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

	auto cursor = decided_block_cache.forward_cursor();

	uint64_t cur_block = start_round;

	HashedBlock top_block;

	auto iter = cursor.begin();

	for (; iter != cursor.end(); ++iter) {
		auto [hs_hash, block_id] = iter.get_hash_and_vm_data();

		if (block_id) {
			auto blockNumber = (*(block_id.value)).block.blockNumber;
			if (blockNumber != cur_block) {
				continue;
			}

			auto correct_hash = management_structures.block_header_hash_map.get_hash(blockNumber);
			if (!correct_hash) {
				throw std::runtime_error("failed to load expected hash from header hash map");
			}

			auto hs_supplied_hash = hash_xdr(*(block_id.value));
			if (hs_supplied_hash != *correct_hash) {
				continue;
			}

			HashedBlockTransactionListPair speedex_data = decided_block_cache.template load_vm_block<HashedBlockTransactionListPair>(hs_hash);
			top_block = speedex_data.hashedBlock;
			
			speedex_replay_trusted_round(management_structures, speedex_data);
			cur_round++;

			if (cur_round > end_round) {
				break;
			}
		}
	}

	//replay remaining blocks, untrusted

	while (iter != cursor.end()) {
		auto [hs_hash, block_id] = iter.get_hash_and_vm_data();

		if (block_id) {
			auto speedex_data = decided_block_cache.template load_vm_block<HashedBlockTransactionListPair>(hs_hash);

			if (try_replay_saved_block(management_structures, speedex_data)) {
				top_block = speedex_data.hashedBlock;
			}
		}
		++iter;
	}

	return top_block;
}


} /* speedex */
