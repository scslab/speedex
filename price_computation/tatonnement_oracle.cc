#include "price_computation/tatonnement_oracle.h"

#include "utils/debug_macros.h"
#include "utils/price.h"

#include <cmath>

namespace speedex {

uint64_t increment_step(const uint64_t step, const uint16_t step_up, const uint16_t step_adjust_radix) {
	uint64_t increment = (step * step_up) >> step_adjust_radix;
		
	if (increment < step) {
		//overflow, if step size is this big we don't care
		return step;
	}

	
	return increment;
}

uint64_t decrement_step(const uint64_t step, const uint16_t step_down, const uint16_t step_adjust_radix) {
	//std::printf("step %lu step_down %lu step * step_down %lu step*step_down*radix %lu\n", step, step_down, step * step_down, (step * step_down) >> step_adjust_radix);
	return (step * step_down) >> step_adjust_radix;
}

#if 0
long double 
TatonnementOracle::get_objective(const uint128_t* supplies, const uint128_t* demands, const Price* prices, const ObjectiveFunctionInputs& inputs) {
	
	/*long double out_val = 0;
	for (size_t i = 0; i < num_assets; i++) {
		double sell_price_d = PriceUtils::to_double(prices[i]);
		for (size_t j = 0; j < num_assets; j++) {
			if (i != j) {
				double buy_price_d = PriceUtils::to_double(prices[j]);
				out_val += inputs.values[i][j] * sell_price_d * std::log2f(sell_price_d / buy_price_d);
			}
		}
	}
	return std::abs(out_val);
	*/
	long double acc = 0;
	for (size_t i = 0; i < num_assets; i++) {
		long double diff = price::amount_to_double(supplies[i], price::PRICE_RADIX) - price::amount_to_double(demands[i], price::PRICE_RADIX);
		diff *= price::to_double(prices[i]);
		acc += diff * diff;
		//acc += diff * diff * diff * diff * diff * diff * diff * diff;//std::abs(diff);// * diff;
	}
	return std::pow(acc, 0.5);
	//return std::pow(acc, 1.0/8.0);

	}
#endif

bool 
TatonnementOracle::check_clearing(
	const uint128_t* demands,
	const uint128_t* supplies,
	const uint8_t tax_rate,
	const uint16_t num_assets) {
	for (size_t i = 0; i < num_assets; i++) {
		if (demands[i] - (demands[i] >> tax_rate) > supplies[i]) {
			return false;
		}
	}
	return true;
}



Price get_trial_price(const uint128_t& demand, const uint128_t& supply, const Price& old_price, const uint64_t& step, const uint16_t volume_relativizer, const TatonnementControlParameters& control_params) {

	uint16_t applied_relativizer = volume_relativizer;//control_params.use_volume_relativizer ? volume_relativizer : 1;

	if (demand > supply) {
		uint128_t diff = demand - supply; // 64 + 24 bits

		uint128_t p_times_step = ((uint128_t)step) * ((uint128_t) old_price);

		#ifdef USE_DEMAND_MULT_PRICES
		uint128_t p_times_diff = (applied_relativizer) * diff;
		#else
		uint128_t p_times_diff = ((uint128_t) old_price * applied_relativizer) * diff;
		#endif

		Price delta = price::safe_multiply_and_drop_lowbits(p_times_step, p_times_diff, control_params.step_radix + price::PRICE_RADIX);

		return price::impose_price_bounds(old_price + delta);
	} else {
		uint128_t diff = supply - demand;

		uint128_t p_times_step = ((uint128_t)step) * ((uint128_t) old_price);
		
		#ifdef USE_DEMAND_MULT_PRICES
		uint128_t p_times_diff = (applied_relativizer) * diff;
		#else
		uint128_t p_times_diff = ((uint128_t) old_price * applied_relativizer) * diff;
		#endif
		
		Price delta = price::safe_multiply_and_drop_lowbits(p_times_step, p_times_diff, control_params.step_radix + price::PRICE_RADIX);

		if (delta >= old_price) {
			return 1;
		}
		return old_price - delta;
	}
}

//To not overflow, need step_radix < 128-price_bits=80
bool TatonnementOracle::set_trial_prices(
	const Price* old_prices,
	Price* new_prices, 
	uint64_t step, 
	const TatonnementControlParameters& control_params, 
	uint128_t* demands, 
	uint128_t* supplies,
	uint16_t* relativizers) {

	//uint8_t step_radix = control_params.step_radix;
	bool changed = false;
	for (size_t i = 0; i < num_assets; i++) {
		new_prices[i] = get_trial_price(demands[i], supplies[i], old_prices[i], step, relativizers[i], control_params);
		if (new_prices[i] != old_prices[i]) {
			changed = true;
		}	
	}
	return changed;
}


#if 0
		if (demands[i] > supplies[i]) {
			uint128_t diff = (demands[i] - supplies[i]);// >> (control_params.diff_reduction);

			// price * (1+alpha) = price + price * alpha
			uint128_t adjustment = (diff * ((uint128_t)step)) * ((uint128_t)old_prices[i]);
			adjustment >>= (step_radix + PriceUtils::PRICE_RADIX);
			if (!adjustment) {
				adjustment = 1;
			}
			uint128_t trial = old_prices[i] + adjustment;
			new_prices[i] = PriceUtils::impose_bounds(trial);

			//uint128_t one = ((uint128_t)1)<<(step_radix + PriceUtils::PRICE_RADIX);



			//uint128_t multiplier = ((diff * step) >> control_params.diff_reduction) + one;
			//uint128_t trial = (((uint128_t)old_prices[i]) * multiplier)>>(step_radix + PriceUtils::PRICE_RADIX);
			//new_prices[i] = PriceUtils::impose_bounds(trial);
			/*std::printf("diff=%f mult=%f original=%f output=%f\n", 
				PriceUtils::amount_to_double(diff), 
				PriceUtils::amount_to_double(multiplier) / PriceUtils::amount_to_double(one),
				PriceUtils::to_double(old_prices[i]), 
				PriceUtils::to_double(new_prices[i]));*/

		} else {
			uint128_t diff = (supplies[i] - demands[i]);// >> (control_params.diff_reduction);
		

			uint128_t adjustment = (diff * ((uint128_t) step)) * ((uint128_t) old_prices[i]);
			adjustment >>= (step_radix + PriceUtils::PRICE_RADIX);
			if (!adjustment) {
				adjustment = 1;
			}
			//uint128_t one = (((uint128_t) 1)<<(step_radix + PriceUtils::PRICE_RADIX));

			//uint128_t temp = (diff * step) >> control_params.diff_reduction;
			//uint128_t multiplier = one - temp;
			//if (temp > one) {
			//	multiplier = one - (one >> 5);
			//}

			//uint128_t trial = (((uint128_t) old_prices[i]) * multiplier)>>(step_radix + PriceUtils::PRICE_RADIX);
			
			uint128_t trial = old_prices[i] - adjustment;
			if (adjustment > old_prices[i]) {
//				std::printf("yoop\n");
				trial = old_prices[i] >> 1;
			}

			new_prices[i] = PriceUtils::impose_bounds(trial);

			/*std::printf("diff=%f mult=%f original=%f output=%f\n", 
				-PriceUtils::amount_to_double(diff), 
				PriceUtils::amount_to_double(multiplier) / PriceUtils::amount_to_double(one),
				PriceUtils::to_double(old_prices[i]), 
				PriceUtils::to_double(new_prices[i]));		*/
		}
		//std::printf("%d %lu %f\n", i, new_prices[i], PriceUtils::to_double(new_prices[i]));
	}
}

