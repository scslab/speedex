#pragma once

#include "speedex/speedex_options.h"

#include "xdr/experiments.h"

#include <cstdint>

namespace speedex {

struct SimulatedProducerNode {
	ExperimentParameters params;
	std::string experiment_data_root;
	std::string results_output_root;
	SpeedexOptions& options;
	const size_t num_threads;

	void run_experiment();
};

} /* speedex */
