#include "block_processing/block_validator.h"

#include "block_processing/serial_transaction_processor.h"

#include "modlog/log_merge_worker.h"

#include "orderbook/commitment_checker.h"

#include "speedex/speedex_management_structures.h"
#include "stats/block_update_stats.h"

#include <atomic>
#include <cstdint>
#include <utility>

#include <mtt/utils/threadlocal_cache.h>

#include "utils/debug_macros.h"

#include <xdrpp/marshal.h>

namespace speedex {

const unsigned int VALIDATION_BATCH_SIZE = 1000;

struct SignedTransactionListWrapper {
	const SignedTransactionList& data;

	template<typename ValidatorType>
	bool operator() (ValidatorType& tx_validator, size_t i, BlockStateUpdateStatsWrapper& stats, SerialAccountModificationLog& serial_account_log) const {
		return tx_validator.validate_transaction(data[i], stats, serial_account_log);
	}

	size_t size() const {
		return data.size();
	}
};


struct AccountModificationBlockWrapper {
	const AccountModificationBlock& data;

	template<typename ValidatorType>
	bool operator() (ValidatorType& tx_validator, size_t i, BlockStateUpdateStatsWrapper& stats, SerialAccountModificationLog& serial_account_log) const {
		bool success = true;
		auto& txs = data[i].new_transactions_self;
		for (size_t j = 0; j < txs.size(); j++) {
			success &= tx_validator.validate_transaction(txs[j], stats, serial_account_log);
		}
		return success;
	}

	size_t size() const {
		return data.size();
	}
};

template<typename TxListOp>
class ParallelValidate {
	using serial_cache_t
		= utils::ThreadlocalCache<SerialTransactionValidator<OrderbookManager>>;

	const TxListOp& txs;
	SpeedexManagementStructures& management_structures;

	const OrderbookStateCommitmentChecker& clearing_commitment;
	ThreadsafeValidationStatistics& main_stats;
	serial_cache_t& serial_validator_cache;

public:

	BlockStateUpdateStatsWrapper state_update_stats;

	bool valid = true;

	void operator() (const tbb::blocked_range<std::size_t> r) {
		if (!valid) return;

		//TBB docs suggest this type of pattern (use local var until end)
		// optimizes better.
		bool temp_valid = true;
		SerialAccountModificationLog serial_account_log(
			management_structures.account_modification_log);
		auto& tx_validator = serial_validator_cache.get(
			management_structures,
			clearing_commitment,
			main_stats);

		for (size_t i = r.begin(); i < r.end(); i++) {
			if (!txs(tx_validator, i, state_update_stats, serial_account_log)) {
				temp_valid = false;
				std::printf("transaction %lu failed\n", i);
				break;
			}
		}
		valid = valid && temp_valid;
	}

	//split constructor can be concurrent with operator()
	ParallelValidate(ParallelValidate& x, tbb::split)
		: txs(x.txs)
		, management_structures(x.management_structures)
		, clearing_commitment(x.clearing_commitment)
		, main_stats(x.main_stats)
		, serial_validator_cache(x.serial_validator_cache)
			{};

	void join(ParallelValidate& other) {
		valid = valid && other.valid;

		if (valid) {
			state_update_stats += other.state_update_stats;
		}
	}

