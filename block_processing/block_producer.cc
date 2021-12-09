#include "block_processing/block_producer.h"

#include <atomic>
#include <cstdint>
#include <mutex>
#include <tbb/parallel_reduce.h>

#include "block_processing/serial_transaction_processor.h"

#include "utils/threadlocal_cache.h"

#include "xdr/block.h"

namespace speedex {

bool delete_tx_from_mempool(TransactionProcessingStatus status) {
	switch(status) {
		case SUCCESS:
			return true;
		case SEQ_NUM_TOO_HIGH:
		case SEQ_NUM_TEMP_IN_USE:
		case NEW_ACCOUNT_TEMP_RESERVED:
			return false;
		case INSUFFICIENT_BALANCE:
		case CANCEL_OFFER_TARGET_NEXIST:
			return true;
		case SOURCE_ACCOUNT_NEXIST:
		case INVALID_OPERATION_TYPE:
		case SEQ_NUM_TOO_LOW:
		case STARTING_BALANCE_TOO_LOW:
		case NEW_ACCOUNT_ALREADY_EXISTS:
		case INVALID_TX_FORMAT:
		case INVALID_OFFER_CATEGORY:
		case INVALID_PRICE:
		case RECIPIENT_ACCOUNT_NEXIST:
		case INVALID_PRINT_MONEY_AMOUNT:
		case INVALID_AMOUNT:
			return true;
		default:
			throw std::runtime_error(
				"forgot to add an error code to delete_tx_from_mempool");
	}
}

class BlockProductionReduce {
	SpeedexManagementStructures& management_structures;
	Mempool& mempool;
	ThreadlocalCache<SerialTransactionProcessor>& serial_processor_cache;
	std::atomic<int64_t>& remaining_block_space;
	std::atomic<uint64_t>& total_block_size;


public:
	std::unordered_map<TransactionProcessingStatus, uint64_t> status_counts;
	BlockStateUpdateStatsWrapper stats;

	
	void operator() (const tbb::blocked_range<std::size_t> r) {

		SerialAccountModificationLog serial_account_log(
			management_structures.account_modification_log);

		SerialTransactionProcessor&  tx_processor 
			= serial_processor_cache.get(management_structures);

		for (size_t i = r.begin(); i < r.end(); i++) {
			auto& chunk = mempool[i];
			std::vector<bool> bitmap;

			int64_t chunk_sz = chunk.size();

			bitmap.resize(chunk_sz, false);


			//reserve space for txs in output block
			int64_t remaining_space 
				= remaining_block_space.fetch_sub(
					chunk_sz, std::memory_order_relaxed);

			if (remaining_space < chunk_sz) {
				remaining_block_space.fetch_add(
					chunk_sz - remaining_space, std::memory_order_relaxed);
				//reduce the number of txs that we look at, according to reservation
				//We'll guarantee that we don't exceed a block limit, but we might ignore a few valid txs.
				chunk_sz = remaining_space;
				//chunk_sz += remaining_space;
				//remaining_block_space.fetch_add(-remaining_space, std::memory_order_relaxed);
				if (chunk_sz <= 0) {
					return;
				}
			}


			int64_t elts_added_to_block = 0;

			for (int64_t j = 0; j < chunk_sz; j++) {
				auto status = tx_processor.process_transaction(
					chunk[j], stats, serial_account_log);
				status_counts[status] ++;
				if (status == TransactionProcessingStatus::SUCCESS) {
					bitmap[j] = true;
					elts_added_to_block++;
				} else if(delete_tx_from_mempool(status)) {
					bitmap[j] = true;
				}
			}
			chunk.set_confirmed_txs(std::move(bitmap));

			auto post_check = remaining_block_space.fetch_add(
				chunk_sz - elts_added_to_block, std::memory_order_relaxed);
			total_block_size.fetch_add(
				elts_added_to_block, std::memory_order_release);
			if (post_check <= 0) {
				return;
			}
		}
	}

