#pragma once

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

/*! \file demand_oracle.h

Runs a single supply/demand query.
Owns background threads to run these queries.
*/

#include "orderbook/orderbook.h"
#include "orderbook/utils.h"

#include <utils/async_worker.h>

#include <utils/compat.h>

using uint128_t = __uint128_t;

namespace speedex {

/*! Demand computation worker.

Each worker is assigned a range of orderbooks,
and, when active, wait on a spinlock for 
a signal from the main thread.

Otherwise, sleep as a regular AsyncWorker.

Each worker should be assigned to only one ParallelDemandOracle.
*/

#define USE_DEMAND_MULT_PRICES

#ifdef USE_DEMAND_MULT_PRICES
constexpr static auto demand_func = &Orderbook::calculate_demands_and_supplies_times_prices;
#else
constexpr static auto demand_func = &Orderbook::calculate_demands_and_supplies;
#endif

class DemandOracleWorker : public utils::AsyncWorker {
	
	unsigned int num_assets;

	size_t starting_work_unit;
	size_t ending_work_unit;

	uint128_t* supplies;
	uint128_t* demands;

	bool round_start = false;
	
	std::atomic<bool> tatonnement_round_flag = false;
	std::atomic<bool> sleep_flag = false;
	std::atomic<bool> round_done_flag = false;


	Price* query_prices;
	std::vector<Orderbook>* query_work_units;
	uint8_t query_smooth_mult;

	bool exists_work_to_do() {
		return round_start;
	}

	//! Basic TTAS spinlock.
	//! Includes memory fences so that main thread can safely 
	//! read data that was written by this thread. 
	//! Return true if thread should go to sleep
	bool spinlock() {
		while(true) {
			bool next_round = tatonnement_round_flag.load(std::memory_order_relaxed);
			if (next_round) {
				tatonnement_round_flag.store(false, std::memory_order_relaxed);
				std::atomic_thread_fence(std::memory_order_acquire);
				return false;
			}
			bool sleep = sleep_flag.load(std::memory_order_relaxed);
			if (sleep) {
				sleep_flag.store(false, std::memory_order_relaxed);
				return true;
			}
			SPINLOCK_PAUSE();
		}
	}

	//! Compute supply/demand for this worker's
	//! assigned orderbooks.
	void get_supply_demand(
		Price* active_prices,
		uint128_t* supplies, 
		uint128_t* demands, 
		std::vector<Orderbook>& work_units,
		const uint8_t smooth_mult) {
			
			for (size_t i = starting_work_unit; i < ending_work_unit; i++) {
				(work_units[i].*demand_func) (active_prices, demands, supplies, smooth_mult);
			}
	}

	void run() {
		std::unique_lock lock(mtx);

		while(true) {

			if ((!done_flag) && (!exists_work_to_do())) {
				cv.wait(lock, [this] () { return done_flag || exists_work_to_do();});
			}
			if (done_flag) return;
			if (round_start) {
				round_start = false;
				
				while(!spinlock()) {
					for (size_t i = 0; i < num_assets; i++) {
						supplies[i] = 0;
						demands[i] = 0;
					}

					get_supply_demand(query_prices, supplies, demands, *query_work_units, query_smooth_mult);
					signal_round_compute_done();
				}
			}
			cv.notify_all();
		}
	}

	//! Signal to main thread that local work is finished.
	void signal_round_compute_done() {
		std::atomic_thread_fence(std::memory_order_release);
		round_done_flag.store(true, std::memory_order_relaxed);
	}
public:

	//! Initialize this demand worker to run on orderbooks between starting_work_unit (incl)
	//! and ending_work_unit (excl)
	void init(unsigned int num_assets_, size_t starting_work_unit_, size_t ending_work_unit_) {
		num_assets = num_assets_;
		starting_work_unit = starting_work_unit_;
		ending_work_unit = ending_work_unit_;
		supplies = new uint128_t[num_assets];
		demands = new uint128_t[num_assets];
		start_async_thread([this] {run();});
	}

	~DemandOracleWorker() {
		terminate_worker();
		delete[] demands;
		delete[] supplies;
	}