#endif

/*
void TatonnementOracle::get_supply_demand(
	Price* active_prices,
	uint128_t* supplies, 
	uint128_t* demands, 
	std::vector<Orderbook>& work_units,
	const uint8_t smooth_mult) {

	auto num_work_units = work_units.size();
	for (size_t i = 0; i < num_work_units; i++) {
		work_units[i].calculate_demands_and_supplies(active_prices, demands, supplies, smooth_mult);//, function_inputs);
	}

//	for (int i = 0; i < num_assets; i++) {
//		std::printf("%d demand=%f supply=%f\n", i, PriceUtils::amount_to_double(demands[i]), PriceUtils::amount_to_double(supplies[i]));
//	}
}*/

void TatonnementOracle::clear_supply_demand_workspaces(uint128_t* supplies, uint128_t* demands) {
	for (size_t i = 0; i < num_assets; i++) {
		supplies[i] = 0;
		demands[i] = 0;
	}
}

/*
TatonnementMeasurements
TatonnementOracle::compute_prices(Price* prices_workspace, const ApproximationParameters approx_params) {

	auto timestamp = init_time_measurement();

	//update_approximation_parameters();

	Price* trial_prices = new Price[num_assets];

	const uint8_t step_radix = 60; // 50 //33

	TatonnementControlParameters control_params(num_assets, WorkUnitManagerUtils::get_num_work_units_by_asset_count(num_assets));
	control_params.step_radix = step_radix;

	//const uint64_t max_step = ((uint64_t)1)<<33;
	const uint64_t min_step = ((uint64_t)1)<<7;

	uint64_t step = min_step;// 1/2^value

	const uint8_t step_adjust_radix = 5;

	TAT_INFO("step up:%f", (1.2 * ((double) (((uint16_t)1) << step_adjust_radix))));
	const uint16_t step_up = (uint16_t) (1.4 * ((double) (((uint16_t)1) << step_adjust_radix)));
	const uint16_t step_down = (uint16_t) (0.8 * ((double) (((uint16_t)1) << step_adjust_radix)));

	uint128_t* supplies_workspace = new uint128_t[num_assets];
	uint128_t* demands_workspace = new uint128_t[num_assets];

	uint128_t* supplies_search = new uint128_t[num_assets];
	uint128_t* demands_search = new uint128_t[num_assets];



	for (size_t i = 0; i < num_assets; i++) {
		TAT_INFO("Starting prices:%lu %f", prices_workspace[i], PriceUtils::to_double(prices_workspace[i]));
	}

	ObjectiveFunctionInputs function_inputs(num_assets);

	//std::lock_guard lock = work_unit_manager.get_lock();

	auto& work_units = work_unit_manager.get_work_units();
	//int num_work_units = work_units.size();

	clear_supply_demand_workspaces(supplies_search, demands_search);
	get_supply_demand(prices_workspace, supplies_search, demands_search, work_units, approx_params.smooth_mult, function_inputs);

	auto prev_objective = get_objective(supplies_search, demands_search, prices_workspace, function_inputs);

	size_t round_number = 0;

	bool clearing = false;

	while (true) {

		if (clearing) {

			TAT_INFO("CLEARING");

			TAT_INFO("ROUND %5d step_size %lf (%lu) objective %f", round_number, PriceUtils::amount_to_double(step, step_radix), step, prev_objective);
			TAT_INFO("Asset\tdemand\tsupply\tprice\ttax revenue\toversupply");
			for (size_t i = 0; i < num_assets; i++) {
				double demand = PriceUtils::amount_to_double(demands_search[i], PriceUtils::PRICE_RADIX);
				demand = demand - PriceUtils::amount_to_double(demands_search[i], PriceUtils::PRICE_RADIX + approx_params.tax_rate);
				double delta = demand - PriceUtils::amount_to_double(supplies_search[i], PriceUtils::PRICE_RADIX);
				TAT_INFO("%d\t%010.3f\t%010.3f\t%010.3f\t%010.3f\t%010.3f", 
					i, 
					PriceUtils::amount_to_double(demands_search[i],PriceUtils::PRICE_RADIX), 
					PriceUtils::amount_to_double(supplies_search[i],PriceUtils::PRICE_RADIX), 
					PriceUtils::to_double(prices_workspace[i]),
					PriceUtils::amount_to_double(demands_search[i], PriceUtils::PRICE_RADIX + approx_params.tax_rate),
					delta);
			}

			for (size_t i = 0; i < num_assets; i++) {
				prices_workspace[i] = trial_prices[i];
			}
			delete[] trial_prices;
			delete[] supplies_workspace;
			delete[] demands_workspace;
			delete[] supplies_search;
			delete[] demands_search;

			internal_measurements.num_rounds = round_number;
			internal_measurements.step_radix = step_radix;
			internal_measurements.runtime = measure_time(timestamp);
			return internal_measurements;
		}
		round_number++;


		set_trial_prices(prices_workspace, trial_prices, step, control_params, demands_search, supplies_search);

		clear_supply_demand_workspaces(supplies_workspace, demands_workspace);

		get_supply_demand(trial_prices, supplies_workspace, demands_workspace, work_units, approx_params.smooth_mult, function_inputs);

		clearing = check_clearing(demands_workspace, supplies_workspace, approx_params.tax_rate, num_assets);

		auto new_objective = get_objective(supplies_workspace, demands_workspace, prices_workspace, function_inputs);


		if (round_number % 100000 == 0) {
			TAT_INFO("ROUND %5d step_size %lf (%lu) objective %f", round_number, PriceUtils::amount_to_double(step, step_radix), step, prev_objective);
			//TAT_INFO("Asset\tdemand\tsupply\tprice\ttax revenue\toversupply");
			for (size_t i = 0; i < num_assets; i++) {
				double demand = PriceUtils::amount_to_double(demands_search[i], PriceUtils::PRICE_RADIX);
				demand = demand - PriceUtils::amount_to_double(demands_search[i], PriceUtils::PRICE_RADIX + approx_params.tax_rate);
				double delta = demand - PriceUtils::amount_to_double(supplies_search[i], PriceUtils::PRICE_RADIX);
				TAT_INFO("%d\t%010.3f\t%010.3f\t%010.3f\t%010.3f\t%010.3f\t%010.3f\t%010.3f", 
					i, 
					PriceUtils::amount_to_double(demands_search[i],PriceUtils::PRICE_RADIX), 
					PriceUtils::amount_to_double(supplies_search[i],PriceUtils::PRICE_RADIX), 
					PriceUtils::to_double(prices_workspace[i]),
					PriceUtils::amount_to_double(demands_workspace[i],PriceUtils::PRICE_RADIX), 
					PriceUtils::amount_to_double(supplies_workspace[i],PriceUtils::PRICE_RADIX), 
					PriceUtils::to_double(trial_prices[i])
					, delta);
			}
			TAT_INFO("tax_rate = %lu smooth_mult = %lu", approx_params.tax_rate, approx_params.smooth_mult);
			
		}


		//std::printf("new_objective:%f prev_objective:%f, result %d\n", new_objective, prev_objective, new_objective <= prev_objective * 2.001);
		if (new_objective <= prev_objective * 1.0001 || step < min_step || clearing) {
			if (step > min_step) {
		//		std::printf("%f %f\n", new_objective, prev_objective);
			}
			//std::printf("%d increment step, updating prices/supply/demand\n", round_number);
			for (size_t i = 0; i < num_assets; i++) {
				//std::printf("original %u new %u\n", prices_workspace[i], trial_prices[i]);
				prices_workspace[i] = trial_prices[i];
				supplies_search[i] = supplies_workspace[i];
				demands_search[i] = demands_workspace[i];
			}
			prev_objective = get_objective(supplies_workspace, demands_workspace, prices_workspace, function_inputs);
			step = increment_step(step, step_up, step_adjust_radix);
		} else {
			step = decrement_step(step, step_down, step_adjust_radix);
		}

	}
}*/

