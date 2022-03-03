
#include "price_computation/lp_solver.h"

#include "simplex/solver.h"

#include "orderbook/utils.h"

#include "utils/time.h"

#include "xdr/types.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <random>
#include <utility>
#include <vector>

#include "config.h"

#if HAVE_LEMON == 1

#include <lemon/list_graph.h>
#include <lemon/network_simplex.h>
#include <lemon/maps.h>

#endif

using namespace speedex;

using int128_t = __int128;

template<typename rand_gen>
std::vector<BoundsInfo> gen_bounds(size_t num_assets, rand_gen& gen) {

	std::uniform_int_distribution<> bounds_dist(100, 1000);
	std::uniform_int_distribution<> gap_dist(10, 100);

	std::vector<BoundsInfo> out;

	size_t num_work_units = get_num_orderbooks_by_asset_count(num_assets);

	for (size_t idx = 0; idx < num_work_units; idx++) {
		uint64_t lb = 1;//bounds_dist(gen);
		uint64_t ub = lb + gap_dist(gen);
		BoundsInfo bds;
		bds.bounds.first = lb;
		bds.bounds.second = ub;
		bds.category = category_from_idx(idx, num_assets);
		out.push_back(bds);
	}
	return out;
}

template<typename rand_gen>
std::vector<Price> gen_prices(size_t num_assets, rand_gen& gen) {
	std::uniform_int_distribution<> price_dist(100, 10000);
	std::vector<Price> price_out;
	for (size_t i = 0; i < num_assets; i++) {
		price_out.push_back(price_dist(gen));
	}
	return price_out;
}

std::unique_ptr<LPInstance> make_instance(size_t num_assets) {
	size_t num_orderbooks = get_num_orderbooks_by_asset_count(num_assets);
	size_t nnz = 1 + 2 * num_orderbooks;

	return std::make_unique<LPInstance>(nnz);
}

bool run_glpk(std::vector<BoundsInfo>& info, std::vector<Price>& prices, std::unique_ptr<LPInstance>& instance, LPSolver& lp_solver, size_t num_assets) {

	ApproximationParameters params {
		.tax_rate = 20,
		.smooth_mult = 10
	};

	return lp_solver.unsafe_check_feasibility(
		prices.data(), 
		instance, 
		params,
		info,
		num_assets);
}

bool run_simplex(std::vector<BoundsInfo> const& info, std::vector<Price> const& prices, size_t num_assets, bool glpk_res) {

	alloc.clear();
	c_alloc.clear();

	SimplexLPSolver solver(num_assets);

	for (auto const& b : info) {
		int128_t lb = ((int128_t)b.bounds.first) * ((int128_t) prices[b.category.sellAsset]);
		int128_t ub = ((int128_t)b.bounds.second) * ((int128_t) prices[b.category.sellAsset]);
		solver.add_orderbook_constraint(lb, ub, b.category);
	}
	//try {
		bool res = solver.check_feasibility();
		return res;
/*		if (res != glpk_res) {
			std::printf("%d %d\n", glpk_res, res);
			throw std::runtime_error("disagreement!");
		}
		return res;
	} catch(...) {

		for (auto const& b : info) {

			int128_t lb = ((int128_t)b.bounds.first) * ((int128_t) prices[b.category.sellAsset]);
			int128_t ub = ((int128_t)b.bounds.second) * ((int128_t) prices[b.category.sellAsset]);
			std::printf("orderbook (sell %u, buy %u): lb %lf ub %lf\n", b.category.sellAsset, b.category.buyAsset, (double) lb, (double) ub);
		}

		exit(1);
	}*/
}

#if HAVE_LEMON == 1

using lemon_instance_t 
	= std::tuple<
		std::unique_ptr<lemon::ListDigraph>,
		std::unique_ptr<lemon::NetworkSimplex<lemon::ListDigraph>>, 
		std::unique_ptr<std::map<std::pair<size_t, size_t>, lemon::ListDigraph::Arc>>
	>;

lemon_instance_t
make_lemon_instance(size_t num_assets)
{
	std::unique_ptr<lemon::ListDigraph> d = std::make_unique<lemon::ListDigraph>();

	std::vector<lemon::ListDigraph::Node> nodes;

	auto arcs = std::make_unique<std::map<std::pair<size_t, size_t>, lemon::ListDigraph::Arc>>();

	for (size_t i = 0; i < num_assets; i ++) {
		nodes.push_back(d->addNode());
	}

	for (size_t i = 0; i < num_assets; i++) {
		for (size_t j = 0; j < num_assets; j++) {
			if (i != j) {
				auto arc = d->addArc(nodes[i], nodes[j]);

				auto pair = std::make_pair(i, j);
				arcs->emplace(pair, arc);
			}
		}
	}

	auto ns = std::make_unique<lemon::NetworkSimplex<lemon::ListDigraph>>(*d);

	return std::make_tuple(std::move(d), std::move(ns), std::move(arcs));
}

