#include <catch2/catch_test_macros.hpp>

#include "xdr/types.h"

#include "simplex/solver.h"

namespace speedex {

TEST_CASE("simplex feasibility tests", "[simplex]")
{
	auto get_category = [](AssetID sell, AssetID buy) {
		OfferCategory out;
		out.type = OfferType::SELL;
		out.sellAsset = sell;
		out.buyAsset = buy;
		return out;
	};

	using solver_t = SimplexLPSolver;
	
	alloc.clear();
	c_alloc.clear();


	SECTION("empty")
	{
		solver_t solver(2);

		REQUIRE(solver.check_feasibility());
	}

	SECTION("one orderbook invalid")
	{
		solver_t solver(2);
		solver.add_orderbook_constraint(10, 20, get_category(0, 1));

		REQUIRE(!solver.check_feasibility());
	}

	SECTION("one orderbook valid")
	{
		solver_t solver(2);
		solver.add_orderbook_constraint(0, 20, get_category(0, 1));

		REQUIRE(solver.check_feasibility());
	}

	SECTION("test_two_orderbooks_valid_1")
	{
		solver_t solver(2);
		solver.add_orderbook_constraint(0, 20, get_category(0, 1));
		solver.add_orderbook_constraint(0, 20, get_category(1, 0));
		REQUIRE(solver.check_feasibility());
	}
	SECTION("test_two_orderbooks_valid_2")
	{
		solver_t solver(2);
		solver.add_orderbook_constraint(5, 20, get_category(0, 1));
		solver.add_orderbook_constraint(10, 20, get_category(1, 0));
		REQUIRE(solver.check_feasibility());
	}

	SECTION("test_two_orderbooks_valid_3")
	{
		solver_t solver(2);
		solver.add_orderbook_constraint(0, 200, get_category(0, 1));
		solver.add_orderbook_constraint(200, 201, get_category(1, 0));
		REQUIRE(solver.check_feasibility());
	}

	SECTION("test_two_orderbooks_invalid_1")
	 {
		solver_t solver(2);
		solver.add_orderbook_constraint(0, 20, get_category(0, 1));
		solver.add_orderbook_constraint(30, 40, get_category(1, 0));
		REQUIRE(!solver.check_feasibility());
	}

	SECTION("test_two_orderbooks_invalid_2")
	{
		solver_t solver(2);
		solver.add_orderbook_constraint(19, 20, get_category(0, 1));
		solver.add_orderbook_constraint(30, 40, get_category(1, 0));
		REQUIRE(!solver.check_feasibility());
	}

	SECTION("test_three_orderbooks_valid_1")
	{
		solver_t solver(3);
		solver.add_orderbook_constraint(0, 10, get_category(0, 1));
		solver.add_orderbook_constraint(0, 100, get_category(1, 2));
		solver.add_orderbook_constraint(0, 20, get_category(2, 0));
		REQUIRE(solver.check_feasibility());
	}

	SECTION("test_three_orderbooks_valid_2")
	{
		solver_t solver(3);
		solver.add_orderbook_constraint(0, 10, get_category(0, 1));
		solver.add_orderbook_constraint(10, 100, get_category(1, 2));
		solver.add_orderbook_constraint(0, 20, get_category(2, 0));
		REQUIRE(solver.check_feasibility());
	}

	SECTION("test_three_orderbooks_valid_3")
	{
		solver_t solver(3);
		solver.add_orderbook_constraint(0, 10, get_category(0, 1));
		solver.add_orderbook_constraint(0, 100, get_category(1, 2));
		solver.add_orderbook_constraint(10, 20, get_category(2, 0));
		REQUIRE(solver.check_feasibility());
	}

	SECTION("test_three_orderbooks_invalid_1")
	{
		solver_t solver(3);
		solver.add_orderbook_constraint(0, 10, get_category(0, 1));
		solver.add_orderbook_constraint(11, 100, get_category(1, 2));
		solver.add_orderbook_constraint(0, 20, get_category(2, 0));
		REQUIRE(!solver.check_feasibility());
	}

	// Experimentally found trial examples

	SECTION("test_exp_two_orderbooks_feasible")
	{
		solver_t solver(2);
		solver.add_orderbook_constraint(100, 641300, get_category(0, 1));
		solver.add_orderbook_constraint(941, 8493466, get_category(1, 0));

		REQUIRE(solver.check_feasibility());
	}

	SECTION("test_exp_three_orderbooks_feasible")
	{
		solver_t solver(3);
		solver.add_orderbook_constraint(100, 902600, get_category(0, 1));
		solver.add_orderbook_constraint(100, 971300, get_category(0, 2));
		solver.add_orderbook_constraint(941, 2548228, get_category(1, 0));
		solver.add_orderbook_constraint(941, 5303476, get_category(1, 2));
		solver.add_orderbook_constraint(6054, 27745482, get_category(2, 0));
		solver.add_orderbook_constraint(6054, 20383818, get_category(2, 1));

		REQUIRE(solver.check_feasibility());
	}

	SECTION("test_exp_three_orderbooks_feasible_2")
	{
		solver_t solver(3);
		solver.add_orderbook_constraint(4897, 151807, get_category(0, 1));
		solver.add_orderbook_constraint(4897, 142013, get_category(0, 2));
		solver.add_orderbook_constraint(4708, 296604, get_category(1, 0));
		solver.add_orderbook_constraint(4708, 98868, get_category(1, 2));
		solver.add_orderbook_constraint(171, 11286, get_category(2, 0));
		solver.add_orderbook_constraint(171, 4446, get_category(2, 1));

		REQUIRE(solver.check_feasibility());
	}
}

} // namespace speedex
