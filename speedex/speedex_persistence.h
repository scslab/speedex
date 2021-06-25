#pragma once

/*! \file speedex_persistence.h

Asynchronously persist data to disk.

Phase 0 (critical round data): transaction block & header to disk, and snapshots
account database to memory. Need to run this before we can send a block out to 
other nodes.

Phase 1: update account database lmdb with new account balances.
Phase 2: Force an msync of the account database lmdb to disk.
Phase 3: Everything else (orderbooks, header hash)
*/

#include <cstdint>

#include "speedex/speedex_management_structures.h"

#include "utils/async_worker.h"

#include "xdr/block.h"


namespace speedex {

std::unique_ptr<AccountModificationBlock>
persist_critical_round_data(
	SpeedexManagementStructures& management_structures,
	const HashedBlock& header,
	BlockDataPersistenceMeasurements& measurements,
	bool get_block = false,
	uint64_t log_offset = 0);

void 
persist_async_phase1(
	SpeedexManagementStructures& management_structures,
	const uint64_t current_block_number,
	BlockDataPersistenceMeasurements& measurements);

void
persist_async_phase2(
	SpeedexManagementStructures& management_structures,
	uint64_t current_block_number,
	BlockDataPersistenceMeasurements& measurements);

void
persist_async_phase3(
	SpeedexManagementStructures& management_structures,
	uint64_t current_block_number,
	BlockDataPersistenceMeasurements& measurements);

class AsyncPersisterPhase3 : public AsyncWorker {
	using AsyncWorker::mtx;
	using AsyncWorker::cv;

	SpeedexManagementStructures& management_structures;
	BlockDataPersistenceMeasurements* latest_measurements = nullptr;
	std::optional<uint64_t> current_block_number = std::nullopt;
	
	bool exists_work_to_do() override final {
		return latest_measurements != nullptr;
	}

	void run();

public:

	AsyncPersisterPhase3(SpeedexManagementStructures& management_structures)
		: AsyncWorker()
		, management_structures(management_structures) {
			start_async_thread([this] {run();});
		}

	void do_async_persist_phase3(
		uint64_t current_block_number_caller, 
		BlockDataPersistenceMeasurements* measurements) 
	{
		wait_for_async_task();

		std::lock_guard lock(mtx);
		latest_measurements = measurements;
		current_block_number = current_block_number_caller;
		cv.notify_one();
	}

	~AsyncPersisterPhase3() {
		wait_for_async_task();
		end_async_thread();
	}
};


class AsyncPersisterPhase2 : public AsyncWorker {
	using AsyncWorker::mtx;
	using AsyncWorker::cv;

	SpeedexManagementStructures& management_structures;
	BlockDataPersistenceMeasurements* latest_measurements = nullptr;
	std::optional<uint64_t> current_block_number = std::nullopt;

	AsyncPersisterPhase3 phase3_persist;

	bool exists_work_to_do() override final {
		return latest_measurements != nullptr;
	}

	void run();

	friend class AsyncPersister;

public:

	AsyncPersisterPhase2(SpeedexManagementStructures& management_structures)
		: AsyncWorker()
		, management_structures(management_structures)
		, phase3_persist(management_structures)
		{
			start_async_thread([this] {run();});
		}

	void do_async_persist_phase2(
		uint64_t current_block_number_caller, 
		BlockDataPersistenceMeasurements* measurements)\
	{
		wait_for_async_task();

		std::lock_guard lock(mtx);
		latest_measurements = measurements;
		current_block_number = current_block_number_caller;
		cv.notify_one();
	}

	~AsyncPersisterPhase2() {
		wait_for_async_task();
		end_async_thread();
	}
};

struct AsyncPersister : public AsyncWorker {
	using AsyncWorker::mtx;
	using AsyncWorker::cv;

	bool done_flag = false;

	std::optional<uint64_t> block_number_to_persist;

	BlockDataPersistenceMeasurements* latest_measurements;

	SpeedexManagementStructures& management_structures;

	uint64_t highest_persisted_block;

	AsyncPersisterPhase2 phase2_persist;

	bool exists_work_to_do() override final {
		return latest_measurements != nullptr;
	}


	void run();

public:

	AsyncPersister(SpeedexManagementStructures& management_structures)
		: AsyncWorker()
		, management_structures(management_structures)
		, phase2_persist(management_structures) {
			start_async_thread([this] {run();});
		}



	uint64_t get_highest_persisted_block() {
		std::lock_guard lock(mtx);
		return highest_persisted_block;
	}

	void do_async_persist(
		const uint64_t persist_block_number, 
		BlockDataPersistenceMeasurements& measurements) {
		
		auto timestamp = init_time_measurement();

		wait_for_async_task();

		measurements.wait_for_persist_time = measure_time(timestamp);
		{
			std::lock_guard lock(mtx);
			if (block_number_to_persist) {
				throw std::runtime_error(
					"can't start persist before last one finishes!");
			}
			block_number_to_persist = persist_block_number;
			latest_measurements = &measurements;
		}
		cv.notify_one();
	}

	void wait_for_async_persist() {
		//clears up all uses of measurements reference
		wait_for_async_task();
		phase2_persist.wait_for_async_task();
		phase2_persist.phase3_persist.wait_for_async_task();
	}

	~AsyncPersister() {
		wait_for_async_persist();
		end_async_thread();
	}
};

} /* speedex */
