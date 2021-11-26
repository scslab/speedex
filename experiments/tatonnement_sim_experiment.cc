#include "experiments/tatonnement_sim_experiment.h"

namespace speedex {

std::optional<TatonnementMeasurements>
TatonnementSimExperiment::run_current_trial(std::unique_ptr<OrderbookManager>& manager_ptr, std::vector<Price> prices) {

	if (prices.size() == 0) {
		prices.resize(num_assets, price::from_double(1));
	}

	std::atomic<bool> cancel_timeout_thread = false;
	std::atomic<bool> timeout_flag = false;

	TatonnementManagementStructures tatonnement(*manager_ptr);

	std::thread th = tatonnement.oracle.launch_timeout_thread(5000, cancel_timeout_thread, timeout_flag);

	auto res = tatonnement.oracle.compute_prices_grid_search(prices.data(), current_approx_params);

	cancel_timeout_thread = true;

	th.join();

	auto lp_results = tatonnement.lp_solver.solve(prices.data(), current_approx_params, !timeout_flag);

	if (!timeout_flag) {
		std::printf("time per thread (micros): %lf\n", res.runtime * 1'000'000.0 / (res.num_rounds * 1.0));
	}

	auto feasible_first = manager_ptr -> get_max_feasible_smooth_mult(lp_results, prices.data());
	std::printf("feasible smooth mult:%u\n", feasible_first);
	res.achieved_smooth_mult = feasible_first;
	res.achieved_fee_rate = lp_results.tax_rate;

/*	uint16_t volumes[num_assets];

	std::printf("finished lp solving\n");

	WorkUnitManagerUtils::get_relative_volumes(lp_results, prices.data(), num_assets, volumes);

//	volumes[0] = 1;

	for (size_t i = 0; i < num_assets; i++) {
		std::printf("%lu %u\n", i, volumes[i]);
	}
	
	tatonnement.oracle.wait_for_all_tatonnement_threads();

	//rerun with normalizers
	cancel_timeout_thread = false;
	timeout_flag = false;
	th = std::thread(
		[this, &cancel_timeout_thread, &timeout_flag] () {
			timeout_thread(5, cancel_timeout_thread, timeout_flag);
		});

	init_price_vec(prices.data(), num_assets);

	auto res2 = tatonnement.oracle.compute_prices_grid_search(prices.data(), current_approx_params, volumes);

	cancel_timeout_thread = true;
	th.join();

	std::printf("second timeout flag: %u\n", timeout_flag);
	auto lp_res_2 = tatonnement.lp_solver.solve(prices.data(), current_approx_params, !timeout_flag);

	auto feasible_second = management_structures.work_unit_manager.get_max_feasible_smooth_mult(lp_res_2, prices.data());

	std::printf("feasible smooth mult 2: %u\n", feasible_second);

	*/
	tatonnement.oracle.wait_for_all_tatonnement_threads();

	if (!timeout_flag) {
		return res;
	} else {
		std::printf("Trial finished via timeout, not success\n");
	}
	return std::nullopt;
}

void 
TatonnementSimExperiment::run_experiment(
	uint8_t smooth_mult, 
	uint8_t tax_rate, 
	const std::vector<size_t>& num_txs_to_try, 
	const std::vector<TatonnementExperimentData>& trials,
	std::vector<Price> prices) {		

	PriceComputationExperiment results;
	results.experiments.resize(num_txs_to_try.size());

	size_t num_trials = trials.size();

	for (size_t i = 0; i < num_txs_to_try.size(); i++) {
		results.experiments[i].num_assets = num_assets;
		results.experiments[i].tax_rate = tax_rate;
		results.experiments[i].smooth_mult = smooth_mult;
		results.experiments[i].num_txs = num_txs_to_try[i];
		results.experiments[i].num_trials = num_trials;
	}

	for (const auto& data : trials) {
		if (data.num_assets != num_assets) {
			throw std::runtime_error("mismatch in #assets between data and config");
		}
		for (size_t i = 0; i < num_txs_to_try.size(); i++) {
			auto num_txs = num_txs_to_try[i];
			std::printf("running trial with %lu txs\n", num_txs);
			if (data.offers.size() < num_txs) {
				throw std::runtime_error("not enough txs!");
			}

			auto manager_ptr = load_experiment_data(data, num_txs);
			current_approx_params.tax_rate = tax_rate;
			current_approx_params.smooth_mult = smooth_mult;
			
			auto current_results = run_current_trial(manager_ptr, prices);

			if (current_results) {
				results.experiments[i].results.push_back(*current_results);
			}
		}
	}

	auto filename = get_experiment_filename(smooth_mult, tax_rate);
	if (save_xdr_to_file(results, filename.c_str())) {
		throw std::runtime_error("couldn't save results file to disk!");
	}
}

} /* speedex */

