#include <cxxtest/TestSuite.h>

#include "xdr/types.h"

#include "simplex/solver.h"

using namespace speedex;

using solver_t = SimplexLPSolver;

class FeasibilityTests : public CxxTest::TestSuite {

	OfferCategory get_category(AssetID sell, AssetID buy) {
		OfferCategory out;
		out.type = OfferType::SELL;
		out.sellAsset = sell;
		out.buyAsset = buy;
		return out;
	}

public:


	void test_empty() {
		solver_t solver(2);

		TS_ASSERT(solver.check_feasibility());
	}

	void test_one_orderbook_invalid() {
		solver_t solver(2);
		solver.add_orderbook_constraint(10, 20, get_category(0, 1));

		TS_ASSERT(!solver.check_feasibility());
	}

	void test_one_orderbook_valid() {
		solver_t solver(2);
		solver.add_orderbook_constraint(0, 20, get_category(0, 1));

		TS_ASSERT(solver.check_feasibility());
	}

	void test_two_orderbooks_valid_1() {
		solver_t solver(2);
		solver.add_orderbook_constraint(0, 20, get_category(0, 1));
		solver.add_orderbook_constraint(0, 20, get_category(1, 0));
		TS_ASSERT(solver.check_feasibility());
	}
	void test_two_orderbooks_valid_2() {
		solver_t solver(2);
		solver.add_orderbook_constraint(5, 20, get_category(0, 1));
		solver.add_orderbook_constraint(10, 20, get_category(1, 0));
		TS_ASSERT(solver.check_feasibility());
	}

	void test_two_orderbooks_valid_3() {
		solver_t solver(2);
		solver.add_orderbook_constraint(0, 200, get_category(0, 1));
		solver.add_orderbook_constraint(200, 201, get_category(1, 0));
		TS_ASSERT(solver.check_feasibility());
	}

	void test_two_orderbooks_invalid_1() {
		solver_t solver(2);
		solver.add_orderbook_constraint(0, 20, get_category(0, 1));
		solver.add_orderbook_constraint(30, 40, get_category(1, 0));
		TS_ASSERT(!solver.check_feasibility());
	}

	void test_two_orderbooks_invalid_2() {
		solver_t solver(2);
		solver.add_orderbook_constraint(19, 20, get_category(0, 1));
		solver.add_orderbook_constraint(30, 40, get_category(1, 0));
		TS_ASSERT(!solver.check_feasibility());
	}

	void test_three_orderbooks_valid_1() {
		solver_t solver(3);
		solver.add_orderbook_constraint(0, 10, get_category(0, 1));
		solver.add_orderbook_constraint(0, 100, get_category(1, 2));
		solver.add_orderbook_constraint(0, 20, get_category(2, 0));
		TS_ASSERT(solver.check_feasibility());
	}

	void test_three_orderbooks_valid_2() {
		solver_t solver(3);
		solver.add_orderbook_constraint(0, 10, get_category(0, 1));
		solver.add_orderbook_constraint(10, 100, get_category(1, 2));
		solver.add_orderbook_constraint(0, 20, get_category(2, 0));
		TS_ASSERT(solver.check_feasibility());
	}

	void test_three_orderbooks_valid_3() {
		solver_t solver(3);
		solver.add_orderbook_constraint(0, 10, get_category(0, 1));
		solver.add_orderbook_constraint(0, 100, get_category(1, 2));
		solver.add_orderbook_constraint(10, 20, get_category(2, 0));
		TS_ASSERT(solver.check_feasibility());
	}

	void test_three_orderbooks_invalid_1() {
		solver_t solver(3);
		solver.add_orderbook_constraint(0, 10, get_category(0, 1));
		solver.add_orderbook_constraint(11, 100, get_category(1, 2));
		solver.add_orderbook_constraint(0, 20, get_category(2, 0));
		TS_ASSERT(!solver.check_feasibility());
	}


};
