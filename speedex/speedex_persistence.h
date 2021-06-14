#pragma once


namespace speedex {

std::unique_ptr<AccountModificationBlock>
edce_persist_critical_round_data(
	EdceManagementStructures& management_structures,
	const HashedBlock& header,
	BlockDataPersistenceMeasurements& measurements,
	bool get_block = false,
	uint64_t log_offset = 0);

void 
edce_persist_async(
	EdceManagementStructures& management_structures,
	const uint64_t current_block_number,
	BlockDataPersistenceMeasurements& measurements);


void
edce_persist_async_phase2(
	EdceManagementStructures& management_structures,
	uint64_t current_block_number,
	BlockDataPersistenceMeasurements& measurements);

void
edce_persist_async_phase3(
	EdceManagementStructures& management_structures,
	uint64_t current_block_number,
	BlockDataPersistenceMeasurements& measurements);

struct EdceAsyncPersisterPhase3 : public AsyncWorker {
	using AsyncWorker::mtx;
	using AsyncWorker::cv;

	EdceManagementStructures& management_structures;
	BlockDataPersistenceMeasurements* latest_measurements = nullptr;
	std::optional<uint64_t> current_block_number = std::nullopt;
	
	bool exists_work_to_do() override final {
		return latest_measurements != nullptr;
	}


	void run() {
		while(true) {
			std::unique_lock lock(mtx);
			if ((!done_flag) && (!exists_work_to_do())) {
				cv.wait(lock, [this] () { return done_flag || exists_work_to_do();});
			}
			if (done_flag) return;
			if (latest_measurements == nullptr) {
				throw std::runtime_error("invalid call to async_persist_phase3!");
			}
			edce_persist_async_phase3(management_structures, *current_block_number, *latest_measurements);
			latest_measurements = nullptr;
			current_block_number = std::nullopt;
			cv.notify_all();
		}
	}

	EdceAsyncPersisterPhase3(EdceManagementStructures& management_structures)
		: AsyncWorker()
		, management_structures(management_structures) {
			start_async_thread([this] {run();});
		}

	void wait_for_async_persist_phase3() {
		wait_for_async_task();
	}

	void do_async_persist_phase3(uint64_t current_block_number_caller, BlockDataPersistenceMeasurements* measurements) {
		wait_for_async_persist_phase3();

		std::lock_guard lock(mtx);
		latest_measurements = measurements;
		current_block_number = current_block_number_caller;
		cv.notify_one();
	}

	~EdceAsyncPersisterPhase3() {
		wait_for_async_persist_phase3();
		end_async_thread();
	}
};


struct EdceAsyncPersisterPhase2 : public AsyncWorker {
	using AsyncWorker::mtx;
	using AsyncWorker::cv;

	EdceManagementStructures& management_structures;
	BlockDataPersistenceMeasurements* latest_measurements = nullptr;
	std::optional<uint64_t> current_block_number = std::nullopt;


	EdceAsyncPersisterPhase3 phase3_persist;

	bool exists_work_to_do() override final {
		return latest_measurements != nullptr;
	}

	void run() {
		while(true) {
			std::unique_lock lock(mtx);
			if ((!done_flag) && (!exists_work_to_do())) {
				cv.wait(lock, [this] () { return done_flag || exists_work_to_do();});
			}
			if (done_flag) return;
			if (latest_measurements == nullptr) {
				throw std::runtime_error("invalid call to async_persist_phase2!");
			}

			edce_persist_async_phase2(management_structures, *current_block_number, *latest_measurements);
			
			phase3_persist.do_async_persist_phase3(*current_block_number, latest_measurements);

			latest_measurements = nullptr;
			current_block_number = std::nullopt;
			cv.notify_all();
		}
	}

	EdceAsyncPersisterPhase2(EdceManagementStructures& management_structures)
		: AsyncWorker()
		, management_structures(management_structures)
		, phase3_persist(management_structures)
       		, syncer(management_structures)	{
			start_async_thread([this] {run();});
		}

	void wait_for_async_persist_phase2() {
		wait_for_async_task();
	}

	void do_async_persist_phase2(uint64_t current_block_number_caller, BlockDataPersistenceMeasurements* measurements) {
		wait_for_async_persist_phase2();

		std::lock_guard lock(mtx);
		latest_measurements = measurements;
		current_block_number = current_block_number_caller;
		cv.notify_one();
	}

	~EdceAsyncPersisterPhase2() {
		syncer.stop();
		wait_for_async_persist_phase2();
		end_async_thread();
	}
};

struct EdceAsyncPersister {
	std::mutex mtx;
	std::condition_variable cv;

	bool done_flag = false;

	std::optional<uint64_t> block_number_to_persist;

	BlockDataPersistenceMeasurements* latest_measurements;

	EdceManagementStructures& management_structures;

	uint64_t highest_persisted_block;

	EdceAsyncPersisterPhase2 phase2_persist;

	EdceAsyncPersister(EdceManagementStructures& management_structures)
		: management_structures(management_structures)
		, phase2_persist(management_structures) {
			start_persist_thread();
		}


	void run() {
		while (true) {
			std::unique_lock lock(mtx);
			if ((!block_number_to_persist) && (!done_flag)) {
				cv.wait(lock, [this] { return ((bool) block_number_to_persist) || done_flag;});
			}
			if (done_flag) return;
			edce_persist_async(management_structures, *block_number_to_persist, *latest_measurements);

			phase2_persist.do_async_persist_phase2(*block_number_to_persist, latest_measurements);

			highest_persisted_block = *block_number_to_persist;

			block_number_to_persist = std::nullopt;
			latest_measurements = nullptr;
			cv.notify_one();
		}
	}

	uint64_t get_highest_persisted_block() {
		std::lock_guard lock(mtx);
		return highest_persisted_block;
	}

	void do_async_persist(const uint64_t persist_block_number, BlockDataPersistenceMeasurements& measurements) {
		
		auto timestamp = init_time_measurement();

		wait_for_async_persist_local();

		measurements.wait_for_persist_time = measure_time(timestamp);
		{
			std::lock_guard lock(mtx);
			if (block_number_to_persist) {
				throw std::runtime_error("can't start persist before last one finishes!");
			}
			block_number_to_persist = persist_block_number;
			latest_measurements = &measurements;
		}
		cv.notify_one();
	}

	void wait_for_async_persist_local() {
		std::unique_lock lock(mtx);
		if (!block_number_to_persist) return;
		cv.wait(lock, [this] { return !block_number_to_persist; });
	}

	void wait_for_async_persist() {
		//clears up all uses of measurements reference
		wait_for_async_persist_local();
		phase2_persist.wait_for_async_persist_phase2();
		phase2_persist.phase3_persist.wait_for_async_persist_phase3();
	}

	void start_persist_thread() {
		std::thread([this] () {
			run();
		}).detach();
	}

	void end_persist_thread() {
		std::lock_guard lock(mtx);
		done_flag = true;
		cv.notify_one();
	}

	~EdceAsyncPersister() {
		wait_for_async_persist();
		end_persist_thread();
	}
};

} /* speedex */
