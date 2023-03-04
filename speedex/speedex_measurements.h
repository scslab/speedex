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

#pragma once

#include "xdr/experiments.h"
#include "xdr/block.h"

#include <mutex>
#include <map>
#include <vector>


namespace speedex {

class SpeedexMeasurements {

	ExperimentParameters params;
	std::map<uint64_t, TaggedSingleBlockResults> measurements;
	mutable std::mutex mtx;

	std::vector<TaggedSingleBlockResults> uncled_measurements;


public:

	SpeedexMeasurements(const ExperimentParameters& params);

	void add_measurement(TaggedSingleBlockResults const& res);

	void insert_async_persistence_measurement(BlockDataPersistenceMeasurements const& data_persistence_measurements, uint64_t block_number);

	ExperimentResultsUnion get_measurements() const;
};

class PersistenceMeasurementLogCallback {

	SpeedexMeasurements& main_log;

	void finish() const;

public:

	PersistenceMeasurementLogCallback(SpeedexMeasurements& main_log, const uint64_t block_number)
		: main_log(main_log)
		, measurements()
		, block_number(block_number)
		{}

	BlockDataPersistenceMeasurements measurements;
	const uint64_t block_number;

	~PersistenceMeasurementLogCallback() {
		finish();
	}
};

} /* speedex */