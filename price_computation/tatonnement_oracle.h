#pragma once

/*! \file tatonnement_oracle.h

Run Tatonnement in one interface.

*/

#include <cstdint>

#include <atomic>
#include <mutex>
#include <condition_variable>
#include <thread>


#include "orderbook/orderbook_manager.h"

#include "price_computation/demand_oracle.h"
#include "price_computation/lp_solver.h"

#include "speedex/approximation_parameters.h"

#include "utils/price.h"

#include "xdr/types.h"
#include "xdr/block.h"

namespace speedex {

typedef __int128 int128_t ;
typedef unsigned __int128 uint128_t;

//! Various control parameters guiding a Tatonnement run.
struct TatonnementControlParameters {

	constexpr static size_t NUM_DEMAND_WORKERS = 5;

	uint8_t step_radix = 55; // 50 //33
	uint64_t min_step = ((uint64_t)1)<<7;
	uint8_t step_adjust_radix = 5;
	uint8_t diff_reduction = 0;
	bool use_in_case_of_timeout = false;
	bool use_volume_relativizer = false;
	bool use_dynamic_relativizer = false;
	std::optional<ParallelDemandOracle<NUM_DEMAND_WORKERS>> oracle;

	//TatonnementControlParameters() : oracle(std::nullopt) {}

	TatonnementControlParameters(size_t num_assets, size_t num_work_units)
		: oracle(std::make_optional<ParallelDemandOracle<NUM_DEMAND_WORKERS>>(num_work_units, num_assets)) {}
};

//! The objective function guiding Tatonnement's step size.
struct MultifuncTatonnementObjective {
	double l2norm_sq = 0;
	double l8norm = 0;

	void eval(const uint128_t* supplies, const uint128_t* demands, const Price* prices, size_t num_assets) {
		double acc_l2 = 0;
		double acc_l8 = 0;
		for (size_t i = 0; i < num_assets; i++) {
			double diff = price::amount_to_double(supplies[i], price::PRICE_RADIX) - price::amount_to_double(demands[i], price::PRICE_RADIX);
			
			#ifndef USE_DEMAND_MULT_PRICES
				diff *= price::to_double(prices[i]);
			#endif
			
			double diff_sq = diff * diff;
			acc_l2 += diff_sq;
			acc_l8 += diff_sq * diff_sq * diff_sq * diff_sq;
		}
		l2norm_sq = acc_l2;
		l8norm = std::pow(acc_l8, 1.0/8.0);
	}

	bool
	is_better_than(const MultifuncTatonnementObjective& reference_objective) {
		return (l2norm_sq <= reference_objective.l2norm_sq * 1.01);
	//	return (l8norm <= reference_objective.l8norm * 1.1); 
	}
};


/*! 

Operates as an oracle for price computation via Tatonnement.

Owns all worker threads.

Call compute_prices_grid_search() to activate query threads.
Timeout can be run by launching a timeout thread.  This thread
should be joined before proceeding to a future block.
So too should wait_for_all_tatonnement_threads() be called 
before modifying the orderbooks.
*/

class TatonnementOracle {
	OrderbookManager& work_unit_manager;
	LPSolver& solver;
	size_t num_assets;
	ApproximationParameters active_approx_params;

	//! Run Tatonnement with multiple control param settings in these threads.
	std::vector<std::thread> worker_threads;

	std::atomic_bool done_tatonnement_flag = true;

	std::atomic_bool timeout_happened = false;
	
	uint16_t num_active_threads = 0;
	bool results_ready = false;

	bool kill_threads_flag = false;

	std::mutex mtx;

	std::condition_variable start_cv, finished_cv;

	Price* internal_shared_price_workspace;
	uint16_t* volume_relativizers;

	TatonnementMeasurements internal_measurements;

	double current_best_utility_ratio = -1;
	bool found_success = false;

	constexpr static size_t LP_CHECK_FREQ = 1000;

