#include <catch2/catch_test_macros.hpp>

#include "orderbook/orderbook_manager.h"

#include "price_computation/lp_solver.h"

namespace speedex
{

TEST_CASE("1asset", "[lp]")
{
	OrderbookManager manager(1);

	LPSolver solver(manager);

	ApproximationParameters approx{
		.tax_rate = 10,
		.smooth_mult = 10
	};

	Price prices[1];

	SECTION("use lower bound")
	{
		solver.solve(prices, approx, true);
	} 
	SECTION("ignore lower bound")
	{
		solver.solve(prices, approx, false);
	}
}

TEST_CASE("1asset feasibility", "[lp]")
{
	OrderbookManager manager(1);

	LPSolver solver(manager);

	auto instance = solver.make_instance();

	ApproximationParameters approx{
		.tax_rate = 10,
		.smooth_mult = 10
	};

	Price prices[1];

	REQUIRE(solver.check_feasibility(prices, instance, approx));
}

}
