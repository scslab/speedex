#include <catch2/catch_test_macros.hpp>

#include <cstdint>

#include "memory_database/revertable_asset.h"

namespace speedex
{


// ensure that we never introduce the type of bug
// that I almost introduced when I nearly 
// swapped a >=0 check for an overflow check
TEST_CASE("transfer available", "[asset]")
{
	RevertableAsset asset(100);

	SECTION("add")
	{
		REQUIRE(asset.conditional_transfer_available(99));
		REQUIRE(asset.conditional_transfer_available(1));
		REQUIRE(asset.conditional_transfer_available(-200));
		REQUIRE(!asset.conditional_transfer_available(-1));

		REQUIRE(asset.commit() == 0);

		REQUIRE(asset.in_valid_state());
	}
	SECTION("unconditional add")
	{
		(asset.transfer_available(-100));
		(asset.transfer_available(-1));

		REQUIRE(asset.commit() == -1);

		REQUIRE(!asset.in_valid_state());
	}

	SECTION("add overflow")
	{

		REQUIRE(asset.conditional_transfer_available(INT64_MAX));
		REQUIRE(!asset.in_valid_state());
	}

// overflow tracking not enabled,
// overflows in validation prevented by asset quantity limits
// and asset transfer amount limits
/*	SECTION("add overflow to pos value")
	{
		asset.transfer_available(INT64_MAX);
		asset.transfer_available(INT64_MAX);
		REQUIRE(!asset.in_valid_state());
	} */

	SECTION("subtract overflow")
	{
		asset.transfer_available(INT64_MIN);

		REQUIRE(!asset.conditional_transfer_available(-100));

		REQUIRE(asset.commit() == INT64_MIN + 100);

		REQUIRE(!asset.in_valid_state());
	}
}

TEST_CASE("escrow negation check", "[asset]")
{
	RevertableAsset asset(0);

	REQUIRE(!asset.conditional_escrow(INT64_MIN));
}

TEST_CASE("escrow negation+1 check", "[asset]")
{
	RevertableAsset asset(INT64_MAX);
	REQUIRE(asset.conditional_escrow(INT64_MIN + 1));
}


}