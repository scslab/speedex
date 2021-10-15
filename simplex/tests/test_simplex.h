#include <cxxtest/TestSuite.h>

#include "xdr/types.h"

#include "simplex/simplex.h"

using namespace speedex;

class SimplexTests : public CxxTest::TestSuite {

	OfferCategory get_category(AssetID sell, AssetID buy) {
		OfferCategory out;
		out.type = OfferType::SELL;
		out.sellAsset = sell;
		out.buyAsset = buy;
		return out;
	}

public:

	void test_2asset_empty() {
		TaxFreeSimplex simplex(2);

		simplex.solve();

		TS_ASSERT_EQUALS(simplex.get_solution(get_category(0, 1)), 0);
		TS_ASSERT_EQUALS(simplex.get_solution(get_category(1, 0)), 0);
	}

	void test_2asset_1sided() {
		TaxFreeSimplex simplex(2);

		simplex.add_orderbook_constraint(100, get_category(0, 1));

		simplex.solve();
		TS_ASSERT_EQUALS(simplex.get_solution(get_category(0, 1)), 0);
		TS_ASSERT_EQUALS(simplex.get_solution(get_category(1, 0)), 0);
	}

	void test_2asset_2sided() {

		TaxFreeSimplex simplex(2);

		simplex.add_orderbook_constraint(100, get_category(0, 1));
		simplex.add_orderbook_constraint(500, get_category(1, 0));

		simplex.solve();
		TS_ASSERT_EQUALS(simplex.get_solution(get_category(0, 1)), 100);
		TS_ASSERT_EQUALS(simplex.get_solution(get_category(1, 0)), 100);
	}

	void test_2asset_with_extras() {
		TaxFreeSimplex simplex(40);

		simplex.add_orderbook_constraint(100, get_category(0, 1));
		simplex.add_orderbook_constraint(500, get_category(1, 0));

		simplex.solve();
		TS_ASSERT_EQUALS(simplex.get_solution(get_category(0, 1)), 100);
		TS_ASSERT_EQUALS(simplex.get_solution(get_category(1, 0)), 100);

		TS_ASSERT_EQUALS(simplex.get_solution(get_category(10, 15)), 0);
	}

};