void TatonnementOracle::run_tatonnement_thread(TatonnementControlParameters* control_params_ptr_) {

	std::vector<Price> local_price_workspace;
	local_price_workspace.resize(num_assets);

	if (control_params_ptr_ == nullptr) {
		throw std::runtime_error("nonsense!");
	}

	std::unique_ptr<TatonnementControlParameters> control_params_ptr(control_params_ptr_);

	if (!control_params_ptr) {
		throw std::runtime_error("nonsense!!");
	}

	auto& control_params = *control_params_ptr;

	std::unique_ptr<LPInstance> instance = solver.make_instance();

	while(true) {
		std::unique_lock lock(mtx);
		start_cv.wait(lock, [this] {
			auto started = done_tatonnement_flag.load(std::memory_order_acquire);
			return (!started) || kill_threads_flag;
		});

		for(size_t i = 0; i < num_assets; i++) {
			local_price_workspace[i] = internal_shared_price_workspace[i];
		}

		if (kill_threads_flag) return;
		num_active_threads ++;
		lock.unlock();

		#ifdef USE_DEMAND_MULT_PRICES
			auto success = better_grid_search_tatonnement_query(control_params, local_price_workspace.data(), instance);
		#else 
			auto success = grid_search_tatonnement_query(control_params, local_price_workspace.data(), instance);
		#endif

		lock.lock();
		num_active_threads --;

		if (success && !found_success)
		{
			found_success = true;
			for(size_t i = 0; i < num_assets; i++) {
				internal_shared_price_workspace[i] = local_price_workspace[i];	
			}
			results_ready = true;
		}

		if (!found_success)
		{	
			auto clearing_params = solver.solve(local_price_workspace.data(), active_approx_params, false /* use_lower_bound */);
			auto [sat, lost] = work_unit_manager.satisfied_and_lost_utility(clearing_params, local_price_workspace.data());

			double my_utility_ratio = lost / sat;
			
			if ((current_best_utility_ratio < 0) || (my_utility_ratio < current_best_utility_ratio))
			{
				current_best_utility_ratio = lost/sat;
				for(size_t i = 0; i < num_assets; i++) {
					internal_shared_price_workspace[i] = local_price_workspace[i];	
				}
			}
		}

		if (num_active_threads == 0)
		{
			results_ready = true;
		}
/*
		if (success || (timeout_happened && control_params.use_in_case_of_timeout)) {

			if (success) {
				TAT_INFO("success with step_radix %lu diff_reduction %lu", control_params.step_radix, control_params.diff_reduction);
			}
			for(size_t i = 0; i < num_assets; i++) {
				internal_shared_price_workspace[i] = local_price_workspace[i];
			}
			results_ready = true;
		} */

		finished_cv.notify_all();
	}
}

