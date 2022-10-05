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

#include "speedex/speedex_measurements.h"

#include "utils/async_worker.h"

#include "xdr/block.h"
#include "xdr/database_commitments.h"


namespace speedex {

class SpeedexManagementStructures;

/*! Call before sending transaction block to a validator.
Persists account block + header, and prepares memory database with a persistence
thunk.
*/
std::unique_ptr<SignedTransactionList>
persist_critical_round_data(
	SpeedexManagementStructures& management_structures,
	const HashedBlock& header,
	BlockDataPersistenceMeasurements& measurements,
	bool get_block,
	bool write_block,
	uint64_t log_offset = 0);

//! Memory database loads persistence thunk into lmdb
void 
persist_async_phase1(
	SpeedexManagementStructures& management_structures,
	const uint64_t current_block_number,
	BlockDataPersistenceMeasurements& measurements);

//! Msync the memory database lmdb
void
persist_async_phase2(
	SpeedexManagementStructures& management_structures,
	uint64_t current_block_number,
	BlockDataPersistenceMeasurements& measurements);

//! Finish persistence (orderbooks, header hash map).
void
persist_async_phase3(
	SpeedexManagementStructures& management_structures,
	uint64_t current_block_number,
	BlockDataPersistenceMeasurements& measurements);

void
persist_after_loading(
	SpeedexManagementStructures& management_structures,
	uint64_t current_block_number);

//! Operates a background thread for phase 3 persistence.
class AsyncPersisterPhase3 : public AsyncWorker {
	using AsyncWorker::mtx;
	using AsyncWorker::cv;

	std::unique_ptr<PersistenceMeasurementLogCallback> persistence_callback;

	SpeedexManagementStructures& management_structures;
	//BlockDataPersistenceMeasurements* latest_measurements = nullptr;
	//std::optional<uint64_t> current_block_number = std::nullopt;
	
	bool exists_work_to_do() override final {
		return persistence_callback != nullptr;
	}

	void run();

public:

	AsyncPersisterPhase3(SpeedexManagementStructures& management_structures)
		: AsyncWorker()
		, persistence_callback(nullptr)
		, management_structures(management_structures) {
			start_async_thread([this] {run();});
		}

	void do_async_persist_phase3(
		std::unique_ptr<PersistenceMeasurementLogCallback> callback);

	~AsyncPersisterPhase3() {
		terminate_worker();
	}
};

//! Operates a background thread for phase 2 persistence.
//! Automatically calls phase 3 when done.
class AsyncPersisterPhase2 : public AsyncWorker {
	using AsyncWorker::mtx;
	using AsyncWorker::cv;

	std::unique_ptr<PersistenceMeasurementLogCallback> persistence_callback;

	SpeedexManagementStructures& management_structures;
	//BlockDataPersistenceMeasurements* latest_measurements = nullptr;
	//std::optional<uint64_t> current_block_number = std::nullopt;

	AsyncPersisterPhase3 phase3_persist;

	bool exists_work_to_do() override final {
		return persistence_callback != nullptr;
	}

	void run();

	friend class AsyncPersister;

public:

	AsyncPersisterPhase2(SpeedexManagementStructures& management_structures)
		: AsyncWorker()
		, persistence_callback(nullptr)
		, management_structures(management_structures)
		, phase3_persist(management_structures)
		{
			start_async_thread([this] {run();});
		}

	void do_async_persist_phase2(
		std::unique_ptr<PersistenceMeasurementLogCallback> callback);

	~AsyncPersisterPhase2() {
		terminate_worker();
	}
};

//! Operates a background thread for phase 1 persistence.
//! Automatically calls phase 2 when done.
struct AsyncPersister : public AsyncWorker {
	using AsyncWorker::mtx;
	using AsyncWorker::cv;

	std::unique_ptr<PersistenceMeasurementLogCallback> persistence_callback;
	//std::optional<uint64_t> block_number_to_persist;

	//BlockDataPersistenceMeasurements* latest_measurements;

	SpeedexManagementStructures& management_structures;

	uint64_t highest_persisted_block;

	AsyncPersisterPhase2 phase2_persist;

	bool exists_work_to_do() override final {
		return persistence_callback != nullptr;
	}

	void run();

public:

	AsyncPersister(SpeedexManagementStructures& management_structures)
		: AsyncWorker()
		, persistence_callback(nullptr)
		, management_structures(management_structures)
		, phase2_persist(management_structures) {
			start_async_thread([this] {run();});
		}

	uint64_t get_highest_persisted_block() {
		std::lock_guard lock(mtx);
		return highest_persisted_block;
	}


	//! Begin persisting a block to disk 
	//! (all blocks up to persist_block_number).
	//! When phase 1 finishes, phase 2 is automatically called.
	void do_async_persist(
		std::unique_ptr<PersistenceMeasurementLogCallback> callback);

	//! Wait for all async persistence phases to complete.
	//! Clears up all uses of measurements object reference.
	//! Should be called before shutdown or before invalidating measurements
	//! object reference.
	void wait_for_async_persist() {
		//clears up all uses of measurements reference
		wait_for_async_task();
		phase2_persist.wait_for_async_task();
		phase2_persist.phase3_persist.wait_for_async_task();
	}

	~AsyncPersister() {
		terminate_worker();
	}
};

} /* speedex */
