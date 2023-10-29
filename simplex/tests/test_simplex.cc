#include <catch2/catch_test_macros.hpp>

#include "xdr/types.h"

#include "simplex/simplex.h"

using xdr::operator==;

namespace speedex
{

namespace test
{

OfferCategory get_category(AssetID sell, AssetID buy) {
	OfferCategory out;
	out.type = OfferType::SELL;
	out.sellAsset = sell;
	out.buyAsset = buy;
	return out;
}

TEST_CASE("2asset empty", "[simplex]")
{	
	TaxFreeSimplex simplex(2);

	simplex.solve();

	REQUIRE(simplex.get_solution(get_category(0, 1)) == 0);
	REQUIRE(simplex.get_solution(get_category(1, 0)) == 0);
}

TEST_CASE("2asset 1sided", "[simplex]")
{
	TaxFreeSimplex simplex(2);

	simplex.add_orderbook_constraint(100, get_category(0, 1));

	simplex.solve();
	REQUIRE(simplex.get_solution(get_category(0, 1)) == 0);
	REQUIRE(simplex.get_solution(get_category(1, 0)) == 0);
}

TEST_CASE("2asset 2sided", "[simplex]")
{
	TaxFreeSimplex simplex(2);

	simplex.add_orderbook_constraint(100, get_category(0, 1));
	simplex.add_orderbook_constraint(500, get_category(1, 0));

	simplex.solve();
	REQUIRE(simplex.get_solution(get_category(0, 1)) == 100);
	REQUIRE(simplex.get_solution(get_category(1, 0)) == 100);
}

TEST_CASE("2asset with extra assets", "[simplex]")
{
	TaxFreeSimplex simplex(40);

	simplex.add_orderbook_constraint(100, get_category(0, 1));
	simplex.add_orderbook_constraint(500, get_category(1, 0));

	simplex.solve();
	REQUIRE(simplex.get_solution(get_category(0, 1)) == 100);
	REQUIRE(simplex.get_solution(get_category(1, 0)) == 100);

	REQUIRE(simplex.get_solution(get_category(10, 15)) == 0);
}

TEST_CASE("3asset", "[simplex]")
{
	TaxFreeSimplex simplex(3);

	simplex.add_orderbook_constraint(100, get_category(0, 1));
	simplex.add_orderbook_constraint(100, get_category(1, 2));
	simplex.add_orderbook_constraint(300, get_category(2, 0));

	simplex.solve();

	REQUIRE(simplex.get_solution(get_category(0, 1)) == 100);
	REQUIRE(simplex.get_solution(get_category(1, 2)) == 100);
	REQUIRE(simplex.get_solution(get_category(2, 0)) == 100);
}

}

} // namespace speedex