	static_assert(LP_CHECK_FREQ >= 2,
		"too small, can't check lp on round 0 (trial_prices unset)");

/*
	long double get_objective(
		const uint128_t* supplies,
		const uint128_t* demands,
		const Price* prices,
		const ObjectiveFunctionInputs& inputs);
*/

	//! Check whether a set of prices clears the market
	static bool check_clearing(
		const uint128_t* demands,
		const uint128_t* supplies,
		const uint8_t tax_rate,
		const uint16_t num_assets);

	int normalize_prices(
		Price* prices_workspace);

	bool set_trial_prices(
		const Price* old_prices,
		Price* new_prices, 
		uint64_t step, 
		const TatonnementControlParameters& control_params, 
		uint128_t* demands, 
		uint128_t* supplies,
		uint16_t* relativizers);

	void clear_supply_demand_workspaces(uint128_t* supplies, uint128_t* demands);
	//void get_supply_demand(
	//	Price* active_prices,
	//	uint128_t* supplies, 
	//	uint128_t* demands, 
	//	std::vector<Orderbook>& work_units,
	//	const uint8_t smooth_mult);

	//! Create Tatonnement threads.
	void start_tatonnement_threads();
	//! Signal tatonnement threads to shut down.  Joins these threads.
	void end_tatonnement_threads();

	//! Runs a tatonnement query thread.
	void run_tatonnement_thread(TatonnementControlParameters* control_params); //owns the input control_params

	//! Signals tatonnement threads to start running queries.
	void start_tatonnement_query();
	//! Wait for tatonnement threads to finish.
	void finish_tatonnement_query();

	//! Run one Tatonnement query with a given set of control params.
	//! return true if this thread is the first to find successful equilibrium
	bool grid_search_tatonnement_query(
		TatonnementControlParameters& control_params, 
		Price* prices_workspace, 
		std::unique_ptr<LPInstance>& lp_instance);
	bool better_grid_search_tatonnement_query(
		TatonnementControlParameters& control_params, 
		Price* prices_workspace, 
		std::unique_ptr<LPInstance>& lp_instance);
	
public:
	TatonnementOracle(
		OrderbookManager& work_unit_manager,
		LPSolver& solver)
	: work_unit_manager(work_unit_manager)
	, solver(solver)
	, num_assets(work_unit_manager.get_num_assets())

	{
		internal_shared_price_workspace = new Price[num_assets];
		volume_relativizers = new uint16_t[num_assets];
		start_tatonnement_threads();
	}

	~TatonnementOracle() {
		end_tatonnement_threads();
		delete[] internal_shared_price_workspace;
		delete[] volume_relativizers;
	}

	/*! Run Tatonnement.

	v_relatizers is an optional pointer to a set of volume normalization
	constants.
	*/
	TatonnementMeasurements
	compute_prices_grid_search(
		Price* prices_workspace, 
		const ApproximationParameters approx_params, 
		const uint16_t* v_relativizers = nullptr);
	
	//! Wait for all running tatonnement query threads to finish their queries,
	//! typically by waiting for them to read a timeout signal or a signal
	//! that some other thread finished a query first.
	//! Important that tatonnement threads are not running while 
	//! orderbooks are modified.
	void wait_for_all_tatonnement_threads();
	
	//! return true iff timeout causes tatonnement end.
	bool signal_grid_search_timeout();

	/*! Start a timeout thread.

	Said thread sleeps for a specified time, then signals a Tatonnement
	timeout flag.  Tatonnement queries periodically check this flag and return
	if they find it set.  Call wait_for_all_tatonnement_threads() to ensure
	all queries have stopped (concurrent modification of orderbooks is UB).

	Cancel this thread by setting cancel_timeout_flag.  If this thread
	is the cause of compute_prices_grid_search() returning,
	then timeout_happend_flag will be set.
	*/
	std::thread 
	launch_timeout_thread(
		uint32_t num_milliseconds, 
		std::atomic<bool>& timeout_happened_flag, 
		std::atomic<bool>& cancel_timeout_flag);

	//TatonnementMeasurements
	//compute_prices(Price* prices_workspace, const ApproximationParameters approx_params);

};
}