	BlockProductionReduce(BlockProductionReduce& x, tbb::split)
		: management_structures(x.management_structures)
		, mempool(x.mempool)
		, serial_processor_cache(x.serial_processor_cache)
		, remaining_block_space(x.remaining_block_space)
		, total_block_size(x.total_block_size)
			{};

	void join(BlockProductionReduce& other) {

		for (auto iter = other.status_counts.begin(); iter != other.status_counts.end(); iter++) {
			status_counts[iter->first] += iter->second;
		}

		stats += other.stats;
	}

	BlockProductionReduce(
		SpeedexManagementStructures& management_structures,
		Mempool& mempool,
		ThreadlocalCache<SerialTransactionProcessor>& serial_processor_cache,
		std::atomic<int64_t>& remaining_block_space,
		std::atomic<uint64_t>& total_block_size)
		: management_structures(management_structures)
		, mempool(mempool)
		, serial_processor_cache(serial_processor_cache)
		, remaining_block_space(remaining_block_space)
		, total_block_size(total_block_size)
		{}
};


//returns block size
uint64_t 
BlockProducer::build_block(
	Mempool& mempool,
	int64_t max_block_size,
	BlockCreationMeasurements& measurements,
	BlockStateUpdateStatsWrapper& state_update_stats) {

	if (management_structures.account_modification_log.size() != 0) {
		throw std::runtime_error("forgot to clear mod log");
	}

	ThreadlocalCache<SerialTransactionProcessor> serial_processor_cache;

	auto lock = mempool.lock_mempool();

	std::atomic<int64_t> remaining_space = max_block_size;
	std::atomic<uint64_t> total_block_size = 0;


	auto producer = BlockProductionReduce(
		management_structures, 
		mempool,
		serial_processor_cache,
		remaining_space, 
		total_block_size);

	tbb::blocked_range<size_t> range(0, mempool.num_chunks());

	BLOCK_INFO("starting produce block from mempool, max size=%ld", max_block_size);

	auto timestamp = init_time_measurement();

	tbb::parallel_reduce(range, producer);

	BLOCK_INFO("done produce block from mempool: duration %lf", measure_time(timestamp));

	MEMPOOL_INFO_F(
		for (auto iter = producer.status_counts.begin(); iter != producer.status_counts.end(); iter++) {
			std::printf("block_producer.cc:   mempool stats: code %d count %lu\n", iter->first, iter->second);
		}
		std::printf("block_producer.cc: new_offers %u cancel_offer %u payment %u new_account %u\n", 
			producer.stats.new_offer_count, producer.stats.cancel_offer_count, producer.stats.payment_count, producer.stats.new_account_count); 
	);

	worker.do_merge();

	size_t num_orderbooks 
		= management_structures.orderbook_manager.get_num_orderbooks();

	auto offer_merge_timestamp = init_time_measurement();
	tbb::parallel_for(
		tbb::blocked_range<size_t>(0, num_orderbooks),
		[&serial_processor_cache] (auto r) {
			for (auto i = r.begin(); i < r.end(); i++) {
				auto& processors = serial_processor_cache.get_objects();
				size_t processors_sz = processors.size();
				for (size_t j = 0; j < processors_sz; j++) {
					if (processors[j]) {
						processors[j]
							->extract_manager_view().partial_finish(i);
					}
				}
			}
		});
	for (auto& proc : serial_processor_cache.get_objects()) {
		if (proc)
			proc->extract_manager_view().partial_finish_conclude();
	}

	measurements.offer_merge_time = measure_time(offer_merge_timestamp);
	BLOCK_INFO("merging in new offers took %lf", measurements.offer_merge_time);

	uint64_t block_size = total_block_size.load(std::memory_order_relaxed);

	BLOCK_INFO("produced block of size %lu", block_size);

	state_update_stats += producer.stats;
	worker.wait_for_merge_finish();
	return block_size;
}



};
