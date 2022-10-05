#pragma once

#include <cstdint>
#include <cstddef>

#include <thread>
#include <atomic>

#include "experiments/tatonnement_sim_setup.h"

#include "speedex/approximation_parameters.h"
#include "speedex/speedex_management_structures.h"

#include "utils/price.h"
#include "utils/save_load_xdr.h"

#include "xdr/experiments.h"

#include <utils/mkdir.h>

namespace speedex {

class TatonnementSimExperiment {

	size_t num_assets;

	std::string data_root; // includes trailing /, or is empty

	std::optional<TatonnementMeasurements>
	run_current_trial(std::unique_ptr<OrderbookManager>& manager_ptr, std::vector<Price> prices);

	ApproximationParameters current_approx_params;

public:

	std::string get_experiment_filename(uint8_t smooth_mult, uint8_t tax_rate) {
		return data_root + std::to_string(tax_rate) + "_" + std::to_string(smooth_mult) + "_results";
	}

	bool check_preexists(uint8_t smooth_mult, uint8_t tax_rate) {
		auto name = get_experiment_filename(smooth_mult, tax_rate);

		if (FILE *file = fopen(name.c_str(), "r")) {
	        fclose(file);
	        return true;
	    } else {
	        return false;
	    }   
	}

	void save_file(uint8_t smooth_mult, uint8_t tax_rate, PriceComputationExperiment& experiment);

	TatonnementSimExperiment(std::string data_root, uint64_t num_assets)
		: num_assets(num_assets)
		, data_root(data_root) {
			utils::mkdir_safe(data_root.c_str());
		}

	void run_experiment(
		uint8_t smooth_mult, 
		uint8_t tax_rate, 
		const std::vector<size_t>& num_txs_to_try, 
		const std::vector<TatonnementExperimentData>& trials,
		std::vector<Price> prices = {});
};

} /* speedex */
