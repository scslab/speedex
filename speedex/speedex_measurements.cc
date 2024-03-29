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

#include "speedex/speedex_measurements.h"

#include <cinttypes>

namespace speedex {

SpeedexMeasurements::SpeedexMeasurements(const ExperimentParameters& params)
	: params(params)
	, measurements()
	, mtx()
	, uncled_measurements()
	{
	}

void SpeedexMeasurements::add_measurement(TaggedSingleBlockResults const& res)
{
	std::lock_guard lock(mtx);

	auto iter = measurements.find(res.blockNumber);

	if (iter == measurements.end()) {
		measurements.emplace(res.blockNumber, res);
		return;
	}

	std::printf("uncling measurement block %" PRIu64 ".  Is this intentional?\n", res.blockNumber);
	uncled_measurements.push_back(iter->second);
	iter->second = res;
}


void 
SpeedexMeasurements::insert_async_persistence_measurement(
	BlockDataPersistenceMeasurements const& data_persistence_measurements, uint64_t block_number)
{
	std::lock_guard lock(mtx);

	auto iter = measurements.find(block_number);

	if (iter == measurements.end()) {

		throw std::runtime_error("can't add async persist measurements for nonexistent block!");
	}

	auto get_measurements = [&] () -> BlockDataPersistenceMeasurements& {
		if (iter -> second.results.type() == NodeType::BLOCK_PRODUCER) {
			return iter -> second.results.productionResults().data_persistence_measurements;
		} else {
			return iter -> second.results.validationResults().data_persistence_measurements;
		}
	};

	auto& db_measurements = get_measurements();

	/*float header_write_time; -- sync
	float account_db_checkpoint_time; -- sync
	float account_db_checkpoint_finish_time; -- async	
	float offer_checkpoint_time; -- async
	float account_log_write_time; -- sync
	float block_hash_map_checkpoint_time; -- async
	float wait_for_persist_time; -- async
	float account_db_checkpoint_sync_time; -- async
	float total_critical_persist_time; -- sync
	float async_persist_wait_time; -- sync
	*/

	db_measurements.account_db_checkpoint_finish_time = data_persistence_measurements.account_db_checkpoint_finish_time;
	db_measurements.account_db_checkpoint_sync_time = data_persistence_measurements.account_db_checkpoint_sync_time;
	db_measurements.offer_checkpoint_time = data_persistence_measurements.offer_checkpoint_time;
	db_measurements.block_hash_map_checkpoint_time = data_persistence_measurements.block_hash_map_checkpoint_time;
	db_measurements.wait_for_persist_time = data_persistence_measurements.wait_for_persist_time;
}

ExperimentResultsUnion
SpeedexMeasurements::get_measurements() const
{
	std::lock_guard lock(mtx);
	ExperimentResultsUnion out;
	for (auto [_, val] : measurements)
	{
		out.block_results.push_back(val);
	}
	out.params = params;

	return out;
}



void 
PersistenceMeasurementLogCallback::finish() const {
	main_log.insert_async_persistence_measurement(measurements, block_number);
}



} /* speedex */