	ParallelValidate(
		const TxListOp& txs,
		SpeedexManagementStructures& management_structures,
		const OrderbookStateCommitmentChecker& clearing_commitment,
		ThreadsafeValidationStatistics& main_stats,
		serial_cache_t& serial_validator_cache)
		: txs(txs)
		, management_structures(management_structures)
		, clearing_commitment(clearing_commitment)
		, main_stats(main_stats)
		, serial_validator_cache(serial_validator_cache)
		, valid(true) 
		{}
};

template<typename WrappedType>
bool 
BlockValidator::validate_transaction_block_(
	const WrappedType& transactions,
	const OrderbookStateCommitmentChecker& clearing_commitment,
	ThreadsafeValidationStatistics& main_stats,
	BlockValidationMeasurements& measurements,
	BlockStateUpdateStatsWrapper& stats) {
	
	using serial_cache_t
		= utils::ThreadlocalCache<SerialTransactionValidator<OrderbookManager>>;

	serial_cache_t serial_validator_cache;

	auto validator = ParallelValidate(
		transactions, 
		management_structures,
		clearing_commitment,
		main_stats, 
		serial_validator_cache);

	BLOCK_INFO("starting to validate %lu txs", transactions.size());

	if (management_structures.account_modification_log.size() != 0) {
		throw std::runtime_error("forgot to clear mod log");
	}

	auto timestamp = utils::init_time_measurement();

	auto range = tbb::blocked_range<size_t>(
		0, transactions.size(), VALIDATION_BATCH_SIZE);

	tbb::parallel_reduce(range, validator);
	BLOCK_INFO("done validating");

	if (!validator.valid) {
		BLOCK_INFO("transaction returned as invalid");
		return false;
	}

	stats += validator.state_update_stats;

	measurements.tx_validation_processing_time = utils::measure_time(timestamp);

	worker.do_merge();

	auto offer_timestamp = utils::init_time_measurement();

	size_t num_orderbooks 
		= management_structures.orderbook_manager.get_num_orderbooks();

	tbb::parallel_for(
		tbb::blocked_range<std::size_t>(0, num_orderbooks),
		[&serial_validator_cache] (auto r) {
			for (auto i = r.begin(); i < r.end(); i++) {
				auto& validators = serial_validator_cache.get_objects();
				size_t validators_sz = validators.size();
				for (size_t j = 0; j < validators_sz; j++) {
					if (validators[j]) {
						validators[j]->extract_manager_view().partial_finish(i);
					}
				}
			}
		});
	for (auto& v :  serial_validator_cache.get_objects()) {
		if (v) {
			v->extract_manager_view().partial_finish_conclude();
		}
	}

	measurements.tx_validation_offer_merge_time = utils::measure_time(offer_timestamp);

	BLOCK_INFO("waiting for merge_in_log_batch join");

	worker.wait_for_merge_finish();

	measurements.tx_validation_trie_merge_time = utils::measure_time(timestamp);

	BLOCK_INFO("tx validation success, checking db state");
	auto res = management_structures.db.check_valid_state(
		management_structures.account_modification_log);
	BLOCK_INFO("done checking db state = %d", res);
	return res;
}


bool 
BlockValidator::validate_transaction_block(
	const AccountModificationBlock& transactions,
	const OrderbookStateCommitmentChecker& clearing_commitment,
	ThreadsafeValidationStatistics& main_stats,
	BlockValidationMeasurements& measurements,
	BlockStateUpdateStatsWrapper& stats) {
	
	AccountModificationBlockWrapper wrapper{transactions};

	return validate_transaction_block_(
		wrapper, clearing_commitment, main_stats, measurements, stats);
}

bool 
BlockValidator::validate_transaction_block(
	const SignedTransactionList& transactions,
	const OrderbookStateCommitmentChecker& clearing_commitment,
	ThreadsafeValidationStatistics& main_stats,
	BlockValidationMeasurements& measurements,
	BlockStateUpdateStatsWrapper& stats) {

	SignedTransactionListWrapper wrapper{transactions};

	return validate_transaction_block_(
		wrapper, clearing_commitment, main_stats, measurements, stats);
}

bool 
BlockValidator::validate_transaction_block(
	const SerializedBlock& transactions,
	const OrderbookStateCommitmentChecker& clearing_commitment,
	ThreadsafeValidationStatistics& main_stats,
	BlockValidationMeasurements& measurements,
	BlockStateUpdateStatsWrapper& stats) {

	SignedTransactionList txs;

	xdr::xdr_from_opaque(transactions, txs);

	SignedTransactionListWrapper wrapper{txs};

	return validate_transaction_block_(
		wrapper, clearing_commitment, main_stats, measurements, stats);
}

class ParallelTrustedReplay {
	const SignedTransactionList& txs;
	SpeedexManagementStructures& management_structures;
	const OrderbookStateCommitmentChecker& clearing_commitment;
	ThreadsafeValidationStatistics& main_stats;
	const uint64_t current_round_number;

public:

	void operator() (const auto r) {
		BlockStateUpdateStatsWrapper stats;

		SerialTransactionValidator<LoadLMDBManagerView> tx_validator(
			management_structures, 
			clearing_commitment, 
			main_stats, 
			current_round_number);

		SerialAccountModificationLog serial_account_log(
			management_structures.account_modification_log);
		
		for (size_t i = r.begin(); i < r.end(); i++) {

			tx_validator.validate_transaction(
				txs[i], 
				stats, 
				serial_account_log, 
				current_round_number);
		}
		tx_validator.extract_manager_view().finish_merge();
	}

	ParallelTrustedReplay(ParallelTrustedReplay& x, tbb::split)
		: txs(x.txs)
		, management_structures(x.management_structures)
		, clearing_commitment(x.clearing_commitment)
		, main_stats(x.main_stats)
		, current_round_number(x.current_round_number) {};

	void join(const ParallelTrustedReplay& other) {}

	ParallelTrustedReplay(
		const SignedTransactionList& txs,
		SpeedexManagementStructures& management_structures,
		const OrderbookStateCommitmentChecker& clearing_commitment,
		ThreadsafeValidationStatistics& main_stats,
		const uint64_t current_round_number)
		: txs(txs)
		, management_structures(management_structures)
		, clearing_commitment(clearing_commitment)
		, main_stats(main_stats)
		, current_round_number(current_round_number) {}
};

void
replay_trusted_block(
	SpeedexManagementStructures& management_structures,
	const SignedTransactionList& block,
	const HashedBlock& header) {

	ThreadsafeValidationStatistics validation_stats(
		management_structures.orderbook_manager.get_num_orderbooks());

	std::vector<Price> prices;

	for (unsigned i = 0; i < header.block.prices.size(); i++) {
		prices.push_back(header.block.prices[i]);
	}

	OrderbookStateCommitmentChecker commitment_checker(
		header.block.internalHashes.clearingDetails, prices, header.block.feeRate);
		
	auto replayer = ParallelTrustedReplay(
		block, management_structures, 
		commitment_checker, 
		validation_stats, 
		header.block.blockNumber);

	tbb::parallel_reduce(tbb::blocked_range<size_t>(0, block.size()), replayer);

	// No need to merge in account modification logs when replaying a trusted block. 
	// No need to export validation stats either.
}

} /* speedex */