void TatonnementOracle::start_tatonnement_threads() {
	size_t num_work_units = get_num_orderbooks_by_asset_count(num_assets);

	bool first = true;

	for (size_t i = 0; i < 3 ; i++) {

		auto params = new TatonnementControlParameters(num_assets, num_work_units);
		if (params == nullptr) {
			throw std::runtime_error("nonsense");
		}
		
		params->min_step = ((uint64_t)1) << 7; // 7
		params->step_adjust_radix = 5; // 5

		params->step_radix = 110 - 16*i;
		params->diff_reduction = 0;//5*(3-i);
		params->use_in_case_of_timeout = first; // only one thread should be set to true.
		first = false;

		params -> use_dynamic_relativizer = true;

		worker_threads.emplace_back(std::thread(
			[this] (TatonnementControlParameters* params) {
				run_tatonnement_thread(params);
			}, params));
	}
	for (size_t i = 0; i < 3; i++) {
		auto params = new TatonnementControlParameters(num_assets, num_work_units);

		params -> min_step = ((uint64_t)1)<<7;
		params->step_adjust_radix = 5;

		params -> step_radix = 110-16*i;
		params->diff_reduction = 0;
		params->use_in_case_of_timeout = false;

		params->use_volume_relativizer = true;
		params->use_dynamic_relativizer = true;

		worker_threads.emplace_back(std::thread(
			[this, params] {
				run_tatonnement_thread(params);
			}));
	}

}

void TatonnementOracle::end_tatonnement_threads() {
	{
		std::unique_lock lock(mtx);
		kill_threads_flag = true;
		done_tatonnement_flag = true;
		start_cv.notify_all();
	}
	for (size_t i = 0; i < worker_threads.size(); i++) {
		worker_threads[i].join();
	}
}

void TatonnementOracle::start_tatonnement_query() {
	std::unique_lock lock(mtx);
	done_tatonnement_flag = false;
	results_ready = false;
	timeout_happened = false;
	
	current_best_utility_ratio = -1;
	found_success = false;
	
	start_cv.notify_all();
}

void TatonnementOracle::wait_for_all_tatonnement_threads() {
	std::unique_lock lock(mtx);
	if (num_active_threads == 0) {
		return;
	}
	finished_cv.wait(lock,
		[this] () {
			return num_active_threads == 0;
		});
}

std::thread 
TatonnementOracle::launch_timeout_thread(uint32_t num_milliseconds, std::atomic<bool>& timeout_happened_flag, std::atomic<bool>& cancel_timeout_flag) {
	return std::thread([this, &timeout_happened_flag, num_milliseconds, &cancel_timeout_flag] () {

		for (size_t i = 0; i < 10; i ++) {
			std::this_thread::sleep_for(std::chrono::milliseconds(num_milliseconds/10));
			if (cancel_timeout_flag.load(std::memory_order_relaxed)) {
				return;
			}
		}
		timeout_happened_flag = signal_grid_search_timeout();
	});
}


