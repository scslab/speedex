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