#else

using lemon_instance_t = int;

#endif


#if HAVE_LEMON == 1

bool run_lemon_ns(std::vector<BoundsInfo> const& info, std::vector<Price> const& prices, size_t num_assets, lemon_instance_t& instance) {

	auto& d = *std::get<0>(instance);
	auto& ns = *std::get<1>(instance);
	auto& arcs = *std::get<2>(instance);

	lemon::ListDigraph::ArcMap<int128_t> ubs(d), lbs(d), costs(d);

	for (auto const& b : info) {
		auto& arc = arcs[{b.category.sellAsset, b.category.buyAsset}];
		int128_t lb = ((int128_t)b.bounds.first) * ((int128_t) prices[b.category.sellAsset]);
		int128_t ub = ((int128_t)b.bounds.second) * ((int128_t) prices[b.category.sellAsset]);

		ubs.set(arc, ub);
		lbs.set(arc, lb);
		costs.set(arc, -1);
	}

	ns.reset();
	ns.upperMap(ubs).lowerMap(lbs).costMap(costs);

	auto res = ns.run();

	using res_t = lemon::NetworkSimplex<lemon::ListDigraph>::ProblemType;

	if (res == res_t::UNBOUNDED) {
		throw std::runtime_error("mistaken setup");
	}

	//std::printf("total lemon value = %ld\n", -ns.totalCost());

	return res == res_t::OPTIMAL;
}

#endif

float overall_sum_simplex = 0;
size_t count = 0;

bool can_run_simplex = true;

template<typename rand_gen>
void run_experiment(size_t num_assets, std::unique_ptr<LPInstance>& instance, LPSolver& lp_solver, lemon_instance_t& lemon_instance, rand_gen& gen, size_t& glpk_successes, size_t& simplex_successes, size_t& lemon_successes) {

	auto prices = gen_prices(num_assets, gen);
	auto bounds = gen_bounds(num_assets, gen);

	auto ts = init_time_measurement();

	bool glpk_res = false;

	if (run_glpk(bounds, prices, instance, lp_solver, num_assets)) {
		glpk_successes++;
		glpk_res = true;
	} 

	float glpk_time = measure_time(ts);

	if (can_run_simplex) {
		if(run_simplex(bounds, prices, num_assets, glpk_res)) {
			simplex_successes++;
		}
	}

	float simplex_time = measure_time(ts);

	overall_sum_simplex += simplex_time;
	count++;

	#if HAVE_LEMON == 1

	if(run_lemon_ns(bounds, prices, num_assets, lemon_instance))
	{
		lemon_successes ++;
	}

	#endif

	float lemon_time = measure_time(ts);

	std::printf("glpk_time(successes=%lu) %lf\t simplex_time(successes=%lu) %lf (avg %lf) lemon-ns(successes=%lu) %lf\n",
		glpk_successes, glpk_time, simplex_successes, simplex_time, overall_sum_simplex / count, lemon_successes, lemon_time);
}


int main(int argc, char const *argv[])
{
	if (argc != 2) {
		std::printf("usage: ./solver_comparison <num_assets>\n");
		return 1;
	}

	size_t num_assets = std::stoi(argv[1]);

	if ((2* num_assets * num_assets + 2 * num_assets) > ((size_t) UINT16_MAX)) {
		std::printf("too many assets, will overflow inside simplex solver\n");
		can_run_simplex = false;
	} 

	auto instance = make_instance(num_assets);
	OrderbookManager manager(num_assets);
	LPSolver lp_solver(manager);

	#if HAVE_LEMON == 1

	auto lemon_inst = make_lemon_instance(num_assets);

	#else

	lemon_instance_t lemon_inst = 0;
	std::printf("lemon library not found, skipping those solvers\n");

	#endif

	size_t glpk_successes = 0;
	size_t simplex_successes = 0;
	size_t lemon_successes = 0;

	std::minstd_rand gen(0);

	while(true) {
		run_experiment(num_assets, instance, lp_solver, lemon_inst, gen, glpk_successes, simplex_successes, lemon_successes);
	}
}