void TatonnementOracle::finish_tatonnement_query() {
	std::unique_lock lock(mtx);

	if (results_ready) return;

	finished_cv.wait(lock,
		[this] () {
			//auto finished = done_tatonnement_flag.load(std::memory_order_acquire);
			//auto num_working = num_active_threads.load(std::memory_order_acquire);
			return results_ready;
		});
}

TatonnementMeasurements
TatonnementOracle::compute_prices_grid_search(Price* prices_workspace, const ApproximationParameters approx_params, const uint16_t* v_relativizers) {

	auto timestamp = init_time_measurement();

	//update_approximation_parameters();
	active_approx_params = approx_params;

	for (size_t i = 0; i < num_assets; i++) {
		internal_shared_price_workspace[i] = prices_workspace[i];
	}
	if (v_relativizers != nullptr) {
		for (size_t i = 0; i < num_assets; i++) {
			volume_relativizers[i] = v_relativizers[i];
		}
	} else {
		for (size_t i = 0; i < num_assets; i++) {
			volume_relativizers[i] = 1;
		}
	}

	start_tatonnement_query();
	finish_tatonnement_query();
	for (size_t i = 0; i < num_assets; i++) {
		prices_workspace[i] = internal_shared_price_workspace[i];
	}
	internal_measurements.runtime = measure_time(timestamp);

	return internal_measurements;
}

bool TatonnementOracle::signal_grid_search_timeout() {
	
	std::lock_guard lock(mtx);

	auto not_first = done_tatonnement_flag.exchange(true, std::memory_order_acq_rel);
	if (!not_first) {
		timeout_happened = true;
	}
	finished_cv.notify_all();
	return !not_first;
}

int
TatonnementOracle::normalize_prices(
	Price* prices_workspace) {

	Price p_min = prices_workspace[0];
	Price p_max = prices_workspace[0];
	for (size_t i = 1; i < num_assets; i++) {
		p_min = std::min(p_min, prices_workspace[i]);
		p_max = std::max(p_max, prices_workspace[i]);
	}

	int space_to_top = __builtin_clzll(p_max) - (64 - price::PRICE_BIT_LEN);

	int space_to_bot = 64 - __builtin_clzll(p_min);

	int abs_dif = space_to_top - space_to_bot;

	if (space_to_top > 10) {
		for (size_t i = 0; i < num_assets; i++) {
		       prices_workspace[i] <<= space_to_top / 2;
		}
		return space_to_top / 2;
	} else if (space_to_top < 3) {
		for (size_t i = 0; i < num_assets; i++) {
			prices_workspace[i] >>= 2;
		}
		return -2;
	}
	
	/* else {
		for (size_t i = 0; i < num_assets; i++) {
			prices_workspace[i] >>= space_to_bot / 2;
		}
		return -space_to_bot / 2;
	} */
	return 0;
}

void set_relativizers(
	TatonnementControlParameters const& control_params, 
	uint16_t* relativizers_out, 
	const uint16_t* volume_relativizers, 
	size_t num_assets, 
	const uint128_t* demands, 
	const uint128_t* supplies)
{
	uint128_t max_min_demand = 0;
	for (size_t i = 0; i < num_assets; i++) {
		max_min_demand = std::max(max_min_demand, std::min(demands[i], supplies[i]));
	}

	constexpr static float MAX_MUL = 1000;

	auto impose_max = [](float mul, uint16_t base) -> uint16_t {
		mul = std::min(mul, MAX_MUL);
		uint32_t b = mul * base;
		return (b > UINT16_MAX)? UINT16_MAX : b;
	};

	for (size_t i = 0; i < num_assets; i++) {
		uint128_t cur_min_demand = std::min(demands[i], supplies[i]);
		uint16_t base_vol_rel = control_params.use_volume_relativizer ? volume_relativizers[i] : 1;

		if (control_params.use_dynamic_relativizer) {
			if (cur_min_demand == 0) {
				relativizers_out[i] = impose_max(MAX_MUL, base_vol_rel);
			} else {
				relativizers_out[i] = impose_max(((float) max_min_demand) / ((float) cur_min_demand), base_vol_rel);
			}
		} else {
			relativizers_out[i] = base_vol_rel;
		}
	//	std::printf("mul %d is %u\n", i, relativizers_out[i]);
	}
}

