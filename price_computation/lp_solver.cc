#include "price_computation/lp_solver.h"
#include "utils/debug_macros.h"

#include <cmath>

namespace speedex {

//! Lower and upper trade bounds for a given orderbook.
struct BoundsInfo {
	std::pair<uint64_t, uint64_t> bounds;
	OfferCategory category;
};


BoundsInfo 
get_bounds_info(
	Orderbook& orderbook, 
	Price* prices, 
	const ApproximationParameters approx_params) 
{
	return BoundsInfo{
		orderbook.get_supply_bounds(prices, approx_params.smooth_mult),
		orderbook.get_category()
	};
	//supply bounds is [lower bound, upper bound]
}

/*

For some reason all of the arrays in GLPK are 1-indexed.

*/
void LPSolver::add_orderbook_range_constraint(
	glp_prob* lp, 
	Orderbook& orderbook, 
	int idx, 
	Price* prices, 
	int *ia, int *ja, double *ar, 
	int& next_available_nnz, 
	const ApproximationParameters approx_params,
	bool use_lower_bound) {

	auto bounds = get_bounds_info(orderbook, prices, approx_params);

	add_orderbook_range_constraint(lp, bounds, idx, prices, ia, ja, ar, next_available_nnz, approx_params.tax_rate, use_lower_bound);
}

void
LPSolver::add_orderbook_range_constraint(
	glp_prob* lp, BoundsInfo& bounds_info, int idx, Price* prices, int *ia, int *ja, double *ar, int& next_available_nnz, const uint8_t tax_rate, bool use_lower_bound) {

	auto& bounds = bounds_info.bounds;
	auto& category = bounds_info.category;

	if (!use_lower_bound) {
		bounds.first = 0;
	}

	if (bounds.first == bounds.second) {
		glp_set_col_bnds(lp, idx, GLP_FX, bounds.first, bounds.second);
	} else {
		glp_set_col_bnds(lp, idx, GLP_DB, bounds.first, bounds.second);
	}
	glp_set_obj_coef(lp, idx, price::to_double(prices[category.sellAsset]));

	// ia is for the row, refers to the particular slack variable.  
	// In this case, the asset.

	double sell_price = price::to_double(prices[category.sellAsset]);
	
	double buy_price = price::to_double(
		prices[category.sellAsset] - (prices[category.sellAsset] >> tax_rate));

	ia[next_available_nnz] = category.sellAsset+1;
	ja[next_available_nnz] = idx;
	ar[next_available_nnz] = sell_price;

	next_available_nnz++;

	ia[next_available_nnz] = category.buyAsset+1;
	ja[next_available_nnz] = idx;
	ar[next_available_nnz] = -buy_price;

	next_available_nnz++;
}



bool LPSolver::check_feasibility(
	Price* prices, 
	std::unique_ptr<LPInstance>& instance, 
	const ApproximationParameters approx_params) {
	
	std::vector<BoundsInfo> bounds;

	auto& work_units = manager.get_orderbooks();
	auto work_units_sz = work_units.size();

	// do demand queries before acquiring lock
	for (auto& work_unit : work_units) {
		bounds.push_back(get_bounds_info(work_unit, prices, approx_params));
	}

	auto* lp = instance -> lp;
	auto* ia = instance -> ia;
	auto* ja = instance -> ja;
	auto* ar = instance -> ar;

	const size_t nnz = instance -> nnz;

	std::lock_guard lock(mtx); // glp is unfortunately not threadsafe
	instance->clear();

	glp_set_obj_dir(lp, GLP_MAX);

	int num_assets = manager.get_num_assets();
	glp_add_rows(lp, num_assets);

	for (int i = 0; i < num_assets; i++) {
		// i'th asset constraint must be positive
		glp_set_row_bnds(lp, i+1, GLP_LO, 0.0, 0.0); 
	}

	if (nnz != 1 + 2 * work_units_sz) {
		throw std::runtime_error("invalid nnz");
	}

	// only 0 if n_assets = 1
	if (work_units_sz > 0) {
		//add in work unit supply availability constraints
		glp_add_cols(lp, work_units_sz);

		int next_available_nnz = 1; // whyyyyyyy
		for (unsigned int i = 0; i < work_units_sz; i++) {
			//check feasibility calls within tatonnement runs always use lower bound on supply
			add_orderbook_range_constraint(lp, bounds[i], i+1, prices, ia, ja, ar, next_available_nnz, approx_params.tax_rate, true);
		}
	}

	glp_load_matrix(lp, nnz - 1, ia, ja, ar);

	glp_smcp parm;
	glp_init_smcp(&parm);

	//use parm to set debug message level
	parm.msg_lev = GLP_MSG_OFF;
	R_INFO_F(parm.msg_lev = GLP_MSG_ALL);

	parm.presolve = GLP_ON;

	auto status = glp_simplex(lp, &parm);

	return (status == 0);
}

bool
LPSolver::unsafe_check_feasibility(
	Price* prices, 
	std::unique_ptr<LPInstance>& instance, 
	const ApproximationParameters approx_params,
	std::vector<BoundsInfo> & info,
	size_t num_assets)
{
	auto* lp = instance -> lp;
	auto* ia = instance -> ia;
	auto* ja = instance -> ja;
	auto* ar = instance -> ar;

	const size_t nnz = instance -> nnz;

	instance->clear();

	glp_set_obj_dir(lp, GLP_MAX);

	glp_add_rows(lp, num_assets);

	for (auto i = 0u; i < num_assets; i++) {
		// i'th asset constraint must be positive
		glp_set_row_bnds(lp, i+1, GLP_LO, 0.0, 0.0); 
	}

	size_t work_units_sz = info.size();

	if (nnz != 1 + 2 * work_units_sz) {
		throw std::runtime_error("invalid nnz");
	}

	//add in work unit supply availability constraints
	glp_add_cols(lp, work_units_sz);

	int next_available_nnz = 1; // whyyyyyyy
	for (unsigned int i = 0; i < work_units_sz; i++) {
		//check feasibility calls within tatonnement runs always use lower bound on supply
		add_orderbook_range_constraint(lp, info[i], i+1, prices, ia, ja, ar, next_available_nnz, approx_params.tax_rate, true);
	}

	glp_load_matrix(lp, nnz - 1, ia, ja, ar);

	glp_smcp parm;
	glp_init_smcp(&parm);

	//use parm to set debug message level
	parm.msg_lev = GLP_MSG_OFF;
	R_INFO_F(parm.msg_lev = GLP_MSG_ALL);

	parm.presolve = GLP_ON;

	auto status = glp_simplex(lp, &parm);

	return (status == 0);
}

ClearingParams 
LPSolver::solve(
	Price* prices, 
	const ApproximationParameters approx_params, 
	bool use_lower_bound) {

	std::unique_lock lock(mtx);

	glp_prob *lp = glp_create_prob();
	glp_set_obj_dir(lp, GLP_MAX);

	auto& orderbooks = manager.get_orderbooks();
	auto work_units_sz = orderbooks.size();

	int num_assets = manager.get_num_assets();
	glp_add_rows(lp, num_assets);

	for (int i = 0; i < num_assets; i++) {
		 // i'th asset constraint must be positive
		glp_set_row_bnds(lp, i+1, GLP_LO, 0.0, 0.0);
	}

	int nnz = 1 + 2 * work_units_sz;

	int* ia = new int[nnz];
	int* ja = new int[nnz];
	double* ar = new double[nnz];

	// avoid glpk error in case of 1asset simulations (no constraints in such case)
	if (work_units_sz > 0)
	{
		//add in work unit supply availability constraints
		glp_add_cols(lp, work_units_sz);
		int next_available_nnz = 1; // whyyyyyyy
		for (unsigned int i = 0; i < work_units_sz; i++) {
			add_orderbook_range_constraint(lp, orderbooks[i], i+1, prices, ia, ja, ar, next_available_nnz, approx_params, use_lower_bound);
		}
	}

	glp_load_matrix(lp, nnz - 1, ia, ja, ar);

	glp_smcp parm;
	glp_init_smcp(&parm);

	//use parm to set debug message level
	parm.msg_lev = GLP_MSG_OFF;
	
	
	//parm.msg_lev = GLP_MSG_ALL;

	//glp_write_lp(lp, NULL, "recent_lp.txt");

	parm.presolve = GLP_ON;

	auto status = glp_simplex(lp, &parm);

	if (status) {
		std::printf("LP Solving Failed with status %d\n", status);
		if (!use_lower_bound) {
			throw std::runtime_error(
				"lp solving failed with lower bounds inactive?");
		}
		
		std::printf("retrying without lower bounds\n");

		lock.unlock();
		return solve(prices, approx_params, false);
	}

	ClearingParams output;
	FractionalAsset* supplies = new FractionalAsset[num_assets];
	FractionalAsset* demands = new FractionalAsset[num_assets];

	for (unsigned int idx = 0; idx < work_units_sz; idx++) {
		double flow = glp_get_col_prim(lp, idx+1);
		OrderbookClearingParams result;

		FractionalAsset rounded_flow = FractionalAsset::from_double(flow);
		result.supply_activated = rounded_flow;

		output.orderbook_params.push_back(result);
		R_INFO("idx = %d flow = %f", idx, flow);

		auto category = category_from_idx(idx, num_assets);
		supplies[category.sellAsset] += rounded_flow;

		auto demanded_flow = price::wide_multiply_val_by_a_over_b(
			rounded_flow.value, 
			prices[category.sellAsset], 
			prices[category.buyAsset]);

		demands[category.buyAsset] += FractionalAsset::from_raw(demanded_flow);
	}
	R_INFO("extra revenue per asset:");
	for (int i = 0; i < num_assets; i++) {
		R_INFO("%d %f", i, glp_get_row_dual(lp, i+1));
	}

	uint8_t output_tax_rate = approx_params.tax_rate;

	for (int i = 0; i < num_assets; i++) {
		uint8_t new_tax_rate = max_tax_param(
			supplies[i], demands[i], approx_params.tax_rate);
		output_tax_rate = std::min(new_tax_rate, output_tax_rate);
	}

	output.tax_rate = output_tax_rate;

	delete[] supplies;
	delete[] demands;

	delete[] ia;
	delete[] ja;
	delete[] ar;

	glp_delete_prob(lp);

	return output;
}


uint8_t 
LPSolver::max_tax_param(
	FractionalAsset supply, FractionalAsset demand, const uint8_t target_tax) {

	if (supply.ceil() >= demand.tax_and_round(target_tax)) {
		return target_tax;
	}

	if (supply.ceil() >= (demand.tax_and_round((target_tax - 1)))) {
		return target_tax - 1;
	}
	


	double eps = std::log2(
		(demand-supply).to_double()) - std::log2(demand.to_double());
	uint8_t tax_rate = std::floor(-eps);
	
	if (tax_rate < target_tax - 1) {
		std::printf("eps %lf demand %lf supply %lf\n", eps, demand.to_double(), supply.to_double());
		std::fflush(stdout);
		throw std::runtime_error(
			"tax rate increased too much due to LP rounding error");
	}
	/*
	 * Known issue: rounding error when number of offers is very small 
	 * (i.e. total supply/demand is small) and tax rate is very high 
	 * (i.e. 2^-22),
	 * rounding from float to fixed point introduces error, which causes this 
	 * check to fail.  The fix is to use fixed point numbers
	 * with a higher level of precision.
	 */
	return tax_rate;
}

std::unique_ptr<LPInstance> 
LPSolver::make_instance() const {
	return std::unique_ptr<LPInstance>(new LPInstance(get_nnz()));
}

} /* speedex */