	//! Called by main thread to wait (on a TTAS spinlock) for
	//! worker thread to finish.
	void wait_for_compute_done_and_get_results(uint128_t* demands_out, uint128_t* supplies_out) {
		while(true) {
			bool res = round_done_flag.load(std::memory_order_relaxed);
			if (res) {
				round_done_flag.store(false, std::memory_order_relaxed);
				std::atomic_thread_fence(std::memory_order_acquire);

				for (size_t i = 0; i < num_assets; i++) {
					demands_out[i] += demands[i];
					supplies_out[i] += supplies[i];
				}
				return;
			}
			SPINLOCK_PAUSE();
		}
	}

	//! Called by main thread to start worker on a given
	//! set of prices.
	void signal_round_start(
		Price* prices, 
		std::vector<Orderbook>* work_units, 
		uint8_t smooth_mult) 
	{
		query_prices = prices;
		query_work_units = work_units;
		query_smooth_mult = smooth_mult;
		std::atomic_thread_fence(std::memory_order_release);
		tatonnement_round_flag.store(true, std::memory_order_relaxed);
	}

	//! Activates worker from sleep.
	//! Worker begins waiting on a spinlock for a further signal.
	void activate_worker() {
		std::lock_guard lock(mtx);
		round_start = true;
		cv.notify_one();
	}
	//! Puts worker to sleep.
	void deactivate_worker() {
		sleep_flag.store(true, std::memory_order_relaxed);
	}
};


/*! Parallelized oracle for supply and demand.

Call activate_oracle() (deactivate_oracle()) before
(after) usage to wake (put to sleep) the background threads.

Each worker should be assigned to only one ParallelDemandOracle.

Not threadsafe.  Each Tatonnement copy should have its own
oracle.

*/ 
template<unsigned int NUM_WORKERS>
class ParallelDemandOracle {

	size_t num_work_units;

	unsigned int num_assets;

	//! Caller thread is reponsible for orderbooks from
	//! main_thread_start_idx(incl) to main_thread_end_idx(excl)
	constexpr static size_t main_thread_start_idx = 0;

	//! Caller thread is reponsible for orderbooks from
	//! main_thread_start_idx(incl) to main_thread_end_idx(excl)
	const size_t main_thread_end_idx;

	DemandOracleWorker workers[NUM_WORKERS];

public:
	//! Initialize oracle with a given number of assets and a given
	//! number of orderbooks.
	ParallelDemandOracle(size_t num_work_units, size_t num_assets)
		: num_work_units(num_work_units)
		, num_assets(num_assets)
		, main_thread_end_idx(num_work_units / (NUM_WORKERS + 1))
	{

		size_t num_shares = NUM_WORKERS + 1;
		//main_thread_end_idx = num_work_units / (num_shares);

		for (size_t i = 0; i < NUM_WORKERS; i++) {
			size_t start_idx = (num_work_units * (i+1)) / num_shares;
			size_t end_idx = (num_work_units * (i+2)) / num_shares;
			workers[i].init(num_assets, start_idx, end_idx);
		}
	}

	//! Compute supply/demand using worker threads.
	void get_supply_demand(
		Price* active_prices,
		uint128_t* supplies, 
		uint128_t* demands, 
		std::vector<Orderbook>& work_units,
		const uint8_t smooth_mult) {

		// Start compute round
		for (size_t i = 0; i < NUM_WORKERS; i++) {
			workers[i].signal_round_start(active_prices, &work_units, smooth_mult);
		}

		// Do work in main thread
		for (size_t i = main_thread_start_idx; i < main_thread_end_idx; i++) {
			(work_units[i].*demand_func)(active_prices, demands, supplies, smooth_mult);
		}

		// Gather results from workers
		for (size_t i = 0; i < NUM_WORKERS; i++) {
			workers[i].wait_for_compute_done_and_get_results(demands, supplies);
		}
	}

	//! Wake worker threads, set them to wait
	//! on spinlocks for round start
	void activate_oracle() {
		for (size_t i = 0; i < NUM_WORKERS; i++) {
			workers[i].activate_worker();
		}
	}

	//! Put worker threads to sleep
	void deactivate_oracle() {
		for (size_t i = 0; i < NUM_WORKERS; i++) {
			workers[i].deactivate_worker();
		}
	}

};

} /* speedex */