bool
TatonnementOracle::better_grid_search_tatonnement_query(
	TatonnementControlParameters& control_params,
	Price* prices_workspace,
	std::unique_ptr<LPInstance>& lp_instance)
{

	#ifndef USE_DEMAND_MULT_PRICES
		throw std::runtime_error("oracle invalid");
	#endif

	Price* trial_prices = new Price[num_assets];

	const uint8_t step_radix = control_params.step_radix;

	const uint64_t min_step =  control_params.min_step;

	uint64_t step = min_step;// 1/2^value

	const uint8_t step_adjust_radix = control_params.step_adjust_radix;
	
	const uint16_t step_up = (uint16_t) (1.4 * ((double) (((uint16_t)1) << step_adjust_radix)));
	const uint16_t step_down = (uint16_t) (0.8 * ((double) (((uint16_t)1) << step_adjust_radix)));

	uint128_t* supplies_workspace = new uint128_t[num_assets];
	uint128_t* demands_workspace = new uint128_t[num_assets];

	uint128_t* supplies_search = new uint128_t[num_assets];
	uint128_t* demands_search = new uint128_t[num_assets];

	auto& work_units = work_unit_manager.get_orderbooks();

	clear_supply_demand_workspaces(supplies_search, demands_search);

	uint16_t* relativizers = new uint16_t[num_assets];

	for (size_t i = 0; i < num_assets; i++) {
		relativizers[i] = volume_relativizers[i];
	}

	auto& demand_oracle = *(control_params.oracle);
	demand_oracle.activate_oracle();

	demand_oracle.
		get_supply_demand(prices_workspace, supplies_search, demands_search, work_units, active_approx_params.smooth_mult);//, function_inputs);

	MultifuncTatonnementObjective prev_objective;
	prev_objective.eval(supplies_search, demands_search, prices_workspace, num_assets);

	int round_number = 0;

	bool clearing = false;

	int force_step_rounds = 0;

	while (true) {

		if (round_number % LP_CHECK_FREQ == LP_CHECK_FREQ - 1) {
			TAT_INFO_F(auto timestamp = init_time_measurement());
			auto solver_res = solver.check_feasibility(trial_prices, lp_instance, active_approx_params);
			if (solver_res) {
				clearing = true;
				TAT_INFO("clearing because lp solver found valid solution: lp time %lf", measure_time(timestamp));
			}
		}

		if (clearing) {

			auto not_first_clear = done_tatonnement_flag.exchange(true, std::memory_order_acq_rel);

			if (!not_first_clear) {

				TAT_INFO("CLEARING");

				TAT_INFO("ROUND %5d step_size %lf (%lu) objective %lf", round_number, price::amount_to_double(step, step_radix), step, prev_objective.l2norm_sq);
				TAT_INFO("Asset\tdemand\tsupply\tprice\ttax revenue\toversupply");
				for (size_t i = 0; i < num_assets; i++) {
					double demand = price::amount_to_double(demands_search[i], price::PRICE_RADIX);
					demand = demand - price::amount_to_double(demands_search[i], price::PRICE_RADIX + active_approx_params.tax_rate);
					double delta = demand - price::amount_to_double(supplies_search[i], price::PRICE_RADIX);
					TAT_INFO("%d\t%010.3f\t%010.3f\t%010.3f\t(%8lu)\t\t%010.3f\t%010.3f", 
						i, 
						price::amount_to_double(demands_search[i],price::PRICE_RADIX), 
						price::amount_to_double(supplies_search[i],price::PRICE_RADIX), 
						price::to_double(prices_workspace[i]),
						prices_workspace[i],
						price::amount_to_double(demands_search[i], price::PRICE_RADIX + active_approx_params.tax_rate),
						delta);
				}
				TAT_INFO("tax_rate %lu smooth_mult %lu", active_approx_params.tax_rate, active_approx_params.smooth_mult);
				for (size_t i = 0; i < num_assets; i++) {
					prices_workspace[i] = trial_prices[i];
				}
				internal_measurements.num_rounds = round_number;
				internal_measurements.step_radix = step_radix;
			}
			delete[] trial_prices;
			delete[] supplies_workspace;
			delete[] demands_workspace;
			delete[] supplies_search;
			delete[] demands_search;
			delete[] relativizers;

			demand_oracle.deactivate_oracle();
			return !not_first_clear;
		}
		round_number++;

		if (round_number % 10 == 9) {
			set_relativizers(control_params, relativizers, volume_relativizers, num_assets, demands_search, supplies_search);
		}


		bool any_change = set_trial_prices(prices_workspace, trial_prices, step, control_params, demands_search, supplies_search, relativizers);

		if (!any_change) {
			force_step_rounds = 10;
		}

		clear_supply_demand_workspaces(supplies_workspace, demands_workspace);

		demand_oracle.
			get_supply_demand(trial_prices, supplies_workspace, demands_workspace, work_units, active_approx_params.smooth_mult);

		clearing = check_clearing(demands_workspace, supplies_workspace, active_approx_params.tax_rate, num_assets);

		MultifuncTatonnementObjective new_objective;
		new_objective.eval(supplies_workspace, demands_workspace, prices_workspace, num_assets);

		if (round_number % 10000 == 9999) {
			auto other_finisher = done_tatonnement_flag.load(std::memory_order_acquire);
			if (other_finisher) {
				delete[] trial_prices;
				delete[] supplies_workspace;
				delete[] demands_workspace;
				delete[] supplies_search;
				delete[] demands_search;
				delete[] relativizers;

				TAT_INFO("thread ending, num rounds was %lu", round_number);
				demand_oracle.deactivate_oracle();
				return false;
			}
		}
		if (round_number % 100 == -1) {
			TAT_INFO("ROUND %5d step_size %lf (%lu) objective %lf step_radix %lu", round_number, price::amount_to_double(step, step_radix), step, prev_objective.l2norm_sq, step_radix);
			for (size_t i = 0; i < num_assets; i++) {
				double demand = price::amount_to_double(demands_search[i], price::PRICE_RADIX);
				demand = demand - price::amount_to_double(demands_search[i], price::PRICE_RADIX + active_approx_params.tax_rate);
				double delta = demand - price::amount_to_double(supplies_search[i], price::PRICE_RADIX);
				TAT_INFO("old: %d\t%15.3f\t%15.3f\t%4.3f\t(%8lu)",
					i, 
					price::amount_to_double(demands_search[i],price::PRICE_RADIX), 
					price::amount_to_double(supplies_search[i],price::PRICE_RADIX), 
					price::to_double(prices_workspace[i]),
					prices_workspace[i]);
				TAT_INFO("new:      \t%15.3f\t%15.3f\t%5.3f\t(%8lu)\t%11.3f",
					price::amount_to_double(demands_workspace[i],price::PRICE_RADIX), 
					price::amount_to_double(supplies_workspace[i],price::PRICE_RADIX), 
					price::to_double(trial_prices[i])
					, trial_prices[i]
					, delta);
			}
			TAT_INFO("any change: %d force step %d", any_change, force_step_rounds);
		}
		bool recalc_obj = false;

		if (new_objective.is_better_than(prev_objective) /*new_objective <= prev_objective * 1.1 */|| step < min_step || clearing || (force_step_rounds > 0)) { /* 1.0001 */
			for (size_t i = 0; i < num_assets; i++) {
				prices_workspace[i] = trial_prices[i];
				supplies_search[i] = supplies_workspace[i];
				demands_search[i] = demands_workspace[i];
			}
			if (force_step_rounds > 0) {
				force_step_rounds--;
			}
			prev_objective = new_objective;
			step = increment_step(step, step_up, step_adjust_radix);
		} else {
			step = decrement_step(step, step_down, step_adjust_radix);
		}

		if (round_number % 1000 == 0) {
			int adjust = normalize_prices(prices_workspace);
			if (adjust != 0) {
				//TAT_INFO("normalize prices %d", adjust);
				recalc_obj = true;
				if (adjust > 0) {
					step >>= adjust;
				} else {
					step <<= adjust;
				}
				if (step < min_step) {
					step = min_step;
				}
			}
		}

		if (recalc_obj) {
			prev_objective.eval(supplies_workspace, demands_workspace, prices_workspace, num_assets);
		}
	}
}

