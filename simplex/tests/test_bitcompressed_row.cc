#include <catch2/catch_test_macros.hpp>

#include "simplex/bitcompressed_row.h"

namespace speedex {


TEST_CASE("small set", "[simplex]")
{
	BitcompressedRow row(10);

	row.set_pos(1);

	row.set_neg(4);

	REQUIRE(row[0] == 0);
	REQUIRE(row[1] == 1);
	REQUIRE(row[2] == 0);
	REQUIRE(row[3] == 0);
	REQUIRE(row[4] == -1);
	REQUIRE(row[5] == 0);
}

TEST_CASE("larger set", "[simplex]")
{
	BitcompressedRow row(100);

	row.set_pos(1);
	row.set_neg(4);
	row.set_pos(50);
	row.set_pos(64);
	row.set_neg(65);


	REQUIRE(row[0] == 0);
	REQUIRE(row[1] == 1);
	REQUIRE(row[2] == 0);
	REQUIRE(row[3] == 0);
	REQUIRE(row[4] == -1);
	REQUIRE(row[5] == 0);

	REQUIRE(row[49] == 0);
	REQUIRE(row[50] == 1);
	REQUIRE(row[51] == 0);
	REQUIRE(row[63] == 0);
	REQUIRE(row[64] == 1);
	REQUIRE(row[65] == -1);
	REQUIRE(row[66] == 0);
}

TEST_CASE("add", "[simplex]")
{
	BitcompressedRow row1(10);
	BitcompressedRow row2(10);

	row1.set_pos(1);
	row2.set_pos(0);

	row1.set_neg(2);
	row2.set_pos(2);

	row1.set_pos(3);
	row2.set_neg(3);

	row1.set_neg(4);
	row2.set_neg(5);

	row1 += row2;

	REQUIRE(row1[0] == 1);
	REQUIRE(row1[1] == 1);
	REQUIRE(row1[2] == 0);
	REQUIRE(row1[3] == 0);
	REQUIRE(row1[4] == -1);
	REQUIRE(row1[5] == -1);
	REQUIRE(row1[6] == 0);
}

TEST_CASE("negate row", "[simplex]")
{
	BitcompressedRow row(10);
	row.set_pos(1);
	row.set_neg(4);

	row.negate();

	REQUIRE(row[0] == 0);
	REQUIRE(row[1] == -1);
	REQUIRE(row[2] == 0);
	REQUIRE(row[3] == 0);
	REQUIRE(row[4] == 1);
	REQUIRE(row[5] == 0);

	row.negate();

	REQUIRE(row[0] == 0);
	REQUIRE(row[1] == 1);
	REQUIRE(row[2] == 0);
	REQUIRE(row[3] == 0);
	REQUIRE(row[4] == -1);
	REQUIRE(row[5] == 0);
}

}
