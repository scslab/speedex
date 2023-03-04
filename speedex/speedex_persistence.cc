/**
 * SPEEDEX: A Scalable, Parallelizable, and Economically Efficient Decentralized Exchange
 * Copyright (C) 2023 Geoffrey Ramseyer

 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "speedex/speedex_persistence.h"

#include "speedex/speedex_management_structures.h"

#include "utils/header_persistence.h"

#include "utils/debug_macros.h"

#include <utils/time.h>

namespace speedex {

void write_tx_data(SignedTransactionList& list_out, SignedTransactionList& list_in)
{
	list_out = std::move(list_in);
}

void write_tx_data(SignedTransactionList& list_out, AccountModificationBlock& block_in)
{
	for (auto& log : block_in)
	{
		for (auto& tx : log.new_transactions_self) {
			list_out.emplace_back(std::move(tx));
		}
	}
}

std::unique_ptr<SignedTransactionList>
persist_critical_round_data(
	SpeedexManagementStructures& management_structures,
	const HashedBlock& header,
	BlockDataPersistenceMeasurements& measurements,
	bool get_block,
	bool write_block,
	uint64_t log_offset) {

	auto timestamp = utils::init_time_measurement();

	if (write_block)
	{
		save_header(header);
	}

	measurements.header_write_time = utils::measure_time(timestamp);

	uint64_t current_block_number = header.block.blockNumber;

	auto block_out = management_structures
		.account_modification_log
		.persist_block(current_block_number + log_offset, get_block, write_block);

	BLOCK_INFO("done writing account log");
	measurements.account_log_write_time = utils::measure_time(timestamp);

	management_structures.db.add_persistence_thunk(
		current_block_number, management_structures.account_modification_log);

	measurements.account_db_checkpoint_time = utils::measure_time(timestamp);

	management_structures.account_modification_log.detached_clear();
	BLOCK_INFO("done persist critical round data");

	if (block_out)
	{
		auto list_out = std::make_unique<SignedTransactionList>();
		write_tx_data(*list_out, *block_out);
		return list_out;
	}
	//offer db thunks, header hash map already updated
	return nullptr;
}

void persist_async_phase1(
	SpeedexManagementStructures& management_structures,
	const uint64_t current_block_number,
	BlockDataPersistenceMeasurements& measurements) {

	BLOCK_INFO("starting async persistence");
	auto timestamp = utils::init_time_measurement();

	management_structures.db.commit_persistence_thunks(current_block_number);
	BLOCK_INFO("done async db persistence\n");
	measurements.account_db_checkpoint_finish_time = utils::measure_time(timestamp);
}

void 
AsyncPersister::run() {
	while (true) {
		std::unique_lock lock(mtx);
		if ((!exists_work_to_do()) && (!done_flag)) {
			cv.wait(lock, [this] { return done_flag || exists_work_to_do();});
		}
		if (done_flag) return;
		persist_async_phase1(
			management_structures, 
			persistence_callback -> block_number, 
			persistence_callback -> measurements);

		highest_persisted_block = persistence_callback -> block_number;

		phase2_persist.do_async_persist_phase2(
			std::move(persistence_callback));

		cv.notify_all();
	}
}

void 
AsyncPersister::do_async_persist(
	std::unique_ptr<PersistenceMeasurementLogCallback> callback)
{
	auto timestamp = utils::init_time_measurement();

	wait_for_async_task();

	callback -> measurements.wait_for_persist_time = utils::measure_time(timestamp);
	{
		std::lock_guard lock(mtx);
		if (persistence_callback) {
			throw std::runtime_error(
				"can't start persist before last one finishes!");
		}
		persistence_callback = std::move(callback);
	}
	cv.notify_all();
}


void
persist_async_phase2(
	SpeedexManagementStructures& management_structures,
	uint64_t current_block_number,
	BlockDataPersistenceMeasurements& measurements) {

	BLOCK_INFO("starting async persistence phase 2");
	auto timestamp = utils::init_time_measurement();

	management_structures.db.force_sync();
	BLOCK_INFO("done async db sync\n");
	measurements.account_db_checkpoint_sync_time = utils::measure_time(timestamp);
}

void 
AsyncPersisterPhase2::run() {
	while(true) {
		std::unique_lock lock(mtx);
		if ((!done_flag) && (!exists_work_to_do())) {
			cv.wait(lock, [this] () { return done_flag || exists_work_to_do();});
		}
		if (done_flag) return;
		if (persistence_callback == nullptr) {
			throw std::runtime_error("invalid call to async_persist_phase2!");
		}

		persist_async_phase2(
			management_structures, 
			persistence_callback -> block_number, 
			persistence_callback -> measurements);
		
		phase3_persist.do_async_persist_phase3(
			std::move(persistence_callback));

		cv.notify_all();
	}
}


void 
AsyncPersisterPhase2::do_async_persist_phase2(
	std::unique_ptr<PersistenceMeasurementLogCallback> callback)
{
	wait_for_async_task();

	std::lock_guard lock(mtx);
	persistence_callback = std::move(callback);

	cv.notify_all();
}


void
persist_async_phase3(
	SpeedexManagementStructures& management_structures,
	uint64_t current_block_number,
	BlockDataPersistenceMeasurements& measurements) {

	BLOCK_INFO("starting async persistence phase 3");
	auto timestamp = utils::init_time_measurement();

	management_structures.orderbook_manager.persist_lmdb(current_block_number);
	BLOCK_INFO("done async offer persistence\n");
	measurements.offer_checkpoint_time = utils::measure_time(timestamp);

	management_structures.block_header_hash_map.persist_lmdb(
		current_block_number);

	measurements.block_hash_map_checkpoint_time = utils::measure_time(timestamp);
	BLOCK_INFO("done async total persistence\n");
}

void 
AsyncPersisterPhase3::run() {
	while(true) {
		std::unique_lock lock(mtx);
		if ((!done_flag) && (!exists_work_to_do())) {
			cv.wait(lock, [this] () { return done_flag || exists_work_to_do();});
		}
		if (done_flag) return;
		if (persistence_callback == nullptr) {
			throw std::runtime_error("invalid call to async_persist_phase3!");
		}
		persist_async_phase3(
			management_structures,
			persistence_callback -> block_number,
			persistence_callback -> measurements);
		
		persistence_callback = nullptr; // destructor calls finish on the callback, which logs measurements
		
		cv.notify_all();
	}
}

void 
AsyncPersisterPhase3::do_async_persist_phase3(
	std::unique_ptr<PersistenceMeasurementLogCallback> callback)
{
	wait_for_async_task();

	std::lock_guard lock(mtx);
	persistence_callback = std::move(callback);
	cv.notify_all();
}

void
persist_after_loading(
	SpeedexManagementStructures& management_structures,
	uint64_t current_block_number)
{
	BlockDataPersistenceMeasurements measurements;

	persist_async_phase1(management_structures, current_block_number, measurements);
	persist_async_phase2(management_structures, current_block_number, measurements);
	persist_async_phase3(management_structures, current_block_number, measurements);
}

} /* speedex */