bool
TatonnementOracle::grid_search_tatonnement_query(
	TatonnementControlParameters& control_params,
	Price* prices_workspace,
	std::unique_ptr<LPInstance>& lp_instance) {

	#ifdef USE_DEMAND_MULT_PRICES
		throw std::runtime_error("oracle invalid");
	#endif

	Price* trial_prices = new Price[num_assets];

	const uint8_t step_radix = control_params.step_radix;

	const uint64_t min_step =  control_params.min_step;

	uint64_t step = min_step;// 1/2^value

	const uint8_t step_adjust_radix = control_params.step_adjust_radix;
	
	const uint16_t step_up = (uint16_t) (1.4 * ((double) (((uint16_t)1) << step_adjust_radix)));
	const uint16_t step_down = (uint16_t) (0.8 * ((double) (((uint16_t)1) << step_adjust_radix)));

	uint128_t* supplies_workspace = new uint128_t[num_assets];
	uint128_t* demands_workspace = new uint128_t[num_assets];

	uint128_t* supplies_search = new uint128_t[num_assets];
	uint128_t* demands_search = new uint128_t[num_assets];

	//std::lock_guard lock = work_unit_manager.get_lock();

	auto& work_units = work_unit_manager.get_orderbooks();

	clear_supply_demand_workspaces(supplies_search, demands_search);

	auto& demand_oracle = *(control_params.oracle);
	demand_oracle.activate_oracle();


	demand_oracle.
		get_supply_demand(prices_workspace, supplies_search, demands_search, work_units, active_approx_params.smooth_mult);//, function_inputs);


	MultifuncTatonnementObjective prev_objective;
	prev_objective.eval(supplies_search, demands_search, prices_workspace, num_assets);

	int round_number = 0;

	bool clearing = false;

	int force_step_rounds = 0;

	while (true) {

		if (round_number % LP_CHECK_FREQ == LP_CHECK_FREQ - 1) {
			TAT_INFO_F(auto timestamp = init_time_measurement());
			auto solver_res = solver.check_feasibility(trial_prices, lp_instance, active_approx_params);
			if (solver_res) {
				clearing = true;
				TAT_INFO("clearing because lp solver found valid solution: lp time %lf", measure_time(timestamp));
			}
		}

		if (clearing) {

			auto not_first_clear = done_tatonnement_flag.exchange(true, std::memory_order_acq_rel);

			if (!not_first_clear) {

				TAT_INFO("CLEARING");

				TAT_INFO("ROUND %5d step_size %lf (%lu) objective %lf", round_number, price::amount_to_double(step, step_radix), step, prev_objective.l2norm_sq);
				TAT_INFO("Asset\tdemand\tsupply\tprice\ttax revenue\toversupply");
				for (size_t i = 0; i < num_assets; i++) {
					double demand = price::amount_to_double(demands_search[i], price::PRICE_RADIX);
					demand = demand - price::amount_to_double(demands_search[i], price::PRICE_RADIX + active_approx_params.tax_rate);
					double delta = demand - price::amount_to_double(supplies_search[i], price::PRICE_RADIX);
					TAT_INFO("%d\t%010.3f\t%010.3f\t%010.3f\t(%8lu)\t\t%010.3f\t%010.3f", 
						i, 
						price::amount_to_double(demands_search[i],price::PRICE_RADIX), 
						price::amount_to_double(supplies_search[i],price::PRICE_RADIX), 
						price::to_double(prices_workspace[i]),
						prices_workspace[i],
						price::amount_to_double(demands_search[i], price::PRICE_RADIX + active_approx_params.tax_rate),
						delta);
				}
				TAT_INFO("tax_rate %lu smooth_mult %lu", active_approx_params.tax_rate, active_approx_params.smooth_mult);
				for (size_t i = 0; i < num_assets; i++) {
					prices_workspace[i] = trial_prices[i];
				}
				internal_measurements.num_rounds = round_number;
				internal_measurements.step_radix = step_radix;
			}
			delete[] trial_prices;
			delete[] supplies_workspace;
			delete[] demands_workspace;
			delete[] supplies_search;
			delete[] demands_search;

			demand_oracle.deactivate_oracle();
			return !not_first_clear;
		}
		round_number++;


		bool any_change = set_trial_prices(prices_workspace, trial_prices, step, control_params, demands_search, supplies_search, volume_relativizers);

		if (!any_change) {
			force_step_rounds = 10;
		}

		clear_supply_demand_workspaces(supplies_workspace, demands_workspace);

		demand_oracle.
			get_supply_demand(trial_prices, supplies_workspace, demands_workspace, work_units, active_approx_params.smooth_mult);

		clearing = check_clearing(demands_workspace, supplies_workspace, active_approx_params.tax_rate, num_assets);

		MultifuncTatonnementObjective new_objective;
		new_objective.eval(supplies_workspace, demands_workspace, prices_workspace, num_assets);

		if (round_number % 10000 == 9999) {
			auto other_finisher = done_tatonnement_flag.load(std::memory_order_acquire);
			if (other_finisher) {
				delete[] trial_prices;
				delete[] supplies_workspace;
				delete[] demands_workspace;
				delete[] supplies_search;
				delete[] demands_search;

				TAT_INFO("thread ending, num rounds was %lu", round_number);
				demand_oracle.deactivate_oracle();
				return false;
			}
		}
		//std::printf("doing a round\n");
		if (round_number % 100000 == -1) {
			TAT_INFO("ROUND %5d step_size %lf (%lu) objective %lf step_radix %lu", round_number, price::amount_to_double(step, step_radix), step, prev_objective.l2norm_sq, step_radix);
			//TAT_INFO("Asset\tdemand\tsupply\tprice\ttax revenue\toversupply");
			for (size_t i = 0; i < num_assets; i++) {
				double demand = price::amount_to_double(demands_search[i], price::PRICE_RADIX);
				demand = demand - price::amount_to_double(demands_search[i], price::PRICE_RADIX + active_approx_params.tax_rate);
				double delta = demand - price::amount_to_double(supplies_search[i], price::PRICE_RADIX);
				/*TAT_INFO("%d\t%14.3f\t%14.3f\t%5.3f\t(%8lu)\t%17.3f"
						, i
						, PriceUtils::amount_to_double(demands_search[i], PriceUtils::PRICE_RADIX)
						, PriceUtils::amount_to_double(supplies_search[i], PriceUtils::PRICE_RADIX)
						, PriceUtils::to_double(prices_workspace[i])
						, prices_workspace[i]
						, delta); */
				TAT_INFO("original: %d\t%11.3f\t%11.3f\t%4.3f\t(%8lu)",
				//%17.3f\t%17.3f\t%5.3f\t(%8lu)\t%17.3f", 
					i, 
					price::amount_to_double(demands_search[i],price::PRICE_RADIX), 
					price::amount_to_double(supplies_search[i],price::PRICE_RADIX), 
					price::to_double(prices_workspace[i]),
					prices_workspace[i]);
				TAT_INFO("new:      \t%11.3f\t%11.3f\t%5.3f\t(%8lu)\t%11.3f",
					price::amount_to_double(demands_workspace[i],price::PRICE_RADIX), 
					price::amount_to_double(supplies_workspace[i],price::PRICE_RADIX), 
					price::to_double(trial_prices[i])
					, trial_prices[i]
					, delta);
			}
			TAT_INFO("any change: %d force step %d", any_change, force_step_rounds);
		}
		bool recalc_obj = false;
		//if (round_number % 1000 == 0) {
		//	if (normalize_prices(prices_workspace)) {
		//		recalc_obj = true;
		//	}
		//}

		if (new_objective.is_better_than(prev_objective) /*new_objective <= prev_objective * 1.1 */|| step < min_step || clearing || (force_step_rounds > 0)) { /* 1.0001 */
			for (size_t i = 0; i < num_assets; i++) {
				prices_workspace[i] = trial_prices[i];
				supplies_search[i] = supplies_workspace[i];
				demands_search[i] = demands_workspace[i];
			}
			if (force_step_rounds > 0) {
				force_step_rounds--;
			}
			recalc_obj = true;
			step = increment_step(step, step_up, step_adjust_radix);
		} else {
			step = decrement_step(step, step_down, step_adjust_radix);
		}

		if (round_number % 1000 == 0) {
			int adjust = normalize_prices(prices_workspace);
			if (adjust != 0) {
				//TAT_INFO("normalize prices %d", adjust);
				recalc_obj = true;
				if (adjust > 0) {
					step >>= adjust;
				} else {
					step <<= adjust;
				}
				if (step < min_step) {
					step = min_step;
				}
			}
			//if (normalize_prices(prices_workspace)) {
			//	recalc_obj = true;
			//}
		}

		if (recalc_obj) {
			prev_objective.eval(supplies_workspace, demands_workspace, prices_workspace, num_assets);
		}
	}
}


} /* namespace speedex */
