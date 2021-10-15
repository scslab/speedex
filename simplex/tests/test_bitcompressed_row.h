#include <cxxtest/TestSuite.h>

#include "simplex/bitcompressed_row.h"

using namespace speedex;

class RowTests : public CxxTest::TestSuite {

public:

	void test_set_small() {
		BitcompressedRow row(10);

		row.set_pos(1);

		row.set_neg(4);

		TS_ASSERT_EQUALS(row[0], 0);
		TS_ASSERT_EQUALS(row[1], 1)
		TS_ASSERT_EQUALS(row[2], 0);
		TS_ASSERT_EQUALS(row[3], 0)
		TS_ASSERT_EQUALS(row[4], -1);
		TS_ASSERT_EQUALS(row[5], 0);
	}

	void test_set_larger() {
		BitcompressedRow row(100);

		row.set_pos(1);
		row.set_neg(4);
		row.set_pos(50);
		row.set_pos(64);
		row.set_neg(65);


		TS_ASSERT_EQUALS(row[0], 0);
		TS_ASSERT_EQUALS(row[1], 1)
		TS_ASSERT_EQUALS(row[2], 0);
		TS_ASSERT_EQUALS(row[3], 0)
		TS_ASSERT_EQUALS(row[4], -1);
		TS_ASSERT_EQUALS(row[5], 0);

		TS_ASSERT_EQUALS(row[49], 0);
		TS_ASSERT_EQUALS(row[50], 1);
		TS_ASSERT_EQUALS(row[51], 0);
		TS_ASSERT_EQUALS(row[63], 0);
		TS_ASSERT_EQUALS(row[64], 1);
		TS_ASSERT_EQUALS(row[65], -1);
		TS_ASSERT_EQUALS(row[66], 0);
	}

	void test_add() {
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

		TS_ASSERT_EQUALS(row1[0], 1);
		TS_ASSERT_EQUALS(row1[1], 1);
		TS_ASSERT_EQUALS(row1[2], 0);
		TS_ASSERT_EQUALS(row1[3], 0);
		TS_ASSERT_EQUALS(row1[4], -1);
		TS_ASSERT_EQUALS(row1[5], -1);
		TS_ASSERT_EQUALS(row1[6], 0);
	}

	void test_negate() {
		BitcompressedRow row(10);
		row.set_pos(1);
		row.set_neg(4);

		row.negate();

		TS_ASSERT_EQUALS(row[0], 0);
		TS_ASSERT_EQUALS(row[1], -1)
		TS_ASSERT_EQUALS(row[2], 0);
		TS_ASSERT_EQUALS(row[3], 0)
		TS_ASSERT_EQUALS(row[4], 1);
		TS_ASSERT_EQUALS(row[5], 0);

		row.negate();

		TS_ASSERT_EQUALS(row[0], 0);
		TS_ASSERT_EQUALS(row[1], 1)
		TS_ASSERT_EQUALS(row[2], 0);
		TS_ASSERT_EQUALS(row[3], 0)
		TS_ASSERT_EQUALS(row[4], -1);
		TS_ASSERT_EQUALS(row[5], 0);
	}

};
