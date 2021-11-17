#include <cxxtest/TestSuite.h>

#include "xdr/types.h"

#include "simplex/sparse.h"

using namespace speedex;

class TableauTests : public CxxTest::TestSuite {

public:

	void test_set() {
		SparseTableau tableau(5);

		tableau.add_row();

		tableau.set(0, 3, 1);
		tableau.set(0, 2, -1);

		TS_ASSERT_EQUALS(tableau.get(0, 2), -1);

		tableau.rows[0].negate();

		TS_ASSERT_EQUALS(tableau.get(0, 3), -1);
	}

	void test_multirow() {
		SparseTableau tableau(5);

		tableau.add_row();
		tableau.rows[0].negate();
		tableau.add_row();
		tableau.rows[1].negate();
		tableau.add_row();

		tableau.set(0, 3, 1);
		tableau.set(0, 1, -1);
		tableau.set(0, 4, 1);

		tableau.integrity_check(false);

		tableau.set(2, 3, 1);

		tableau.set(1, 2, 1);
		tableau.set(1, 3, 1);
		tableau.set(1, 4, -1);

		tableau.set(2, 1, -1);
		tableau.set(2, 4, -1);

		tableau.integrity_check(false);
	}

	void test_pivot_row_select() {
		SparseTableau tableau(5);

		tableau.add_row();
		tableau.rows[0].negate();
		tableau.add_row();
		tableau.rows[1].negate();
		tableau.add_row();

		tableau.set(0, 3, 1);
		tableau.set(0, 1, -1);
		tableau.set(0, 4, 1);

		tableau.set(1, 1, 1);
		tableau.set(1, 4, 1);
		tableau.rows[1].negate();
		tableau.set(1, 3, 1);

		tableau.set(1, 0, -1);

		tableau.set(0, 0, 1);
		tableau.set(2, 0, 1);

		tableau.rows[0].set_value(100);
		tableau.rows[1].set_value(200);
		tableau.rows[2].set_value(50);

		tableau.integrity_check(false);

		TS_ASSERT_EQUALS(tableau.get_pivot_row(0), 2);

		TS_ASSERT_THROWS_ANYTHING(tableau.get_pivot_row(1));

		TS_ASSERT_EQUALS(tableau.get_pivot_row(4), 0);


	}

};
